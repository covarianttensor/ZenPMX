#include "models/PmxRigidBodyTableModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

#include <algorithm>
#include <cmath>

namespace pmx {

PmxRigidBodyTableModel::PmxRigidBodyTableModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxRigidBodyTableModel::onDisplayLanguageChanged);
    }
}

int PmxRigidBodyTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_controller) return 0;
    return static_cast<int>(m_controller->document().rigidBodies.size());
}

int PmxRigidBodyTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant PmxRigidBodyTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        switch (section) {
        case ColName:           return isJp ? QStringLiteral("剛体名") : QStringLiteral("Rigid Body Name");
        case ColBoneIndex:      return isJp ? QStringLiteral("関連ボーン") : QStringLiteral("Bone Index");
        case ColGroup:          return isJp ? QStringLiteral("グループ") : QStringLiteral("Group");
        case ColMask:           return isJp ? QStringLiteral("非衝突マスク") : QStringLiteral("Collision Mask");
        case ColShape:          return isJp ? QStringLiteral("形状") : QStringLiteral("Shape");
        case ColSizeX:          return isJp ? QStringLiteral("サイズ X") : QStringLiteral("Size X");
        case ColSizeY:          return isJp ? QStringLiteral("サイズ Y") : QStringLiteral("Size Y");
        case ColSizeZ:          return isJp ? QStringLiteral("サイズ Z") : QStringLiteral("Size Z");
        case ColPosX:           return isJp ? QStringLiteral("位置 X") : QStringLiteral("Position X");
        case ColPosY:           return isJp ? QStringLiteral("位置 Y") : QStringLiteral("Position Y");
        case ColPosZ:           return isJp ? QStringLiteral("位置 Z") : QStringLiteral("Position Z");
        case ColRotX:           return isJp ? QStringLiteral("回転 X") : QStringLiteral("Rotation X");
        case ColRotY:           return isJp ? QStringLiteral("回転 Y") : QStringLiteral("Rotation Y");
        case ColRotZ:           return isJp ? QStringLiteral("回転 Z") : QStringLiteral("Rotation Z");
        case ColMass:           return isJp ? QStringLiteral("質量") : QStringLiteral("Mass");
        case ColLinearDamping:  return isJp ? QStringLiteral("移動減衰") : QStringLiteral("Linear Damping");
        case ColAngularDamping: return isJp ? QStringLiteral("回転減衰") : QStringLiteral("Angular Damping");
        case ColRestitution:    return isJp ? QStringLiteral("反発力") : QStringLiteral("Restitution");
        case ColFriction:       return isJp ? QStringLiteral("摩擦力") : QStringLiteral("Friction");
        case ColPhysicsMode:    return isJp ? QStringLiteral("物理演算") : QStringLiteral("Physics Mode");
        default: break;
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxRigidBodyTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant PmxRigidBodyTableModel::getRigidBodyFieldValue(const PmxRigidBody& rb, int column) {
    switch (column) {
    case ColName:           return rb.nameJp;
    case ColBoneIndex:      return rb.boneIndex;
    case ColGroup:          return static_cast<int>(rb.groupIndex);
    case ColMask:           return static_cast<int>(rb.nonCollisionMask);
    case ColShape:          return static_cast<int>(rb.shape);
    case ColSizeX:          return rb.size.x;
    case ColSizeY:          return rb.size.y;
    case ColSizeZ:          return rb.size.z;
    case ColPosX:           return rb.position.x;
    case ColPosY:           return rb.position.y;
    case ColPosZ:           return rb.position.z;
    case ColRotX:           return rb.rotation.x;
    case ColRotY:           return rb.rotation.y;
    case ColRotZ:           return rb.rotation.z;
    case ColMass:           return rb.mass;
    case ColLinearDamping:  return rb.linearDamping;
    case ColAngularDamping: return rb.angularDamping;
    case ColRestitution:    return rb.restitution;
    case ColFriction:       return rb.friction;
    case ColPhysicsMode:    return static_cast<int>(rb.physicsMode);
    default: break;
    }
    return {};
}

void PmxRigidBodyTableModel::applyValueToRigidBody(PmxRigidBody& rb, int column, const QVariant& value) {
    switch (column) {
    case ColName:           rb.nameJp = value.toString(); break;
    case ColBoneIndex:      rb.boneIndex = value.toInt(); break;
    case ColGroup:          rb.groupIndex = static_cast<uint8_t>(std::clamp(value.toInt(), 0, 15)); break;
    case ColMask:           rb.nonCollisionMask = static_cast<uint16_t>(value.toUInt()); break;
    case ColShape:          rb.shape = static_cast<uint8_t>(std::clamp(value.toInt(), 0, 2)); break;
    case ColSizeX:          rb.size.x = value.toFloat(); break;
    case ColSizeY:          rb.size.y = value.toFloat(); break;
    case ColSizeZ:          rb.size.z = value.toFloat(); break;
    case ColPosX:           rb.position.x = value.toFloat(); break;
    case ColPosY:           rb.position.y = value.toFloat(); break;
    case ColPosZ:           rb.position.z = value.toFloat(); break;
    case ColRotX:           rb.rotation.x = value.toFloat(); break;
    case ColRotY:           rb.rotation.y = value.toFloat(); break;
    case ColRotZ:           rb.rotation.z = value.toFloat(); break;
    case ColMass:           rb.mass = value.toFloat(); break;
    case ColLinearDamping:  rb.linearDamping = value.toFloat(); break;
    case ColAngularDamping: rb.angularDamping = value.toFloat(); break;
    case ColRestitution:    rb.restitution = value.toFloat(); break;
    case ColFriction:       rb.friction = value.toFloat(); break;
    case ColPhysicsMode:    rb.physicsMode = static_cast<uint8_t>(std::clamp(value.toInt(), 0, 2)); break;
    default: break;
    }
}

QVariant PmxRigidBodyTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }

    const auto& rigidBodies = m_controller->document().rigidBodies;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= rigidBodies.size() || col < 0 || col >= ColumnCount) {
        return {};
    }

    const auto& rb = rigidBodies[row];

    if (role == NameJpRole) return rb.nameJp;
    if (role == NameEnRole) return rb.nameEn;

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (col == ColName) {
            bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
            return isJp ? rb.nameJp : rb.nameEn;
        }
        return getRigidBodyFieldValue(rb, col);
    }

    return {};
}

