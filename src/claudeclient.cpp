#include "claudeclient.h"

#include <algorithm>

#include <QDir>
#include <QHash>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QMimeDatabase>
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

// Messages of the current branch (leaf back to the root) of a conversation,
// as the saved-copy map { name, leaf, updatedAt, messages }.
QVariantMap conversationCopy(const QJsonObject &conv)
{
    QString leaf = conv.value(QStringLiteral("current_leaf_message_uuid")).toString();
    QHash<QString, QJsonObject> byId;
    QList<QJsonObject> ordered;
    const QJsonArray msgs = conv.value(QStringLiteral("chat_messages")).toArray();
    for (const QJsonValue &v : msgs) {
        const QJsonObject m = v.toObject();
        byId.insert(m.value(QStringLiteral("uuid")).toString(), m);
        ordered << m;
    }
    QList<QJsonObject> branch;
    if (!leaf.isEmpty() && byId.contains(leaf)) {
        QString cur = leaf;
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
            leaf = branch.last().value(QStringLiteral("uuid")).toString();
    }
    QVariantList messages;
    for (const QJsonObject &m : branch) {
        QVariantMap item;
        item[QStringLiteral("uuid")] = m.value(QStringLiteral("uuid")).toString();
        item[QStringLiteral("fromUser")] = m.value(QStringLiteral("sender")).toString() == QLatin1String("human");
        item[QStringLiteral("text")] = messageText(m);
        messages << item;
    }
    QVariantMap copy;
    copy[QStringLiteral("name")] = conv.value(QStringLiteral("name")).toString();
    copy[QStringLiteral("leaf")] = leaf;
    copy[QStringLiteral("updatedAt")] = conv.value(QStringLiteral("updated_at")).toString();
    copy[QStringLiteral("messages")] = messages;
    return copy;
}

QString cacheDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/chats");
}

QString cacheFile(const QString &name)
{
    QString safe = name;
    safe.replace(QLatin1Char('/'), QLatin1Char('_'));
    return cacheDir() + QLatin1Char('/') + safe + QStringLiteral(".json");
}

QVariant readCache(const QString &name)
{
    QFile f(cacheFile(name));
    if (!f.open(QIODevice::ReadOnly))
        return QVariant();
    return QJsonDocument::fromJson(f.readAll()).toVariant();
}

void writeCache(const QString &name, const QVariant &data)
{
    QDir().mkpath(cacheDir());
    QFile f(cacheFile(name));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument::fromVariant(data).toJson(QJsonDocument::Compact));
        f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
}

// Newest first (ISO 8601 times compare as text).
void sortByUpdated(QVariantList &list)
{
    std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("updatedAt")).toString()
               > b.toMap().value(QStringLiteral("updatedAt")).toString();
    });
}

} // namespace

ClaudeClient::ClaudeClient(QObject *parent)
    : QObject(parent)
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    m_sessionKey = s.value(QStringLiteral("sessionKey")).toString();
    m_textScale = s.value(QStringLiteral("textScale"), 0.85).toReal();
    s.beginGroup(QStringLiteral("seen"));
    for (const QString &k : s.childKeys())
        m_seen.insert(k, s.value(k).toString());
    s.endGroup();
}

void ClaudeClient::setTextScale(qreal scale)
{
    scale = qBound<qreal>(0.6, scale, 1.4);
    if (qFuzzyCompare(scale, m_textScale))
        return;
    m_textScale = scale;
    QSettings s(settingsPath(), QSettings::IniFormat);
    s.setValue(QStringLiteral("textScale"), m_textScale);
    emit textScaleChanged();
}

void ClaudeClient::saveSeen()
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    s.remove(QStringLiteral("seen"));
    s.beginGroup(QStringLiteral("seen"));
    for (auto it = m_seen.constBegin(); it != m_seen.constEnd(); ++it)
        s.setValue(it.key(), it.value());
    s.endGroup();
}

