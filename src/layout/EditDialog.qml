import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: editDialog
    title: "编辑命令"
    modal: true
    anchors.centerIn: parent
    width: 500
    height: 500
    
    property string originalName: ""
    property string originalCommand: ""
    
    function openEditDialog(name, command) {
        originalName = name
        originalCommand = command
        nameField.text = name
        commandField.text = command
        nameField.forceActiveFocus()
        open()
    }
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20
        
        ColumnLayout {
            spacing: 12
            Layout.fillWidth: true
            
            Label {
                text: "命令名称:"
                font.pointSize: 12
            }
            
            TextField {
                id: nameField
                Layout.fillWidth: true
                Material.containerStyle: Material.Outlined
                placeholderText: "请输入命令名称"
                
                onTextChanged: {
                    errorLabel.visible = false
                }
            }
            
            Label {
                text: "启动命令:"
                font.pointSize: 12
            }
            
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 100
                
                TextArea {
                    id: commandField
                    Material.containerStyle: Material.Outlined
                    placeholderText: "请输入启动命令"
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                }
            }
            
            Label {
                id: errorLabel
                text: "命令名称已存在，请使用其他名称"
                color: Material.color(Material.Red)
                visible: false
                font.pointSize: 11
            }
        }
    }
    
    standardButtons: Dialog.Apply | Dialog.Cancel
    
    onApplied: {
        var newName = nameField.text.trim()
        var newCommand = commandField.text.trim()

        if (!newName || !newCommand) {
            errorLabel.text = "命令名称和启动命令不能为空"
            errorLabel.visible = true
            return
        }
        
        // 检查名称是否唯一（排除自己）
        if (newName !== originalName && !commandManager.isCommandNameUnique(newName, originalName)) {
            errorLabel.text = "命令名称已存在，请使用其他名称"
            errorLabel.visible = true
            return
        }
        
        // 执行编辑
        if (commandManager.editCommand(originalName, newName, newCommand)) {
            close()
        } else {
            errorLabel.text = "保存失败，请重试"
            errorLabel.visible = true
        }
    }
    
    onRejected: {
        close()
    }
    
    onClosed: {
        nameField.text = ""
        commandField.text = ""
        errorLabel.visible = false
    }
}
