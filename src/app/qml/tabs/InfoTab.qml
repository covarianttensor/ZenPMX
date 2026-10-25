import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: rootTab
    color: "#1e1e22"
    clip: true

    property string filterText: ""
    property var selectedRows: []
    property int lastSelectedRow: -1
    property int filterColumn: 0

    function getColWidth(col) {
        return col === 0 ? 150 : 300;
    }

    function rowMatchesFilter(row) {
        if (!filterText || filterText.trim() === "") return true;
        let val0 = "";
        let val1 = "";
        if (tableView.model) {
            let d0 = tableView.model.data(tableView.model.index(row, 0));
            if (d0 !== undefined && d0 !== null) val0 = d0.toString();
            let d1 = tableView.model.data(tableView.model.index(row, 1));
            if (d1 !== undefined && d1 !== null) val1 = d1.toString();
        }
        try {
            let rx = new RegExp(filterText, "i");
            if (rx.test(val0) || rx.test(val1)) return true;
        } catch (e) {
            let ft = filterText.toLowerCase();
            if (val0.toLowerCase().includes(ft) || val1.toLowerCase().includes(ft)) return true;
        }
        return false;
    }

    function handleRowClick(row, modifiers) {
        if (modifiers & Qt.ControlModifier) {
            let idx = selectedRows.indexOf(row);
            let updated = selectedRows.slice();
            if (idx !== -1) {
                updated.splice(idx, 1);
            } else {
                updated.push(row);
            }
            selectedRows = updated;
            lastSelectedRow = row;
        } else if (modifiers & Qt.ShiftModifier) {
            let start = lastSelectedRow >= 0 ? lastSelectedRow : 0;
            let min = Math.min(start, row);
            let max = Math.max(start, row);
            let list = [];
            for (let i = min; i <= max; ++i) {
                list.push(i);
            }
            selectedRows = list;
        } else {
            selectedRows = [row];
            lastSelectedRow = row;
        }
    }

    onFilterTextChanged: {
        tableView.forceLayout();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        HorizontalHeaderView {
            id: header
            syncView: tableView
            Layout.fillWidth: true
            implicitHeight: 26
            clip: true
            columnWidthProvider: function(col) { return rootTab.getColWidth(col); }

            delegate: Rectangle {
                implicitHeight: 26
                color: "#2a2a2e"
                border.color: "#3a3a3e"
                border.width: 1

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: Text.AlignVCenter
                    color: "#b0b0b8"
                    font.pixelSize: 11
                    font.bold: true
                    text: display !== undefined ? display.toString() : ""
                    elide: Text.ElideRight
                }
            }
        }

        TableView {
            id: tableView
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: (typeof bridge !== "undefined" && bridge) ? bridge.infoModel : null
            boundsBehavior: Flickable.StopAtBounds
            columnSpacing: 1
            rowSpacing: 1
            clip: true

            ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            columnWidthProvider: function(col) {
                return rootTab.getColWidth(col);
            }

            rowHeightProvider: function(row) {
                return rootTab.rowMatchesFilter(row) ? 26 : 0;
            }

            delegate: Rectangle {
                implicitWidth: rootTab.getColWidth(column)
                implicitHeight: rootTab.rowMatchesFilter(row) ? 26 : 0
                visible: implicitHeight > 0
                clip: true

                property bool isRowSelected: rootTab.selectedRows.indexOf(row) !== -1

                color: isRowSelected ? "#2a4a7f" : (row % 2 === 0 ? "#27272b" : "#212124")
                border.color: isRowSelected ? "#4a8af4" : "#333336"
                border.width: 1

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    verticalAlignment: Text.AlignVCenter
                    color: isRowSelected ? "#ffffff" : "#e0e0e0"
                    text: display !== undefined ? display.toString() : ""
                    elide: Text.ElideRight
                    font.pixelSize: 11
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: function(mouse) {
                        rootTab.handleRowClick(row, mouse.modifiers);
                    }
                }
            }
        }
    }
}