bool PmxRigidBodyTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_controller) {
        return false;
    }

    const auto& rigidBodies = m_controller->document().rigidBodies;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= rigidBodies.size() || col < 0 || col >= ColumnCount) {
        return false;
    }

    const auto& rb = rigidBodies[row];
    int cmdId = makeCommandId(7, row, col);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    if (col == ColName) {
        bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        int nameCmdId = isJp ? makeCommandId(7, row, col) : makeCommandId(7, row, col + 32);
        if (isJp) {
            QString oldVal = rb.nameJp;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.rigidBodies.size()) {
                        d.rigidBodies[row].nameJp = v;
                    }
                },
                notify, QStringLiteral("Edit RigidBody Name (JP)")));
            return true;
        } else {
            QString oldVal = rb.nameEn;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.rigidBodies.size()) {
                        d.rigidBodies[row].nameEn = v;
                    }
                },
                notify, QStringLiteral("Edit RigidBody Name (EN)")));
            return true;
        }
    }

    // Bone index
    if (col == ColBoneIndex) {
        int32_t oldVal = rb.boneIndex;
        int32_t newVal = value.toInt();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<int32_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const int32_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.rigidBodies.size()) {
                    applyValueToRigidBody(d.rigidBodies[row], col, v);
                }
            },
            notify, QStringLiteral("Edit RigidBody Bone Index")));
        return true;
    }

    // Collision mask
    if (col == ColMask) {
        uint16_t oldVal = rb.nonCollisionMask;
        uint16_t newVal = static_cast<uint16_t>(value.toUInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint16_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const uint16_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.rigidBodies.size()) {
                    applyValueToRigidBody(d.rigidBodies[row], col, v);
                }
            },
            notify, QStringLiteral("Edit RigidBody Collision Mask")));
        return true;
    }

    // Uint8 columns: Group, Shape, PhysicsMode
    if (col == ColGroup || col == ColShape || col == ColPhysicsMode) {
        uint8_t oldVal = static_cast<uint8_t>(getRigidBodyFieldValue(rb, col).toInt());
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const uint8_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.rigidBodies.size()) {
                    applyValueToRigidBody(d.rigidBodies[row], col, v);
                }
            },
            notify, QStringLiteral("Edit RigidBody Flag/Mode Property")));
        return true;
    }

    // Float columns: SizeX..Z, PosX..Z, RotX..Z, Mass, LinearDamping, AngularDamping, Restitution, Friction
    if (col >= ColSizeX && col <= ColFriction) {
        float oldVal = getRigidBodyFieldValue(rb, col).toFloat();
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const float& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.rigidBodies.size()) {
                    applyValueToRigidBody(d.rigidBodies[row], col, v);
                }
            },
            notify, QStringLiteral("Edit RigidBody Float Property")));
        return true;
    }

    return false;
}