// Items not known yet: on the very first list everything counts as seen;
// later, new items count as not seen (marker until opened).
void ClaudeClient::rememberItems(const QVariantList &items, const QString &idKey)
{
    QSettings s(settingsPath(), QSettings::IniFormat);
    const QString flag = QStringLiteral("seenInit/") + idKey;
    const bool init = s.value(flag).toBool();
    bool changed = false;
    for (const QVariant &v : items) {
        const QVariantMap m = v.toMap();
        const QString id = m.value(idKey).toString();
        if (id.isEmpty() || m_seen.contains(id))
            continue;
        m_seen.insert(id, init ? QString() : m.value(QStringLiteral("updatedAt")).toString());
        changed = true;
    }
    if (!init && !items.isEmpty())
        s.setValue(flag, true);
    if (changed) {
        saveSeen();
        ++m_seenVersion;
        emit seenChanged();
    }
}

bool ClaudeClient::isNew(const QString &id, const QString &updatedAt) const
{
    return m_seen.contains(id) && !updatedAt.isEmpty() && updatedAt > m_seen.value(id);
}

void ClaudeClient::markSeen(const QString &id, const QString &updatedAt)
{
    if (id.isEmpty() || m_seen.value(id) == updatedAt)
        return;
    m_seen.insert(id, updatedAt);
    saveSeen();
    ++m_seenVersion;
    emit seenChanged();
}

QString ClaudeClient::orgPath() const
{
    return QStringLiteral("/api/organizations/") + m_org;
}

void ClaudeClient::setSessionKey(const QString &key)
{
    // Accepts the bare value, "sessionKey=...", or a whole Cookie header;
    // drops spaces, line breaks and quotes picked up while copying.
    QString k = key;
    const int at = k.indexOf(QLatin1String("sessionKey="));
    if (at >= 0) {
        k = k.mid(at + 11);
        const int end = k.indexOf(QLatin1Char(';'));
        if (end >= 0)
            k = k.left(end);
    }
    QString clean;
    for (const QChar c : k) {
        if (!c.isSpace() && c != QLatin1Char('"') && c != QLatin1Char('\''))
            clean += c;
    }
    m_sessionKey = clean;
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
    QDir(cacheDir()).removeRecursively();
    m_pending.clear();
    emit attachmentsChanged();
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
    // Only the session cookie, like a plain request with curl. Web-client
    // headers (anthropic-client-platform, Origin) make claude.ai expect
    // more than the cookie and answer "Invalid authorization".
    req.setRawHeader("Cookie", "sessionKey=" + m_sessionKey.toUtf8());
    req.setRawHeader("User-Agent", UserAgent);
    req.setRawHeader("Accept", "application/json");
    req.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    req.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
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
    if (status)
        msg += QStringLiteral(" (HTTP %1, %2)").arg(status).arg(reply->url().path());
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
        refreshUsage();
    });
}

void ClaudeClient::refreshConversations()
{
    if (m_org.isEmpty())
        return;
    // Show the list from the last run right away.
    if (m_conversations.isEmpty()) {
        const QVariantList cached = readCache(QStringLiteral("list-") + m_org).toList();
        if (!cached.isEmpty()) {
            m_conversations = cached;
            emit conversationsChanged();
        }
    }
    QNetworkReply *r = get(orgPath() + QStringLiteral("/chat_conversations?limit=100"));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        QByteArray data;
        if (!checkReply(r, &data))
            return;
        QVariantList fresh;
        const QJsonArray list = QJsonDocument::fromJson(data).array();
        for (const QJsonValue &v : list) {
            const QJsonObject c = v.toObject();
            QVariantMap m;
            m[QStringLiteral("uuid")] = c.value(QStringLiteral("uuid")).toString();
            m[QStringLiteral("name")] = c.value(QStringLiteral("name")).toString();
            m[QStringLiteral("updatedAt")] = c.value(QStringLiteral("updated_at")).toString();
            fresh << m;
        }
        sortByUpdated(fresh);
        if (fresh == m_conversations)
            return;
        m_conversations = fresh;
        writeCache(QStringLiteral("list-") + m_org, m_conversations);
        emit conversationsChanged();
        prefetchRecent();
    });
}

