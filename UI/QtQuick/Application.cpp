/*
 * Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <UI/Qt/EventLoopImplementationQt.h>
#include <UI/Qt/StringUtils.h>
#include <UI/QtQuick/Application.h>
#include <UI/QtQuick/WebContentView.h>

#include <QClipboard>
#include <QMimeData>

namespace Ladybird {

Application::Application() = default;
Application::~Application() = default;

void Application::create_platform_options(WebView::BrowserOptions&, WebView::RequestServerOptions&, WebView::WebContentOptions&)
{
}

NonnullOwnPtr<Core::EventLoop> Application::create_platform_event_loop()
{
    if (!browser_options().headless_mode.has_value()) {
        // Load Qt's xdgdesktopportal platform theme plugin before QGuiApplication constructs,
        // so QStyleHints::colorScheme and the system palette reflect the desktop's dark mode
        // preference. Only overrides if the user hasn't explicitly picked a different theme.
        if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORMTHEME"))
            qputenv("QT_QPA_PLATFORMTHEME", "xdgdesktopportal");

        Core::EventLoopManager::install(*new EventLoopManagerQt);
        m_application = make<QGuiApplication>(arguments().argc, arguments().argv);
    }

    auto event_loop = WebView::Application::create_platform_event_loop();

    if (!browser_options().headless_mode.has_value())
        static_cast<EventLoopImplementationQt&>(event_loop->impl()).set_main_loop();

    return event_loop;
}

Optional<WebView::ViewImplementation&> Application::active_web_view() const
{
    if (!m_active_view)
        return {};
    return *m_active_view;
}

Optional<WebView::ViewImplementation&> Application::open_blank_new_tab(Web::HTML::ActivateTab activate_tab) const
{
    auto* view = new WebContentView();
    bool activate = activate_tab == Web::HTML::ActivateTab::Yes;
    const_cast<Application*>(this)->emit new_tab_added(view, activate);
    return *view;
}

NonnullRefPtr<Application::BookmarkPromise> Application::display_add_bookmark_dialog() const
{
    auto promise = BookmarkPromise::construct();
    promise->reject(Error::from_errno(ECANCELED));
    return promise;
}

NonnullRefPtr<Application::BookmarkPromise> Application::display_edit_bookmark_dialog(WebView::BookmarkItem::Bookmark const&) const
{
    auto promise = BookmarkPromise::construct();
    promise->reject(Error::from_errno(ECANCELED));
    return promise;
}

NonnullRefPtr<Application::BookmarkFolderPromise> Application::display_add_bookmark_folder_dialog() const
{
    auto promise = BookmarkFolderPromise::construct();
    promise->reject(Error::from_errno(ECANCELED));
    return promise;
}

NonnullRefPtr<Application::BookmarkFolderPromise> Application::display_edit_bookmark_folder_dialog(WebView::BookmarkItem::Folder const&) const
{
    auto promise = BookmarkFolderPromise::construct();
    promise->reject(Error::from_errno(ECANCELED));
    return promise;
}

Utf16String Application::clipboard_text() const
{
    return utf16_string_from_qstring(QGuiApplication::clipboard()->text());
}

Vector<Web::Clipboard::SystemClipboardRepresentation> Application::clipboard_entries() const
{
    auto const* mime_data = QGuiApplication::clipboard()->mimeData();
    if (!mime_data)
        return {};

    Vector<Web::Clipboard::SystemClipboardRepresentation> representations;
    for (auto const& format : mime_data->formats())
        representations.empend(ak_byte_string_from_qbytearray(mime_data->data(format)), ak_string_from_qstring(format));
    return representations;
}

void Application::insert_clipboard_entry(Web::Clipboard::SystemClipboardRepresentation entry)
{
    auto* mime_data = new QMimeData;
    mime_data->setData(qstring_from_ak_string(entry.mime_type), qbytearray_from_ak_string(entry.data));
    QGuiApplication::clipboard()->setMimeData(mime_data);
}

}
