import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Rectangle {
    id: root
    implicitHeight: 48
    color: "#252528"
    border.color: "#3a3a3d"
    border.width: 1

    property var viewportItem: null

    FileDialog {
        id: vmdDialog
        title: "Load VMD Motion"
        nameFilters: ["VMD Motion Files (*.vmd)", "All Files (*)"]
        onAccepted: {
            if (root.viewportItem) {
                root.viewportItem.loadVmdMotion(selectedFile)
            }
        }
    }

    FileDialog {
        id: vpdDialog
        title: "Load VPD Pose"
        nameFilters: ["VPD Pose Files (*.vpd)", "All Files (*)"]
        onAccepted: {
            if (root.viewportItem) {
                root.viewportItem.loadVpdPose(selectedFile)
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        ToolButton {
            id: playBtn
            text: "Play"
            enabled: root.viewportItem ? !root.viewportItem.isPlaying : false
            onClicked: {
                if (root.viewportItem) root.viewportItem.play()
            }
        }

        ToolButton {
            id: pauseBtn
            text: "Pause"
            enabled: root.viewportItem ? root.viewportItem.isPlaying : false
            onClicked: {
                if (root.viewportItem) root.viewportItem.pause()
            }
        }

        ToolButton {
            id: stopBtn
            text: "Stop"
            onClicked: {
                if (root.viewportItem) root.viewportItem.stop()
            }
        }

        ToolSeparator {}

        Label {
            text: "Frame:"
            verticalAlignment: Text.AlignVCenter
            color: "#e0e0e0"
        }

        Slider {
            id: frameSlider
            Layout.fillWidth: true
            from: 0
            to: (root.viewportItem && root.viewportItem.maxFrame > 0) ? root.viewportItem.maxFrame : 1
            stepSize: 1.0
            value: root.viewportItem ? root.viewportItem.currentFrame : 0
            onMoved: {
                if (root.viewportItem) {
                    root.viewportItem.currentFrame = value
                }
            }
        }

        SpinBox {
            id: frameSpinBox
            from: 0
            to: (root.viewportItem && root.viewportItem.maxFrame > 0) ? root.viewportItem.maxFrame : 999999
            stepSize: 1
            value: root.viewportItem ? Math.round(root.viewportItem.currentFrame) : 0
            editable: true
            Layout.preferredWidth: 95
            onValueModified: {
                if (root.viewportItem) {
                    root.viewportItem.currentFrame = value
                }
            }
        }

        Label {
            text: "/ " + (root.viewportItem ? root.viewportItem.maxFrame : 0)
            verticalAlignment: Text.AlignVCenter
            color: "#aaaaaa"
            Layout.preferredWidth: 50
        }

        ToolSeparator {}

        CheckBox {
            id: loopCheck
            text: "Loop"
            checked: root.viewportItem ? root.viewportItem.loopPlayback : false
            onToggled: {
                if (root.viewportItem) root.viewportItem.loopPlayback = checked
            }
        }

        ToolSeparator {}

        CheckBox {
            id: physicsToggle
            text: "Physics"
            checked: root.viewportItem ? root.viewportItem.physicsEnabled : true
            onToggled: {
                if (root.viewportItem) root.viewportItem.physicsEnabled = checked
            }
        }

        Button {
            text: "Reset Physics"
            onClicked: {
                if (root.viewportItem) root.viewportItem.resetPhysics()
            }
        }

        ToolSeparator {}

        Button {
            text: "Load VMD"
            onClicked: vmdDialog.open()
        }

        Button {
            text: "Load VPD"
            onClicked: vpdDialog.open()
        }

        Button {
            text: "Reset Pose"
            onClicked: {
                if (root.viewportItem) root.viewportItem.resetPose()
            }
        }
    }
}
