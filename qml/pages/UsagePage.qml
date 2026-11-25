import QtQuick 2.2
import Sailfish.Silica 1.0

// Plan usage limits of the claude.ai account: one bar per limit window
// (5-hour session, weekly, weekly per model) with the reset time.
Page {
    id: page
    allowedOrientations: Orientation.All

    Component.onCompleted: claude.refreshUsage()

    function resetText(iso) {
        if (!iso) return ""
        var d = new Date(iso)
        if (isNaN(d.getTime())) return ""
        var mins = Math.round((d.getTime() - Date.now()) / 60000)
        var when = Qt.formatDateTime(d, "ddd hh:mm")
        if (mins <= 0) return qsTr("Resets now")
        if (mins < 60) return qsTr("Resets in %1 min (%2)").arg(mins).arg(when)
        if (mins < 48 * 60) return qsTr("Resets in %1 h %2 min (%3)").arg(Math.floor(mins / 60)).arg(mins % 60).arg(when)
        return qsTr("Resets %1").arg(when)
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: col.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: claude.refreshUsage()
            }
        }

        Column {
            id: col
            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("Usage")
                description: claude.account
            }

            Repeater {
                model: claude.usage
                Column {
                    width: col.width
                    ProgressBar {
                        width: parent.width
                        minimumValue: 0
                        maximumValue: 100
                        value: Math.min(100, modelData.percent)
                        label: modelData.label
                        valueText: Math.round(modelData.percent) + " %"
                        highlighted: modelData.percent >= 80
                    }
                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        visible: text !== ""
                        text: page.resetText(modelData.resetsAt)
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.secondaryColor
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            Label {
                visible: claude.usageError !== ""
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("Usage not available: %1").arg(claude.usageError)
            }
        }
        VerticalScrollDecorator {}
    }
}
