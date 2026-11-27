import QtQuick 2.2
import Sailfish.Silica 1.0

// Start a new Claude Code session: pick a repository (from existing
// sessions, whose environment is reused) and write the first message.
Page {
    id: dialog
    allowedOrientations: Orientation.All
    property var repos: code.repositories()
    property string repoUrl: repos.length > 0 ? repos[0].url : ""
    property bool starting: false

    // Stays open until the session exists (errors stay visible), then
    // replaces itself with the new session.
    Connections {
        target: code
        onSessionCreated: {
            dialog.starting = false
            code.openSession(sessionUuid)
            pageStack.replace(Qt.resolvedUrl("CodeSessionPage.qml"))
        }
        onErrorChanged: if (code.error !== "") dialog.starting = false
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height

        Column {
            id: col
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("New Code session") }

            ComboBox {
                id: repoBox
                width: parent.width
                label: qsTr("Repository")
                visible: dialog.repos.length > 0
                menu: ContextMenu {
                    Repeater {
                        model: dialog.repos
                        MenuItem {
                            text: modelData.repo
                            onClicked: dialog.repoUrl = modelData.url
                        }
                    }
                }
            }
            Label {
                visible: dialog.repos.length === 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("No repository found. Start one session on claude.ai/code first; "
                         + "new sessions reuse its repository and environment.")
            }

            TextArea {
                id: prompt
                width: parent.width
                label: qsTr("First message")
                placeholderText: qsTr("What should Claude do?")
                focus: true
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: dialog.starting ? qsTr("Starting...") : qsTr("Start session")
                enabled: !dialog.starting && dialog.repoUrl !== "" && prompt.text.trim() !== ""
                onClicked: {
                    dialog.starting = true
                    code.createSession(dialog.repoUrl, prompt.text)
                }
            }

            Label {
                visible: code.error !== "" && !dialog.starting
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
                text: code.error
            }
        }
        VerticalScrollDecorator {}
    }
}
