import QtQuick 2.2
import Sailfish.Silica 1.0

// Start page: switch between normal claude.ai chats ("Claude") and
// Claude Code sessions ("Code").
Page {
    id: page
    allowedOrientations: Orientation.All
    property bool codeMode: false
    property string usageSummary: {
        var parts = []
        for (var i = 0; i < claude.usage.length; ++i) {
            var u = claude.usage[i]
            if (u.key === "five_hour") parts.push(qsTr("5h %1%").arg(Math.round(u.percent)))
            else if (u.key === "seven_day") parts.push(qsTr("week %1%").arg(Math.round(u.percent)))
        }
        return parts.length ? " \u00b7 " + parts.join(" \u00b7 ") : ""
    }

    // Local date/time of an ISO time: "14:05" today, else "Mon 24.11. 14:05".
    function when(iso) {
        if (!iso) return ""
        var d = new Date(iso)
        if (isNaN(d.getTime())) return iso.substring(0, 16).replace("T", " ")
        var now = new Date()
        if (d.toDateString() === now.toDateString()) return Qt.formatTime(d, "hh:mm")
        return Qt.formatDateTime(d, d.getFullYear() === now.getFullYear() ? "ddd dd.MM. hh:mm" : "dd.MM.yyyy")
    }

    function openChat(uuid) {
        if (uuid) claude.openConversation(uuid)
        else claude.newConversation()
        pageStack.push(Qt.resolvedUrl("ChatPage.qml"))
    }
    function openSession(id) {
        code.openSession(id)
        pageStack.push(Qt.resolvedUrl("CodeSessionPage.qml"))
    }
    function refresh() {
        if (!claude.loggedIn) claude.connectAccount()
        else if (codeMode) code.refreshSessions()
        else claude.refreshConversations()
        claude.refreshUsage()
    }

    onCodeModeChanged: if (codeMode && code.sessions.length === 0) code.refreshSessions()

    SilicaListView {
        id: list
        anchors.fill: parent
        model: page.codeMode ? code.sessions : claude.conversations

        header: Column {
            width: list.width
            PageHeader {
                title: page.codeMode ? qsTr("Code") : qsTr("Claude")
                description: claude.account + page.usageSummary
            }
            // Claude | Code switch
            Row {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                height: Theme.itemSizeSmall
                Repeater {
                    model: [ { label: qsTr("Claude"), code: false }, { label: qsTr("Code"), code: true } ]
                    BackgroundItem {
                        width: parent.width / 2
                        height: parent.height
                        highlighted: down || page.codeMode === modelData.code
                        onClicked: page.codeMode = modelData.code
                        Label {
                            anchors.centerIn: parent
                            text: modelData.label
                            font.bold: page.codeMode === modelData.code
                            color: page.codeMode === modelData.code ? Theme.highlightColor : Theme.primaryColor
                        }
                        Rectangle {
                            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
                            height: Theme.paddingSmall / 2
                            color: Theme.highlightColor
                            visible: page.codeMode === modelData.code
                        }
                    }
                }
            }
            Item { width: 1; height: Theme.paddingMedium }
        }

        PullDownMenu {
            busy: claude.busy || code.busy
            MenuItem {
                text: qsTr("Log out")
                onClicked: remorse.execute(qsTr("Logging out"), function() {
                    code.closeSession()
                    claude.logout()
                    pageStack.replace(Qt.resolvedUrl("LoginPage.qml"))
                })
            }
            MenuItem {
                text: page.codeMode ? qsTr("Show Claude chats") : qsTr("Show Code sessions")
                onClicked: page.codeMode = !page.codeMode
            }
            MenuItem {
                text: qsTr("Usage")
                enabled: claude.loggedIn
                onClicked: pageStack.push(Qt.resolvedUrl("UsagePage.qml"))
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: page.refresh()
            }
            MenuItem {
                text: qsTr("New chat")
                visible: !page.codeMode
                enabled: claude.loggedIn
                onClicked: page.openChat("")
            }
        }

        delegate: ListItem {
            id: item
            contentHeight: Theme.itemSizeMedium
            menu: page.codeMode ? null : deleteMenu
            onClicked: page.codeMode ? page.openSession(modelData.id) : page.openChat(modelData.uuid)

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                Label {
                    width: parent.width
                    text: page.codeMode ? (modelData.title !== "" ? modelData.title : qsTr("Untitled session"))
                                        : (modelData.name !== "" ? modelData.name : qsTr("Untitled chat"))
                    truncationMode: TruncationMode.Fade
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    text: (page.codeMode ? modelData.status + "  " : "") + page.when(modelData.updatedAt)
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    truncationMode: TruncationMode.Fade
                }
            }

            Component {
                id: deleteMenu
                ContextMenu {
                    MenuItem {
                        text: qsTr("Delete")
                        onClicked: {
                            var uuid = modelData.uuid
                            item.remorseAction(qsTr("Deleting"), function() { claude.deleteConversation(uuid) })
                        }
                    }
                }
            }
        }

        ViewPlaceholder {
            property string err: page.codeMode ? code.error : claude.error
            enabled: list.count === 0 && !claude.busy && !code.busy
            text: err !== "" ? err
                             : (!claude.loggedIn ? qsTr("Not connected")
                                                 : (page.codeMode ? qsTr("No Code sessions") : qsTr("No chats yet")))
            hintText: page.codeMode ? qsTr("Pull down to refresh")
                                    : (claude.loggedIn ? qsTr("Pull down to start a new chat") : qsTr("Pull down to retry"))
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: (claude.busy || code.busy) && list.count === 0
        }

        VerticalScrollDecorator {}
    }

    RemorsePopup { id: remorse }
}
