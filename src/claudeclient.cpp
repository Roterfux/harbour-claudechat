#include "claudeclient.h"

#include <algorithm>

#include <QDir>
#include <QHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSettings>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUrl>
#include <QUuid>

namespace {

const QString BaseUrl = QStringLiteral("https://claude.ai");
const QString RootParent = QStringLiteral("00000000-0000-4000-8000-000000000000");
const QByteArray UserAgent = QByteArrayLiteral(
    "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");

QString settingsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
           + QStringLiteral("/settings.conf");
}

QString newUuid()
{
    QString s = QUuid::createUuid().toString();
    return s.mid(1, s.length() - 2);
}

// Text of a chat message: the "text" blocks of "content", else legacy "text".
QString messageText(const QJsonObject &m)
{
    QStringList parts;
    const QJsonArray content = m.value(QStringLiteral("content")).toArray();
    for (const QJsonValue &v : content) {
        const QJsonObject block = v.toObject();
        if (block.value(QStringLiteral("type")).toString() == QLatin1String("text"))
            parts << block.value(QStringLiteral("text")).toString();
    }
    if (parts.isEmpty())
        return m.value(QStringLiteral("text")).toString();
    return parts.join(QStringLiteral("\n\n"));
}

} // namespace

ClaudeClient::ClaudeClient(QObject *parent)
    : QObject(parent)
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    m_sessionKey = s.value(QStringLiteral("sessionKey")).toString();
}

QString ClaudeClient::orgPath() const
{
    return QStringLiteral("/api/organizations/") + m_org;
}

void ClaudeClient::setSessionKey(const QString &key)
{
    QString k = key.trimmed();
    if (k.startsWith(QLatin1String("sessionKey=")))
        k = k.mid(11);
    m_sessionKey = k;
    QDir().mkpath(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation));
    {
        QSettings s(settingsPath(), QSettings::IniFormat);
        s.setValue(QStringLiteral("sessionKey"), m_sessionKey);
        s.sync();
    }
    QFile::setPermissions(settingsPath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    emit sessionKeyChanged();
    connectAccount();
}

void ClaudeClient::logout()
{
    stop();
    m_sessionKey.clear();
    m_org.clear();
    m_account.clear();
    {
        QSettings s(settingsPath(), QSettings::IniFormat);
        s.remove(QStringLiteral("sessionKey"));
    }
    m_conversations.clear();
    m_messages.clear();
    m_conversationId.clear();
    m_conversationName.clear();
    emit conversationsChanged();
    emit messagesChanged();
    emit conversationChanged();
    emit sessionKeyChanged();
    setLoggedIn(false);
}

QNetworkRequest ClaudeClient::request(const QString &path, bool json) const
{
    QNetworkRequest req(QUrl(BaseUrl + path));
    req.setRawHeader("Cookie", "sessionKey=" + m_sessionKey.toUtf8());
    req.setRawHeader("User-Agent", UserAgent);
    req.setRawHeader("Origin", BaseUrl.toUtf8());
    req.setRawHeader("Referer", (BaseUrl + QStringLiteral("/")).toUtf8());
    req.setRawHeader("anthropic-client-platform", "web_claude_ai");
    req.setRawHeader("Accept", "application/json");
    if (json)
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return req;
}

QNetworkReply *ClaudeClient::get(const QString &path)
{
    setBusy(true);
    QNetworkReply *r = m_nam.get(request(path));
    connect(r, &QNetworkReply::finished, this, [this]() { setBusy(false); });
    return r;
}

QNetworkReply *ClaudeClient::post(const QString &path, const QVariantMap &body)
{
    setBusy(true);
    QNetworkReply *r = m_nam.post(request(path, true),
                                  QJsonDocument(QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this]() { setBusy(false); });
    return r;
}

