import QtQuick 2.2
import Sailfish.Silica 1.0
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
}
