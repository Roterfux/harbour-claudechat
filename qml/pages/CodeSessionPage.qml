import QtQuick 2.2
import Sailfish.Silica 1.0

// One Claude Code session: messages and tool calls, refreshed every few
// seconds while open; send a message into the session.
Page {
    id: page
    allowedOrientations: Orientation.All

    Component.onDestruction: code.closeSession()

    Connections {
        target: code
        onMessagesChanged: if (list.atYEnd || list.count < 3) scrollTimer.restart()
    }
    Timer { id: scrollTimer; interval: 50; onTriggered: list.positionViewAtEnd() }

    SilicaListView {
        id: list
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: input.top }
        clip: true
        model: code.messages

        header: PageHeader {
            title: code.sessionTitle !== "" ? code.sessionTitle : qsTr("Code session")
            description: code.sessionStatus
        }

        PullDownMenu {
            busy: code.busy
            MenuItem {
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally("https://claude.ai/code/" + code.sessionId)
            }
            MenuItem {
                text: qsTr("Reload")
                onClicked: code.openSession(code.sessionId)
            }
        }

        delegate: Loader {
            width: list.width
            sourceComponent: modelData.kind === "tool" ? toolLine : messageItem
            property var entry: modelData
        }

        footer: Column {
            width: list.width
            Label {
                visible: code.error !== ""
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: code.error
            }
            Item { width: 1; height: Theme.paddingLarge }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !code.busy
            text: code.error !== "" ? code.error : qsTr("No messages")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: code.busy && list.count === 0
        }

        VerticalScrollDecorator {}
    }

    Component {
        id: messageItem
        MessageItem {
            fromUser: entry.fromUser
            text: entry.text
        }
    }
    Component {
        id: toolLine
        Label {
            x: 2 * Theme.horizontalPageMargin
            width: list.width - 3 * Theme.horizontalPageMargin
            height: implicitHeight + Theme.paddingSmall
            text: "⚙ " + entry.text
            font.family: "monospace"
            font.pixelSize: Theme.fontSizeExtraSmall
            color: Theme.secondaryColor
            elide: Text.ElideRight
        }
    }

    Item {
        id: input
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: Math.max(field.height, sendButton.height) + Theme.paddingSmall

        TextArea {
            id: field
            anchors { left: parent.left; right: sendButton.left; bottom: parent.bottom }
            placeholderText: qsTr("Message the session")
            label: ""
            labelVisible: false
        }
        IconButton {
            id: sendButton
            anchors { right: parent.right; rightMargin: Theme.paddingMedium; bottom: parent.bottom; bottomMargin: Theme.paddingMedium }
            icon.source: "image://theme/icon-m-send"
            enabled: field.text.trim() !== "" && code.sessionId !== ""
            onClicked: {
                code.send(field.text)
                field.text = ""
            }
        }
    }
}
