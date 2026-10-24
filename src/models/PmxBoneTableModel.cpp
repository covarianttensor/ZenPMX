#include "models/PmxBoneTableModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

#include <algorithm>
#include <cmath>

namespace pmx {

PmxBoneTableModel::PmxBoneTableModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxBoneTableModel::onDisplayLanguageChanged);
    }
}

int PmxBoneTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_controller) return 0;
    return static_cast<int>(m_controller->document().bones.size());
}

int PmxBoneTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant PmxBoneTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        switch (section) {
        case ColName:        return isJp ? QStringLiteral("ボーン名") : QStringLiteral("Bone Name");
        case ColPosX:        return isJp ? QStringLiteral("位置 X") : QStringLiteral("Position X");
        case ColPosY:        return isJp ? QStringLiteral("位置 Y") : QStringLiteral("Position Y");
        case ColPosZ:        return isJp ? QStringLiteral("位置 Z") : QStringLiteral("Position Z");
        case ColParentIndex: return isJp ? QStringLiteral("親ボーン") : QStringLiteral("Parent Index");
        case ColDeformTier:  return isJp ? QStringLiteral("変形階層") : QStringLiteral("Deform Tier");
        case ColBoneFlags:   return isJp ? QStringLiteral("ボーンフラグ") : QStringLiteral("Bone Flags");
        default: break;
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxBoneTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant PmxBoneTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }

    const auto& bones = m_controller->document().bones;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= bones.size() || col < 0 || col >= ColumnCount) {
        return {};
    }

    const auto& bone = bones[row];

    if (role == NameJpRole) return bone.nameJp;
    if (role == NameEnRole) return bone.nameEn;

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
        case ColName: {
            bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
            return isJp ? bone.nameJp : bone.nameEn;
        }
        case ColPosX:        return bone.position.x;
        case ColPosY:        return bone.position.y;
        case ColPosZ:        return bone.position.z;
        case ColParentIndex: return bone.parentBoneIndex;
        case ColDeformTier:  return bone.deformTier;
        case ColBoneFlags:   return static_cast<int>(bone.boneFlags);
        default: break;
        }
    }

    return {};
}

QVariant PmxBoneTableModel::getBoneFieldValue(const PmxBone& bone, int column) {
    switch (column) {
    case ColName:        return bone.nameJp;
    case ColPosX:        return bone.position.x;
    case ColPosY:        return bone.position.y;
    case ColPosZ:        return bone.position.z;
    case ColParentIndex: return bone.parentBoneIndex;
    case ColDeformTier:  return bone.deformTier;
    case ColBoneFlags:   return static_cast<int>(bone.boneFlags);
    default: break;
    }
    return {};
}

void PmxBoneTableModel::applyValueToBone(PmxBone& bone, int column, const QVariant& value) {
    switch (column) {
    case ColName:        bone.nameJp = value.toString(); break;
    case ColPosX:        bone.position.x = value.toFloat(); break;
    case ColPosY:        bone.position.y = value.toFloat(); break;
    case ColPosZ:        bone.position.z = value.toFloat(); break;
    case ColParentIndex: bone.parentBoneIndex = value.toInt(); break;
    case ColDeformTier:  bone.deformTier = value.toInt(); break;
    case ColBoneFlags:   bone.boneFlags = static_cast<uint16_t>(value.toInt()); break;
    default: break;
    }
}