// Reads the reply; on an error sets error text (and drops the session on 401/403).
bool ClaudeClient::checkReply(QNetworkReply *reply, QByteArray *data)
{
    reply->deleteLater();
    const QByteArray body = reply->readAll();
    if (data)
        *data = body;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300)
        return true;

    QString msg;
    const QJsonObject err = QJsonDocument::fromJson(body).object().value(QStringLiteral("error")).toObject();
    if (!err.isEmpty())
        msg = err.value(QStringLiteral("message")).toString();
    if (msg.isEmpty())
        msg = status ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
    setError(msg);

    if (status == 401 || (status == 403 && err.value(QStringLiteral("details")).toObject()
                              .value(QStringLiteral("error_code")).toString().startsWith(QLatin1String("account_session")))) {
        setLoggedIn(false);
        emit sessionInvalid();
    }
    return false;
}

void ClaudeClient::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void ClaudeClient::setBusy(bool on)
{
    const bool was = busy();
    m_busy += on ? 1 : -1;
    if (m_busy < 0)
        m_busy = 0;
    if (was != busy())
        emit busyChanged();
}

void ClaudeClient::setLoggedIn(bool on)
{
    if (m_loggedIn == on)
        return;
    m_loggedIn = on;
    emit loggedInChanged();
}

// Checks the session and picks the organization (account) to chat in.
void ClaudeClient::connectAccount()
{
    if (m_sessionKey.isEmpty())
        return;
    setError(QString());
    QNetworkReply *r = get(QStringLiteral("/api/organizations"));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        QByteArray data;
        if (!checkReply(r, &data))
            return;
        const QJsonArray orgs = QJsonDocument::fromJson(data).array();
        QJsonObject pick;
        for (const QJsonValue &v : orgs) {
            const QJsonObject o = v.toObject();
            if (o.value(QStringLiteral("capabilities")).toArray().contains(QStringLiteral("chat"))) {
                pick = o;
                break;
            }
        }
        if (pick.isEmpty() && !orgs.isEmpty())
            pick = orgs.first().toObject();
        if (pick.isEmpty()) {
            setError(tr("No claude.ai organization found for this session"));
            return;
        }
        m_org = pick.value(QStringLiteral("uuid")).toString();
        m_account = pick.value(QStringLiteral("name")).toString();
        setLoggedIn(true);
        refreshConversations();
    });
}

void ClaudeClient::refreshConversations()
{
    if (m_org.isEmpty())
        return;
    QNetworkReply *r = get(orgPath() + QStringLiteral("/chat_conversations?limit=100"));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        QByteArray data;
        if (!checkReply(r, &data))
            return;
        m_conversations.clear();
        const QJsonArray list = QJsonDocument::fromJson(data).array();
        for (const QJsonValue &v : list) {
            const QJsonObject c = v.toObject();
            QVariantMap m;
            m[QStringLiteral("uuid")] = c.value(QStringLiteral("uuid")).toString();
            m[QStringLiteral("name")] = c.value(QStringLiteral("name")).toString();
            m[QStringLiteral("updatedAt")] = c.value(QStringLiteral("updated_at")).toString();
            m_conversations << m;
        }
        emit conversationsChanged();
    });
}

void ClaudeClient::openConversation(const QString &uuid)
{
    stop();
    m_conversationId = uuid;
    m_conversationName.clear();
    for (const QVariant &v : m_conversations) {
        const QVariantMap c = v.toMap();
        if (c.value(QStringLiteral("uuid")).toString() == uuid)
            m_conversationName = c.value(QStringLiteral("name")).toString();
    }
    m_leafId.clear();
    m_messages.clear();
    emit conversationChanged();
    emit messagesChanged();
    loadConversation();
}

void ClaudeClient::newConversation()
{
    stop();
    m_conversationId.clear();
    m_conversationName.clear();
    m_leafId.clear();
    m_messages.clear();
    emit conversationChanged();
    emit messagesChanged();
}

