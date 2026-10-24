import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "tabs"

Item {
    id: root

    property alias currentIndex: tabBar.currentIndex
    property alias filterText: searchField.text

    function currentTableId() {
        switch (tabBar.currentIndex) {
        case 0: return 1; // Info
        case 1: return 4; // Vertex
        case 2: return 2; // Material
        case 3: return 3; // Bone
        case 4: return 5; // Morph
        case 5: return 6; // DisplayFrame
        case 6: return 7; // RigidBody
        case 7: return 8; // Joint
        default: return 0;
        }
    }

    function isBatchSupported() {
        let tid = currentTableId();
        return tid === 2 || tid === 3 || tid === 4 || tid === 7 || tid === 8;
    }

    function getCurrentTab() {
        return stackLayout.children[tabBar.currentIndex];
    }

    BatchOperationDialog {
        id: batchDialog
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        TabBar {
            id: tabBar
            Layout.fillWidth: true

            property bool isJp: bridge.displayLanguage === "Japanese"

            TabButton {
                text: tabBar.isJp ? "情報" : "Info"
                implicitWidth: Math.max(50, root.width / 8)
            }
            TabButton {
                text: tabBar.isJp ? "頂点" : "Vertex"
                implicitWidth: Math.max(50, root.width / 8)
            }
            TabButton {
                text: tabBar.isJp ? "材質" : "Material"
                implicitWidth: Math.max(50, root.width / 8)
            }
            TabButton {
                text: tabBar.isJp ? "ボーン" : "Bone"
                implicitWidth: Math.max(50, root.width / 8)
            }
            TabButton {
                text: tabBar.isJp ? "モーフ" : "Morph"
                implicitWidth: Math.max(50, root.width / 8)
            }
            TabButton {
                text: tabBar.isJp ? "表示枠" : "Display"
                implicitWidth: Math.max(50, root.width / 8)
            }
            TabButton {
                text: tabBar.isJp ? "剛体" : "RigidBody"
                implicitWidth: Math.max(50, root.width / 8)
            }
            TabButton {
                text: tabBar.isJp ? "Joint" : "Joint"
                implicitWidth: Math.max(50, root.width / 8)
            }
        }

        // Action & Search/Filter Bar
        Rectangle {
            Layout.fillWidth: true
            height: 38
            color: "#252529"
            border.color: "#35353a"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 6

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: tabBar.isJp ? "絞り込み (部分一致 / 正規表現)..." : "Filter (substring / regex)..."
                    font.pixelSize: 11
                    selectByMouse: true
                }

                Button {
                    id: batchButton
                    text: tabBar.isJp ? "一括演算..." : "Batch Math..."
                    font.pixelSize: 11
                    enabled: root.isBatchSupported()
                    onClicked: {
                        let curTab = root.getCurrentTab();
                        let sel = (curTab && curTab.selectedRows) ? curTab.selectedRows : [];
                        batchDialog.openForTable(root.currentTableId(), sel);
                    }
                }

                ToolButton {
                    text: tabBar.isJp ? "全選択" : "All"
                    font.pixelSize: 10
                    onClicked: {
                        let curTab = root.getCurrentTab();
                        if (curTab && curTab.selectAll) curTab.selectAll();
                    }
                }

                ToolButton {
                    text: tabBar.isJp ? "解除" : "None"
                    font.pixelSize: 10
                    onClicked: {
                        let curTab = root.getCurrentTab();
                        if (curTab && curTab.clearSelection) curTab.clearSelection();
                    }
                }
            }
        }

        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            InfoTab {
                id: infoTab
                filterText: searchField.text
            }

            VertexTab {
                id: vertexTab
                filterText: searchField.text
            }

            MaterialTab {
                id: materialTab
                filterText: searchField.text
            }

            BoneTab {
                id: boneTab
                filterText: searchField.text
            }

            MorphTab {
                id: morphTab
                filterText: searchField.text
            }

            DisplayFrameTab {
                id: displayFrameTab
                filterText: searchField.text
            }

            RigidBodyTab {
                id: rigidBodyTab
                filterText: searchField.text
            }

            JointTab {
                id: jointTab
                filterText: searchField.text
            }
        }
    }
}
