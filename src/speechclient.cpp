#include "speechclient.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRegularExpression>

namespace {
const QString Service = QStringLiteral("org.mkiol.Speech");
const QString Path = QStringLiteral("/");
const QString Interface = QStringLiteral("org.mkiol.Speech");
const int ModeSingleSentence = 2;   // Speech Note: automatic, manual, single sentence
}

SpeechClient::SpeechClient(QObject *parent)
    : QObject(parent)
{
    // Speech Note ends a task that is not kept alive.
    m_keepAlive.setInterval(4000);
    connect(&m_keepAlive, &QTimer::timeout, this, [this]() {
        if (m_task >= 0 && iface())
            iface()->asyncCall(QStringLiteral("KeepAlive"), m_task);
    });
    checkAvailable();
}

QDBusInterface *SpeechClient::iface()
{
    if (!m_iface) {
        m_iface = new QDBusInterface(Service, Path, Interface, QDBusConnection::sessionBus(), this);
        m_iface->setTimeout(15000);
    }
    if (!m_connected) {
        QDBusConnection bus = QDBusConnection::sessionBus();
        m_connected = bus.connect(Service, Path, Interface, QStringLiteral("SttTextDecoded"),
                                  this, SLOT(onTextDecoded(QString,QString,int)));
        bus.connect(Service, Path, Interface, QStringLiteral("SttIntermediateTextDecoded"),
                    this, SLOT(onIntermediateTextDecoded(QString,QString,int)));
        bus.connect(Service, Path, Interface, QStringLiteral("StatePropertyChanged"),
                    this, SLOT(onStateChanged(int)));
    }
    return m_iface;
}

void SpeechClient::checkAvailable()
{
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    bool ok = bus && bus->isServiceRegistered(Service).value();
    if (!ok && bus)
        ok = bus->call(QStringLiteral("ListActivatableNames")).arguments().value(0).toStringList().contains(Service);
    if (ok != m_available) {
        m_available = ok;
        emit availableChanged();
    }
    if (!ok)
        setError(tr("Speech Note is not installed or not running"));
}

int SpeechClient::readState()
{
    const QVariant v = iface()->property("State");
    return v.isValid() ? v.toInt() : -1;
}

void SpeechClient::listen()
{
    if (m_listening)
        return;
    stop();
    setError(QString());
    if (m_idleState < 0)
        m_idleState = readState();
    m_partial.clear();
    emit partialTextChanged();
    m_sawBusy = false;
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(
        iface()->asyncCall(QStringLiteral("SttStartListen"), ModeSingleSentence, QString(), QString()), this);
    setListening(true);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
        w->deleteLater();
        QDBusPendingReply<int> r = *w;
        if (r.isError() || r.value() < 0) {
            setListening(false);
            setError(r.isError() ? tr("Speech Note: %1").arg(r.error().message())
                                 : tr("Speech Note could not start listening (speech model installed?)"));
            checkAvailable();
            return;
        }
        m_task = r.value();
        m_keepAlive.start();
    });
}

void SpeechClient::stopListening()
{
    if (!m_listening)
        return;
    if (m_task >= 0)
        iface()->asyncCall(QStringLiteral("SttStopListen"), m_task);
    setListening(false);
}

// Text to speech; Markdown is reduced to plain sentences first.
void SpeechClient::speak(const QString &text)
{
    stop();
    QString t = text;
    t.replace(QRegularExpression(QStringLiteral("```[\\s\\S]*?(```|$)")), tr(" (code omitted) "));
    t.replace(QRegularExpression(QStringLiteral("\\[([^\\]]+)\\]\\([^)]*\\)")), QStringLiteral("\\1"));
    t.replace(QRegularExpression(QStringLiteral("https?://\\S+")), tr("link"));
    t.replace(QRegularExpression(QStringLiteral("[*_`#>|~]+")), QStringLiteral(" "));
    t.replace(QRegularExpression(QStringLiteral("^\\s*[-+]\\s+"), QRegularExpression::MultilineOption), QString());
    t = t.simplified();
    if (t.isEmpty())
        return;
    setError(QString());
    if (m_idleState < 0)
        m_idleState = readState();
    m_sawBusy = false;
    QDBusPendingCallWatcher *w = new QDBusPendingCallWatcher(
        iface()->asyncCall(QStringLiteral("TtsPlaySpeech"), t, QString()), this);
    setSpeaking(true);
    connect(w, &QDBusPendingCallWatcher::finished, this, [this, w]() {
        w->deleteLater();
        QDBusPendingReply<int> r = *w;
        if (r.isError() || r.value() < 0) {
            setSpeaking(false);
            setError(r.isError() ? tr("Speech Note: %1").arg(r.error().message())
                                 : tr("Speech Note could not speak (voice model installed?)"));
            return;
        }
        m_task = r.value();
        m_keepAlive.start();
    });
}

void SpeechClient::stop()
{
    if (m_task >= 0 && (m_listening || m_speaking))
        iface()->asyncCall(QStringLiteral("Cancel"), m_task);
    m_keepAlive.stop();
    m_task = -1;
    setListening(false);
    setSpeaking(false);
}

void SpeechClient::onIntermediateTextDecoded(const QString &text, const QString &, int task)
{
    if (!m_listening || (m_task >= 0 && task != m_task))
        return;
    m_partial = text;
    emit partialTextChanged();
}

void SpeechClient::onTextDecoded(const QString &text, const QString &, int task)
{
    if (!m_listening || (m_task >= 0 && task != m_task))
        return;
    m_partial.clear();
    emit partialTextChanged();
    // Single sentence mode: one result ends the listening.
    m_keepAlive.stop();
    m_task = -1;
    setListening(false);
    if (!text.trimmed().isEmpty())
        emit textRecognized(text.trimmed());
}

// Speech Note's state goes busy while listening/speaking and back to the
// idle value afterwards; that return ends our listening/speaking.
void SpeechClient::onStateChanged(int state)
{
    if (!m_listening && !m_speaking)
        return;
    if (m_idleState < 0 || state != m_idleState) {
        m_sawBusy = true;
        return;
    }
    if (!m_sawBusy)
        return;
    m_keepAlive.stop();
    m_task = -1;
    if (m_speaking) {
        setSpeaking(false);
        emit speechFinished();
    }
    if (m_listening)
        setListening(false);
}

void SpeechClient::setListening(bool on)
{
    if (m_listening == on)
        return;
    m_listening = on;
    emit listeningChanged();
}

void SpeechClient::setSpeaking(bool on)
{
    if (m_speaking == on)
        return;
    m_speaking = on;
    emit speakingChanged();
}

void SpeechClient::setError(const QString &error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}
