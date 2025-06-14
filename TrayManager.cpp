#include "TrayManager.h"
#include <QIcon>
#include <QCoreApplication>
#include <QPixmap>
#include <QPainter>

TrayManager::TrayManager(QObject *parent)
    : QObject(parent)
    , m_trayIcon(nullptr)
    , m_trayMenu(nullptr)
    , m_showAction(nullptr)
    , m_exitAction(nullptr)
{
    createTrayIcon();
    createTrayMenu();
}

TrayManager::~TrayManager()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

void TrayManager::createTrayIcon()
{
    m_trayIcon = new QSystemTrayIcon(this);
    
    // 尝试加载资源中的图标，如果失败则创建默认图标
    QIcon icon(":/icon.png");
    
    if (icon.isNull()) {
        // 如果资源图标不存在，创建一个简单的图标
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        
        // 绘制一个简单的图标
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // 绘制背景圆
        painter.setBrush(QBrush(QColor(64, 128, 255)));
        painter.setPen(QPen(QColor(32, 64, 128), 2));
        painter.drawEllipse(4, 4, 24, 24);
        
        // 绘制一个"C"字母表示Command
        painter.setPen(QPen(Qt::white, 3, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(12, 12, 8, 8, 30 * 16, 300 * 16);
        
        painter.end();
        
        icon = QIcon(pixmap);
    }
    
    m_trayIcon->setIcon(icon);
    m_trayIcon->setToolTip("命令启动器 - 双击显示主窗口");

    connect(m_trayIcon, &QSystemTrayIcon::activated,
            this, &TrayManager::onTrayIconActivated);
}

void TrayManager::createTrayMenu()
{
    m_trayMenu = new QMenu();

    m_showAction = new QAction("显示主窗口", this);
    connect(m_showAction, &QAction::triggered,
            this, &TrayManager::onShowMainWindow);

    m_exitAction = new QAction("退出", this);
    connect(m_exitAction, &QAction::triggered,
            this, &TrayManager::onExitApplication);

    m_trayMenu->addAction(m_showAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_exitAction);

    if (m_trayIcon) {
        m_trayIcon->setContextMenu(m_trayMenu);
    }
}

void TrayManager::showTrayIcon()
{
    if (m_trayIcon && QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon->show();
    }
}

void TrayManager::hideTrayIcon()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
}

bool TrayManager::isTrayAvailable() const
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayManager::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        emit showMainWindow();
        break;
    default:
        break;
    }
}

void TrayManager::onShowMainWindow()
{
    emit showMainWindow();
}

void TrayManager::onExitApplication()
{
    emit exitApplication();
}
