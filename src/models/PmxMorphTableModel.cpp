#include "models/PmxMorphTableModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

namespace pmx {

PmxMorphTableModel::PmxMorphTableModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxMorphTableModel::onDisplayLanguageChanged);
    }
}

int PmxMorphTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_controller) return 0;
    return static_cast<int>(m_controller->document().morphs.size());
}

int PmxMorphTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

int PmxMorphTableModel::getMorphOffsetCount(const PmxMorph& m) {
    switch (m.morphType) {
    case 0: // Group
    case 9: // Flip
        return static_cast<int>(m.groupOffsets.size());
    case 1: // Vertex
        return static_cast<int>(m.vertexOffsets.size());
    case 2: // Bone
        return static_cast<int>(m.boneOffsets.size());
    case 3: // UV
    case 4: // Ext UV1
    case 5: // Ext UV2
    case 6: // Ext UV3
    case 7: // Ext UV4
        return static_cast<int>(m.uvOffsets.size());
    case 8: // Material
        return static_cast<int>(m.materialOffsets.size());
    case 10: // Impulse
        return static_cast<int>(m.impulseOffsets.size());
    default:
        return 0;
    }
}

QVariant PmxMorphTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        switch (section) {
        case ColName:        return isJp ? QStringLiteral("モーフ名") : QStringLiteral("Morph Name");
        case ColPanel:       return isJp ? QStringLiteral("パネル") : QStringLiteral("Panel");
        case ColMorphType:   return isJp ? QStringLiteral("モーフ種類") : QStringLiteral("Morph Type");
        case ColOffsetCount: return isJp ? QStringLiteral("オフセット数") : QStringLiteral("Offset Count");
        default: break;
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxMorphTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    if (index.column() == ColOffsetCount) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant PmxMorphTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }

    const auto& morphs = m_controller->document().morphs;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= morphs.size() || col < 0 || col >= ColumnCount) {
        return {};
    }

    const auto& morph = morphs[row];

    if (role == NameJpRole) return morph.nameJp;
    if (role == NameEnRole) return morph.nameEn;
    if (role == PanelRole) return static_cast<int>(morph.panelCategory);
    if (role == MorphTypeRole) return static_cast<int>(morph.morphType);
    if (role == OffsetCountRole) return getMorphOffsetCount(morph);

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
        case ColName: {
            bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
            return isJp ? morph.nameJp : morph.nameEn;
        }
        case ColPanel:       return static_cast<int>(morph.panelCategory);
        case ColMorphType:   return static_cast<int>(morph.morphType);
        case ColOffsetCount: return getMorphOffsetCount(morph);
        default: break;
        }
    }

    return {};
}

bool PmxMorphTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_controller) {
        return false;
    }

    const auto& morphs = m_controller->document().morphs;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= morphs.size() || col < 0 || col >= ColumnCount) {
        return false;
    }

    const auto& morph = morphs[row];
    int cmdId = makeCommandId(5, row, col);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    if (col == ColName) {
        bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        int nameCmdId = isJp ? makeCommandId(5, row, col) : makeCommandId(5, row, col + 32);
        if (isJp) {
            QString oldVal = morph.nameJp;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.morphs.size()) {
                        d.morphs[row].nameJp = v;
                    }
                },
                notify, QStringLiteral("Edit Morph Name (JP)")));
            return true;
        } else {
            QString oldVal = morph.nameEn;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.morphs.size()) {
                        d.morphs[row].nameEn = v;
                    }
                },
                notify, QStringLiteral("Edit Morph Name (EN)")));
            return true;
        }
    }

    if (col == ColPanel) {
        int panel = value.toInt();
        if (panel < 0 || panel > 4) return false;
        uint8_t oldVal = morph.panelCategory;
        uint8_t newVal = static_cast<uint8_t>(panel);
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const uint8_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.morphs.size()) {
                    d.morphs[row].panelCategory = v;
                }
            },
            notify, QStringLiteral("Edit Morph Panel Category")));
        return true;
    }

    if (col == ColMorphType) {
        int type = value.toInt();
        if (type < 0 || type > 10) return false;
        uint8_t oldVal = morph.morphType;
        uint8_t newVal = static_cast<uint8_t>(type);
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const uint8_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.morphs.size()) {
                    d.morphs[row].morphType = v;
                }
            },
            notify, QStringLiteral("Edit Morph Type")));
        return true;
    }

    return false;
}

void PmxMorphTableModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxMorphTableModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    if (rowCount() > 0) {
        emit dataChanged(index(0, ColName), index(rowCount() - 1, ColName),
                         {Qt::DisplayRole, Qt::EditRole});
    }
}

} // namespace pmx
