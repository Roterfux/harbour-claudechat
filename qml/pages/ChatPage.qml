import QtQuick 2.2
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    // Keep the view at the end (newest message) unless the user scrolled up.
    property bool stickToEnd: true
    function scrollDown() { stickToEnd = true; scrollTimer.restart() }
    Timer {
        id: scrollTimer
        interval: 30
        onTriggered: if (page.stickToEnd) list.positionViewAtEnd()
    }

    Connections {
        target: claude
        onConversationChanged: page.scrollDown()
        onMessagesChanged: if (page.stickToEnd) scrollTimer.restart()
        onStreamTextChanged: if (page.stickToEnd) scrollTimer.restart()
    }
    Component.onCompleted: scrollDown()

    SilicaListView {
        id: list
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: input.top }
        clip: true
        model: claude.messages
        onContentHeightChanged: if (page.stickToEnd) scrollTimer.restart()
        onCountChanged: if (page.stickToEnd) scrollTimer.restart()
        onMovementEnded: page.stickToEnd = atYEnd
        onFlickEnded: page.stickToEnd = atYEnd

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
            fromUser: modelData.fromUser
            text: modelData.text
        }

        // Answer while it is streaming in.
        footer: Column {
            width: list.width
            MessageItem {
                visible: claude.sending
                fromUser: false
                text: claude.streamText !== "" ? claude.streamText : "…"
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
