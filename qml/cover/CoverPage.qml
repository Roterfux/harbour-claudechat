import QtQuick 2.2
import Sailfish.Silica 1.0

CoverBackground {
    property bool codeRunning: {
        for (var i = 0; i < code.sessions.length; ++i)
            if (code.sessions[i].status === "running") return true
        return false
    }
    property bool working: claude.sending || codeRunning

    BusyIndicator {
        anchors { top: parent.top; horizontalCenter: parent.horizontalCenter; topMargin: Theme.paddingLarge }
        size: BusyIndicatorSize.Small
        running: working
    }
    Image {
        anchors.centerIn: parent
        source: "/usr/share/icons/hicolor/172x172/apps/harbour-claudechat.png"
        width: parent.width * 0.6
        height: width
        opacity: 0.4
    }
    Label {
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right; margins: Theme.paddingLarge }
        text: claude.sending ? qsTr("Claude is writing...")
                             : codeRunning ? qsTr("Code session working...")
                             : (claude.conversationName !== "" ? claude.conversationName : qsTr("Claude Chat"))
        wrapMode: Text.Wrap
        maximumLineCount: 3
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        font.pixelSize: Theme.fontSizeSmall
    }
}
