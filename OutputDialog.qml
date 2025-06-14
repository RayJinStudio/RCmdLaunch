import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: outputWindow
    property string currentCommand
    property bool autoScroll: true
    
    // 组件初始化状态
    property bool componentReady: false

    width: 800
    height: 600
    title: currentCommand ? "控制台输出 - " + currentCommand : "控制台输出"
    visible: false

    // 设置窗口位置，避免多个窗口重叠
    property int windowIndex: 0
    x: 100 + (windowIndex * 50)
    y: 100 + (windowIndex * 50)

    // Material Design 3 样式
    Material.theme: Material.Light
    Material.primary: Material.Blue
    Material.accent: Material.LightBlue

    // 设置窗口背景色
    color: Material.backgroundColor
    
    Component.onCompleted: {
        componentReady = true
    }

    function showOutput(name) {
        currentCommand = name
        updateOutput()

        // 添加安全检查，确保窗口对象存在
        if (outputWindow) {
            outputWindow.show()
            outputWindow.raise()
            outputWindow.requestActivate()
        }
    }    
    
    function updateOutput() {
        if (currentCommand && commandManager && componentReady) {
            var newText = commandManager.getOutput(currentCommand)
            if (textArea) {
                textArea.text = newText
                
                // 自动滚动到底部（如果启用）
                if (autoScroll && newText.length > 0) {
                    textArea.cursorPosition = textArea.length
                }
            }
        }
    }
  // 连接到命令管理器的输出更新信号
    Connections {
        target: commandManager
        function onOutputUpdated(name) {
            // 添加安全检查，确保窗口存在且可见，组件已初始化，并且是当前命令
            if (outputWindow && outputWindow.visible && componentReady && currentCommand === name) {
                updateOutput()
            }
        }
    }
    
    // 关闭窗口时隐藏而不是销毁
    onClosing: function(close) {
        close.accepted = false
        if (outputWindow) {
            outputWindow.hide()
        }
    }

    // 工具栏 - 使用anchor定位固定在顶部
    Pane {
        id: toolbarPane
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 16
        height: 60
        Material.elevation: 1

        RowLayout {
            anchors.fill: parent
            spacing: 12

            Switch {
                id: autoScrollSwitch
                text: "自动滚动"
                checked: outputWindow.autoScroll
                onCheckedChanged: outputWindow.autoScroll = checked
                Material.accent: Material.primary
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "刷新"
                Material.background: Material.primary
                Material.foreground: "white"
                onClicked: outputWindow.updateOutput()
            }

            Button {
                text: "清空"
                enabled: outputWindow.currentCommand && !commandManager.isRunning(outputWindow.currentCommand)
                Material.background: Material.Orange
                Material.foreground: "white"
                onClicked: {
                    if (outputWindow.currentCommand) {
                        commandManager.clearOutput(outputWindow.currentCommand)
                        outputWindow.updateOutput()
                    }
                }
            }

            Button {
                text: "复制全部"
                Material.background: Material.Grey
                Material.foreground: "white"
                onClicked: {
                    textArea.selectAll()
                    textArea.copy()
                    textArea.deselect()
                }
            }
        }
    }

    // 输出文本区域 - 使用anchor定位填充剩余空间
    Pane {
        id: outputPane
        anchors.top: toolbarPane.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        anchors.topMargin: 8
        Material.elevation: 2
        padding: 0

        ScrollView {
            anchors.fill: parent

            TextArea {
                id: textArea
                readOnly: true
                wrapMode: Text.Wrap
                selectByMouse: true
                font.family: "Consolas, 'Courier New', monospace"
                font.pixelSize: 13

                // Material Design 3 暗色主题用于终端
                color: "#e8eaed"
                background: Rectangle {
                    color: "#1f1f1f"
                    border.color: Material.primary
                    border.width: 1
                    radius: 4
                }

                selectionColor: Material.accent
                selectedTextColor: "#ffffff"

                // 添加一些内边距
                padding: 12
            }
        }
    }
}
