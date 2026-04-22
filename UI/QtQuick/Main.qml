// Copyright (c) 2026, Penk Chen <penk.chen@qt.io>
// SPDX-License-Identifier: BSD-2-Clause

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.ladybird.QtQuick

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    visible: true
    title: currentView && currentView.title.length > 0 ? currentView.title + " — Ladybird" : "Ladybird"

    property WebContentView currentView: stackLayout.count > 0 ? stackLayout.children[stackLayout.currentIndex] : null

    header: Column {
        width: parent.width

        RowLayout {
            width: parent.width
            spacing: 0

            TabBar {
                id: tabBar
                Layout.fillWidth: true
                currentIndex: stackLayout.currentIndex
                onCurrentIndexChanged: {
                    stackLayout.currentIndex = currentIndex
                    if (currentView)
                        currentView.forceActiveFocus()
                }
            }

            Button {
                text: "+"
                flat: true
                onClicked: addTab(null, true)
            }
        }

        Pane {
            width: parent.width
            padding: 4
            RowLayout {
                anchors.fill: parent
                spacing: 4

                Button { text: "◀"; flat: true; onClicked: currentView && currentView.go_back() }
                Button { text: "▶"; flat: true; onClicked: currentView && currentView.go_forward() }
                Button {
                    text: currentView && currentView.loading ? "✕" : "↻"
                    flat: true
                    onClicked: currentView && currentView.reload()
                }

                TextField {
                    Layout.fillWidth: true
                    text: currentView ? currentView.url : ""
                    selectByMouse: true
                    onAccepted: if (currentView) currentView.load_url(text)
                    Keys.onEscapePressed: if (currentView) text = currentView.url
                }
            }
        }
    }

    StackLayout {
        id: stackLayout
        anchors.fill: parent
    }

    Menu {
        id: contextMenu
    }

    Component {
        id: menuItemComponent
        MenuItem {
            property int actionId: -1
            onTriggered: if (currentView) currentView.activate_context_menu_action(actionId)
        }
    }

    Component { id: separatorComponent; MenuSeparator {} }

    Component {
        id: tabViewComponent
        WebContentView {
            onContext_menu_requested: (position, items) => showContextMenu(position, items)
        }
    }

    Component {
        id: tabButtonComponent
        TabButton {
            id: tabButton
            property WebContentView view
            implicitWidth: 200
            implicitHeight: 28
            font.pixelSize: 12
            text: view && view.title.length > 0 ? view.title : (view && view.url.length > 0 ? view.url : "New Tab")
            rightPadding: 22

            Label {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 6
                text: "×"
                font.pixelSize: 14
                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -4
                    onClicked: closeTab(tabButton.view)
                }
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.MiddleButton
                onClicked: closeTab(tabButton.view)
                propagateComposedEvents: true
            }
        }
    }

    Connections {
        target: ladybirdApplication
        function onNew_tab_added(view, activate) {
            hostTab(view, activate)
        }
    }

    Component.onCompleted: addTab("https://ladybird.org/", true)

    function addTab(url, activate) {
        const view = tabViewComponent.createObject(stackLayout)
        if (url)
            view.load_url(url)
        registerTab(view, activate)
    }

    function hostTab(view, activate) {
        view.parent = stackLayout
        registerTab(view, activate)
    }

    function registerTab(view, activate) {
        const button = tabButtonComponent.createObject(null, { view: view })
        tabBar.addItem(button)
        if (activate) {
            tabBar.currentIndex = tabBar.count - 1
            view.forceActiveFocus()
        }
    }

    function closeTab(view) {
        if (tabBar.count <= 1)
            return
        for (let i = 0; i < tabBar.count; i++) {
            const button = tabBar.itemAt(i)
            if (button.view === view) {
                tabBar.removeItem(button)
                view.destroy()
                return
            }
        }
    }

    function showContextMenu(position, items) {
        while (contextMenu.count > 0)
            contextMenu.removeItem(contextMenu.itemAt(0))
        for (const item of items) {
            if (item.isSeparator) {
                contextMenu.addItem(separatorComponent.createObject(null))
            } else {
                contextMenu.addItem(menuItemComponent.createObject(null, {
                    text: item.text,
                    enabled: item.enabled,
                    checkable: item.checkable,
                    checked: item.checked,
                    actionId: item.id,
                }))
            }
        }
        contextMenu.popup(currentView, position)
    }
}
