#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <sailfishapp.h>

#include "claudeclient.h"
#include "codeclient.h"
#include "speechclient.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName(QStringLiteral("org.roterfux"));
    app->setApplicationName(QStringLiteral("harbour-claudechat"));

    ClaudeClient client;
    CodeClient code(&client);
    SpeechClient speech;

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("claude"), &client);
    view->rootContext()->setContextProperty(QStringLiteral("code"), &code);
    view->rootContext()->setContextProperty(QStringLiteral("speech"), &speech);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
