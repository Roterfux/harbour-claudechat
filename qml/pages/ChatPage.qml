import QtQuick 2.2
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import "../js/listsync.js" as ListSync

Page {
    id: page
    allowedOrientations: Orientation.All

    // Only changed rows are updated (no rebuild of all messages = no flicker).
    property bool loading: claude.loadingChat
    function syncMessages() { if (!page.loading) ListSync.sync(messageModel, claude.messages) }

    // Show the messages only when fully loaded: hidden while loading, then
    // laid out at the end and faded in once (no visible swapping).
    property bool revealed: !page.loading
    onLoadingChanged: {
        if (loading) {
            revealed = false
        } else {
            syncMessages()
            revealTimer.restart()
        }
    }
    Timer {
        id: revealTimer
        interval: 150      // let rich text lay out first
        onTriggered: { list.positionViewAtEnd(); page.revealed = true; page.scrollDown() }
    }
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

    LoadingOverlay {
        active: !page.revealed
        progress: claude.loadProgress
        text: qsTr("Loading chat...")
        z: 5
    }
    LoadingBar {
        anchors.top: parent.top
        running: claude.busy && !claude.sending && page.revealed
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
        opacity: page.revealed ? 1 : 0
        Behavior on opacity { FadeAnimation {} }
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

        VerticalScrollDecorator {}
    }

    Component {
        id: filePicker
        ContentPickerPage {
            title: qsTr("Attach file")
            onSelectedContentPropertiesChanged: claude.addAttachment(selectedContentProperties.filePath)
        }
    }

    Item {
        id: input
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        height: attachRow.height + Math.max(field.height, sendButton.height) + Theme.paddingSmall

        // Attached files (tap the x to remove).
        Flow {
            id: attachRow
            anchors { left: parent.left; right: parent.right; top: parent.top
                      leftMargin: Theme.horizontalPageMargin; rightMargin: Theme.horizontalPageMargin }
            spacing: Theme.paddingSmall
            height: claude.attachments.length > 0 ? implicitHeight + Theme.paddingSmall : 0
            Repeater {
                model: claude.attachments
                Rectangle {
                    height: chipRow.height
                    width: chipRow.width + Theme.paddingMedium
                    radius: height / 2
                    color: Theme.rgba(modelData.error !== "" ? "red" : Theme.highlightBackgroundColor, 0.25)
                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "\uD83D\uDCCE " + modelData.name
                                  + (modelData.uploading ? " \u2026" : "")
                                  + (modelData.error !== "" ? " (" + modelData.error + ")" : "")
                            font.pixelSize: Theme.fontSizeExtraSmall
                            truncationMode: TruncationMode.Fade
                            width: Math.min(implicitWidth, input.width * 0.7)
                        }
                        IconButton {
                            width: Theme.iconSizeSmall + Theme.paddingSmall
                            height: width
                            icon.source: "image://theme/icon-s-clear-opaque-cross"
                            onClicked: claude.removeAttachment(index)
                        }
                    }
                }
            }
        }

        IconButton {
            id: attachButton
            anchors { left: parent.left; bottom: parent.bottom; bottomMargin: Theme.paddingMedium }
            icon.source: "image://theme/icon-m-attach"
            enabled: claude.loggedIn && !claude.sending
            onClicked: pageStack.push(filePicker)
        }

        TextArea {
            id: field
            anchors { left: attachButton.right; right: sendButton.left; bottom: parent.bottom }
            placeholderText: qsTr("Message Claude")
            label: ""
            labelVisible: false
            enabled: claude.loggedIn
        }
        IconButton {
            id: sendButton
            anchors { right: parent.right; rightMargin: Theme.paddingMedium; bottom: parent.bottom; bottomMargin: Theme.paddingMedium }
            icon.source: claude.sending ? "image://theme/icon-m-clear" : "image://theme/icon-m-send"
            enabled: claude.sending || (claude.loggedIn && attachmentsOk
                                        && (field.text.trim() !== "" || claude.attachments.length > 0))
            property bool attachmentsOk: {
                for (var i = 0; i < claude.attachments.length; ++i)
                    if (claude.attachments[i].uploading || claude.attachments[i].error !== "") return false
                return true
            }
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
