#ifndef CLAUDECLIENT_H
#define CLAUDECLIENT_H

#include <QObject>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QPointer>

class QNetworkReply;

// Native client for the claude.ai web API, authenticated with the
// "sessionKey" cookie of a logged-in browser session (no API key).
// Unofficial: claude.ai can change these routes at any time.
class ClaudeClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasSessionKey READ hasSessionKey NOTIFY sessionKeyChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY loggedInChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString account READ account NOTIFY loggedInChanged)
    Q_PROPERTY(QVariantList conversations READ conversations NOTIFY conversationsChanged)
    Q_PROPERTY(QString conversationId READ conversationId NOTIFY conversationChanged)
    Q_PROPERTY(QString conversationName READ conversationName NOTIFY conversationChanged)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY messagesChanged)
    Q_PROPERTY(QString streamText READ streamText NOTIFY streamTextChanged)
    Q_PROPERTY(QVariantList usage READ usage NOTIFY usageChanged)
    Q_PROPERTY(QString usageError READ usageError NOTIFY usageChanged)

public:
    explicit ClaudeClient(QObject *parent = nullptr);

    bool hasSessionKey() const { return !m_sessionKey.isEmpty(); }
    bool loggedIn() const { return m_loggedIn; }
    bool busy() const { return m_busy > 0; }
    bool sending() const { return !m_stream.isNull(); }
    QString error() const { return m_error; }
    QString account() const { return m_account; }
    QVariantList conversations() const { return m_conversations; }
    QString conversationId() const { return m_conversationId; }
    QString conversationName() const { return m_conversationName; }
    QVariantList messages() const { return m_messages; }
    QString streamText() const { return m_streamText; }
    QVariantList usage() const { return m_usage; }
    QString usageError() const { return m_usageError; }

    // For CodeClient (same login).
    QString sessionKey() const { return m_sessionKey; }
    QString orgId() const { return m_org; }

    Q_INVOKABLE void setSessionKey(const QString &key);
    Q_INVOKABLE void logout();
    Q_INVOKABLE void connectAccount();
    Q_INVOKABLE void refreshConversations();
    Q_INVOKABLE void openConversation(const QString &uuid);
    Q_INVOKABLE void newConversation();
    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void deleteConversation(const QString &uuid);
    Q_INVOKABLE void refreshUsage();

signals:
    void sessionKeyChanged();
    void loggedInChanged();
    void busyChanged();
    void sendingChanged();
    void errorChanged();
    void conversationsChanged();
    void conversationChanged();
    void messagesChanged();
    void streamTextChanged();
    void sessionInvalid();
    void answerFinished(const QString &chatName, const QString &text);
    void usageChanged();

private:
    QNetworkRequest request(const QString &path, bool json = false) const;
    QNetworkReply *get(const QString &path);
    QNetworkReply *post(const QString &path, const QVariantMap &body);
    bool checkReply(QNetworkReply *reply, QByteArray *data);
    void setError(const QString &error);
    void setBusy(bool on);
    void setLoggedIn(bool on);
    void loadConversation();
    void startCompletion(const QString &text);
    void handleStreamData();
    void handleStreamLine(const QByteArray &line);
    void setTitle(const QString &firstPrompt);
    QString orgPath() const;

    QNetworkAccessManager m_nam;
    QString m_sessionKey;
    QString m_org;
    QString m_account;
    bool m_loggedIn = false;
    int m_busy = 0;
    QString m_error;
    QVariantList m_conversations;
    QString m_conversationId;
    QString m_conversationName;
    QString m_leafId;
    QVariantList m_messages;
    QPointer<QNetworkReply> m_stream;
    QByteArray m_streamBuffer;
    QString m_streamText;
    QString m_pendingPrompt;
    QVariantList m_usage;
    QString m_usageError;
};

#endif
