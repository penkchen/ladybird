/*
 * Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWebView/Application.h>

#include <QGuiApplication>
#include <QObject>

namespace Ladybird {

class WebContentView;

class Application
    : public QObject
    , public WebView::Application {
    Q_OBJECT
    WEB_VIEW_APPLICATION(Application)

public:
    virtual ~Application() override;

    void set_active_view(WebView::ViewImplementation* view) { m_active_view = view; }

signals:
    void new_tab_added(Ladybird::WebContentView* view, bool activate);

private:
    explicit Application();

    virtual void create_platform_options(WebView::BrowserOptions&, WebView::RequestServerOptions&, WebView::WebContentOptions&) override;
    virtual NonnullOwnPtr<Core::EventLoop> create_platform_event_loop() override;

    virtual Optional<WebView::ViewImplementation&> active_web_view() const override;
    virtual Optional<WebView::ViewImplementation&> open_blank_new_tab(Web::HTML::ActivateTab) const override;

    virtual Optional<ByteString> ask_user_for_download_path(StringView) const override { return {}; }
    virtual void display_download_confirmation_dialog(StringView, LexicalPath const&) const override { }
    virtual void display_error_dialog(StringView) const override { }

    virtual Utf16String clipboard_text() const override;
    virtual Vector<Web::Clipboard::SystemClipboardRepresentation> clipboard_entries() const override;
    virtual void insert_clipboard_entry(Web::Clipboard::SystemClipboardRepresentation) override;

    virtual void rebuild_bookmarks_menu() const override { }
    virtual void update_bookmarks_bar_display(bool) const override { }
    virtual void show_bookmark_context_menu(Gfx::IntPoint, Optional<WebView::BookmarkItem const&>, Optional<String const&>) override { }
    virtual Optional<BookmarkID> bookmark_item_id_for_context_menu() const override { return {}; }
    virtual NonnullRefPtr<BookmarkPromise> display_add_bookmark_dialog() const override;
    virtual NonnullRefPtr<BookmarkPromise> display_edit_bookmark_dialog(WebView::BookmarkItem::Bookmark const&) const override;
    virtual NonnullRefPtr<BookmarkFolderPromise> display_add_bookmark_folder_dialog() const override;
    virtual NonnullRefPtr<BookmarkFolderPromise> display_edit_bookmark_folder_dialog(WebView::BookmarkItem::Folder const&) const override;

    OwnPtr<QGuiApplication> m_application;
    WebView::ViewImplementation* m_active_view { nullptr };
};

}
