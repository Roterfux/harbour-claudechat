import QtQuick 2.2
import Sailfish.Silica 1.0
import "../js/listsync.js" as ListSync

Page {
    id: page
    allowedOrientations: Orientation.All

    // Only changed rows are updated (no rebuild of all messages = no flicker).
    function syncMessages() { ListSync.sync(messageModel, claude.messages) }
    Connections {
        target: claude
        onMessagesChanged: page.syncMessages()
    }
    // Streaming text is shown at most ~6 times a second (Markdown re-render).
    property string shownStream: ""
    onShownStreamChanged: scrollDown()
    Timer {
        id: streamTimer
        interval: 150
        onTriggered: page.shownStream = claude.streamText
    }
    Connections {
        target: claude
        onStreamTextChanged: {
            if (claude.streamText === "") { streamTimer.stop(); page.shownStream = "" }
            else if (!streamTimer.running) streamTimer.start()
        }
    }

    LoadingBar {
        anchors.top: parent.top
        running: claude.busy && !claude.sending
        z: 10
    }

    // Always jump to the end (newest message) on every update: open, new
    // messages, streaming, poll, reload, back to this page, app to front.
    property bool settling: false
    function scrollDown() {
        settling = true
        settleTimer.restart()
        scrollTimer.restart()
    }
    Timer {
        id: scrollTimer
        interval: 30
        onTriggered: list.positionViewAtEnd()
    }
    Timer {
        id: settleTimer
        interval: 800
        onTriggered: { list.positionViewAtEnd(); page.settling = false }
    }
    onStatusChanged: if (status === PageStatus.Active) scrollDown()
    Connections {
        target: Qt.application
        onStateChanged: if (Qt.application.state === Qt.ApplicationActive) page.scrollDown()
    }

    Connections {
        target: claude
        onConversationChanged: page.scrollDown()
        onMessagesChanged: page.scrollDown()
        onSendingChanged: page.scrollDown()
    }
    Component.onCompleted: { syncMessages(); scrollDown() }

    SilicaListView {
        id: list
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: input.top }
        clip: true
        model: ListModel { id: messageModel }
        onContentHeightChanged: if (page.settling) scrollTimer.restart()
        onCountChanged: page.scrollDown()

        header: PageHeader {
            title: claude.conversationName !== "" ? claude.conversationName
                                                  : (claude.conversationId !== "" ? qsTr("Chat") : qsTr("New chat"))
        }

        PullDownMenu {
            busy: claude.busy || claude.sending
            MenuItem {
                text: qsTr("New chat")
                onClicked: claude.newConversation()
            }
            MenuItem {
                text: qsTr("Reload")
                enabled: claude.conversationId !== "" && !claude.sending
                onClicked: claude.openConversation(claude.conversationId)
            }
        }

        delegate: MessageItem {
            fromUser: model.fromUser
            text: model.text
        }

        // Answer while it is streaming in.
        footer: Column {
            width: list.width
            MessageItem {
                visible: claude.sending && page.shownStream !== ""
                fromUser: false
                text: page.shownStream
            }
            WorkingIndicator {
                running: claude.sending
                text: claude.streamText === "" ? qsTr("Claude is thinking...") : qsTr("Claude is writing...")
            }
            Label {
                visible: claude.error !== ""
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: claude.error
            }
            Item { width: 1; height: Theme.paddingLarge }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !claude.sending && !claude.busy
            text: qsTr("Ask Claude anything")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: claude.busy && list.count === 0 && !claude.sending
        }

        VerticalScrollDecorator {}
    }

    Item {
        id: input
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Math.max(field.height, sendButton.height) + Theme.paddingSmall

        TextArea {
            id: field
            anchors { left: parent.left; right: sendButton.left; bottom: parent.bottom }
            placeholderText: qsTr("Message Claude")
            label: ""
            labelVisible: false
            enabled: claude.loggedIn
        }
        IconButton {
            id: sendButton
            anchors { right: parent.right; rightMargin: Theme.paddingMedium; bottom: parent.bottom; bottomMargin: Theme.paddingMedium }
            icon.source: claude.sending ? "image://theme/icon-m-clear" : "image://theme/icon-m-send"
            enabled: claude.sending || (claude.loggedIn && field.text.trim() !== "")
            onClicked: {
                if (claude.sending) {
                    claude.stop()
                } else {
                    claude.send(field.text)
                    page.scrollDown()
                    field.text = ""
                }
            }
        }
    }
}