void ClaudeClient::openConversation(const QString &uuid)
{
    stop();
    m_conversationId = uuid;
    m_conversationName.clear();
    QString listUpdated;
    for (const QVariant &v : m_conversations) {
        const QVariantMap c = v.toMap();
        if (c.value(QStringLiteral("uuid")).toString() == uuid) {
            m_conversationName = c.value(QStringLiteral("name")).toString();
            listUpdated = c.value(QStringLiteral("updatedAt")).toString();
        }
    }
    m_leafId.clear();
    m_messages.clear();
    // Saved copy: shown at once if unchanged (see below), otherwise only as
    // the offline fallback while the chat loads.
    const QVariantMap cached = readCache(QStringLiteral("chat-") + uuid).toMap();
    m_cachedMessages = cached.value(QStringLiteral("messages")).toList();
    if (!cached.isEmpty()) {
        m_leafId = cached.value(QStringLiteral("leaf")).toString();
        if (m_conversationName.isEmpty())
            m_conversationName = cached.value(QStringLiteral("name")).toString();
    }
    // Unchanged since the saved copy (same last change time as in the chat
    // list): show it at once; the check with claude.ai runs in the background.
    const QString cachedUpdated = cached.value(QStringLiteral("updatedAt")).toString();
    if (!m_cachedMessages.isEmpty() && !cachedUpdated.isEmpty() && cachedUpdated == listUpdated) {
        m_messages = m_cachedMessages;
        emit conversationChanged();
        emit messagesChanged();
        setLoadingChat(false);
        loadConversation();
        return;
    }
    emit conversationChanged();
    emit messagesChanged();
    setLoadingChat(true);
    loadConversation();
}

// Loads the newest chats in the background into their saved copies, so
// opening one is instant (unchanged copies are shown without waiting).
void ClaudeClient::prefetchRecent()
{
    const int count = qMin(4, m_conversations.size());
    for (int i = 0; i < count; ++i) {
        const QVariantMap c = m_conversations.at(i).toMap();
        const QString id = c.value(QStringLiteral("uuid")).toString();
        const QString updated = c.value(QStringLiteral("updatedAt")).toString();
        if (id.isEmpty() || id == m_conversationId)
            continue;
        if (readCache(QStringLiteral("chat-") + id).toMap().value(QStringLiteral("updatedAt")).toString() == updated)
            continue;   // saved copy is up to date
        QNetworkReply *r = m_nam.get(request(orgPath() + QStringLiteral("/chat_conversations/") + id
                                             + QStringLiteral("?tree=False&rendering_mode=messages")));
        connect(r, &QNetworkReply::finished, this, [r, id]() {
            r->deleteLater();
            const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (r->error() != QNetworkReply::NoError || status < 200 || status >= 300)
                return;   // quietly skipped; opening the chat loads it normally
            writeCache(QStringLiteral("chat-") + id, conversationCopy(QJsonDocument::fromJson(r->readAll()).object()));
        });
    }
}

void ClaudeClient::setLoadingChat(bool on)
{
    if (m_loadingChat == on)
        return;
    m_loadingChat = on;
    m_loadProgress = -1;
    emit loadingChatChanged();
}

