#ifndef CODECLIENT_H
#define CODECLIENT_H

#include <QObject>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QTimer>

class ClaudeClient;
class QNetworkReply;

// Claude Code sessions (claude.ai/code) of the same claude.ai login:
// list sessions, read a session's events, send a message into it.
// Uses the sessions API of the Claude Code web client with the sessionKey
// cookie. Unofficial and unverified: routes can change at any time.
class CodeClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionTitle READ sessionTitle NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionStatus READ sessionStatus NOTIFY sessionChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)

public:
    explicit CodeClient(ClaudeClient *claude, QObject *parent = nullptr);

    bool busy() const { return m_busy > 0; }
    QString error() const { return m_error; }
    QVariantList sessions() const { return m_sessions; }
    QString sessionId() const { return m_sessionId; }
    QString sessionTitle() const { return m_sessionTitle; }
    QString sessionStatus() const { return m_sessionStatus; }
    QVariantList messages() const { return m_messages; }

    Q_INVOKABLE void refreshSessions();
    Q_INVOKABLE void openSession(const QString &id);
    Q_INVOKABLE void closeSession();
    Q_INVOKABLE void send(const QString &text);

signals:
    void busyChanged();
    void errorChanged();
    void sessionsChanged();
    void sessionChanged();
    void messagesChanged();

private:
    QNetworkRequest request(const QString &path, bool json = false) const;
    bool checkReply(QNetworkReply *reply, QByteArray *data);
    void setBusy(bool on);
    void setError(const QString &error);
    void loadSession(bool quiet);
    void loadEvents(const QString &afterId, bool quiet);
    void addEvents(const QVariantList &events);

    ClaudeClient *m_claude;
    QNetworkAccessManager m_nam;
    QTimer m_poll;
    int m_busy = 0;
    QString m_error;
    QVariantList m_sessions;
    QString m_sessionId;
    QString m_sessionTitle;
    QString m_sessionStatus;
    QVariantList m_events;      // raw events of the open session
    QStringList m_eventIds;
    QVariantList m_messages;    // display items built from m_events
};

#endif
