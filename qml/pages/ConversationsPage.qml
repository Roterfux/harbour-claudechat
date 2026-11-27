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

    function updatedOf(list, idKey, id) {
        for (var i = 0; i < list.length; ++i)
            if (list[i][idKey] === id) return list[i].updatedAt
        return ""
    }
    function openChat(uuid) {
        if (uuid) claude.markSeen(uuid, updatedOf(claude.conversations, "uuid", uuid))
        if (uuid) claude.openConversation(uuid)
        else claude.newConversation()
        pageStack.push(Qt.resolvedUrl("ChatPage.qml"))
    }
    function openSession(id) {
        claude.markSeen(id, updatedOf(code.sessions, "id", id))
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

    // Markers: remember new items; the chat/session open on screen counts as seen.
    function seenOpen(list, idKey, openId) {
        claude.rememberItems(list, idKey)
        for (var i = 0; i < list.length; ++i)
            if (list[i][idKey] === openId) claude.markSeen(openId, list[i].updatedAt)
    }
    Connections {
        target: claude
        onConversationsChanged: page.seenOpen(claude.conversations, "uuid", claude.conversationId)
    }
    Connections {
        target: code
        onSessionsChanged: page.seenOpen(code.sessions, "id", code.sessionId)
    }
    Component.onCompleted: {
        seenOpen(claude.conversations, "uuid", "")
        seenOpen(code.sessions, "id", "")
    }

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
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
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

            Rectangle {
                id: doneDot
                property bool show: {
                    claude.seenVersion   // re-evaluate when markers change
                    var id = page.codeMode ? modelData.id : modelData.uuid
                    return claude.isNew(id, modelData.updatedAt) && !(page.codeMode && modelData.status === "running")
                }
                visible: show
                width: Theme.paddingMedium * 1.5
                height: width
                radius: width / 2
                color: Theme.highlightColor
                anchors { left: parent.left; leftMargin: Theme.paddingSmall; verticalCenter: parent.verticalCenter }
            }
            BusyIndicator {
                id: rowBusy
                anchors { right: parent.right; rightMargin: Theme.horizontalPageMargin; verticalCenter: parent.verticalCenter }
                size: BusyIndicatorSize.ExtraSmall
                running: page.codeMode && modelData.status === "running"
                visible: running
            }
            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin - (rowBusy.visible ? rowBusy.width + Theme.paddingMedium : 0)
                anchors.verticalCenter: parent.verticalCenter
                Label {
                    width: parent.width
                    font.bold: doneDot.visible
                    text: page.codeMode ? (modelData.title !== "" ? modelData.title : qsTr("Untitled session"))
                                        : (modelData.name !== "" ? modelData.name : qsTr("Untitled chat"))
                    truncationMode: TruncationMode.Fade
                    color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
                }
                Label {
                    width: parent.width
                    text: (doneDot.visible ? (page.codeMode ? qsTr("done") : qsTr("new")) + "  " : "")
                          + (page.codeMode ? modelData.status + "  " : "") + page.when(modelData.updatedAt)
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
