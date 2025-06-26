#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QApplication>

class TrayManager : public QObject
{
    Q_OBJECT

public:
    explicit TrayManager(QObject *parent = nullptr);
    ~TrayManager();

    Q_INVOKABLE void showTrayIcon();
    Q_INVOKABLE void hideTrayIcon();
    Q_INVOKABLE bool isTrayAvailable() const;

signals:
    void showMainWindow();
    void exitApplication();

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowMainWindow();
    void onExitApplication();

private:
    void createTrayIcon();
    void createTrayMenu();

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_showAction;
    QAction *m_exitAction;
};
