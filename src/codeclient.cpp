#include "codeclient.h"
#include "claudeclient.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

namespace {

const QString BaseUrl = QStringLiteral("https://claude.ai");
const QByteArray UserAgent = QByteArrayLiteral(
    "Mozilla/5.0 (X11; Linux x86_64; rv:128.0) Gecko/20100101 Firefox/128.0");

QString str(const QVariantMap &m, const char *key)
{
    return m.value(QLatin1String(key)).toString();
}

// Short one-line summary of a tool call (command, file, pattern, ...).
QString toolSummary(const QVariantMap &block)
{
    const QString name = str(block, "name");
    const QVariantMap in = block.value(QStringLiteral("input")).toMap();
    static const char *keys[] = { "command", "file_path", "path", "pattern", "url", "query",
                                  "description", "prompt", nullptr };
    QString detail;
    for (int i = 0; keys[i] && detail.isEmpty(); ++i)
        detail = str(in, keys[i]);
    detail = detail.simplified();
    if (detail.length() > 160)
        detail = detail.left(157) + QStringLiteral("...");
    return detail.isEmpty() ? name : name + QStringLiteral(": ") + detail;
}

bool isTechnical(const QString &text)
{
    const QString t = text.trimmed();
    return t.isEmpty() || t.startsWith(QLatin1Char('<')) || t.startsWith(QLatin1String("Caveat:"))
           || t.startsWith(QLatin1String("[Request interrupted"));
}

QVariantMap item(const QString &kind, const QString &text)
{
    QVariantMap m;
    m[QStringLiteral("kind")] = kind;          // "user", "assistant", "tool"
    m[QStringLiteral("fromUser")] = kind == QLatin1String("user");
    m[QStringLiteral("text")] = text;
    return m;
}

} // namespace

CodeClient::CodeClient(ClaudeClient *claude, QObject *parent)
    : QObject(parent), m_claude(claude)
{
    // While a session is open, fetch new events now and then (the session
    // keeps working in the cloud after a message was sent).
    m_poll.setInterval(4000);
    connect(&m_poll, &QTimer::timeout, this, [this]() { loadSession(true); });
    // Watch all sessions in the background (also while the app is in the
    // background) to notify when one stops running.
    m_watch.setInterval(30000);
    connect(&m_watch, &QTimer::timeout, this, [this]() { fetchSessions(true); });
    connect(m_claude, &ClaudeClient::loggedInChanged, this, [this]() {
        if (m_claude->loggedIn()) {
            refreshSessions();
            m_watch.start();
        } else {
            m_watch.stop();
            m_lastStatus.clear();
        }
    });
}

QNetworkRequest CodeClient::request(const QString &path, bool json) const
{
    QNetworkRequest req(QUrl(BaseUrl + path));
    req.setRawHeader("Cookie", "sessionKey=" + m_claude->sessionKey().toUtf8());
    req.setRawHeader("User-Agent", UserAgent);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("anthropic-version", "2023-06-01");
    req.setRawHeader("anthropic-beta", "ccr-byoc-2025-07-29");
    req.setRawHeader("x-organization-uuid", m_claude->orgId().toUtf8());
    req.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
    req.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
    if (json)
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    return req;
}

bool CodeClient::checkReply(QNetworkReply *reply, QByteArray *data, bool counted)
{
    reply->deleteLater();
    if (counted)
        setBusy(false);
    const QByteArray body = reply->readAll();
    if (data)
        *data = body;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
        setError(QString());
        return true;
    }
    QString msg = QJsonDocument::fromJson(body).object().value(QStringLiteral("error")).toObject()
                      .value(QStringLiteral("message")).toString();
    if (msg.isEmpty())
        msg = status ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
    if (status)
        msg += QStringLiteral(" (HTTP %1, %2)").arg(status).arg(reply->url().path());
    setError(msg);
    return false;
}

void CodeClient::setBusy(bool on)
{
    const bool was = busy();
    m_busy = qMax(0, m_busy + (on ? 1 : -1));
    if (was != busy())
        emit busyChanged();
}

void CodeClient::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void CodeClient::refreshSessions()
{
    fetchSessions(false);
}

