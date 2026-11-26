import QtQuick 2.2
import Sailfish.Silica 1.0

// Microphone button: tap to speak one sentence (Speech Note), tap again to
// stop. Highlighted and pulsing while listening; shows a speaker while an
// answer is read aloud (tap stops it).
IconButton {
    id: mic
    property bool voiceMode: false

    icon.source: speech.speaking ? "image://theme/icon-m-speaker-on" : "image://theme/icon-m-mic"
    highlighted: speech.listening || speech.speaking || voiceMode
    onClicked: {
        if (speech.listening || speech.speaking) {
            speech.stop()
        } else {
            speech.checkAvailable()
            speech.listen()
        }
    }

    SequentialAnimation on opacity {
        running: speech.listening
        loops: Animation.Infinite
        NumberAnimation { to: 0.4; duration: 500 }
        NumberAnimation { to: 1.0; duration: 500 }
        onRunningChanged: if (!running) mic.opacity = 1.0
    }

    // Short error hint under the button row (e.g. Speech Note missing).
    Label {
        visible: speech.error !== "" && !speech.listening && !speech.speaking
        anchors { bottom: parent.top; right: parent.right }
        width: Math.min(implicitWidth, Screen.width - 2 * Theme.horizontalPageMargin)
        wrapMode: Text.Wrap
        horizontalAlignment: Text.AlignRight
        font.pixelSize: Theme.fontSizeTiny
        color: Theme.highlightColor
        text: speech.error
    }
}
