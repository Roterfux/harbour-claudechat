#ifndef SPEECHCLIENT_H
#define SPEECHCLIENT_H

#include <QObject>
#include <QTimer>

class QDBusInterface;

// Voice via Speech Note (dsnote, org.mkiol.Speech on the session bus):
// speech-to-text from the microphone and text-to-speech, both offline.
// Speech Note must be installed with a speech model for the language.
class SpeechClient : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
    Q_PROPERTY(bool speaking READ speaking NOTIFY speakingChanged)
    Q_PROPERTY(QString partialText READ partialText NOTIFY partialTextChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit SpeechClient(QObject *parent = nullptr);

    bool available() const { return m_available; }
    bool listening() const { return m_listening; }
    bool speaking() const { return m_speaking; }
    QString partialText() const { return m_partial; }
    QString error() const { return m_error; }

    Q_INVOKABLE void checkAvailable();
    Q_INVOKABLE void listen();          // one sentence, then stops
    Q_INVOKABLE void stopListening();
    Q_INVOKABLE void speak(const QString &text);
    Q_INVOKABLE void stop();            // stop listening and speaking

signals:
    void availableChanged();
    void listeningChanged();
    void speakingChanged();
    void partialTextChanged();
    void errorChanged();
    void textRecognized(const QString &text);
    void speechFinished();

private slots:
    void onTextDecoded(const QString &text, const QString &lang, int task);
    void onIntermediateTextDecoded(const QString &text, const QString &lang, int task);
    void onStateChanged(int state);

private:
    QDBusInterface *iface();
    void setListening(bool on);
    void setSpeaking(bool on);
    void setError(const QString &error);
    int readState();

    QDBusInterface *m_iface = nullptr;
    bool m_connected = false;
    bool m_available = false;
    bool m_listening = false;
    bool m_speaking = false;
    QString m_partial;
    QString m_error;
    int m_task = -1;
    int m_idleState = -1;     // Speech Note's state value while idle
    bool m_sawBusy = false;   // state left idle since our request
    QTimer m_keepAlive;
};

#endif