double PmxRigidBodyTableModel::applyMathOp(double current, const QString& op, double operand) {
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

bool PmxRigidBodyTableModel::applyBatchOperation(const QList<int>& rowIndices,
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
    if (opType != QStringLiteral("+") && opType != QStringLiteral("-") && opType != QStringLiteral("*") && opType != QStringLiteral("=")) {
        return false;
    }

    const auto& rigidBodies = m_controller->document().rigidBodies;
    int rbCount = static_cast<int>(rigidBodies.size());

    QList<PmxBatchOperationCommand::Delta> deltas;
    deltas.reserve(rowIndices.size());
    bool hasAnyChange = false;

    for (int r : rowIndices) {
        if (r < 0 || r >= rbCount) {
            continue;
        }
        const auto& rb = rigidBodies[r];
        QVariant oldVal = getRigidBodyFieldValue(rb, column);
        double currentNum = oldVal.toDouble();
        double newNum = applyMathOp(currentNum, opType, operand);
        if (!std::isfinite(newNum)) {
            continue;
        }
        QVariant newVal;

        if (column == ColBoneIndex) {
            newVal = static_cast<int>(newNum);
        } else if (column == ColMask) {
            newVal = static_cast<int>(std::clamp(static_cast<int>(newNum), 0, 65535));
        } else if (column == ColGroup) {
            newVal = static_cast<int>(std::clamp(static_cast<int>(newNum), 0, 15));
        } else if (column == ColShape || column == ColPhysicsMode) {
            newVal = static_cast<int>(std::clamp(static_cast<int>(newNum), 0, 2));
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
        if (r >= 0 && static_cast<size_t>(r) < d.rigidBodies.size()) {
            applyValueToRigidBody(d.rigidBodies[r], c, val);
        }
    };

    auto notify = [this](int minR, int maxR, int c) {
        emit dataChanged(index(minR, c), index(maxR, c), {Qt::DisplayRole, Qt::EditRole});
    };

    QString cmdText = QStringLiteral("Batch Edit RigidBody Col %1 (%2 %3)")
                          .arg(column)
                          .arg(opType)
                          .arg(operand);

    m_controller->pushCommand(new PmxBatchOperationCommand(
        m_controller, column, deltas, applier, notify, cmdText));

    return true;
}

void PmxRigidBodyTableModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxRigidBodyTableModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    if (rowCount() > 0) {
        emit dataChanged(index(0, ColName), index(rowCount() - 1, ColName),
                         {Qt::DisplayRole, Qt::EditRole});
    }
}

} // namespace pmx