void CodeClient::fetchSessions(bool quiet)
{
    if (m_claude->orgId().isEmpty())
        return;
    if (!quiet)
        setBusy(true);
    QNetworkReply *r = m_nam.get(request(QStringLiteral("/v1/sessions?limit=50")));
    connect(r, &QNetworkReply::finished, this, [this, r, quiet]() {
        QByteArray data;
        if (!checkReply(r, &data, !quiet))
            return;
        const QJsonDocument doc = QJsonDocument::fromJson(data);
        const QVariantList list = doc.isArray() ? doc.array().toVariantList()
                                                : doc.object().value(QStringLiteral("data")).toArray().toVariantList();
        m_sessions.clear();
        for (const QVariant &v : list) {
            const QVariantMap s = v.toMap();
            QVariantMap m;
            m[QStringLiteral("id")] = str(s, "id");
            m[QStringLiteral("title")] = str(s, "title");
            m[QStringLiteral("status")] = str(s, "session_status");
            m[QStringLiteral("updatedAt")] = str(s, "updated_at");
            m_sessions << m;

            const QString id = str(s, "id");
            const QString status = str(s, "session_status");
            const QString before = m_lastStatus.value(id);
            if (before == QLatin1String("running") && status != before && !status.isEmpty())
                emit sessionFinished(id, str(s, "title"), status);
            m_lastStatus.insert(id, status);
        }
        std::sort(m_sessions.begin(), m_sessions.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("updatedAt")).toString()
                   > b.toMap().value(QStringLiteral("updatedAt")).toString();
        });
        emit sessionsChanged();
    });
}

void CodeClient::openSession(const QString &id)
{
    m_sessionId = id;
    m_sessionTitle.clear();
    m_sessionStatus.clear();
    for (const QVariant &v : m_sessions) {
        const QVariantMap s = v.toMap();
        if (str(s, "id") == id) {
            m_sessionTitle = str(s, "title");
            m_sessionStatus = str(s, "status");
        }
    }
    m_events.clear();
    m_eventIds.clear();
    m_messages.clear();
    emit sessionChanged();
    emit messagesChanged();
    setLoadingSession(true);
    loadSession(false);
    m_poll.start();
}

void CodeClient::setLoadingSession(bool on)
{
    if (m_loadingSession == on)
        return;
    m_loadingSession = on;
    emit loadingSessionChanged();
}

void CodeClient::closeSession()
{
    m_poll.stop();
    setLoadingSession(false);
    m_sessionId.clear();
    emit sessionChanged();
}

// Session details (title, status), then new events.
void CodeClient::loadSession(bool quiet)
{
    if (m_sessionId.isEmpty())
        return;
    const QString id = m_sessionId;
    if (!quiet)
        setBusy(true);
    QNetworkReply *r = m_nam.get(request(QStringLiteral("/v1/sessions/") + id));
    connect(r, &QNetworkReply::finished, this, [this, r, id, quiet]() {
        QByteArray data;
        if (!checkReply(r, &data, !quiet) || id != m_sessionId) {
            if (id == m_sessionId)
                setLoadingSession(false);
            return;
        }
        const QJsonObject s = QJsonDocument::fromJson(data).object();
        const QString title = s.value(QStringLiteral("title")).toString();
        const QString status = s.value(QStringLiteral("session_status")).toString();
        if (m_sessionStatus == QLatin1String("running") && status != m_sessionStatus && !status.isEmpty()
                && m_lastStatus.value(id) == QLatin1String("running"))
            emit sessionFinished(id, title.isEmpty() ? m_sessionTitle : title, status);
        if (!status.isEmpty())
            m_lastStatus.insert(id, status);
        if (title != m_sessionTitle || status != m_sessionStatus) {
            if (!title.isEmpty())
                m_sessionTitle = title;
            m_sessionStatus = status;
            emit sessionChanged();
        }
        loadEvents(m_eventIds.isEmpty() ? QString() : m_eventIds.last(), quiet);
    });
}