// Loads the messages of the current branch (leaf back to the root).
void ClaudeClient::loadConversation()
{
    if (m_conversationId.isEmpty())
        return;
    const QString id = m_conversationId;
    QNetworkReply *r = get(orgPath() + QStringLiteral("/chat_conversations/") + id
                           + QStringLiteral("?tree=True&rendering_mode=messages&render_all_tools=true"));
    connect(r, &QNetworkReply::finished, this, [this, r, id]() {
        QByteArray data;
        if (!checkReply(r, &data) || id != m_conversationId)
            return;
        const QJsonObject conv = QJsonDocument::fromJson(data).object();
        m_conversationName = conv.value(QStringLiteral("name")).toString();
        m_leafId = conv.value(QStringLiteral("current_leaf_message_uuid")).toString();

        QHash<QString, QJsonObject> byId;
        QList<QJsonObject> ordered;
        const QJsonArray msgs = conv.value(QStringLiteral("chat_messages")).toArray();
        for (const QJsonValue &v : msgs) {
            const QJsonObject m = v.toObject();
            byId.insert(m.value(QStringLiteral("uuid")).toString(), m);
            ordered << m;
        }

        QList<QJsonObject> branch;
        if (!m_leafId.isEmpty() && byId.contains(m_leafId)) {
            QString cur = m_leafId;
            while (byId.contains(cur) && branch.size() <= byId.size()) {
                const QJsonObject m = byId.value(cur);
                branch.prepend(m);
                cur = m.value(QStringLiteral("parent_message_uuid")).toString();
            }
        } else {
            std::sort(ordered.begin(), ordered.end(), [](const QJsonObject &a, const QJsonObject &b) {
                return a.value(QStringLiteral("index")).toInt() < b.value(QStringLiteral("index")).toInt();
            });
            branch = ordered;
            if (!branch.isEmpty())
                m_leafId = branch.last().value(QStringLiteral("uuid")).toString();
        }

        m_messages.clear();
        for (const QJsonObject &m : branch) {
            QVariantMap item;
            item[QStringLiteral("uuid")] = m.value(QStringLiteral("uuid")).toString();
            item[QStringLiteral("fromUser")] = m.value(QStringLiteral("sender")).toString() == QLatin1String("human");
            item[QStringLiteral("text")] = messageText(m);
            m_messages << item;
        }
        emit conversationChanged();
        emit messagesChanged();
    });
}

void ClaudeClient::send(const QString &text)
{
    const QString prompt = text.trimmed();
    if (prompt.isEmpty() || m_org.isEmpty() || sending())
        return;
    setError(QString());

    QVariantMap user;
    user[QStringLiteral("uuid")] = QString();
    user[QStringLiteral("fromUser")] = true;
    user[QStringLiteral("text")] = prompt;
    m_messages << user;
    emit messagesChanged();

    if (!m_conversationId.isEmpty()) {
        startCompletion(prompt);
        return;
    }

    // New chat: create the conversation first.
    QVariantMap body;
    body[QStringLiteral("uuid")] = newUuid();
    body[QStringLiteral("name")] = QString();
    QNetworkReply *r = post(orgPath() + QStringLiteral("/chat_conversations"), body);
    connect(r, &QNetworkReply::finished, this, [this, r, prompt]() {
        QByteArray data;
        if (!checkReply(r, &data)) {
            if (!m_messages.isEmpty())
                m_messages.removeLast();
            emit messagesChanged();
            return;
        }
        m_conversationId = QJsonDocument::fromJson(data).object().value(QStringLiteral("uuid")).toString();
        m_leafId.clear();
        emit conversationChanged();
        m_pendingPrompt = prompt;
        startCompletion(prompt);
    });
}

