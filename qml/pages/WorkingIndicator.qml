import QtQuick 2.2
import Sailfish.Silica 1.0

// "Claude is working" line: spinning indicator plus text.
Item {
    id: root
    property bool running: false
    property string text: qsTr("Claude is working...")

    width: parent ? parent.width : 0
    height: running ? Theme.itemSizeSmall : 0
    visible: running

    Row {
        x: Theme.horizontalPageMargin
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.paddingMedium
        BusyIndicator {
            size: BusyIndicatorSize.ExtraSmall
            running: root.running
            anchors.verticalCenter: parent.verticalCenter
        }
        Label {
            text: root.text
            color: Theme.highlightColor
            font.pixelSize: Theme.fontSizeSmall
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
