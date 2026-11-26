import QtQuick 2.2
import Sailfish.Silica 1.0

// Thin indeterminate progress bar along the top edge while content loads.
Item {
    id: bar
    property bool running: false

    width: parent ? parent.width : 0
    height: Theme.paddingSmall
    clip: true
    opacity: running ? 1 : 0
    Behavior on opacity { FadeAnimation {} }

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.highlightBackgroundColor, 0.2)
    }
    Rectangle {
        id: runner
        width: bar.width / 3
        height: parent.height
        color: Theme.highlightColor
        NumberAnimation on x {
            running: bar.running || bar.opacity > 0
            loops: Animation.Infinite
            from: -runner.width
            to: bar.width
            duration: 1100
            easing.type: Easing.InOutQuad
        }
    }
}