void ClaudeClient::startCompletion(const QString &text)
{
    QVariantMap body;
    body[QStringLiteral("prompt")] = text;
    body[QStringLiteral("parent_message_uuid")] = m_leafId.isEmpty() ? RootParent : m_leafId;
    body[QStringLiteral("timezone")] = QString::fromUtf8(QTimeZone::systemTimeZoneId());
    body[QStringLiteral("attachments")] = QVariantList();
    body[QStringLiteral("files")] = QVariantList();
    body[QStringLiteral("sync_sources")] = QVariantList();
    body[QStringLiteral("rendering_mode")] = QStringLiteral("messages");

    QNetworkRequest req = request(orgPath() + QStringLiteral("/chat_conversations/") + m_conversationId
                                  + QStringLiteral("/completion"), true);
    req.setRawHeader("Accept", "text/event-stream");

    m_streamBuffer.clear();
    m_streamText.clear();
    emit streamTextChanged();
    m_stream = m_nam.post(req, QJsonDocument(QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Compact));
    emit sendingChanged();

    QNetworkReply *r = m_stream;
    connect(r, &QNetworkReply::readyRead, this, [this, r]() {
        if (r == m_stream)
            handleStreamData();
    });
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        const bool current = (r == m_stream);
        if (current) {
            handleStreamData();
            if (!m_streamBuffer.trimmed().isEmpty())
                handleStreamLine(m_streamBuffer);
            m_streamBuffer.clear();
        }
        const bool aborted = r->error() == QNetworkReply::OperationCanceledError;
        if (!aborted)
            checkReply(r, nullptr);
        else
            r->deleteLater();
        if (!current)
            return;
        m_stream.clear();
        emit sendingChanged();

        if (!m_streamText.isEmpty()) {
            QVariantMap answer;
            answer[QStringLiteral("uuid")] = QString();
            answer[QStringLiteral("fromUser")] = false;
            answer[QStringLiteral("text")] = m_streamText;
            m_messages << answer;
            emit messagesChanged();
        }
        m_streamText.clear();
        emit streamTextChanged();

        if (!m_pendingPrompt.isEmpty() && !aborted) {
            setTitle(m_pendingPrompt);
            m_pendingPrompt.clear();
        }
        // Reload for the real message ids (needed as parent of the next message).
        loadConversation();
        refreshConversations();
    });
}

void ClaudeClient::handleStreamData()
{
    if (m_stream.isNull())
        return;
    m_streamBuffer += m_stream->readAll();
    int nl;
    while ((nl = m_streamBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_streamBuffer.left(nl);
        m_streamBuffer.remove(0, nl + 1);
        handleStreamLine(line);
    }
}

// One server-sent-events line. Handles the current event format
// (content_block_delta / text_delta) and the older {"completion": ...} one.
void ClaudeClient::handleStreamLine(const QByteArray &rawLine)
{
    const QByteArray line = rawLine.trimmed();
    if (!line.startsWith("data:"))
        return;
    const QJsonObject ev = QJsonDocument::fromJson(line.mid(5).trimmed()).object();
    if (ev.isEmpty())
        return;

    const QString type = ev.value(QStringLiteral("type")).toString();
    QString add;
    if (type == QLatin1String("content_block_delta")) {
        const QJsonObject delta = ev.value(QStringLiteral("delta")).toObject();
        if (delta.value(QStringLiteral("type")).toString() == QLatin1String("text_delta"))
            add = delta.value(QStringLiteral("text")).toString();
    } else if (ev.contains(QStringLiteral("completion"))) {
        add = ev.value(QStringLiteral("completion")).toString();
    } else if (type == QLatin1String("error") || ev.contains(QStringLiteral("error"))) {
        const QJsonValue e = ev.value(QStringLiteral("error"));
        setError(e.isObject() ? e.toObject().value(QStringLiteral("message")).toString() : e.toString());
    }
    if (!add.isEmpty()) {
        m_streamText += add;
        emit streamTextChanged();
    }
}

// Lets claude.ai generate a title for a new chat (best effort).
void ClaudeClient::setTitle(const QString &firstPrompt)
{
    const QString id = m_conversationId;
    QVariantMap body;
    body[QStringLiteral("message_content")] = firstPrompt;
    body[QStringLiteral("recent_titles")] = QVariantList();
    QNetworkReply *r = m_nam.post(request(orgPath() + QStringLiteral("/chat_conversations/") + id
                                          + QStringLiteral("/title"), true),
                                  QJsonDocument(QJsonObject::fromVariantMap(body)).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        r->deleteLater();
        if (r->error() == QNetworkReply::NoError)
            refreshConversations();
    });
}

void ClaudeClient::stop()
{
    if (m_stream.isNull())
        return;
    m_stream->abort();
}

void ClaudeClient::deleteConversation(const QString &uuid)
{
    if (m_org.isEmpty() || uuid.isEmpty())
        return;
    setBusy(true);
    QNetworkReply *r = m_nam.deleteResource(request(orgPath() + QStringLiteral("/chat_conversations/") + uuid));
    connect(r, &QNetworkReply::finished, this, [this, r, uuid]() {
        setBusy(false);
        if (!checkReply(r, nullptr))
            return;
        if (uuid == m_conversationId)
            newConversation();
        refreshConversations();
    });
}
