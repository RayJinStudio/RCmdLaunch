import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects


ApplicationWindow {
    id: mainWindow
    width: 900
    height: 700
    visible: true
    title: "命令启动器"
    
    // Material Design 3 配置
    Material.theme: Material.Light
    Material.primary: Material.Blue
    Material.accent: Material.LightBlue
    
    // 设置窗口背景色
    color: Material.backgroundColor

    // 添加最小化到托盘的功能
    function showWindow() {
        mainWindow.show()
        mainWindow.raise()
        mainWindow.requestActivate()
    }

    function minimizeToTray() {
        if (trayManager.isTrayAvailable()) {
            mainWindow.hide()
        } else {
            mainWindow.showMinimized()
        }
    }    // 重写关闭事件，使其最小化到托盘而不是退出
    onClosing: function(close) {
        if (trayManager && trayManager.isTrayAvailable()) {
            close.accepted = false
            minimizeToTray()        }
    }    // 标题区域 - 使用anchor定位固定在顶部
    Pane {
        id: titlePane
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        height: 100
        Material.elevation: 1
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 8
              Label {
                text: "命令启动器"
                font.pointSize: 24
                font.weight: Font.Medium
                color: Material.primary
            }
              Label {
                text: "管理和执行您的常用命令"
                font.pointSize: 13
                color: Material.hintTextColor
            }
              Label {
                text: "当前打开的输出窗口: " + Object.keys(mainWindow.outputDialogs).length + " 个"
                font.pointSize: 11
                color: Material.accent
                visible: Object.keys(mainWindow.outputDialogs).length > 0
            }
        }
    }

    // 添加命令卡片 - 使用anchor定位固定在标题下方
    Pane {
        id: addCommandPane
        anchors.top: titlePane.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 24
        anchors.topMargin: 12
        height: 120
        Material.elevation: 1
        
        ColumnLayout {
            anchors.fill: parent
            spacing: 16
              Label {
                text: "添加新命令"
                font.pointSize: 16
                font.weight: Font.Medium
                color: Material.foreground
            }
              RowLayout {
                Layout.fillWidth: true
                spacing: 16
                
                TextField {
                    id: nameField
                    placeholderText: "命令名称"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 200
                    Material.containerStyle: Material.Outlined
                }
                
                TextField {
                    id: cmdField
                    placeholderText: "启动命令"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 400
                    Material.containerStyle: Material.Outlined
                }
                
                Button {
                    text: "添加命令"
                    Material.background: Material.primary
                    Material.foreground: "white"
                    onClicked: {
                        if (nameField.text && cmdField.text) {
                            commandManager.addCommand(nameField.text, cmdField.text)
                            nameField.clear()
                            cmdField.clear()
                        }
                    }
                }
            }
        }
    }

    // 命令列表卡片 - 使用anchor定位填充剩余空间
    Pane {
        id: commandListPane
        anchors.top: addCommandPane.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        anchors.topMargin: 12
        Material.elevation: 1          
        ColumnLayout {
            anchors.fill: parent
            spacing: 16
            
            Label {
                text: "命令列表"
                font.pointSize: 16
                font.weight: Font.Medium
                color: Material.foreground
            }            // 使用ScrollView包装ListView以支持滚动
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                
                ListView {
                    id: listView
                    anchors.fill: parent
                    spacing: 8

                    // 使用Instantiator来创建模型
                    model: ListModel {
                        id: commandListModel
                    }

                    // 监听commandManager的变化并更新模型
                    Connections {
                        target: commandManager
                        function onCommandListChanged() {
                            commandListModel.clear()
                            var list = commandManager.commandList
                            for (var i = 0; i < list.length; i++) {
                                commandListModel.append({"cmd": list[i]})
                            }
                        }
                    }

                    Component.onCompleted: {
                        // 初始加载
                        var list = commandManager.commandList
                        for (var i = 0; i < list.length; i++) {
                            commandListModel.append({"cmd": list[i]})
                        }
                    }

                    delegate: Pane {
                        width: listView.width
                        Material.elevation: 2
                        
                        property var cmd: model.cmd

                        RowLayout {
                            anchors.fill: parent
                            spacing: 16

                            // 命令信息区域
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 4
                                  Label {
                                    text: cmd.name
                                    font.pointSize: 14
                                    font.weight: Font.Medium
                                    color: Material.foreground
                                }
                                
                                Label {
                                    text: cmd.command
                                    font.pointSize: 11
                                    color: Material.hintTextColor
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }

                            // 状态指示器
                            Rectangle {
                                Layout.preferredWidth: 12
                                Layout.preferredHeight: 12
                                radius: 6
                                color: cmd.isRunning ? Material.color(Material.Green) : Material.color(Material.Red)
                                opacity: cmd.isRunning ? 1.0: 1.0
                                SequentialAnimation on opacity {
                                    running: cmd.isRunning
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 800 }
                                    NumberAnimation { to: 1.0; duration: 800 }
                                }
                            }
                            RowLayout {
                                spacing: 8

                                // Loading指示器 - 在停止时显示
                                BusyIndicator {
                                    id: loadingIndicator
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    visible: cmd.isRunning && cmd.isStopping
                                    running: visible
                                    Material.accent: Material.primary
                                }
                                Button {
                                    text: cmd.isRunning ? "停止" : "启动"
                                    Material.background: cmd.isRunning ? Material.Red : Material.Green
                                    Material.foreground: "white"
                                    onClicked: {
                                        if (cmd.isRunning)
                                            commandManager.stopCommand(cmd.name)
                                        else
                                            commandManager.startCommand(cmd.name)
                                    }
                                }
                                Button {
                                    text: "详情"
                                    Material.background: Material.primary
                                    Material.foreground: "white"
                                    onClicked: mainWindow.getOutputDialog(cmd.name).showOutput(cmd.name)
                                }
                            }
                        }
                    }
                }            
            }
        }    
    }

    // 创建输出窗口组件实例
    Component {
        id: outputDialogComponent
        OutputDialog {
        }
    }
      // 存储每个命令的OutputDialog实例
    property var outputDialogs: ({})
    property int nextWindowIndex: 0
    
    // 为指定命令创建或获取OutputDialog实例
    function getOutputDialog(commandName) {
        if (!outputDialogs[commandName]) {
            var dialog = outputDialogComponent.createObject(null)
            dialog.windowIndex = nextWindowIndex
            nextWindowIndex = (nextWindowIndex + 1) % 10  // 最多10个不同位置，然后循环
            outputDialogs[commandName] = dialog
        }
        return outputDialogs[commandName]
    }
    
    // 清理所有OutputDialog实例
    function cleanupOutputDialogs() {
        for (var commandName in outputDialogs) {
            if (outputDialogs[commandName]) {
                outputDialogs[commandName].destroy()
            }
        }
        outputDialogs = {}
    }
    
    // 应用退出时清理
    Component.onDestruction: {
        cleanupOutputDialogs()
    }
}
