#pragma once

#include <QObject>
#include <QProcess>
#include <QMap>
#include <QList>
#include <QPointer>
#include <QVariant>

class CommandEntry : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString command READ command CONSTANT)
    Q_PROPERTY(QString cmdOutput READ cmdOutput NOTIFY outputChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY runningChanged)

public:
    CommandEntry(const QString& name, const QString& command, QObject* parent = nullptr)
        : QObject(parent), m_name(name), m_command(command), process(nullptr), isStopping(false) {}QString name() const { return m_name; }
    QString command() const { return m_command; }
    QString cmdOutput() const { return m_output; }    QString output() const { return m_output; }
    bool isRunning() const { return process && process->state() == QProcess::Running; }
    QProcess* process = nullptr;
    bool isStopping = false;  // 标记是否正在主动停止

    void appendOutput(const QString& out) {
        m_output += out;
        emit outputChanged();
    }

    void clearOutput() {
        m_output.clear();
        emit outputChanged();
    }

signals:
    void outputChanged();
    void runningChanged();

private:
    QString m_name;
    QString m_command;
    QString m_output;
};

class CommandManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QObject*> commandList READ commandList NOTIFY commandListChanged)

public:
    explicit CommandManager(QObject* parent = nullptr);
    QList<QObject*> commandList();

    Q_INVOKABLE void addCommand(const QString& name, const QString& command);
    Q_INVOKABLE void startCommand(const QString& name);
    Q_INVOKABLE void stopCommand(const QString& name);
    Q_INVOKABLE QString getOutput(const QString& name);
    Q_INVOKABLE bool isRunning(const QString& name);
    Q_INVOKABLE void clearOutput(const QString& name);

signals:
    void commandListChanged();
    void outputUpdated(const QString& name);
    void commandStatusChanged(const QString& name, bool running);

private:
    QMap<QString, CommandEntry*> m_commandMap;
    QList<QObject*> m_commandList;

    void handleProcessOutput(CommandEntry* entry);
    void handleProcessError(CommandEntry* entry, QProcess::ProcessError error);
    void handleProcessFinished(CommandEntry* entry, int exitCode, QProcess::ExitStatus exitStatus);
};
