import QtQuick 2.2
import Sailfish.Silica 1.0

// One-time setup: paste the claude.ai "sessionKey" cookie. It is stored
// and used for every request, so there is no login screen afterwards.
Dialog {
    id: dialog
    objectName: "loginPage"
    property bool expired: false
    property string value: field.text.trim()

    allowedOrientations: Orientation.All
    canAccept: value.length > 20
    acceptDestination: Qt.resolvedUrl("ConversationsPage.qml")
    acceptDestinationAction: PageStackAction.Replace
    onAccepted: claude.setSessionKey(value)

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height

        Column {
            id: col
            width: parent.width
            spacing: Theme.paddingMedium

            DialogHeader {
                title: qsTr("Connect claude.ai")
                acceptText: qsTr("Connect")
            }
            Label {
                visible: dialog.expired || claude.error !== ""
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: claude.error !== "" ? claude.error : qsTr("The session has expired.")
            }
            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Log in to claude.ai in a desktop browser. Open the developer tools (F12), "
                         + "Storage/Application > Cookies > https://claude.ai, and copy the value of "
                         + "\"sessionKey\" (starts with sk-ant-sid). Paste it here once; the app stays "
                         + "logged in until the key expires. Treat it like a password.")
            }
            TextArea {
                id: field
                width: parent.width
                label: qsTr("sessionKey")
                placeholderText: label
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
            }
        }
        VerticalScrollDecorator {}
    }
}
