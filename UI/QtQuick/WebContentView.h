/*
 * Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGfx/Cursor.h>
#include <LibGfx/Rect.h>
#include <LibURL/URL.h>
#include <LibWebView/ViewImplementation.h>

#include <QPointF>
#include <QQuickItem>

class QFocusEvent;
class QHoverEvent;
class QKeyEvent;
class QMouseEvent;
class QSinglePointEvent;
class QWheelEvent;

namespace Ladybird {

class WebContentView
    : public QQuickItem
    , public WebView::ViewImplementation {
    Q_OBJECT
    Q_PROPERTY(QString url READ url_string NOTIFY url_changed)
    Q_PROPERTY(QString title READ title_string NOTIFY title_changed)
    Q_PROPERTY(bool loading READ is_loading NOTIFY loading_changed)

public:
    explicit WebContentView(QQuickItem* parent = nullptr);
    virtual ~WebContentView() override;

    Q_INVOKABLE void load_url(QString const& url);
    Q_INVOKABLE void go_back();
    Q_INVOKABLE void go_forward();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void activate_context_menu_action(int action_id);

    QString url_string() const;
    QString title_string() const;
    bool is_loading() const { return m_is_loading; }

signals:
    void url_changed();
    void title_changed();
    void loading_changed();
    void context_menu_requested(QPointF const& position, QVariantList const& items);

protected:
    virtual QSGNode* updatePaintNode(QSGNode*, UpdatePaintNodeData*) override;
    virtual void geometryChange(QRectF const& new_geometry, QRectF const& old_geometry) override;
    virtual void itemChange(ItemChange, ItemChangeData const&) override;

    virtual void mousePressEvent(QMouseEvent*) override;
    virtual void mouseDoubleClickEvent(QMouseEvent*) override;
    virtual void mouseMoveEvent(QMouseEvent*) override;
    virtual void mouseReleaseEvent(QMouseEvent*) override;
    virtual void wheelEvent(QWheelEvent*) override;
    virtual void hoverMoveEvent(QHoverEvent*) override;
    virtual void hoverLeaveEvent(QHoverEvent*) override;

    virtual void keyPressEvent(QKeyEvent*) override;
    virtual void keyReleaseEvent(QKeyEvent*) override;

    virtual void focusInEvent(QFocusEvent*) override;
    virtual void focusOutEvent(QFocusEvent*) override;

private:
    // ^WebView::ViewImplementation
    virtual void initialize_client(CreateNewClient) override;
    virtual Web::DevicePixelSize viewport_size() const override;
    virtual Gfx::IntPoint to_content_position(Gfx::IntPoint widget_position) const override;
    virtual Gfx::IntPoint to_widget_position(Gfx::IntPoint content_position) const override;

    void update_viewport_size();
    void update_cursor(Gfx::Cursor cursor);

    void enqueue_native_mouse_event(Web::MouseEvent::Type, QSinglePointEvent const&);
    void enqueue_native_key_event(Web::KeyEvent::Type, QKeyEvent const&);
    void finish_handling_key_event(Web::KeyEvent const&);

    Gfx::IntSize m_viewport_size;
    void* m_pinned_native_texture { nullptr };

    u64 m_last_click_timestamp { 0 };
    QPointF m_last_click_position;
    int m_click_count { 0 };

    bool m_is_loading { false };
};

}
