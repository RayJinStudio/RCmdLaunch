#include "CommandManager.h"
#include <QDebug>
#include <QVariant>
#include <QTimer>

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
    entry->m_isStopping = false;  // 确保重置停止标志

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
                         // 停止并清理定时器
                         if (entry->stopTimer) {
                             entry->stopTimer->stop();
                         }
                           entry->m_isStopping = false;  // 重置停止标志
                         
                         // 只有在非主动停止且异常退出时才显示警告
                         if (exitStatus == QProcess::CrashExit && !entry->m_isStopping) {
                             qWarning() << "Process crashed with exit code:" << exitCode;
                         }
                         
                         emit commandStatusChanged(entry->name(), false);
                         emit entry->runningChanged();
                         emit entry->stoppingChanged();
                     });    QObject::connect(process, &QProcess::errorOccurred, [this, entry](QProcess::ProcessError err) {
        // 只有在非主动停止的情况下才输出错误
        if (!entry->m_isStopping) {
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
        entry->m_isStopping = true;  // 标记为主动停止
        emit entry->stoppingChanged();  // 发射信号通知UI更新
        
        // 创建定时器用于超时控制
        if (!entry->stopTimer) {
            entry->stopTimer = new QTimer(this);
            entry->stopTimer->setSingleShot(true);
        }
        
        // 连接进程结束信号，确保在进程正常结束时清理
        QObject::connect(entry->process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         entry->stopTimer, &QTimer::stop, Qt::UniqueConnection);
        
        // 设置3秒超时，如果进程还没结束则强制杀死
        QObject::connect(entry->stopTimer, &QTimer::timeout, [this, entry]() {
            forceKillProcess(entry);
        });
        
        // 先尝试温和的终止
        entry->process->terminate();
        entry->stopTimer->start(3000);  // 3秒超时
        
        // 立即更新UI状态，不等待进程实际结束
        entry->clearOutput();
        emit commandStatusChanged(name, false);
        emit entry->runningChanged();
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

void CommandManager::forceKillProcess(CommandEntry* entry) {
    if (entry->process && entry->process->state() != QProcess::NotRunning) {
        qDebug() << "Force killing process for command:" << entry->name();
        entry->process->kill();
        
        // 创建一个单次定时器，在1秒后完成清理工作
        QTimer* killTimer = new QTimer(this);
        killTimer->setSingleShot(true);
        QObject::connect(killTimer, &QTimer::timeout, [this, entry, killTimer]() {
            entry->m_isStopping = false;  // 重置标记
            emit entry->stoppingChanged();  // 发射信号通知UI更新
            killTimer->deleteLater();
        });
        killTimer->start(1000);
    } else {
        entry->m_isStopping = false;  // 重置标记
        emit entry->stoppingChanged();  // 发射信号通知UI更新
    }
}
