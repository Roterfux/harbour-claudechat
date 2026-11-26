import QtQuick 2.2
import Sailfish.Silica 1.0

// One Claude Code session: messages and tool calls, refreshed every few
// seconds while open; send a message into the session.
Page {
    id: page
    allowedOrientations: Orientation.All
    property bool showTools: false

    Component.onDestruction: { code.closeSession(); speech.stop() }
    Connections {
        target: speech
        onTextRecognized: field.text = field.text.trim() === "" ? text : field.text.trim() + " " + text
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
        target: code
        onSessionChanged: page.scrollDown()
        onMessagesChanged: page.scrollDown()
    }
    Component.onCompleted: scrollDown()

    SilicaListView {
        id: list
        anchors { top: parent.top; left: parent.left; right: parent.right; bottom: input.top }
        clip: true
        model: code.messages
        onContentHeightChanged: if (page.settling) scrollTimer.restart()
        onCountChanged: page.scrollDown()

        header: PageHeader {
            title: code.sessionTitle !== "" ? code.sessionTitle : qsTr("Code session")
            description: code.sessionStatus === "running" ? qsTr("working...") : code.sessionStatus
        }

        PullDownMenu {
            busy: code.busy
            MenuItem {
                text: page.showTools ? qsTr("Hide tool calls") : qsTr("Show tool calls")
                onClicked: page.showTools = !page.showTools
            }
            MenuItem {
                text: qsTr("Open in browser")
                onClicked: Qt.openUrlExternally("https://claude.ai/code/" + code.sessionId)
            }
            MenuItem {
                text: qsTr("Reload")
                onClicked: code.openSession(code.sessionId)
            }
        }

        // Tool calls ("Bash: ...") are hidden unless switched on.
        delegate: Loader {
            width: list.width
            property bool hidden: modelData.kind === "tool" && !page.showTools
            sourceComponent: hidden ? null : (modelData.kind === "tool" ? toolLine : messageItem)
            property var entry: modelData
        }

        footer: Column {
            width: list.width
            WorkingIndicator {
                running: code.sessionStatus === "running"
            }
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
            anchors { left: parent.left; right: sendButton.left; rightMargin: Theme.itemSizeSmall; bottom: parent.bottom }
            placeholderText: speech.listening ? (speech.partialText !== "" ? speech.partialText : qsTr("Listening...")) : qsTr("Message the session")
            label: ""
            labelVisible: false
        }
        MicButton {
            anchors { right: sendButton.left; bottom: parent.bottom; bottomMargin: Theme.paddingMedium }
        }
        IconButton {
            id: sendButton
            anchors { right: parent.right; rightMargin: Theme.paddingMedium; bottom: parent.bottom; bottomMargin: Theme.paddingMedium }
            icon.source: "image://theme/icon-m-send"
            enabled: field.text.trim() !== "" && code.sessionId !== ""
            onClicked: {
                code.send(field.text)
                page.scrollDown()
                field.text = ""
            }
        }
    }
}