void ClaudeClient::newConversation()
{
    stop();
    setLoadingChat(false);
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
                           + QStringLiteral("?tree=False&rendering_mode=messages"));
    if (m_loadingChat) {
        connect(r, &QNetworkReply::downloadProgress, this, [this, id](qint64 got, qint64 total) {
            if (id != m_conversationId || !m_loadingChat)
                return;
            m_loadProgress = total > 0 ? qreal(got) / qreal(total) : -1;
            emit loadingChatChanged();
        });
    }
    connect(r, &QNetworkReply::finished, this, [this, r, id]() {
        QByteArray data;
        if (id != m_conversationId) {
            r->deleteLater();
            return;
        }
        if (!checkReply(r, &data)) {
            if (m_loadingChat && m_messages.isEmpty() && !m_cachedMessages.isEmpty()) {
                m_messages = m_cachedMessages;   // offline: last known state
                emit messagesChanged();
            }
            setLoadingChat(false);
            return;
        }
        const QVariantMap cache = conversationCopy(QJsonDocument::fromJson(data).object());
        m_conversationName = cache.value(QStringLiteral("name")).toString();
        m_leafId = cache.value(QStringLiteral("leaf")).toString();
        const QVariantList fresh = cache.value(QStringLiteral("messages")).toList();
        writeCache(QStringLiteral("chat-") + id, cache);
        emit conversationChanged();
        if (fresh != m_messages) {
            m_messages = fresh;
            emit messagesChanged();
        }
        setLoadingChat(false);
    });
}

void ClaudeClient::send(const QString &text)
{
    const QString prompt = text.trimmed();
    if ((prompt.isEmpty() && m_pending.isEmpty()) || m_org.isEmpty() || sending() || !attachmentsReady())
        return;
    setError(QString());

    // Text files go along as "attachments" (content inline), uploaded
    // images/PDFs as "files" (their upload ids).
    QVariantList attachments, files;
    QStringList names;
    for (const QVariantMap &a : m_pending) {
        names << QStringLiteral("\U0001F4CE ") + a.value(QStringLiteral("name")).toString();
        if (a.value(QStringLiteral("kind")).toString() == QLatin1String("text")) {
            QVariantMap att;
            att[QStringLiteral("file_name")] = a.value(QStringLiteral("name"));
            att[QStringLiteral("file_type")] = a.value(QStringLiteral("mime"));
            att[QStringLiteral("file_size")] = a.value(QStringLiteral("size"));
            att[QStringLiteral("extracted_content")] = a.value(QStringLiteral("content"));
            attachments << att;
        } else {
            files << a.value(QStringLiteral("uuid"));
        }
    }
    m_pending.clear();
    emit attachmentsChanged();

    QVariantMap user;
    user[QStringLiteral("uuid")] = QString();
    user[QStringLiteral("fromUser")] = true;
    user[QStringLiteral("text")] = names.isEmpty() ? prompt
                                                   : (prompt + QStringLiteral("\n\n") + names.join(QStringLiteral("\n"))).trimmed();
    m_messages << user;
    emit messagesChanged();

    if (!m_conversationId.isEmpty()) {
        startCompletion(prompt, attachments, files);
        return;
    }

    // New chat: create the conversation first.
    QVariantMap body;
    body[QStringLiteral("uuid")] = newUuid();
    body[QStringLiteral("name")] = QString();
    QNetworkReply *r = post(orgPath() + QStringLiteral("/chat_conversations"), body);
    connect(r, &QNetworkReply::finished, this, [this, r, prompt, attachments, files]() {
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
        startCompletion(prompt, attachments, files);
    });
}

QVariantList ClaudeClient::attachments() const
{
    QVariantList list;
    for (const QVariantMap &a : m_pending) {
        QVariantMap m;
        m[QStringLiteral("name")] = a.value(QStringLiteral("name"));
        m[QStringLiteral("kind")] = a.value(QStringLiteral("kind"));
        m[QStringLiteral("uploading")] = a.value(QStringLiteral("uploading")).toBool();
        m[QStringLiteral("error")] = a.value(QStringLiteral("error")).toString();
        list << m;
    }
    return list;
}

bool ClaudeClient::attachmentsReady() const
{
    for (const QVariantMap &a : m_pending)
        if (a.value(QStringLiteral("uploading")).toBool() || !a.value(QStringLiteral("error")).toString().isEmpty())
            return false;
    return true;
}

void ClaudeClient::removeAttachment(int index)
{
    if (index < 0 || index >= m_pending.size())
        return;
    m_pending.removeAt(index);
    emit attachmentsChanged();
}

