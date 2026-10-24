import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: dialog
    title: bridge.displayLanguage === "Japanese" ? "一括数値演算" : "Batch Math Operation"
    modal: true
    anchors.centerIn: Overlay.overlay
    width: 380
    standardButtons: Dialog.Apply | Dialog.Cancel

    property int tableId: 4
    property var selectedRows: []
    property var numericColumns: []

    function populateColumns(tid) {
        tableId = tid;
        let isJp = bridge.displayLanguage === "Japanese";
        let cols = [];

        if (tid === 4) { // Vertex
            cols = [
                { text: isJp ? "位置 X (PosX)" : "Position X", column: 1 },
                { text: isJp ? "位置 Y (PosY)" : "Position Y", column: 2 },
                { text: isJp ? "位置 Z (PosZ)" : "Position Z", column: 3 },
                { text: isJp ? "法線 X (NormX)" : "Normal X", column: 4 },
                { text: isJp ? "法線 Y (NormY)" : "Normal Y", column: 5 },
                { text: isJp ? "法線 Z (NormZ)" : "Normal Z", column: 6 },
                { text: isJp ? "UV U" : "UV U", column: 7 },
                { text: isJp ? "UV V" : "UV V", column: 8 },
                { text: isJp ? "変形方式 (WeightType)" : "Weight Type", column: 9 },
                { text: isJp ? "ボーン 1" : "Bone 0", column: 10 },
                { text: isJp ? "ボーン 2" : "Bone 1", column: 11 },
                { text: isJp ? "ボーン 3" : "Bone 2", column: 12 },
                { text: isJp ? "ボーン 4" : "Bone 3", column: 13 },
                { text: isJp ? "ウェイト 1" : "Weight 0", column: 14 },
                { text: isJp ? "ウェイト 2" : "Weight 1", column: 15 },
                { text: isJp ? "ウェイト 3" : "Weight 2", column: 16 },
                { text: isJp ? "ウェイト 4" : "Weight 3", column: 17 },
                { text: isJp ? "エッジ倍率 (EdgeScale)" : "Edge Scale", column: 18 }
            ];
        } else if (tid === 2) { // Material
            cols = [
                { text: isJp ? "拡散色 R" : "Diffuse R", column: 1 },
                { text: isJp ? "拡散色 G" : "Diffuse G", column: 2 },
                { text: isJp ? "拡散色 B" : "Diffuse B", column: 3 },
                { text: isJp ? "不透明度 A" : "Diffuse A (Alpha)", column: 4 },
                { text: isJp ? "反射色 R" : "Specular R", column: 5 },
                { text: isJp ? "反射色 G" : "Specular G", column: 6 },
                { text: isJp ? "反射色 B" : "Specular B", column: 7 },
                { text: isJp ? "反射強度 (Power)" : "Specular Power", column: 8 },
                { text: isJp ? "環境色 R" : "Ambient R", column: 9 },
                { text: isJp ? "環境色 G" : "Ambient G", column: 10 },
                { text: isJp ? "環境色 B" : "Ambient B", column: 11 },
                { text: isJp ? "描画フラグ" : "Draw Flags", column: 12 },
                { text: isJp ? "エッジ色 R" : "Edge R", column: 13 },
                { text: isJp ? "エッジ色 G" : "Edge G", column: 14 },
                { text: isJp ? "エッジ色 B" : "Edge B", column: 15 },
                { text: isJp ? "エッジ A" : "Edge A", column: 16 },
                { text: isJp ? "エッジサイズ" : "Edge Size", column: 17 },
                { text: isJp ? "テクスチャ番号" : "Texture Index", column: 18 },
                { text: isJp ? "スフィア番号" : "Sphere Texture Index", column: 19 },
                { text: isJp ? "スフィアモード" : "Sphere Mode", column: 20 },
                { text: isJp ? "共有Toonフラグ" : "Shared Toon Flag", column: 21 },
                { text: isJp ? "Toon番号" : "Toon Index", column: 22 },
                { text: isJp ? "面頂点数" : "Face Vertex Count", column: 24 }
            ];
        } else if (tid === 3) { // Bone
            cols = [
                { text: isJp ? "位置 X" : "Position X", column: 1 },
                { text: isJp ? "位置 Y" : "Position Y", column: 2 },
                { text: isJp ? "位置 Z" : "Position Z", column: 3 },
                { text: isJp ? "親ボーン番号" : "Parent Index", column: 4 },
                { text: isJp ? "変形階層" : "Deform Tier", column: 5 },
                { text: isJp ? "ボーンフラグ" : "Bone Flags", column: 6 }
            ];
        } else if (tid === 7) { // RigidBody
            cols = [
                { text: isJp ? "関連ボーン番号" : "Bone Index", column: 1 },
                { text: isJp ? "グループ" : "Group", column: 2 },
                { text: isJp ? "非衝突マスク" : "Collision Mask", column: 3 },
                { text: isJp ? "形状" : "Shape", column: 4 },
                { text: isJp ? "サイズ X" : "Size X", column: 5 },
                { text: isJp ? "サイズ Y" : "Size Y", column: 6 },
                { text: isJp ? "サイズ Z" : "Size Z", column: 7 },
                { text: isJp ? "位置 X" : "Position X", column: 8 },
                { text: isJp ? "位置 Y" : "Position Y", column: 9 },
                { text: isJp ? "位置 Z" : "Position Z", column: 10 },
                { text: isJp ? "回転 X" : "Rotation X", column: 11 },
                { text: isJp ? "回転 Y" : "Rotation Y", column: 12 },
                { text: isJp ? "回転 Z" : "Rotation Z", column: 13 },
                { text: isJp ? "質量" : "Mass", column: 14 },
                { text: isJp ? "移動減衰" : "Linear Damping", column: 15 },
                { text: isJp ? "回転減衰" : "Angular Damping", column: 16 },
                { text: isJp ? "反発力" : "Restitution", column: 17 },
                { text: isJp ? "摩擦力" : "Friction", column: 18 },
                { text: isJp ? "物理モード" : "Physics Mode", column: 19 }
            ];
        } else if (tid === 8) { // Joint
            cols = [
                { text: isJp ? "Joint種別" : "Joint Type", column: 1 },
                { text: isJp ? "関連剛体A" : "RigidBody A", column: 2 },
                { text: isJp ? "関連剛体B" : "RigidBody B", column: 3 },
                { text: isJp ? "位置 X" : "Position X", column: 4 },
                { text: isJp ? "位置 Y" : "Position Y", column: 5 },
                { text: isJp ? "位置 Z" : "Position Z", column: 6 },
                { text: isJp ? "回転 X" : "Rotation X", column: 7 },
                { text: isJp ? "回転 Y" : "Rotation Y", column: 8 },
                { text: isJp ? "回転 Z" : "Rotation Z", column: 9 },
                { text: isJp ? "移動制限 Min X" : "Pos Min X", column: 10 },
                { text: isJp ? "移動制限 Max X" : "Pos Max X", column: 11 },
                { text: isJp ? "移動制限 Min Y" : "Pos Min Y", column: 12 },
                { text: isJp ? "移動制限 Max Y" : "Pos Max Y", column: 13 },
                { text: isJp ? "移動制限 Min Z" : "Pos Min Z", column: 14 },
                { text: isJp ? "移動制限 Max Z" : "Pos Max Z", column: 15 },
                { text: isJp ? "回転制限 Min X" : "Rot Min X", column: 16 },
                { text: isJp ? "回転制限 Max X" : "Rot Max X", column: 17 },
                { text: isJp ? "回転制限 Min Y" : "Rot Min Y", column: 18 },
                { text: isJp ? "回転制限 Max Y" : "Rot Max Y", column: 19 },
                { text: isJp ? "回転制限 Min Z" : "Rot Min Z", column: 20 },
                { text: isJp ? "回転制限 Max Z" : "Rot Max Z", column: 21 },
                { text: isJp ? "ばね定数 移動 X" : "Spring Pos X", column: 22 },
                { text: isJp ? "ばね定数 移動 Y" : "Spring Pos Y", column: 23 },
                { text: isJp ? "ばね定数 移動 Z" : "Spring Pos Z", column: 24 },
                { text: isJp ? "ばね定数 回転 X" : "Spring Rot X", column: 25 },
                { text: isJp ? "ばね定数 回転 Y" : "Spring Rot Y", column: 26 },
                { text: isJp ? "ばね定数 回転 Z" : "Spring Rot Z", column: 27 }
            ];
        }

        numericColumns = cols;
        columnCombo.currentIndex = 0;
    }

    function openForTable(tid, rows) {
        selectedRows = rows ? rows.slice() : [];
        populateColumns(tid);
        open();
    }

    function executeBatch() {
        if (numericColumns.length === 0) return;
        let col = numericColumns[columnCombo.currentIndex].column;
        let op = operationCombo.currentIndex;
        let val = parseFloat(operandInput.text);
        if (isNaN(val)) return;

        let targetRows = selectedRows.slice();
        if (targetRows.length === 0) {
            let total = bridge.rowCount(tableId);
            targetRows = [];
            for (let i = 0; i < total; ++i) {
                targetRows.push(i);
            }
        }

        bridge.applyBatchOperation(tableId, targetRows, col, op, val);
        close();
    }

    onApplied: executeBatch()
    onAccepted: executeBatch()

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        Label {
            text: {
                let isJp = bridge.displayLanguage === "Japanese";
                if (dialog.selectedRows.length > 0) {
                    return isJp ? ("対象: 選択中の " + dialog.selectedRows.length + " 行")
                                : ("Target: " + dialog.selectedRows.length + " selected row(s)");
                } else {
                    let total = bridge.rowCount(dialog.tableId);
                    return isJp ? ("対象: 全 " + total + " 行")
                                : ("Target: All " + total + " rows");
                }
            }
            color: "#aaaaae"
            font.pixelSize: 11
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: bridge.displayLanguage === "Japanese" ? "対象列:" : "Column:"
                Layout.preferredWidth: 65
            }

            ComboBox {
                id: columnCombo
                Layout.fillWidth: true
                model: dialog.numericColumns
                textRole: "text"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: bridge.displayLanguage === "Japanese" ? "演算子:" : "Operation:"
                Layout.preferredWidth: 65
            }

            ComboBox {
                id: operationCombo
                Layout.fillWidth: true
                model: [
                    bridge.displayLanguage === "Japanese" ? "+ (加算)" : "+ (Add)",
                    bridge.displayLanguage === "Japanese" ? "- (減算)" : "- (Subtract)",
                    bridge.displayLanguage === "Japanese" ? "* (乗算)" : "* (Multiply)",
                    bridge.displayLanguage === "Japanese" ? "= (代入)" : "= (Assign)"
                ]
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: bridge.displayLanguage === "Japanese" ? "演算値:" : "Operand:"
                Layout.preferredWidth: 65
            }

            TextField {
                id: operandInput
                Layout.fillWidth: true
                text: "0.0"
                validator: DoubleValidator { locale: "C" }
                selectByMouse: true
            }
        }
    }
}