void CodeClient::loadEvents(const QString &afterId, bool quiet)
{
    const QString id = m_sessionId;
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("limit"), QStringLiteral("500"));
    if (!afterId.isEmpty())
        q.addQueryItem(QStringLiteral("after_id"), afterId);
    if (!quiet)
        setBusy(true);
    QNetworkReply *r = m_nam.get(request(QStringLiteral("/v1/sessions/") + id + QStringLiteral("/events?")
                                         + q.toString(QUrl::FullyEncoded)));
    connect(r, &QNetworkReply::finished, this, [this, r, id, quiet]() {
        QByteArray data;
        if (!checkReply(r, &data, !quiet) || id != m_sessionId) {
            if (id == m_sessionId)
                setLoadingSession(false);
            return;
        }
        const QJsonObject page = QJsonDocument::fromJson(data).object();
        addEvents(page.value(QStringLiteral("data")).toArray().toVariantList());
        if (m_loadingSession)
            emit loadingSessionChanged();   // loadedEvents grew
        const QString last = page.value(QStringLiteral("last_id")).toString();
        if (page.value(QStringLiteral("has_more")).toBool() && !last.isEmpty())
            loadEvents(last, quiet);
        else
            setLoadingSession(false);
    });
}

// Turns session events (Claude Code SDK messages) into display items:
// user text, Claude's text, and one line per tool call.
void CodeClient::addEvents(const QVariantList &events)
{
    bool changed = false;
    for (const QVariant &v : events) {
        const QVariantMap ev = v.toMap();
        QString eid = str(ev, "uuid");
        if (eid.isEmpty())
            eid = str(ev, "id");
        if (!eid.isEmpty()) {
            if (m_eventIds.contains(eid))
                continue;
            m_eventIds << eid;
        }
        m_events << ev;

        const QString type = str(ev, "type");
        if (type != QLatin1String("user") && type != QLatin1String("assistant"))
            continue;
        const QVariant content = ev.value(QStringLiteral("message")).toMap().value(QStringLiteral("content"));
        if (content.type() == QVariant::String) {
            if (!isTechnical(content.toString())) {
                m_messages << item(type, content.toString());
                changed = true;
            }
            continue;
        }
        QStringList texts;
        for (const QVariant &b : content.toList()) {
            const QVariantMap block = b.toMap();
            const QString bt = str(block, "type");
            if (bt == QLatin1String("text")) {
                if (!isTechnical(str(block, "text")))
                    texts << str(block, "text");
            } else if (bt == QLatin1String("tool_use")) {
                if (!texts.isEmpty()) {
                    m_messages << item(type, texts.join(QStringLiteral("\n\n")));
                    texts.clear();
                }
                m_messages << item(QStringLiteral("tool"), toolSummary(block));
            }
            // tool_result and thinking blocks are not shown.
        }
        if (!texts.isEmpty() && !texts.join(QString()).trimmed().isEmpty())
            m_messages << item(type, texts.join(QStringLiteral("\n\n")));
        changed = true;
    }
    if (changed)
        emit messagesChanged();
}

void CodeClient::send(const QString &text)
{
    const QString msg = text.trimmed();
    if (msg.isEmpty() || m_sessionId.isEmpty())
        return;
    QString uuid = QUuid::createUuid().toString();
    uuid = uuid.mid(1, uuid.length() - 2);

    QJsonObject message;
    message[QStringLiteral("role")] = QStringLiteral("user");
    message[QStringLiteral("content")] = msg;
    QJsonObject ev;
    ev[QStringLiteral("uuid")] = uuid;
    ev[QStringLiteral("session_id")] = m_sessionId;
    ev[QStringLiteral("type")] = QStringLiteral("user");
    ev[QStringLiteral("parent_tool_use_id")] = QJsonValue::Null;
    ev[QStringLiteral("message")] = message;
    QJsonObject body;
    body[QStringLiteral("events")] = QJsonArray() << ev;

    // Show it right away; the poll brings Claude's work.
    m_messages << item(QStringLiteral("user"), msg);
    m_eventIds << uuid;
    emit messagesChanged();

    setBusy(true);
    QNetworkReply *r = m_nam.post(request(QStringLiteral("/v1/sessions/") + m_sessionId + QStringLiteral("/events"), true),
                                  QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(r, &QNetworkReply::finished, this, [this, r]() {
        if (checkReply(r, nullptr))
            loadSession(true);
    });
}
