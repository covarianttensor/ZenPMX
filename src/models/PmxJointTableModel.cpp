#include "models/PmxJointTableModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

#include <algorithm>
#include <cmath>

namespace pmx {

PmxJointTableModel::PmxJointTableModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxJointTableModel::onDisplayLanguageChanged);
    }
}

int PmxJointTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_controller) return 0;
    return static_cast<int>(m_controller->document().joints.size());
}

int PmxJointTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant PmxJointTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        switch (section) {
        case ColName:           return isJp ? QStringLiteral("Joint名") : QStringLiteral("Joint Name");
        case ColJointType:      return isJp ? QStringLiteral("接続方式") : QStringLiteral("Joint Type");
        case ColRigidBodyA:     return isJp ? QStringLiteral("剛体A") : QStringLiteral("Rigid Body A");
        case ColRigidBodyB:     return isJp ? QStringLiteral("剛体B") : QStringLiteral("Rigid Body B");
        case ColPosX:           return isJp ? QStringLiteral("位置 X") : QStringLiteral("Position X");
        case ColPosY:           return isJp ? QStringLiteral("位置 Y") : QStringLiteral("Position Y");
        case ColPosZ:           return isJp ? QStringLiteral("位置 Z") : QStringLiteral("Position Z");
        case ColRotX:           return isJp ? QStringLiteral("回転 X") : QStringLiteral("Rotation X");
        case ColRotY:           return isJp ? QStringLiteral("回転 Y") : QStringLiteral("Rotation Y");
        case ColRotZ:           return isJp ? QStringLiteral("回転 Z") : QStringLiteral("Rotation Z");
        case ColPosMinX:        return isJp ? QStringLiteral("移動下限 X") : QStringLiteral("Linear Min X");
        case ColPosMaxX:        return isJp ? QStringLiteral("移動上限 X") : QStringLiteral("Linear Max X");
        case ColPosMinY:        return isJp ? QStringLiteral("移動下限 Y") : QStringLiteral("Linear Min Y");
        case ColPosMaxY:        return isJp ? QStringLiteral("移動上限 Y") : QStringLiteral("Linear Max Y");
        case ColPosMinZ:        return isJp ? QStringLiteral("移動下限 Z") : QStringLiteral("Linear Min Z");
        case ColPosMaxZ:        return isJp ? QStringLiteral("移動上限 Z") : QStringLiteral("Linear Max Z");
        case ColRotMinX:        return isJp ? QStringLiteral("回転下限 X") : QStringLiteral("Angular Min X");
        case ColRotMaxX:        return isJp ? QStringLiteral("回転上限 X") : QStringLiteral("Angular Max X");
        case ColRotMinY:        return isJp ? QStringLiteral("回転下限 Y") : QStringLiteral("Angular Min Y");
        case ColRotMaxY:        return isJp ? QStringLiteral("回転上限 Y") : QStringLiteral("Angular Max Y");
        case ColRotMinZ:        return isJp ? QStringLiteral("回転下限 Z") : QStringLiteral("Angular Min Z");
        case ColRotMaxZ:        return isJp ? QStringLiteral("回転上限 Z") : QStringLiteral("Angular Max Z");
        case ColSpringPosX:     return isJp ? QStringLiteral("バネ移動 X") : QStringLiteral("Spring Linear X");
        case ColSpringPosY:     return isJp ? QStringLiteral("バネ移動 Y") : QStringLiteral("Spring Linear Y");
        case ColSpringPosZ:     return isJp ? QStringLiteral("バネ移動 Z") : QStringLiteral("Spring Linear Z");
        case ColSpringRotX:     return isJp ? QStringLiteral("バネ回転 X") : QStringLiteral("Spring Angular X");
        case ColSpringRotY:     return isJp ? QStringLiteral("バネ回転 Y") : QStringLiteral("Spring Angular Y");
        case ColSpringRotZ:     return isJp ? QStringLiteral("バネ回転 Z") : QStringLiteral("Spring Angular Z");
        default: break;
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxJointTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant PmxJointTableModel::getJointFieldValue(const PmxJoint& j, int column) {
    switch (column) {
    case ColName:           return j.nameJp;
    case ColJointType:      return static_cast<int>(j.jointType);
    case ColRigidBodyA:     return j.rigidBodyIndexA;
    case ColRigidBodyB:     return j.rigidBodyIndexB;
    case ColPosX:           return j.position.x;
    case ColPosY:           return j.position.y;
    case ColPosZ:           return j.position.z;
    case ColRotX:           return j.rotation.x;
    case ColRotY:           return j.rotation.y;
    case ColRotZ:           return j.rotation.z;
    case ColPosMinX:        return j.linearLowerLimit.x;
    case ColPosMaxX:        return j.linearUpperLimit.x;
    case ColPosMinY:        return j.linearLowerLimit.y;
    case ColPosMaxY:        return j.linearUpperLimit.y;
    case ColPosMinZ:        return j.linearLowerLimit.z;
    case ColPosMaxZ:        return j.linearUpperLimit.z;
    case ColRotMinX:        return j.angularLowerLimit.x;
    case ColRotMaxX:        return j.angularUpperLimit.x;
    case ColRotMinY:        return j.angularLowerLimit.y;
    case ColRotMaxY:        return j.angularUpperLimit.y;
    case ColRotMinZ:        return j.angularLowerLimit.z;
    case ColRotMaxZ:        return j.angularUpperLimit.z;
    case ColSpringPosX:     return j.springLinearStiffness.x;
    case ColSpringPosY:     return j.springLinearStiffness.y;
    case ColSpringPosZ:     return j.springLinearStiffness.z;
    case ColSpringRotX:     return j.springAngularStiffness.x;
    case ColSpringRotY:     return j.springAngularStiffness.y;
    case ColSpringRotZ:     return j.springAngularStiffness.z;
    default: break;
    }
    return {};
}

void PmxJointTableModel::applyValueToJoint(PmxJoint& j, int column, const QVariant& value) {
    switch (column) {
    case ColName:           j.nameJp = value.toString(); break;
    case ColJointType:      j.jointType = static_cast<uint8_t>(value.toInt()); break;
    case ColRigidBodyA:     j.rigidBodyIndexA = value.toInt(); break;
    case ColRigidBodyB:     j.rigidBodyIndexB = value.toInt(); break;
    case ColPosX:           j.position.x = value.toFloat(); break;
    case ColPosY:           j.position.y = value.toFloat(); break;
    case ColPosZ:           j.position.z = value.toFloat(); break;
    case ColRotX:           j.rotation.x = value.toFloat(); break;
    case ColRotY:           j.rotation.y = value.toFloat(); break;
    case ColRotZ:           j.rotation.z = value.toFloat(); break;
    case ColPosMinX:        j.linearLowerLimit.x = value.toFloat(); break;
    case ColPosMaxX:        j.linearUpperLimit.x = value.toFloat(); break;
    case ColPosMinY:        j.linearLowerLimit.y = value.toFloat(); break;
    case ColPosMaxY:        j.linearUpperLimit.y = value.toFloat(); break;
    case ColPosMinZ:        j.linearLowerLimit.z = value.toFloat(); break;
    case ColPosMaxZ:        j.linearUpperLimit.z = value.toFloat(); break;
    case ColRotMinX:        j.angularLowerLimit.x = value.toFloat(); break;
    case ColRotMaxX:        j.angularUpperLimit.x = value.toFloat(); break;
    case ColRotMinY:        j.angularLowerLimit.y = value.toFloat(); break;
    case ColRotMaxY:        j.angularUpperLimit.y = value.toFloat(); break;
    case ColRotMinZ:        j.angularLowerLimit.z = value.toFloat(); break;
    case ColRotMaxZ:        j.angularUpperLimit.z = value.toFloat(); break;
    case ColSpringPosX:     j.springLinearStiffness.x = value.toFloat(); break;
    case ColSpringPosY:     j.springLinearStiffness.y = value.toFloat(); break;
    case ColSpringPosZ:     j.springLinearStiffness.z = value.toFloat(); break;
    case ColSpringRotX:     j.springAngularStiffness.x = value.toFloat(); break;
    case ColSpringRotY:     j.springAngularStiffness.y = value.toFloat(); break;
    case ColSpringRotZ:     j.springAngularStiffness.z = value.toFloat(); break;
    default: break;
    }
}

QVariant PmxJointTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }

    const auto& joints = m_controller->document().joints;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= joints.size() || col < 0 || col >= ColumnCount) {
        return {};
    }

    const auto& j = joints[row];

    if (role == NameJpRole) return j.nameJp;
    if (role == NameEnRole) return j.nameEn;

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (col == ColName) {
            bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
            return isJp ? j.nameJp : j.nameEn;
        }
        return getJointFieldValue(j, col);
    }

    return {};
}

