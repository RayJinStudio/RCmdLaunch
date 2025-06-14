#include "CommandManager.h"
#include <QDebug>
#include <QVariant>

CommandManager::CommandManager(QObject* parent) : QObject(parent) {}

QList<QObject*> CommandManager::commandList() {
    return m_commandList;
}

void CommandManager::addCommand(const QString& name, const QString& command) {
    if (m_commandMap.contains(name)) {
        qWarning() << "Command with name already exists:" << name;
        return;
    }

    qDebug() << "Adding command:" << name << command;
    auto* entry = new CommandEntry(name, command, this);
    m_commandMap.insert(name, entry);
    m_commandList.append(entry);
    qDebug() << "Command list size:" << m_commandList.size();
    emit commandListChanged();
    qDebug() << "Emitted commandListChanged signal";
}

void CommandManager::startCommand(const QString& name) {
    if (!m_commandMap.contains(name)) return;

    CommandEntry* entry = m_commandMap.value(name);
    if (entry->process && entry->process->state() != QProcess::NotRunning) {
        qDebug() << "Command already running:" << name;
        return;
    }    QProcess* process = new QProcess(this);
    entry->process = process;
    entry->isStopping = false;  // 确保重置停止标志

    QObject::connect(process, &QProcess::readyReadStandardOutput, [this, entry]() {
        QByteArray out = entry->process->readAllStandardOutput();
#ifdef Q_OS_WIN
        entry->appendOutput(QString::fromLocal8Bit(out));
#else
        entry->appendOutput(QString::fromUtf8(out));
#endif
        emit outputUpdated(entry->name());
    });

    QObject::connect(process, &QProcess::readyReadStandardError, [this, entry]() {
        QByteArray out = entry->process->readAllStandardError();
#ifdef Q_OS_WIN
        entry->appendOutput(QString::fromLocal8Bit(out));
#else
        entry->appendOutput(QString::fromUtf8(out));
#endif
        emit outputUpdated(entry->name());
    });    QObject::connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     [this, entry](int exitCode, QProcess::ExitStatus exitStatus) {
                         entry->isStopping = false;  // 重置停止标志
                         
                         // 只有在非主动停止且异常退出时才显示警告
                         if (exitStatus == QProcess::CrashExit && !entry->isStopping) {
                             qWarning() << "Process crashed with exit code:" << exitCode;
                         }
                         
                         emit commandStatusChanged(entry->name(), false);
                         emit entry->runningChanged();
                     });QObject::connect(process, &QProcess::errorOccurred, [this, entry](QProcess::ProcessError err) {
        // 只有在非主动停止的情况下才输出错误
        if (!entry->isStopping) {
            qWarning() << "Process error:" << err;
        }
        emit commandStatusChanged(entry->name(), false);
        emit entry->runningChanged();
    });

#ifdef Q_OS_WIN
    process->start("cmd.exe", QStringList() << "/C" << entry->command());
#else
    process->start("bash", QStringList() << "-c" << entry->command());
#endif

    if (!process->waitForStarted()) {
        qWarning() << "Failed to start process:" << entry->command();
        delete process;
        entry->process = nullptr;    } else {
        emit commandStatusChanged(name, true);
        emit entry->runningChanged();
    }
}

void CommandManager::stopCommand(const QString& name) {
    if (!m_commandMap.contains(name)) return;

    CommandEntry* entry = m_commandMap.value(name);
    if (entry->process && entry->process->state() != QProcess::NotRunning) {
        entry->isStopping = true;  // 标记为主动停止
        
        // 创建一个定时器来处理强制终止
        QTimer* killTimer = new QTimer(this);
        killTimer->setSingleShot(true);
        killTimer->setInterval(3000); // 3秒后强制杀死
        
        // 连接进程结束信号
        connect(entry->process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                [this, name, killTimer](int exitCode, QProcess::ExitStatus exitStatus) {
                    Q_UNUSED(exitCode)
                    Q_UNUSED(exitStatus)
                    
                    killTimer->stop();
                    killTimer->deleteLater();
                    
                    if (m_commandMap.contains(name)) {
                        CommandEntry* entry = m_commandMap.value(name);
                        entry->isStopping = false;  // 重置标记
                        entry->clearOutput();
                        emit commandStatusChanged(name, false);
                        emit entry->runningChanged();
                    }
                });
        
        // 连接定时器超时信号 - 强制杀死进程
        connect(killTimer, &QTimer::timeout, [this, name, killTimer]() {
            if (m_commandMap.contains(name)) {
                CommandEntry* entry = m_commandMap.value(name);
                if (entry->process && entry->process->state() != QProcess::NotRunning) {
                    entry->process->kill();
                }
            }
            killTimer->deleteLater();
        });
        
        // 启动定时器并尝试温和终止
        killTimer->start();
        entry->process->terminate();
    }
}

QString CommandManager::getOutput(const QString& name) {
    if (!m_commandMap.contains(name)) return "";
    return m_commandMap[name]->output();
}

bool CommandManager::isRunning(const QString& name) {
    if (!m_commandMap.contains(name)) return false;
    auto* p = m_commandMap[name]->process;
    return p && p->state() != QProcess::NotRunning;
}

void CommandManager::clearOutput(const QString& name) {
    if (!m_commandMap.contains(name)) return;
    m_commandMap[name]->clearOutput();
    emit outputUpdated(name);
}
