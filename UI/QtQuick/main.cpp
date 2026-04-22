/*
 * Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibMain/Main.h>
#include <LibWebView/Application.h>
#include <LibWebView/BrowserProcess.h>
#include <LibWebView/URL.h>
#include <LibWebView/Utilities.h>
#include <UI/QtQuick/Application.h>
#include <UI/QtQuick/WebContentView.h>

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>
#include <qqml.h>

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    AK::set_rich_debug_enabled(true);

    auto app = TRY(Ladybird::Application::create(arguments));

    QQuickStyle::setStyle("Fusion");

    qmlRegisterType<Ladybird::WebContentView>("org.ladybird.QtQuick", 1, 0, "WebContentView");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("ladybirdApplication", app.ptr());
    engine.loadFromModule("org.ladybird.QtQuick", "Main");
    if (engine.rootObjects().isEmpty())
        return Error::from_string_literal("Failed to load QML root");

    return app->execute();
}
