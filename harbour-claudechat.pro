TARGET = harbour-claudechat

CONFIG += sailfishapp

# App version = Version: in the spec (shown in Settings).
APP_VERSION = $$system(sed -n 's/^Version:[[:space:]]*//p' $$PWD/rpm/harbour-claudechat.spec)
isEmpty(APP_VERSION): APP_VERSION = unknown
DEFINES += APP_VERSION=\\\"$$APP_VERSION\\\"
QT += network

SOURCES += src/main.cpp \
    src/claudeclient.cpp \
    src/codeclient.cpp

HEADERS += src/claudeclient.h \
    src/codeclient.h

DISTFILES += \
    harbour-claudechat.desktop \
    qml/harbour-claudechat.qml \
    qml/cover/*.qml \
    qml/pages/*.qml \
    qml/js/*.js \
    rpm/harbour-claudechat.spec

icons86.files = icons/86x86/harbour-claudechat.png
icons86.path = /usr/share/icons/hicolor/86x86/apps
icons108.files = icons/108x108/harbour-claudechat.png
icons108.path = /usr/share/icons/hicolor/108x108/apps
icons128.files = icons/128x128/harbour-claudechat.png
icons128.path = /usr/share/icons/hicolor/128x128/apps
icons172.files = icons/172x172/harbour-claudechat.png
icons172.path = /usr/share/icons/hicolor/172x172/apps
INSTALLS += icons86 icons108 icons128 icons172
