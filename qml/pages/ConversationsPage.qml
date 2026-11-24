import QtQuick 2.2
import Sailfish.Silica 1.0

Page {
    id: page
    allowedOrientations: Orientation.All

    function openChat(uuid) {
        if (uuid) claude.openConversation(uuid)
        else claude.newConversation()
        pageStack.push(Qt.resolvedUrl("ChatPage.qml"))
    }

    SilicaListView {
        id: list
        anchors.fill: parent
        model: claude.conversations

        header: PageHeader {
            title: qsTr("Claude")
            description: claude.account
        }

        PullDownMenu {
            busy: claude.busy
            MenuItem {
                text: qsTr("Log out")
                onClicked: remorse.execute(qsTr("Logging out"), function() {
                    claude.logout()
                    pageStack.replace(Qt.resolvedUrl("LoginPage.qml"))
                })
            }
            MenuItem {
                text: qsTr("Refresh")
                onClicked: claude.loggedIn ? claude.refreshConversations() : claude.connectAccount()
            }
            MenuItem {
                text: qsTr("New chat")
                enabled: claude.loggedIn
                onClicked: page.openChat("")
            }
        }

        delegate: ListItem {
            id: item
            contentHeight: Theme.itemSizeSmall
            menu: ContextMenu {
                MenuItem {
                    text: qsTr("Delete")
                    onClicked: {
                        var uuid = modelData.uuid
                        item.remorseAction(qsTr("Deleting"), function() { claude.deleteConversation(uuid) })
                    }
                }
            }
            onClicked: page.openChat(modelData.uuid)

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter
                text: modelData.name !== "" ? modelData.name : qsTr("Untitled chat")
                truncationMode: TruncationMode.Fade
                color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
            }
        }

        ViewPlaceholder {
            enabled: list.count === 0 && !claude.busy
            text: claude.error !== "" ? claude.error
                                      : (claude.loggedIn ? qsTr("No chats yet") : qsTr("Not connected"))
            hintText: claude.loggedIn ? qsTr("Pull down to start a new chat") : qsTr("Pull down to retry")
        }

        BusyIndicator {
            anchors.centerIn: parent
            size: BusyIndicatorSize.Large
            running: claude.busy && list.count === 0
        }

        VerticalScrollDecorator {}
    }

    RemorsePopup { id: remorse }
}