// Text files (by MIME type or a known source extension, up to 2 MB) are
// read and sent inline. Images and PDFs are uploaded to claude.ai first.
void ClaudeClient::addAttachment(const QString &filePath)
{
    QString path = filePath;
    if (path.startsWith(QLatin1String("file://")))
        path = QUrl(path).toLocalFile();
    QFileInfo info(path);
    if (!info.isFile()) {
        setError(tr("File not found: %1").arg(path));
        return;
    }
    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info);
    static const QStringList textExt = {
        QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("csv"), QStringLiteral("json"),
        QStringLiteral("xml"), QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("ini"),
        QStringLiteral("log"), QStringLiteral("qml"), QStringLiteral("js"), QStringLiteral("ts"),
        QStringLiteral("py"), QStringLiteral("c"), QStringLiteral("cpp"), QStringLiteral("h"),
        QStringLiteral("hpp"), QStringLiteral("java"), QStringLiteral("kt"), QStringLiteral("go"),
        QStringLiteral("rs"), QStringLiteral("sh"), QStringLiteral("html"), QStringLiteral("css"),
        QStringLiteral("sql"), QStringLiteral("pro"), QStringLiteral("spec"), QStringLiteral("conf") };
    const bool isText = mime.inherits(QStringLiteral("text/plain"))
                        || textExt.contains(info.suffix().toLower());

    QVariantMap a;
    a[QStringLiteral("name")] = info.fileName();
    a[QStringLiteral("mime")] = mime.name();
    a[QStringLiteral("size")] = info.size();

    if (isText) {
        if (info.size() > 2 * 1024 * 1024) {
            setError(tr("%1 is too large (max. 2 MB for text)").arg(info.fileName()));
            return;
        }
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            setError(tr("Cannot read %1").arg(info.fileName()));
            return;
        }
        a[QStringLiteral("kind")] = QStringLiteral("text");
        a[QStringLiteral("content")] = QString::fromUtf8(f.readAll());
        m_pending << a;
        emit attachmentsChanged();
        return;
    }

    if (!mime.name().startsWith(QLatin1String("image/")) && mime.name() != QLatin1String("application/pdf")) {
        setError(tr("%1: only text files, images and PDFs can be attached").arg(info.fileName()));
        return;
    }
    if (info.size() > 30 * 1024 * 1024) {
        setError(tr("%1 is too large (max. 30 MB)").arg(info.fileName()));
        return;
    }
    QFile *file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        setError(tr("Cannot read %1").arg(info.fileName()));
        return;
    }
    const QString key = newUuid();
    a[QStringLiteral("kind")] = QStringLiteral("file");
    a[QStringLiteral("key")] = key;
    a[QStringLiteral("uploading")] = true;
    m_pending << a;
    emit attachmentsChanged();

    QHttpMultiPart *multi = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart part;
    part.setHeader(QNetworkRequest::ContentTypeHeader, mime.name());
    part.setHeader(QNetworkRequest::ContentDispositionHeader,
                   QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(info.fileName()));
    part.setBodyDevice(file);
    file->setParent(multi);
    multi->append(part);

    QNetworkRequest req = request(QStringLiteral("/api/") + m_org + QStringLiteral("/upload"));
    QNetworkReply *r = m_nam.post(req, multi);
    multi->setParent(r);
    connect(r, &QNetworkReply::finished, this, [this, r, key]() {
        r->deleteLater();
        const QByteArray body = r->readAll();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        for (QVariantMap &a : m_pending) {
            if (a.value(QStringLiteral("key")).toString() != key)
                continue;
            a[QStringLiteral("uploading")] = false;
            const QJsonObject obj = QJsonDocument::fromJson(body).object();
            const QString uuid = obj.value(QStringLiteral("file_uuid")).toString();
            if (r->error() == QNetworkReply::NoError && status >= 200 && status < 300 && !uuid.isEmpty()) {
                a[QStringLiteral("uuid")] = uuid;
            } else {
                QString msg = obj.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
                if (msg.isEmpty())
                    msg = status ? QStringLiteral("HTTP %1").arg(status) : r->errorString();
                a[QStringLiteral("error")] = tr("upload failed: %1").arg(msg);
            }
        }
        emit attachmentsChanged();
    });
}

