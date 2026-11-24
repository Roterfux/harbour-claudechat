#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScopedPointer>
#include <sailfishapp.h>

#include "claudeclient.h"
#include "codeclient.h"

int main(int argc, char *argv[])
{
    QScopedPointer<QGuiApplication> app(SailfishApp::application(argc, argv));
    app->setOrganizationName(QStringLiteral("org.roterfux"));
    app->setApplicationName(QStringLiteral("harbour-claudechat"));

    ClaudeClient client;
    CodeClient code(&client);

    QScopedPointer<QQuickView> view(SailfishApp::createView());
    view->rootContext()->setContextProperty(QStringLiteral("claude"), &client);
    view->rootContext()->setContextProperty(QStringLiteral("code"), &code);
    view->setSource(SailfishApp::pathToMainQml());
    view->show();

    return app->exec();
}
