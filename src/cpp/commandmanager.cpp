#include "CommandManager.h"
#include <QDebug>
#include <QVariant>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>

CommandManager::CommandManager(QObject* parent) : QObject(parent) {
    initializeDatabase();
    loadSavedCommands();
}

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
    
    // 保存到数据库
    saveCommand(name, command);
    
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

void CommandManager::removeCommand(const QString& name) {
    if (!m_commandMap.contains(name)) return;
    
    CommandEntry* entry = m_commandMap.value(name);
    
    // 先停止命令（如果正在运行）
    if (entry->process && entry->process->state() != QProcess::NotRunning) {
        stopCommand(name);
    }
    
    // 从数据库删除
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM commands WHERE name = ?");
    query.addBindValue(name);
    if (!query.exec()) {
        qWarning() << "Failed to delete command from database:" << query.lastError().text();
    }
    
    // 从内存中删除
    m_commandMap.remove(name);
    m_commandList.removeOne(entry);
    entry->deleteLater();
    
    emit commandListChanged();
}

void CommandManager::saveCommand(const QString& name, const QString& command) {
    QSqlQuery query(m_database);
    
    // 使用 INSERT OR REPLACE 来处理更新和插入
    query.prepare("INSERT OR REPLACE INTO commands (name, command, created_at) VALUES (?, ?, datetime('now'))");
    query.addBindValue(name);
    query.addBindValue(command);
    
    if (!query.exec()) {
        qWarning() << "Failed to save command:" << query.lastError().text();
        return;
    }
    
    qDebug() << "Command saved:" << name;
}

void CommandManager::loadSavedCommands() {
    QSqlQuery query("SELECT name, command FROM commands ORDER BY created_at", m_database);
    
    while (query.next()) {
        QString name = query.value(0).toString();
        QString command = query.value(1).toString();
        
        if (!createCommandFromDatabase(name, command)) {
            qWarning() << "Failed to create command from database:" << name;
        }
    }
    
    if (query.lastError().isValid()) {
        qWarning() << "Failed to load commands:" << query.lastError().text();
    }
}

bool CommandManager::initializeDatabase() {
    // 获取应用安装目录
    QString dataPath = QCoreApplication::applicationDirPath();
    QDir dataDir;
    if (!dataDir.mkpath(dataPath)) {
        qWarning() << "Failed to create data directory:" << dataPath;
        return false;
    }
    
    QString dbPath = dataPath + "/commands.db";
    
    m_database = QSqlDatabase::addDatabase("QSQLITE");
    m_database.setDatabaseName(dbPath);
    
    if (!m_database.open()) {
        qWarning() << "Failed to open database:" << m_database.lastError().text();
        return false;
    }
    
    // 创建表
    QSqlQuery query(m_database);
    QString createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS commands (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE NOT NULL,
            command TEXT NOT NULL,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createTableSQL)) {
        qWarning() << "Failed to create table:" << query.lastError().text();
        return false;
    }
    
    qDebug() << "Database initialized successfully at:" << dbPath;
    return true;
}

bool CommandManager::createCommandFromDatabase(const QString& name, const QString& command) {
    if (m_commandMap.contains(name)) {
        qWarning() << "Command with name already exists:" << name;
        return false;
    }

    qDebug() << "Loading command from database:" << name << command;
    auto* entry = new CommandEntry(name, command, this);
    m_commandMap.insert(name, entry);
    m_commandList.append(entry);
    return true;
}

bool CommandManager::editCommand(const QString& oldName, const QString& newName, const QString& newCommand) {
    if (!m_commandMap.contains(oldName)) {
        qWarning() << "Command not found:" << oldName;
        return false;
    }
    
    // 如果名称改变了，检查新名称是否唯一
    if (oldName != newName) {
        if (!isCommandNameUnique(newName, oldName)) {
            qWarning() << "Command name already exists:" << newName;
            return false;
        }
    }
    
    CommandEntry* entry = m_commandMap.value(oldName);
    
    // 如果命令正在运行，先停止它
    if (entry->process && entry->process->state() != QProcess::NotRunning) {
        stopCommand(oldName);
    }
    
    // 更新数据库
    QSqlQuery query(m_database);
    
    if (oldName != newName) {
        // 如果名称改变了，使用UPDATE更新名称和命令
        query.prepare("UPDATE commands SET name = ?, command = ?, updated_at = datetime('now') WHERE name = ?");
        query.addBindValue(newName);
        query.addBindValue(newCommand);
        query.addBindValue(oldName);
    } else {
        // 如果只是更新命令内容
        query.prepare("UPDATE commands SET command = ?, updated_at = datetime('now') WHERE name = ?");
        query.addBindValue(newCommand);
        query.addBindValue(oldName);
    }
    
    if (!query.exec()) {
        qWarning() << "Failed to update command in database:" << query.lastError().text();
        return false;
    }
    
    // 更新内存中的数据
    if (oldName != newName) {
        // 从旧的映射中移除
        m_commandMap.remove(oldName);
        
        // 创建新的CommandEntry
        CommandEntry* newEntry = new CommandEntry(newName, newCommand, this);
        m_commandMap.insert(newName, newEntry);
        
        // 在列表中替换
        int index = m_commandList.indexOf(entry);
        if (index >= 0) {
            m_commandList.replace(index, newEntry);
        }
        
        // 删除旧的entry
        entry->deleteLater();
    } else {
        // 只更新命令内容，需要更新CommandEntry的私有成员
        // 由于无法直接访问私有成员，我们需要重新创建entry
        m_commandMap.remove(oldName);
        CommandEntry* newEntry = new CommandEntry(newName, newCommand, this);
        m_commandMap.insert(newName, newEntry);
        
        int index = m_commandList.indexOf(entry);
        if (index >= 0) {
            m_commandList.replace(index, newEntry);
        }
        
        entry->deleteLater();
    }
    
    emit commandListChanged();
    qDebug() << "Command edited successfully:" << oldName << "->" << newName;
    return true;
}

bool CommandManager::isCommandNameUnique(const QString& name, const QString& excludeName) {
    // 检查内存中的命令
    for (auto it = m_commandMap.constBegin(); it != m_commandMap.constEnd(); ++it) {
        if (it.key() == name && it.key() != excludeName) {
            return false;
        }
        printf("Checking command: %s\n", qPrintable(it.key()));
    }
    return true;
}

QString CommandManager::getCommandContent(const QString& name) {
    if (!m_commandMap.contains(name)) {
        return "";
    }
    return m_commandMap[name]->command();
}
