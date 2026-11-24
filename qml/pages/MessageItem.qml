import QtQuick 2.2
import Sailfish.Silica 1.0
import "../js/markdown.js" as Markdown

// One chat message. User messages are indented, highlighted and plain;
// Claude's answers are rendered as Markdown. Long press copies the text
// (the original Markdown).
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
            textFormat: item.fromUser ? Text.PlainText : Text.RichText
            text: item.fromUser ? item.text : Markdown.toHtml(item.text, {
                codeBg: Theme.rgba(Theme.primaryColor, 0.1).toString(),
                codeColor: Theme.primaryColor.toString(),
                linkColor: Theme.highlightColor.toString(),
                quoteColor: Theme.secondaryColor.toString(),
                ruleColor: Theme.secondaryColor.toString()
            })
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontSizeSmall
            color: item.fromUser ? Theme.highlightColor : Theme.primaryColor
            linkColor: Theme.highlightColor
            onLinkActivated: Qt.openUrlExternally(link)
        }
    }
}
