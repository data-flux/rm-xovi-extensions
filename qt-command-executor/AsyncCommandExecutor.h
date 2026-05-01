#include <QObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QVariant>
#include <QQmlEngine>


class AsyncCommandExecutor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString command WRITE setCommand READ getCommand)
    Q_PROPERTY(QStringList arguments WRITE setArguments READ getArguments)
    Q_PROPERTY(QVariantMap environment MEMBER m_environment)
    Q_PROPERTY(bool running MEMBER m_running WRITE setRunning NOTIFY runningChanged)
    Q_PROPERTY(int exitCode MEMBER m_exitCode NOTIFY exitCodeChanged)

signals:
    void runningChanged();
    void exitCodeChanged();
    void stdOutAvailable(const QString &output);
    void stdErrAvailable(const QString &errors);

public:
    explicit AsyncCommandExecutor(QObject *parent = nullptr) : QObject(parent), m_command(""), m_arguments(), m_running(false), m_exitCode(-1) {
        QObject::connect(&m_process, &QProcess::finished,
                       this, &AsyncCommandExecutor::finished);
        
        m_process.setProcessChannelMode(QProcess::ForwardedChannels);
    }

    Q_INVOKABLE bool startCommand(int timeout = 30000)
    {
        if(m_running) {
            qmlEngine(this)->throwError(tr("Command already running."));
            return false;
        }
        
        switch((QObject::receivers(SIGNAL(stdOutAvailable(const QString &))) > 0) * 2 + (QObject::receivers(SIGNAL(stdErrAvailable(const QString &))) > 0))
        {
            case 1:
                m_process.setProcessChannelMode(QProcess::ForwardedOutputChannel);
            break;
            case 2:
                m_process.setProcessChannelMode(QProcess::ForwardedErrorChannel);
            break;
            case 3:
                m_process.setProcessChannelMode(QProcess::SeparateChannels);
            break;
        }
        
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("LD_PRELOAD");
        for(auto iter = m_environment.begin(), end = m_environment.end(); iter != end; iter++)
        {
            auto value = iter.value().toString();
            env.insert(iter.key(), value);
        }
        
        m_process.setProcessEnvironment(env);
        m_process.start();
        
        bool started = m_process.waitForStarted(timeout);
        
        if(started) {
            m_running = true;
            emit runningChanged();
        }
        
        return started;
    }
    
    Q_INVOKABLE bool terminateCommand(int timeout = 30000)
    {
        if(!m_running)
            return true;
        
        m_process.terminate();
        
        if(timeout != 0)
            return m_process.waitForFinished(timeout);
        else
            return !m_running;
    }
    
    void setRunning(bool new_running) {
        if(new_running == true && m_running == false)
        {
            bool started = startCommand(100);
            if(!started)
            {
                qmlEngine(this)->throwError(tr("Command did not start successfully."));
            }
        }
        else if(new_running == false && m_running == true)
        {
            bool ended = terminateCommand(100);
            if(!ended)
            {
                qmlEngine(this)->throwError(tr("Command did not terminate."));
            }
        }
    }
    
    QString getCommand() const {
        return m_process.program();
    }
    
    void setCommand(const QString& new_command) {
        m_process.setProgram(new_command);
    }
    
    QStringList getArguments() const {
        return m_process.arguments();
    }
    
    void setArguments(const QStringList& new_arguments) {
        m_process.setArguments(new_arguments);
    }

private:
    QString m_command;
    QStringList m_arguments;
    QVariantMap m_environment;
    bool m_running;
    int m_exitCode;
    
    QProcess m_process;

private slots:
    void finished(int exitCode, QProcess::ExitStatus exitStatus) {
        m_running = false;
        
        if(exitStatus == QProcess::NormalExit) 
        {
            m_exitCode = exitCode;
        }
        else
        {
            m_exitCode = -m_process.error() -1;
        }
        
        QByteArray read_data = m_process.readAllStandardError();
        if(read_data.length() > 0)
        {
            QString read_string(read_data);
            emit stdErrAvailable(read_string);
        }
        
        read_data = m_process.readAllStandardOutput();
        if(read_data.length() > 0)
        {
            QString read_string(read_data);
            emit stdOutAvailable(read_string);
        }
        
        emit runningChanged();
        emit exitCodeChanged();
    }
};

