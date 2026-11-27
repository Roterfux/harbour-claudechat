#ifndef CLAUDECLIENT_H
#define CLAUDECLIENT_H

#include <QObject>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QHash>

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
    Q_PROPERTY(int seenVersion READ seenVersion NOTIFY seenChanged)
    // First load of an opened chat: true until all messages are there.
    Q_PROPERTY(bool loadingChat READ loadingChat NOTIFY loadingChatChanged)
    // Message text size as a factor of the normal size (setting).
    Q_PROPERTY(qreal textScale READ textScale WRITE setTextScale NOTIFY textScaleChanged)
    Q_PROPERTY(qreal loadProgress READ loadProgress NOTIFY loadingChatChanged)

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
    int seenVersion() const { return m_seenVersion; }
    bool loadingChat() const { return m_loadingChat; }
    qreal textScale() const { return m_textScale; }
    void setTextScale(qreal scale);
    qreal loadProgress() const { return m_loadProgress; }

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
    // "Done/new" markers: last seen change time per chat or Code session.
    Q_INVOKABLE void rememberItems(const QVariantList &items, const QString &idKey);
    Q_INVOKABLE bool isNew(const QString &id, const QString &updatedAt) const;
    Q_INVOKABLE void markSeen(const QString &id, const QString &updatedAt);

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
    void seenChanged();
    void loadingChatChanged();
    void textScaleChanged();

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
    QHash<QString, QString> m_seen;   // id -> updatedAt when last seen ("" = never)
    int m_seenVersion = 0;
    bool m_loadingChat = false;
    qreal m_textScale = 0.85;
    qreal m_loadProgress = -1;   // 0..1, -1 = unknown
    QVariantList m_cachedMessages;   // shown only if loading fails
    void setLoadingChat(bool on);
    void saveSeen();
};

#endif