bool PmxBoneTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_controller) {
        return false;
    }

    const auto& bones = m_controller->document().bones;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= bones.size() || col < 0 || col >= ColumnCount) {
        return false;
    }

    const auto& bone = bones[row];
    int cmdId = makeCommandId(3, row, col);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    switch (col) {
    case ColName: {
        bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        if (isJp) {
            QString oldVal = bone.nameJp;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, cmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                        d.bones[row].nameJp = v;
                    }
                },
                notify, QStringLiteral("Edit Bone Name (JP)")));
            return true;
        } else {
            QString oldVal = bone.nameEn;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, cmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                        d.bones[row].nameEn = v;
                    }
                },
                notify, QStringLiteral("Edit Bone Name (EN)")));
            return true;
        }
    }
    case ColPosX: {
        float oldVal = bone.position.x;
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const float& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                    d.bones[row].position.x = v;
                }
            },
            notify, QStringLiteral("Edit Bone Position X")));
        return true;
    }
    case ColPosY: {
        float oldVal = bone.position.y;
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const float& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                    d.bones[row].position.y = v;
                }
            },
            notify, QStringLiteral("Edit Bone Position Y")));
        return true;
    }
    case ColPosZ: {
        float oldVal = bone.position.z;
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const float& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                    d.bones[row].position.z = v;
                }
            },
            notify, QStringLiteral("Edit Bone Position Z")));
        return true;
    }
    case ColParentIndex: {
        int32_t oldVal = bone.parentBoneIndex;
        int32_t newVal = value.toInt();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<int32_t>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const int32_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                    d.bones[row].parentBoneIndex = v;
                }
            },
            notify, QStringLiteral("Edit Bone Parent Index")));
        return true;
    }
    case ColDeformTier: {
        int32_t oldVal = bone.deformTier;
        int32_t newVal = value.toInt();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<int32_t>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const int32_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                    d.bones[row].deformTier = v;
                }
            },
            notify, QStringLiteral("Edit Bone Deform Tier")));
        return true;
    }
    case ColBoneFlags: {
        uint16_t oldVal = bone.boneFlags;
        uint16_t newVal = static_cast<uint16_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint16_t>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const uint16_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.bones.size()) {
                    d.bones[row].boneFlags = v;
                }
            },
            notify, QStringLiteral("Edit Bone Flags")));
        return true;
    }
    default:
        break;
    }

    return false;
}

double PmxBoneTableModel::applyMathOp(double current, const QString& op, double operand) {
    if (op == QStringLiteral("=")) {
        return operand;
    } else if (op == QStringLiteral("+")) {
        return current + operand;
    } else if (op == QStringLiteral("-")) {
        return current - operand;
    } else if (op == QStringLiteral("*")) {
        return current * operand;
    } else if (op == QStringLiteral("/")) {
        return (operand != 0.0) ? (current / operand) : current;
    }
    return current;
}

bool PmxBoneTableModel::applyBatchOperation(const QList<int>& rowIndices,
                                           int column,
                                           const QString& opType,
                                           double operand)
{
    if (!m_controller || rowIndices.isEmpty()) {
        return false;
    }

    // Reject non-numeric column
    if (column == ColName || column < 0 || column >= ColumnCount) {
        return false;
    }

    // Reject invalid operand
    if (std::isnan(operand) || std::isinf(operand)) {
        return false;
    }

    // Reject invalid operator
    if (opType != QStringLiteral("+") && opType != QStringLiteral("*") && opType != QStringLiteral("=")) {
        return false;
    }

    const auto& bones = m_controller->document().bones;
    int boneCount = static_cast<int>(bones.size());

    QList<PmxBatchOperationCommand::Delta> deltas;
    deltas.reserve(rowIndices.size());
    bool hasAnyChange = false;

    for (int r : rowIndices) {
        if (r < 0 || r >= boneCount) {
            continue;
        }
        const auto& bone = bones[r];
        QVariant oldVal = getBoneFieldValue(bone, column);
        double currentNum = oldVal.toDouble();
        double newNum = applyMathOp(currentNum, opType, operand);
        QVariant newVal;

        if (column == ColPosX || column == ColPosY || column == ColPosZ) {
            newVal = static_cast<float>(newNum);
        } else if (column == ColBoneFlags) {
            newVal = static_cast<int>(std::clamp(static_cast<int>(newNum), 0, 65535));
        } else {
            newVal = static_cast<int>(newNum);
        }

        if (oldVal != newVal) {
            hasAnyChange = true;
        }
        deltas.append({r, oldVal, newVal});
    }

    if (deltas.isEmpty() || !hasAnyChange) {
        return false;
    }

    auto applier = [](pmx::PmxDocument& d, int r, int c, const QVariant& val) {
        if (r >= 0 && static_cast<size_t>(r) < d.bones.size()) {
            applyValueToBone(d.bones[r], c, val);
        }
    };

    auto notify = [this](int minR, int maxR, int c) {
        emit dataChanged(index(minR, c), index(maxR, c), {Qt::DisplayRole, Qt::EditRole});
    };

    QString cmdText = QStringLiteral("Batch Edit Bone Col %1 (%2 %3)")
                          .arg(column)
                          .arg(opType)
                          .arg(operand);

    m_controller->pushCommand(new PmxBatchOperationCommand(
        m_controller, column, deltas, applier, notify, cmdText));

    return true;
}

void PmxBoneTableModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxBoneTableModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    if (rowCount() > 0) {
        emit dataChanged(index(0, ColName), index(rowCount() - 1, ColName),
                         {Qt::DisplayRole, Qt::EditRole});
    }
}

} // namespace pmx
