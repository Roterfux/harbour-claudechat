import QtQuick 2.2
import Sailfish.Silica 1.0

// One chat message. User messages are indented and highlighted;
// long press copies the text.
ListItem {
    id: item
    property bool fromUser: false
    property string text: ""

    width: ListView.view ? ListView.view.width : parent.width
    contentHeight: bubble.height + Theme.paddingMedium

    menu: ContextMenu {
        MenuItem {
            text: qsTr("Copy")
            onClicked: Clipboard.text = item.text
        }
    }

    Rectangle {
        id: bubble
        x: item.fromUser ? 3 * Theme.horizontalPageMargin : Theme.horizontalPageMargin
        y: Theme.paddingMedium / 2
        width: item.width - 4 * Theme.horizontalPageMargin
        height: label.height + 2 * Theme.paddingMedium
        radius: Theme.paddingMedium
        color: item.fromUser ? Theme.rgba(Theme.highlightBackgroundColor, 0.2)
                             : Theme.rgba(Theme.primaryColor, 0.05)

        Label {
            id: label
            x: Theme.paddingMedium
            y: Theme.paddingMedium
            width: parent.width - 2 * Theme.paddingMedium
            text: item.text
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: item.fromUser ? Theme.highlightColor : Theme.primaryColor
        }
    }
}
