#include <QQmlContext>
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include "CommandManager.h"
#include "TrayManager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置 Material Design 样式
    QQuickStyle::setStyle("Material");

    QQmlApplicationEngine engine;

    CommandManager commandManager;
    TrayManager trayManager;    // 注册到 QML 上下文 —— 必须在 loadFromModule 之前
    engine.rootContext()->setContextProperty("commandManager", &commandManager);
    engine.rootContext()->setContextProperty("trayManager", &trayManager);
      // 连接托盘管理器信号
    QObject::connect(&trayManager, &TrayManager::exitApplication, &app, &QApplication::quit);
    
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);   
    
    engine.loadFromModule("RCmdLaunch", "Main");

    // 获取主窗口对象
    QObject *mainWindow = nullptr;
    if (!engine.rootObjects().isEmpty()) {
        mainWindow = engine.rootObjects().first();
    }

    // 连接托盘管理器的显示主窗口信号
    if (mainWindow) {
        QObject::connect(&trayManager, &TrayManager::showMainWindow, mainWindow, [mainWindow]() {
            QMetaObject::invokeMethod(mainWindow, "showWindow");
        });
    }

    // 显示系统托盘图标（如果可用）
    if (trayManager.isTrayAvailable()) {
        trayManager.showTrayIcon();
    }

    return app.exec();
}
