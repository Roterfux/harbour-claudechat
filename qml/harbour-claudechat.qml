import QtQuick 2.2
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import "pages"

ApplicationWindow {
    id: app

    initialPage: claude.hasSessionKey ? Qt.resolvedUrl("pages/ConversationsPage.qml")
                                      : Qt.resolvedUrl("pages/LoginPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    Component.onCompleted: claude.connectAccount()

    // Session expired or key wrong: ask for a new key.
    Connections {
        target: claude
        onSessionInvalid: {
            if (pageStack.currentPage && pageStack.currentPage.objectName === "loginPage")
                return
            pageStack.clear()
            pageStack.push(Qt.resolvedUrl("pages/LoginPage.qml"), { expired: true }, PageStackAction.Immediate)
        }
    }

    // Notifications: Claude answered (when the app is in the background),
    // a Code session stopped running (unless it is open on screen).
    function notify(summary, body) {
        var n = notificationComponent.createObject(app, { summary: summary, body: body,
                                                          previewSummary: summary, previewBody: body })
        n.publish()
    }
    function shorten(text) {
        var t = (text || "").replace(/\s+/g, " ").trim()
        return t.length > 140 ? t.substring(0, 137) + "..." : t
    }
    Component {
        id: notificationComponent
        Notification {
            appName: qsTr("Claude Chat")
            appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-claudechat.png"
            expireTimeout: 0
        }
    }

    Connections {
        target: claude
        onAnswerFinished: {
            if ((Qt.application.state === Qt.ApplicationActive))
                return
            app.notify(chatName !== "" ? qsTr("Claude answered: %1").arg(chatName) : qsTr("Claude answered"),
                       app.shorten(text))
        }
    }
    Connections {
        target: code
        onSessionFinished: {
            if ((Qt.application.state === Qt.ApplicationActive) && code.sessionId === sessionUuid)
                return
            var what = status === "idle" || status === "completed" ? qsTr("finished")
                     : status === "requires_action" ? qsTr("needs your input")
                     : status
            app.notify(qsTr("Code session %1").arg(what), title !== "" ? title : sessionUuid)
        }
    }
}
