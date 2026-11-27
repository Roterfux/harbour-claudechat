import QtQuick 2.2
import Sailfish.Silica 1.0

// Centered "loading" view while a chat/session loads before it is shown:
// progress bar (percent when known, else running bar) and text.
Column {
    id: overlay
    property bool active: false
    property real progress: -1     // 0..1, -1 = unknown
    property string text: qsTr("Loading...")

    anchors.centerIn: parent
    width: parent.width
    spacing: Theme.paddingLarge
    visible: opacity > 0
    opacity: active ? 1 : 0
    Behavior on opacity { FadeAnimation {} }

    ProgressBar {
        width: parent.width
        visible: overlay.progress >= 0
        minimumValue: 0
        maximumValue: 100
        value: Math.round(overlay.progress * 100)
        valueText: value + " %"
    }
    LoadingBar {
        visible: overlay.progress < 0
        x: Theme.horizontalPageMargin
        width: parent.width - 2 * Theme.horizontalPageMargin
        running: overlay.active
    }
    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        text: overlay.text
        color: Theme.secondaryHighlightColor
        font.pixelSize: Theme.fontSizeSmall
    }
}