bool PmxJointTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_controller) {
        return false;
    }

    const auto& joints = m_controller->document().joints;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= joints.size() || col < 0 || col >= ColumnCount) {
        return false;
    }

    const auto& j = joints[row];
    int cmdId = makeCommandId(8, row, col);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    if (col == ColName) {
        bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        int nameCmdId = isJp ? makeCommandId(8, row, col) : makeCommandId(8, row, col + 32);
        if (isJp) {
            QString oldVal = j.nameJp;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.joints.size()) {
                        d.joints[row].nameJp = v;
                    }
                },
                notify, QStringLiteral("Edit Joint Name (JP)")));
            return true;
        } else {
            QString oldVal = j.nameEn;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.joints.size()) {
                        d.joints[row].nameEn = v;
                    }
                },
                notify, QStringLiteral("Edit Joint Name (EN)")));
            return true;
        }
    }

    if (col == ColJointType) {
        uint8_t oldVal = j.jointType;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const uint8_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.joints.size()) {
                    applyValueToJoint(d.joints[row], col, v);
                }
            },
            notify, QStringLiteral("Edit Joint Type")));
        return true;
    }

    if (col == ColRigidBodyA || col == ColRigidBodyB) {
        int32_t oldVal = (col == ColRigidBodyA) ? j.rigidBodyIndexA : j.rigidBodyIndexB;
        int32_t newVal = value.toInt();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<int32_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const int32_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.joints.size()) {
                    applyValueToJoint(d.joints[row], col, v);
                }
            },
            notify, QStringLiteral("Edit Joint RigidBody Index")));
        return true;
    }

    // Float columns: PosX..SpringRotZ (columns 4..27)
    if (col >= ColPosX && col <= ColSpringRotZ) {
        float oldVal = getJointFieldValue(j, col).toFloat();
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const float& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.joints.size()) {
                    applyValueToJoint(d.joints[row], col, v);
                }
            },
            notify, QStringLiteral("Edit Joint Float Property")));
        return true;
    }

    return false;
}

