import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import PmxViewport 1.0

ApplicationWindow {
    id: root
    visible: true
    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    title: (bridge.currentFilePath !== "" ? bridge.currentFilePath : "Untitled") + (bridge.isModified ? " *" : "") + " - PMX Editor"

    palette {
        window: "#1e1e20"
        windowText: "#e0e0e0"
        base: "#252528"
        text: "#e0e0e0"
        button: "#323236"
        buttonText: "#ffffff"
        highlight: "#2d5bb9"
        highlightedText: "#ffffff"
    }

    header: ToolBar {
        background: Rectangle {
            color: "#2a2a2d"
            border.color: "#3a3a3d"
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6

            ToolButton {
                text: "Open"
                onClicked: openDialog.open()
            }

            ToolButton {
                text: "Save"
                enabled: bridge.isModified
                onClicked: {
                    if (bridge.currentFilePath === "") {
                        saveDialog.open()
                    } else {
                        bridge.saveFile()
                    }
                }
            }

            ToolSeparator {}

            ToolButton {
                text: "Undo"
                enabled: bridge.canUndo
                onClicked: bridge.undo()
            }

            ToolButton {
                text: "Redo"
                enabled: bridge.canRedo
                onClicked: bridge.redo()
            }

            ToolSeparator {}

            ToolButton {
                text: bridge.displayLanguage
                ToolTip.visible: hovered
                ToolTip.text: "Toggle Japanese / English UI Language"
                onClicked: bridge.toggleLanguage()
            }

            ToolSeparator {}

            CheckBox {
                id: gridCheck
                text: "Grid"
                checked: true
            }

            CheckBox {
                id: bonesCheck
                text: "Bones"
                checked: true
            }

            CheckBox {
                id: physicsCheck
                text: "Physics"
                checked: true
            }

            CheckBox {
                id: jointsCheck
                text: "Joints"
                checked: true
            }

            CheckBox {
                id: wireframeCheck
                text: "Wireframe"
                checked: false
            }

            ToolSeparator {}

            Label {
                text: "Gizmo:"
                verticalAlignment: Text.AlignVCenter
            }

            ComboBox {
                id: gizmoCombo
                model: ["None", "Translate", "Rotate", "Scale"]
                currentIndex: viewport.gizmoMode
                onActivated: function(index) {
                    viewport.gizmoMode = index
                }
            }

            Item { Layout.fillWidth: true }

            Label {
                text: bridge.statusMessage
                color: "#aaaaaa"
                elide: Text.ElideRight
                Layout.maximumWidth: 400
            }
        }
    }

    FileDialog {
        id: openDialog
        title: "Open PMX Model"
        nameFilters: ["PMX Model Files (*.pmx)", "All Files (*)"]
        onAccepted: {
            bridge.openFile(selectedFile)
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save PMX Model"
        fileMode: FileDialog.SaveFile
        nameFilters: ["PMX Model Files (*.pmx)", "All Files (*)"]
        onAccepted: {
            bridge.saveFile(selectedFile)
        }
    }

    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Center 3D Viewport
        Item {
            SplitView.fillWidth: true
            SplitView.fillHeight: true

            PmxViewportItem {
                id: viewport
                anchors.fill: parent
                controller: bridge.controller
                showGrid: gridCheck.checked
                showBones: bonesCheck.checked
                showRigidBodies: physicsCheck.checked
                showJoints: jointsCheck.checked
                wireframe: wireframeCheck.checked
                focus: true
            }
        }

        // Right Inspector Panel
        Item {
            SplitView.preferredWidth: 460
            SplitView.minimumWidth: 320
            SplitView.maximumWidth: 1200
            SplitView.fillHeight: true

            InspectorPanel {
                id: inspectorPanel
                anchors.fill: parent
            }
        }
    }

    footer: ToolBar {
        height: 24
        background: Rectangle {
            color: "#18181a"
            border.color: "#2d2d30"
            border.width: 1
        }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8

            Label {
                text: bridge.currentFilePath !== "" ? bridge.currentFilePath : "No file opened"
                font.pixelSize: 11
                color: "#888888"
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
        }
    }
}