void ClaudeClient::startCompletion(const QString &text, const QVariantList &attachments, const QVariantList &files)
{
    QVariantMap body;
    body[QStringLiteral("prompt")] = text;
    body[QStringLiteral("parent_message_uuid")] = m_leafId.isEmpty() ? RootParent : m_leafId;
    body[QStringLiteral("timezone")] = QString::fromUtf8(QTimeZone::systemTimeZoneId());
    body[QStringLiteral("attachments")] = attachments;
    body[QStringLiteral("files")] = files;
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
            if (!aborted)
                emit answerFinished(m_conversationName, m_streamText);
            refreshUsage();
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

namespace {

QString usageLabel(const QString &key)
{
    if (key == QLatin1String("five_hour"))
        return ClaudeClient::tr("Current session (5 hours)");
    if (key == QLatin1String("seven_day"))
        return ClaudeClient::tr("Weekly, all models");
    if (key == QLatin1String("seven_day_opus"))
        return ClaudeClient::tr("Weekly, Opus");
    if (key == QLatin1String("seven_day_sonnet"))
        return ClaudeClient::tr("Weekly, Sonnet");
    if (key == QLatin1String("seven_day_oauth_apps"))
        return ClaudeClient::tr("Weekly, apps");
    if (key == QLatin1String("extra_usage"))
        return ClaudeClient::tr("Extra usage");
    QString t = key;
    t.replace(QLatin1Char('_'), QLatin1Char(' '));
    return t.left(1).toUpper() + t.mid(1);
}

int usageOrder(const QString &key)
{
    static const QStringList order = { QStringLiteral("five_hour"), QStringLiteral("seven_day"),
                                       QStringLiteral("seven_day_opus"), QStringLiteral("seven_day_sonnet") };
    const int i = order.indexOf(key);
    return i < 0 ? order.size() : i;
}

} // namespace

// Plan usage limits (claude.ai "Usage" settings): every window object that
// has a "utilization" (percent used) and usually "resets_at".
void ClaudeClient::refreshUsage()
{
    if (m_org.isEmpty())
        return;
    QNetworkReply *r = m_nam.get(request(orgPath() + QStringLiteral("/usage")));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        r->deleteLater();
        const QByteArray body = r->readAll();
        const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (r->error() != QNetworkReply::NoError || status < 200 || status >= 300) {
            m_usageError = status ? QStringLiteral("HTTP %1 (%2)").arg(status).arg(r->url().path())
                                  : r->errorString();
            emit usageChanged();
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(body).object();
        QList<QVariantMap> items;
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            const QJsonObject w = it.value().toObject();
            if (w.isEmpty() || !w.contains(QStringLiteral("utilization")))
                continue;
            if (w.contains(QStringLiteral("is_enabled")) && !w.value(QStringLiteral("is_enabled")).toBool())
                continue;
            const QJsonValue u = w.value(QStringLiteral("utilization"));
            if (u.isNull())
                continue;
            QVariantMap m;
            m[QStringLiteral("key")] = it.key();
            m[QStringLiteral("label")] = usageLabel(it.key());
            m[QStringLiteral("percent")] = u.toDouble();
            m[QStringLiteral("resetsAt")] = w.value(QStringLiteral("resets_at")).toString();
            items << m;
        }
        std::sort(items.begin(), items.end(), [](const QVariantMap &a, const QVariantMap &b) {
            const int oa = usageOrder(a.value(QStringLiteral("key")).toString());
            const int ob = usageOrder(b.value(QStringLiteral("key")).toString());
            return oa != ob ? oa < ob : a.value(QStringLiteral("key")).toString() < b.value(QStringLiteral("key")).toString();
        });
        m_usage.clear();
        for (const QVariantMap &m : items)
            m_usage << m;
        m_usageError = m_usage.isEmpty() ? tr("No usage data in the answer") : QString();
        emit usageChanged();
    });
}