double PmxJointTableModel::applyMathOp(double current, const QString& op, double operand) {
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

bool PmxJointTableModel::applyBatchOperation(const QList<int>& rowIndices,
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

    const auto& joints = m_controller->document().joints;
    int jointCount = static_cast<int>(joints.size());

    QList<PmxBatchOperationCommand::Delta> deltas;
    deltas.reserve(rowIndices.size());
    bool hasAnyChange = false;

    for (int r : rowIndices) {
        if (r < 0 || r >= jointCount) {
            continue;
        }
        const auto& j = joints[r];
        QVariant oldVal = getJointFieldValue(j, column);
        double currentNum = oldVal.toDouble();
        double newNum = applyMathOp(currentNum, opType, operand);
        if (!std::isfinite(newNum)) {
            continue;
        }
        QVariant newVal;

        if (column == ColJointType) {
            newVal = static_cast<int>(std::clamp(static_cast<int>(newNum), 0, 255));
        } else if (column == ColRigidBodyA || column == ColRigidBodyB) {
            newVal = static_cast<int>(newNum);
        } else {
            newVal = static_cast<float>(newNum);
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
        if (r >= 0 && static_cast<size_t>(r) < d.joints.size()) {
            applyValueToJoint(d.joints[r], c, val);
        }
    };

    auto notify = [this](int minR, int maxR, int c) {
        emit dataChanged(index(minR, c), index(maxR, c), {Qt::DisplayRole, Qt::EditRole});
    };

    QString cmdText = QStringLiteral("Batch Edit Joint Col %1 (%2 %3)")
                          .arg(column)
                          .arg(opType)
                          .arg(operand);

    m_controller->pushCommand(new PmxBatchOperationCommand(
        m_controller, column, deltas, applier, notify, cmdText));

    return true;
}

void PmxJointTableModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxJointTableModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    if (rowCount() > 0) {
        emit dataChanged(index(0, ColName), index(rowCount() - 1, ColName),
                         {Qt::DisplayRole, Qt::EditRole});
    }
}

} // namespace pmx
