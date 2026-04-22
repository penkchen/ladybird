/*
 * Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Platform.h>
#include <LibGfx/Bitmap.h>
#include <LibGfx/SharedImageBuffer.h>
#include <LibWeb/HTML/VisibilityState.h>
#include <LibWeb/Page/InputEvent.h>
#include <LibWeb/UIEvents/KeyCode.h>
#include <LibWeb/UIEvents/MouseButton.h>
#include <LibWebView/URL.h>
#include <UI/Qt/StringUtils.h>
#include <UI/QtQuick/Application.h>
#include <UI/QtQuick/Platform.h>
#include <UI/QtQuick/WebContentView.h>

#include <LibWebView/Menu.h>

#include <QCursor>
#include <QFocusEvent>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPixmap>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGTexture>
#include <QStyleHints>
#include <QVariantMap>
#include <QWheelEvent>

namespace Ladybird {

WebContentView::WebContentView(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFlag(ItemIsFocusScope, true);
    setActiveFocusOnTab(true);

    // FIXME: IME composition (CJK) needs a preedit IPC to WebContent.

    m_device_pixel_ratio = 1.0;
    if (auto* w = window())
        m_device_pixel_ratio = w->devicePixelRatio();

    initialize_client(CreateNewClient::Yes);

    set_system_visibility_state(isVisible()
            ? Web::HTML::VisibilityState::Visible
            : Web::HTML::VisibilityState::Hidden);

    on_ready_to_paint = [this]() {
        update();
    };

    on_cursor_change = [this](auto cursor) {
        update_cursor(cursor);
    };

    on_finish_handling_key_event = [this](auto const& event) {
        finish_handling_key_event(event);
    };

    on_url_change = [this](auto const&) {
        emit url_changed();
    };

    on_title_change = [this](auto const&) {
        emit title_changed();
    };

    on_load_start = [this](auto const&, bool) {
        m_is_loading = true;
        emit loading_changed();
    };

    on_load_finish = [this](auto const&) {
        m_is_loading = false;
        emit loading_changed();
    };

    auto show_menu = [this](WebView::Menu& menu) {
        return [this, &menu](Gfx::IntPoint position) {
            QVariantList items;
            for (auto& item : menu.items()) {
                item.visit(
                    [&](NonnullRefPtr<WebView::Action>& action) {
                        if (!action->visible())
                            return;
                        QVariantMap entry;
                        entry["isSeparator"] = false;
                        entry["id"] = static_cast<int>(action->id());
                        entry["text"] = qstring_from_ak_string(action->text());
                        entry["enabled"] = action->enabled();
                        entry["checkable"] = action->is_checkable();
                        entry["checked"] = action->is_checkable() ? action->checked() : false;
                        items.append(entry);
                    },
                    [&](NonnullRefPtr<WebView::Menu>&) {
                        // FIXME: Submenus are not rendered yet.
                    },
                    [&](WebView::Separator) {
                        QVariantMap entry;
                        entry["isSeparator"] = true;
                        items.append(entry);
                    });
            }
            auto logical = QPointF(position.x() / m_device_pixel_ratio, position.y() / m_device_pixel_ratio);
            emit context_menu_requested(logical, items);
        };
    };
    page_context_menu().on_activation = show_menu(page_context_menu());
    link_context_menu().on_activation = show_menu(link_context_menu());
    image_context_menu().on_activation = show_menu(image_context_menu());
    media_context_menu().on_activation = show_menu(media_context_menu());
}

void WebContentView::activate_context_menu_action(int action_id)
{
    auto target = static_cast<WebView::ActionID>(action_id);
    auto try_activate = [&](WebView::Menu& menu) {
        bool found = false;
        menu.for_each_action([&](WebView::Action& action) {
            if (!found && action.id() == target) {
                action.activate();
                found = true;
            }
        });
        return found;
    };
    try_activate(page_context_menu())
        || try_activate(link_context_menu())
        || try_activate(image_context_menu())
        || try_activate(media_context_menu());
}

WebContentView::~WebContentView()
{
    auto& application = static_cast<Application&>(WebView::Application::the());
    if (auto active = WebView::Application::the().active_web_view(); active.has_value() && &active.value() == this)
        application.set_active_view(nullptr);
#if defined(AK_OS_MACOS) || defined(AK_OS_LINUX)
    release_native_texture(m_pinned_native_texture);
#endif
}

void WebContentView::load_url(QString const& url)
{
    auto maybe_url = WebView::sanitize_url(ak_string_from_qstring(url));
    if (maybe_url.has_value())
        load(maybe_url.release_value());
}

void WebContentView::go_back()
{
    traverse_the_history_by_delta(-1);
}

void WebContentView::go_forward()
{
    traverse_the_history_by_delta(1);
}

void WebContentView::reload()
{
    ViewImplementation::reload();
}

QString WebContentView::url_string() const
{
    return qstring_from_ak_string(url().serialize());
}

QString WebContentView::title_string() const
{
    return qstring_from_ak_string(title().to_utf8());
}

void WebContentView::initialize_client(WebView::ViewImplementation::CreateNewClient create_new_client)
{
    ViewImplementation::initialize_client(create_new_client);
}

Web::DevicePixelSize WebContentView::viewport_size() const
{
    return m_viewport_size.to_type<Web::DevicePixels>();
}

Gfx::IntPoint WebContentView::to_content_position(Gfx::IntPoint widget_position) const
{
    return widget_position;
}

Gfx::IntPoint WebContentView::to_widget_position(Gfx::IntPoint content_position) const
{
    return content_position;
}

void WebContentView::update_viewport_size()
{
    auto scaled_width = int(width() * m_device_pixel_ratio);
    auto scaled_height = int(height() * m_device_pixel_ratio);
    m_viewport_size = Gfx::IntSize(scaled_width, scaled_height);
    handle_resize();
}

void WebContentView::geometryChange(QRectF const& new_geometry, QRectF const& old_geometry)
{
    QQuickItem::geometryChange(new_geometry, old_geometry);
    if (auto* w = window())
        m_device_pixel_ratio = w->devicePixelRatio();
    update_viewport_size();
}

void WebContentView::itemChange(ItemChange change, ItemChangeData const& value)
{
    QQuickItem::itemChange(change, value);

    if (change == ItemVisibleHasChanged) {
        set_system_visibility_state(value.boolValue
                ? Web::HTML::VisibilityState::Visible
                : Web::HTML::VisibilityState::Hidden);
    } else if (change == ItemDevicePixelRatioHasChanged) {
        m_device_pixel_ratio = value.realValue;
        update_viewport_size();
    }
}

QSGNode* WebContentView::updatePaintNode(QSGNode* old_node, UpdatePaintNodeData*)
{
    Gfx::SharedImageBuffer const* buffer = nullptr;
    Gfx::IntSize bitmap_size;

    if (m_client_state.has_usable_bitmap) {
        buffer = m_client_state.front_bitmap.shared_image_buffer.ptr();
        bitmap_size = m_client_state.front_bitmap.last_painted_size.to_type<int>();
    } else if (m_backup_shared_image_buffer) {
        buffer = m_backup_shared_image_buffer.ptr();
        bitmap_size = m_backup_bitmap_size.to_type<int>();
    }

    if (!buffer || bitmap_size.is_empty()) {
        delete old_node;
        return nullptr;
    }

    // QSGSimpleTextureNode misrenders under the RHI scenegraph backend; use QSGImageNode.
    auto* node = static_cast<QSGImageNode*>(old_node);
    if (!node) {
        node = window()->createImageNode();
        node->setFiltering(QSGTexture::Nearest);
        node->setOwnsTexture(true);
    }

    QSGTexture* texture = nullptr;

#ifdef AK_OS_MACOS
    texture = acquire_iosurface_texture(window(), buffer->iosurface_handle(),
        QSize(bitmap_size.width(), bitmap_size.height()),
        &m_pinned_native_texture);
#endif

#if defined(AK_OS_LINUX) && defined(USE_VULKAN_DMABUF_IMAGES)
    if (auto const& dmabuf_handle = buffer->linux_dmabuf_handle(); dmabuf_handle.has_value()) {
        texture = acquire_linux_dmabuf_texture(window(), *dmabuf_handle,
            QSize(dmabuf_handle->size.width(), dmabuf_handle->size.height()),
            &m_pinned_native_texture);
    }
#endif

    if (!texture) {
        auto bitmap = buffer->bitmap();
        QImage full(bitmap->scanline_u8(0), bitmap->width(), bitmap->height(),
            static_cast<qsizetype>(bitmap->pitch()), QImage::Format_RGB32);
        auto cropped = full.copy(0, 0, bitmap_size.width(), bitmap_size.height());
        texture = window()->createTextureFromImage(cropped, QQuickWindow::TextureIsOpaque);
    }

    // A stale front bitmap from before a resize can be wider than the item; clip to avoid overflow.
    auto draw_w = min(bitmap_size.width(), m_viewport_size.width());
    auto draw_h = min(bitmap_size.height(), m_viewport_size.height());

    node->setTexture(texture);
    node->setRect(QRectF(0, 0, draw_w / m_device_pixel_ratio, draw_h / m_device_pixel_ratio));
    node->setSourceRect(QRectF(0, 0, draw_w, draw_h));

    return node;
}

static Web::UIEvents::MouseButton get_button_from_qt_mouse_button(Qt::MouseButton button)
{
    if (button == Qt::MouseButton::LeftButton)
        return Web::UIEvents::MouseButton::Primary;
    if (button == Qt::MouseButton::RightButton)
        return Web::UIEvents::MouseButton::Secondary;
    if (button == Qt::MouseButton::MiddleButton)
        return Web::UIEvents::MouseButton::Middle;
    if (button == Qt::MouseButton::BackButton)
        return Web::UIEvents::MouseButton::Backward;
    if (button == Qt::MouseButton::ForwardButton)
        return Web::UIEvents::MouseButton::Forward;
    return Web::UIEvents::MouseButton::None;
}

static Web::UIEvents::MouseButton get_buttons_from_qt_mouse_buttons(Qt::MouseButtons buttons)
{
    auto result = Web::UIEvents::MouseButton::None;
    if (buttons.testFlag(Qt::MouseButton::LeftButton))
        result |= Web::UIEvents::MouseButton::Primary;
    if (buttons.testFlag(Qt::MouseButton::RightButton))
        result |= Web::UIEvents::MouseButton::Secondary;
    if (buttons.testFlag(Qt::MouseButton::MiddleButton))
        result |= Web::UIEvents::MouseButton::Middle;
    if (buttons.testFlag(Qt::MouseButton::BackButton))
        result |= Web::UIEvents::MouseButton::Backward;
    if (buttons.testFlag(Qt::MouseButton::ForwardButton))
        result |= Web::UIEvents::MouseButton::Forward;
    return result;
}

static Web::UIEvents::KeyModifier get_modifiers_from_qt_keyboard_modifiers(Qt::KeyboardModifiers modifiers)
{
    auto result = Web::UIEvents::KeyModifier::Mod_None;
    if (modifiers.testFlag(Qt::AltModifier))
        result |= Web::UIEvents::KeyModifier::Mod_Alt;
    if (modifiers.testFlag(Qt::ControlModifier))
        result |= Web::UIEvents::KeyModifier::Mod_Ctrl;
    if (modifiers.testFlag(Qt::MetaModifier))
        result |= Web::UIEvents::KeyModifier::Mod_Super;
    if (modifiers.testFlag(Qt::ShiftModifier))
        result |= Web::UIEvents::KeyModifier::Mod_Shift;
    if (modifiers.testFlag(Qt::KeypadModifier))
        result |= Web::UIEvents::KeyModifier::Mod_Keypad;
    return result;
}

static Web::UIEvents::KeyCode get_keycode_from_qt_key_event(QKeyEvent const& event)
{
    struct Mapping {
        constexpr Mapping(Qt::Key q, Web::UIEvents::KeyCode s)
            : qt_key(q)
            , web_key(s)
        {
        }

        Qt::Key qt_key;
        Web::UIEvents::KeyCode web_key;
    };

    // FIXME: Qt doesn't distinguish left vs right modifier keys; we default to the left variant.
    static constexpr Mapping mappings[] = {
        { Qt::Key_0, Web::UIEvents::Key_0 },
        { Qt::Key_1, Web::UIEvents::Key_1 },
        { Qt::Key_2, Web::UIEvents::Key_2 },
        { Qt::Key_3, Web::UIEvents::Key_3 },
        { Qt::Key_4, Web::UIEvents::Key_4 },
        { Qt::Key_5, Web::UIEvents::Key_5 },
        { Qt::Key_6, Web::UIEvents::Key_6 },
        { Qt::Key_7, Web::UIEvents::Key_7 },
        { Qt::Key_8, Web::UIEvents::Key_8 },
        { Qt::Key_9, Web::UIEvents::Key_9 },
        { Qt::Key_A, Web::UIEvents::Key_A },
        { Qt::Key_Alt, Web::UIEvents::Key_LeftAlt },
        { Qt::Key_Ampersand, Web::UIEvents::Key_Ampersand },
        { Qt::Key_Apostrophe, Web::UIEvents::Key_Apostrophe },
        { Qt::Key_AsciiCircum, Web::UIEvents::Key_Circumflex },
        { Qt::Key_AsciiTilde, Web::UIEvents::Key_Tilde },
        { Qt::Key_Asterisk, Web::UIEvents::Key_Asterisk },
        { Qt::Key_At, Web::UIEvents::Key_AtSign },
        { Qt::Key_B, Web::UIEvents::Key_B },
        { Qt::Key_Backslash, Web::UIEvents::Key_Backslash },
        { Qt::Key_Backspace, Web::UIEvents::Key_Backspace },
        { Qt::Key_Bar, Web::UIEvents::Key_Pipe },
        { Qt::Key_BraceLeft, Web::UIEvents::Key_LeftBrace },
        { Qt::Key_BraceRight, Web::UIEvents::Key_RightBrace },
        { Qt::Key_BracketLeft, Web::UIEvents::Key_LeftBracket },
        { Qt::Key_BracketRight, Web::UIEvents::Key_RightBracket },
        { Qt::Key_C, Web::UIEvents::Key_C },
        { Qt::Key_CapsLock, Web::UIEvents::Key_CapsLock },
        { Qt::Key_Colon, Web::UIEvents::Key_Colon },
        { Qt::Key_Comma, Web::UIEvents::Key_Comma },
        { Qt::Key_Control, Web::UIEvents::Key_LeftControl },
        { Qt::Key_D, Web::UIEvents::Key_D },
        { Qt::Key_Delete, Web::UIEvents::Key_Delete },
        { Qt::Key_Dollar, Web::UIEvents::Key_Dollar },
        { Qt::Key_Down, Web::UIEvents::Key_Down },
        { Qt::Key_E, Web::UIEvents::Key_E },
        { Qt::Key_End, Web::UIEvents::Key_End },
        { Qt::Key_Equal, Web::UIEvents::Key_Equal },
        { Qt::Key_Enter, Web::UIEvents::Key_Return },
        { Qt::Key_Escape, Web::UIEvents::Key_Escape },
        { Qt::Key_Exclam, Web::UIEvents::Key_ExclamationPoint },
        { Qt::Key_exclamdown, Web::UIEvents::Key_ExclamationPoint },
        { Qt::Key_F, Web::UIEvents::Key_F },
        { Qt::Key_F1, Web::UIEvents::Key_F1 },
        { Qt::Key_F10, Web::UIEvents::Key_F10 },
        { Qt::Key_F11, Web::UIEvents::Key_F11 },
        { Qt::Key_F12, Web::UIEvents::Key_F12 },
        { Qt::Key_F2, Web::UIEvents::Key_F2 },
        { Qt::Key_F3, Web::UIEvents::Key_F3 },
        { Qt::Key_F4, Web::UIEvents::Key_F4 },
        { Qt::Key_F5, Web::UIEvents::Key_F5 },
        { Qt::Key_F6, Web::UIEvents::Key_F6 },
        { Qt::Key_F7, Web::UIEvents::Key_F7 },
        { Qt::Key_F8, Web::UIEvents::Key_F8 },
        { Qt::Key_F9, Web::UIEvents::Key_F9 },
        { Qt::Key_G, Web::UIEvents::Key_G },
        { Qt::Key_Greater, Web::UIEvents::Key_GreaterThan },
        { Qt::Key_H, Web::UIEvents::Key_H },
        { Qt::Key_Home, Web::UIEvents::Key_Home },
        { Qt::Key_I, Web::UIEvents::Key_I },
        { Qt::Key_Insert, Web::UIEvents::Key_Insert },
        { Qt::Key_J, Web::UIEvents::Key_J },
        { Qt::Key_K, Web::UIEvents::Key_K },
        { Qt::Key_L, Web::UIEvents::Key_L },
        { Qt::Key_Left, Web::UIEvents::Key_Left },
        { Qt::Key_Less, Web::UIEvents::Key_LessThan },
        { Qt::Key_M, Web::UIEvents::Key_M },
        { Qt::Key_Menu, Web::UIEvents::Key_Menu },
        { Qt::Key_Meta, Web::UIEvents::Key_LeftSuper },
        { Qt::Key_Minus, Web::UIEvents::Key_Minus },
        { Qt::Key_N, Web::UIEvents::Key_N },
        { Qt::Key_NumberSign, Web::UIEvents::Key_Hashtag },
        { Qt::Key_NumLock, Web::UIEvents::Key_NumLock },
        { Qt::Key_O, Web::UIEvents::Key_O },
        { Qt::Key_P, Web::UIEvents::Key_P },
        { Qt::Key_PageDown, Web::UIEvents::Key_PageDown },
        { Qt::Key_PageUp, Web::UIEvents::Key_PageUp },
        { Qt::Key_ParenLeft, Web::UIEvents::Key_LeftParen },
        { Qt::Key_ParenRight, Web::UIEvents::Key_RightParen },
        { Qt::Key_Percent, Web::UIEvents::Key_Percent },
        { Qt::Key_Period, Web::UIEvents::Key_Period },
        { Qt::Key_Plus, Web::UIEvents::Key_Plus },
        { Qt::Key_Print, Web::UIEvents::Key_PrintScreen },
        { Qt::Key_Q, Web::UIEvents::Key_Q },
        { Qt::Key_Question, Web::UIEvents::Key_QuestionMark },
        { Qt::Key_QuoteDbl, Web::UIEvents::Key_DoubleQuote },
        { Qt::Key_QuoteLeft, Web::UIEvents::Key_Backtick },
        { Qt::Key_R, Web::UIEvents::Key_R },
        { Qt::Key_Return, Web::UIEvents::Key_Return },
        { Qt::Key_Right, Web::UIEvents::Key_Right },
        { Qt::Key_S, Web::UIEvents::Key_S },
        { Qt::Key_ScrollLock, Web::UIEvents::Key_ScrollLock },
        { Qt::Key_Semicolon, Web::UIEvents::Key_Semicolon },
        { Qt::Key_Shift, Web::UIEvents::Key_LeftShift },
        { Qt::Key_Slash, Web::UIEvents::Key_Slash },
        { Qt::Key_Space, Web::UIEvents::Key_Space },
        { Qt::Key_Super_L, Web::UIEvents::Key_LeftSuper },
        { Qt::Key_Super_R, Web::UIEvents::Key_RightSuper },
        { Qt::Key_SysReq, Web::UIEvents::Key_SysRq },
        { Qt::Key_T, Web::UIEvents::Key_T },
        { Qt::Key_Tab, Web::UIEvents::Key_Tab },
        { Qt::Key_U, Web::UIEvents::Key_U },
        { Qt::Key_Underscore, Web::UIEvents::Key_Underscore },
        { Qt::Key_Up, Web::UIEvents::Key_Up },
        { Qt::Key_V, Web::UIEvents::Key_V },
        { Qt::Key_W, Web::UIEvents::Key_W },
        { Qt::Key_X, Web::UIEvents::Key_X },
        { Qt::Key_Y, Web::UIEvents::Key_Y },
        { Qt::Key_Z, Web::UIEvents::Key_Z },
    };

    for (auto const& mapping : mappings) {
        if (event.key() == mapping.qt_key)
            return mapping.web_key;
    }
    return Web::UIEvents::Key_Invalid;
}

void WebContentView::enqueue_native_mouse_event(Web::MouseEvent::Type type, QSinglePointEvent const& event)
{
    Web::DevicePixelPoint position {
        static_cast<int>(event.position().x() * m_device_pixel_ratio),
        static_cast<int>(event.position().y() * m_device_pixel_ratio),
    };
    Web::DevicePixelPoint screen_position {
        static_cast<int>(event.globalPosition().x() * m_device_pixel_ratio),
        static_cast<int>(event.globalPosition().y() * m_device_pixel_ratio),
    };

    auto button = get_button_from_qt_mouse_button(event.button());
    auto buttons = get_buttons_from_qt_mouse_buttons(event.buttons());
    auto modifiers = get_modifiers_from_qt_keyboard_modifiers(event.modifiers());

    if (button == Web::UIEvents::MouseButton::None
        && (type == Web::MouseEvent::Type::MouseDown || type == Web::MouseEvent::Type::MouseUp)) {
        return;
    }

    int wheel_delta_x = 0;
    int wheel_delta_y = 0;

    if (type == Web::MouseEvent::Type::MouseWheel) {
        auto const& wheel_event = static_cast<QWheelEvent const&>(event);

        if (auto pixel_delta = -wheel_event.pixelDelta(); !pixel_delta.isNull()) {
            wheel_delta_x = pixel_delta.x();
            wheel_delta_y = pixel_delta.y();
        } else {
            auto angle_delta = -wheel_event.angleDelta();
            auto delta_x = -static_cast<float>(angle_delta.x()) / 120.0f;
            auto delta_y = static_cast<float>(angle_delta.y()) / 120.0f;

            static constexpr float scroll_step_size = 24.0f;
            auto wheel_scroll_lines = static_cast<float>(QGuiApplication::styleHints()->wheelScrollLines());
            auto step_x = delta_x * wheel_scroll_lines * m_device_pixel_ratio;
            auto step_y = delta_y * wheel_scroll_lines * m_device_pixel_ratio;

            wheel_delta_x = static_cast<int>(step_x * scroll_step_size);
            wheel_delta_y = static_cast<int>(step_y * scroll_step_size);
        }
    }

    enqueue_input_event(Web::MouseEvent {
        type,
        position, screen_position,
        button, buttons, modifiers,
        wheel_delta_x, wheel_delta_y,
        m_click_count,
        nullptr });
}

struct KeyData : Web::BrowserInputData {
    explicit KeyData(QKeyEvent const& event)
        : event(adopt_own(*event.clone()))
    {
    }

    NonnullOwnPtr<QKeyEvent> event;
};

void WebContentView::enqueue_native_key_event(Web::KeyEvent::Type type, QKeyEvent const& event)
{
    auto keycode = get_keycode_from_qt_key_event(event);
    auto modifiers = get_modifiers_from_qt_keyboard_modifiers(event.modifiers());

    auto text = event.text();
    auto code_point = text.isEmpty() ? 0u : text[0].unicode();

    auto build = [&]() -> Web::KeyEvent {
        // Qt maps Shift+Tab to Qt::Key_Backtab; restore it to Tab+Shift.
        if (event.key() == Qt::Key_Backtab)
            return { type, Web::UIEvents::KeyCode::Key_Tab, Web::UIEvents::Mod_Shift, '\t', event.isAutoRepeat(), make<KeyData>(event) };
        if (event.key() == Qt::Key_Enter || event.key() == Qt::Key_Return)
            return { type, Web::UIEvents::KeyCode::Key_Return, modifiers, '\n', event.isAutoRepeat(), make<KeyData>(event) };
        return { type, keycode, modifiers, code_point, event.isAutoRepeat(), make<KeyData>(event) };
    };

    enqueue_input_event(build());
}

void WebContentView::finish_handling_key_event(Web::KeyEvent const& key_event)
{
    auto& data = as<KeyData>(*key_event.browser_data);
    auto& event = *data.event;

    switch (key_event.type) {
    case Web::KeyEvent::Type::KeyDown:
        QQuickItem::keyPressEvent(&event);
        break;
    case Web::KeyEvent::Type::KeyUp:
        QQuickItem::keyReleaseEvent(&event);
        break;
    }
}

void WebContentView::mousePressEvent(QMouseEvent* event)
{
    auto elapsed = event->timestamp() - m_last_click_timestamp;
    auto distance = (event->position() - m_last_click_position).manhattanLength();

    auto const double_click_interval = static_cast<u64>(QGuiApplication::styleHints()->mouseDoubleClickInterval());
    auto const start_drag_distance = QGuiApplication::styleHints()->startDragDistance();

    if (elapsed < double_click_interval && distance < start_drag_distance) {
        ++m_click_count;
        if (m_click_count < 1)
            m_click_count = 1;
    } else {
        m_click_count = 1;
    }
    m_last_click_timestamp = event->timestamp();
    m_last_click_position = event->position();

    enqueue_native_mouse_event(Web::MouseEvent::Type::MouseDown, *event);
    forceActiveFocus();
}

void WebContentView::mouseDoubleClickEvent(QMouseEvent* event)
{
    // Route through mousePressEvent so the click-count logic handles 2nd and 3rd clicks.
    mousePressEvent(event);
}

void WebContentView::mouseMoveEvent(QMouseEvent* event)
{
    enqueue_native_mouse_event(Web::MouseEvent::Type::MouseMove, *event);
}

void WebContentView::mouseReleaseEvent(QMouseEvent* event)
{
    enqueue_native_mouse_event(Web::MouseEvent::Type::MouseUp, *event);

    if (event->button() == Qt::MouseButton::BackButton)
        traverse_the_history_by_delta(-1);
    else if (event->button() == Qt::MouseButton::ForwardButton)
        traverse_the_history_by_delta(1);
}

void WebContentView::wheelEvent(QWheelEvent* event)
{
    // Leave Ctrl+wheel to the outer UI for zoom.
    if (event->modifiers().testFlag(Qt::ControlModifier)) {
        event->ignore();
        return;
    }
    enqueue_native_mouse_event(Web::MouseEvent::Type::MouseWheel, *event);
}

void WebContentView::hoverMoveEvent(QHoverEvent* event)
{
    enqueue_native_mouse_event(Web::MouseEvent::Type::MouseMove, *event);
}

void WebContentView::hoverLeaveEvent(QHoverEvent*)
{
    Web::MouseEvent mouse_event {};
    mouse_event.type = Web::MouseEvent::Type::MouseLeave;
    enqueue_input_event(AK::move(mouse_event));
}

void WebContentView::keyPressEvent(QKeyEvent* event)
{
    enqueue_native_key_event(Web::KeyEvent::Type::KeyDown, *event);
}

void WebContentView::keyReleaseEvent(QKeyEvent* event)
{
    enqueue_native_key_event(Web::KeyEvent::Type::KeyUp, *event);
}

void WebContentView::focusInEvent(QFocusEvent*)
{
    static_cast<Application&>(WebView::Application::the()).set_active_view(this);
    client().async_set_has_focus(m_client_state.page_index, true);
}

void WebContentView::focusOutEvent(QFocusEvent*)
{
    client().async_set_has_focus(m_client_state.page_index, false);
}

void WebContentView::update_cursor(Gfx::Cursor cursor)
{
    cursor.visit([this](Gfx::StandardCursor standard_cursor) {
        switch (standard_cursor) {
        case Gfx::StandardCursor::Hidden:
            setCursor(Qt::BlankCursor);
            break;
        case Gfx::StandardCursor::Arrow:
            setCursor(Qt::ArrowCursor);
            break;
        case Gfx::StandardCursor::Crosshair:
            setCursor(Qt::CrossCursor);
            break;
        case Gfx::StandardCursor::IBeam:
            setCursor(Qt::IBeamCursor);
            break;
        case Gfx::StandardCursor::ResizeHorizontal:
            setCursor(Qt::SizeHorCursor);
            break;
        case Gfx::StandardCursor::ResizeVertical:
            setCursor(Qt::SizeVerCursor);
            break;
        case Gfx::StandardCursor::ResizeDiagonalTLBR:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case Gfx::StandardCursor::ResizeDiagonalBLTR:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case Gfx::StandardCursor::ResizeColumn:
            setCursor(Qt::SplitHCursor);
            break;
        case Gfx::StandardCursor::ResizeRow:
            setCursor(Qt::SplitVCursor);
            break;
        case Gfx::StandardCursor::Hand:
            setCursor(Qt::PointingHandCursor);
            break;
        case Gfx::StandardCursor::Help:
            setCursor(Qt::WhatsThisCursor);
            break;
        case Gfx::StandardCursor::OpenHand:
            setCursor(Qt::OpenHandCursor);
            break;
        case Gfx::StandardCursor::Drag:
            setCursor(Qt::ClosedHandCursor);
            break;
        case Gfx::StandardCursor::DragCopy:
            setCursor(Qt::DragCopyCursor);
            break;
        case Gfx::StandardCursor::Move:
            setCursor(Qt::DragMoveCursor);
            break;
        case Gfx::StandardCursor::Wait:
            setCursor(Qt::BusyCursor);
            break;
        case Gfx::StandardCursor::Disallowed:
            setCursor(Qt::ForbiddenCursor);
            break;
        case Gfx::StandardCursor::Eyedropper:
        case Gfx::StandardCursor::Zoom:
        default:
            setCursor(Qt::ArrowCursor);
            break;
        } },
        [this](Gfx::ImageCursor const& image_cursor) {
            if (!image_cursor.bitmap.is_valid())
                return;

            auto const& bitmap = *image_cursor.bitmap.bitmap();
            QImage qimage { bitmap.scanline_u8(0), bitmap.width(), bitmap.height(), QImage::Format_ARGB32 };
            if (qimage.isNull())
                return;

            auto qpixmap = QPixmap::fromImage(qimage);
            setCursor(QCursor { qpixmap, image_cursor.hotspot.x(), image_cursor.hotspot.y() });
        });
}

}
