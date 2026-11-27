import QtQuick 2.2
import Sailfish.Silica 1.0

// App settings: message text size (with a live preview).
Page {
    id: page
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height + Theme.paddingLarge

        Column {
            id: col
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: qsTr("Settings") }

            DetailItem {
                label: qsTr("Version")
                value: Qt.application.version
            }

            SectionHeader { text: qsTr("Text size") }

            Slider {
                id: sizeSlider
                width: parent.width
                minimumValue: 60
                maximumValue: 140
                stepSize: 5
                value: Math.round(claude.textScale * 100)
                valueText: value + " %"
                label: qsTr("Message text")
                onReleased: claude.textScale = value / 100
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Default (85 %)")
                onClicked: { claude.textScale = 0.85; sizeSlider.value = 85 }
            }

            SectionHeader { text: qsTr("Preview") }

            MessageItem {
                fromUser: true
                width: col.width
                text: qsTr("How big is this text?")
            }
            MessageItem {
                fromUser: false
                width: col.width
                text: qsTr("This is how **answers** look, with `inline code` and a list:\n\n- first point\n- second point")
            }
        }
        VerticalScrollDecorator {}
    }
}
