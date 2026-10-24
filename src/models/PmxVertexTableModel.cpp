#include "models/PmxVertexTableModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

#include <algorithm>
#include <cmath>

namespace pmx {

PmxVertexTableModel::PmxVertexTableModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxVertexTableModel::onDisplayLanguageChanged);
    }
}

int PmxVertexTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_controller) return 0;
    return static_cast<int>(m_controller->document().vertices.size());
}

int PmxVertexTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant PmxVertexTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        switch (section) {
        case ColIndex:      return isJp ? QStringLiteral("頂点Index") : QStringLiteral("Index");
        case ColPosX:       return isJp ? QStringLiteral("位置 X") : QStringLiteral("Position X");
        case ColPosY:       return isJp ? QStringLiteral("位置 Y") : QStringLiteral("Position Y");
        case ColPosZ:       return isJp ? QStringLiteral("位置 Z") : QStringLiteral("Position Z");
        case ColNormX:      return isJp ? QStringLiteral("法線 X") : QStringLiteral("Normal X");
        case ColNormY:      return isJp ? QStringLiteral("法線 Y") : QStringLiteral("Normal Y");
        case ColNormZ:      return isJp ? QStringLiteral("法線 Z") : QStringLiteral("Normal Z");
        case ColUvU:        return isJp ? QStringLiteral("UV U") : QStringLiteral("UV U");
        case ColUvV:        return isJp ? QStringLiteral("UV V") : QStringLiteral("UV V");
        case ColWeightType: return isJp ? QStringLiteral("ウェイト変形方式") : QStringLiteral("Weight Type");
        case ColBone0:      return isJp ? QStringLiteral("ボーン1") : QStringLiteral("Bone 0");
        case ColBone1:      return isJp ? QStringLiteral("ボーン2") : QStringLiteral("Bone 1");
        case ColBone2:      return isJp ? QStringLiteral("ボーン3") : QStringLiteral("Bone 2");
        case ColBone3:      return isJp ? QStringLiteral("ボーン4") : QStringLiteral("Bone 3");
        case ColWeight0:    return isJp ? QStringLiteral("ウェイト1") : QStringLiteral("Weight 0");
        case ColWeight1:    return isJp ? QStringLiteral("ウェイト2") : QStringLiteral("Weight 1");
        case ColWeight2:    return isJp ? QStringLiteral("ウェイト3") : QStringLiteral("Weight 2");
        case ColWeight3:    return isJp ? QStringLiteral("ウェイト4") : QStringLiteral("Weight 3");
        case ColEdgeScale:  return isJp ? QStringLiteral("エッジ倍率") : QStringLiteral("Edge Scale");
        default: break;
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxVertexTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    if (index.column() == ColIndex) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant PmxVertexTableModel::getVertexFieldValue(const PmxVertex& v, int column, int row) {
    switch (column) {
    case ColIndex:      return row;
    case ColPosX:       return v.position.x;
    case ColPosY:       return v.position.y;
    case ColPosZ:       return v.position.z;
    case ColNormX:      return v.normal.x;
    case ColNormY:      return v.normal.y;
    case ColNormZ:      return v.normal.z;
    case ColUvU:        return v.uv.x;
    case ColUvV:        return v.uv.y;
    case ColWeightType: return static_cast<int>(v.weightType);
    case ColBone0:      return v.boneIndices[0];
    case ColBone1:      return v.boneIndices[1];
    case ColBone2:      return v.boneIndices[2];
    case ColBone3:      return v.boneIndices[3];
    case ColWeight0:    return v.boneWeights[0];
    case ColWeight1:    return v.boneWeights[1];
    case ColWeight2:    return v.boneWeights[2];
    case ColWeight3:    return v.boneWeights[3];
    case ColEdgeScale:  return v.edgeScale;
    default: break;
    }
    return {};
}

void PmxVertexTableModel::applyValueToVertex(PmxVertex& v, int column, const QVariant& value) {
    switch (column) {
    case ColPosX:       v.position.x = value.toFloat(); break;
    case ColPosY:       v.position.y = value.toFloat(); break;
    case ColPosZ:       v.position.z = value.toFloat(); break;
    case ColNormX:      v.normal.x = value.toFloat(); break;
    case ColNormY:      v.normal.y = value.toFloat(); break;
    case ColNormZ:      v.normal.z = value.toFloat(); break;
    case ColUvU:        v.uv.x = value.toFloat(); break;
    case ColUvV:        v.uv.y = value.toFloat(); break;
    case ColWeightType: v.weightType = static_cast<WeightType>(std::clamp(value.toInt(), 0, 4)); break;
    case ColBone0:      v.boneIndices[0] = value.toInt(); break;
    case ColBone1:      v.boneIndices[1] = value.toInt(); break;
    case ColBone2:      v.boneIndices[2] = value.toInt(); break;
    case ColBone3:      v.boneIndices[3] = value.toInt(); break;
    case ColWeight0:    v.boneWeights[0] = value.toFloat(); break;
    case ColWeight1:    v.boneWeights[1] = value.toFloat(); break;
    case ColWeight2:    v.boneWeights[2] = value.toFloat(); break;
    case ColWeight3:    v.boneWeights[3] = value.toFloat(); break;
    case ColEdgeScale:  v.edgeScale = value.toFloat(); break;
    default: break;
    }
}

QVariant PmxVertexTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }

    const auto& vertices = m_controller->document().vertices;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= vertices.size() || col < 0 || col >= ColumnCount) {
        return {};
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        return getVertexFieldValue(vertices[row], col, row);
    }

    return {};
}

bool PmxVertexTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_controller) {
        return false;
    }

    const auto& vertices = m_controller->document().vertices;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= vertices.size() || col <= ColIndex || col >= ColumnCount) {
        return false;
    }

    const auto& v = vertices[row];
    int cmdId = makeCommandId(4, row, col);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    // Float columns: PosX, PosY, PosZ, NormX, NormY, NormZ, UvU, UvV, Weight0..3, EdgeScale
    if ((col >= ColPosX && col <= ColUvV) || (col >= ColWeight0 && col <= ColEdgeScale)) {
        float oldVal = getVertexFieldValue(v, col, row).toFloat();
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const float& val) {
                if (row >= 0 && static_cast<size_t>(row) < d.vertices.size()) {
                    applyValueToVertex(d.vertices[row], col, val);
                }
            },
            notify, QStringLiteral("Edit Vertex Float Property")));
        return true;
    }

    // Bone indices: Bone0..3
    if (col >= ColBone0 && col <= ColBone3) {
        int32_t oldVal = getVertexFieldValue(v, col, row).toInt();
        int32_t newVal = value.toInt();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<int32_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const int32_t& val) {
                if (row >= 0 && static_cast<size_t>(row) < d.vertices.size()) {
                    applyValueToVertex(d.vertices[row], col, val);
                }
            },
            notify, QStringLiteral("Edit Vertex Bone Index")));
        return true;
    }

    // WeightType
    if (col == ColWeightType) {
        uint8_t oldVal = static_cast<uint8_t>(v.weightType);
        uint8_t newVal = static_cast<uint8_t>(std::clamp(value.toInt(), 0, 4));
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const uint8_t& val) {
                if (row >= 0 && static_cast<size_t>(row) < d.vertices.size()) {
                    applyValueToVertex(d.vertices[row], col, val);
                }
            },
            notify, QStringLiteral("Edit Vertex Weight Type")));
        return true;
    }

    return false;
}

double PmxVertexTableModel::applyMathOp(double current, const QString& op, double operand) {
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

bool PmxVertexTableModel::applyBatchOperation(const QList<int>& rowIndices,
                                              int column,
                                              const QString& opType,
                                              double operand)
{
    if (!m_controller || rowIndices.isEmpty()) {
        return false;
    }

    // Reject non-numeric or read-only columns
    if (column <= ColIndex || column >= ColumnCount) {
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

    const auto& vertices = m_controller->document().vertices;
    int vertexCount = static_cast<int>(vertices.size());

    QList<PmxBatchOperationCommand::Delta> deltas;
    deltas.reserve(rowIndices.size());
    bool hasAnyChange = false;

    for (int r : rowIndices) {
        if (r < 0 || r >= vertexCount) {
            continue;
        }
        const auto& v = vertices[r];
        QVariant oldVal = getVertexFieldValue(v, column, r);
        double currentNum = oldVal.toDouble();
        double newNum = applyMathOp(currentNum, opType, operand);
        QVariant newVal;

        if (column == ColWeightType) {
            newVal = static_cast<int>(std::clamp(static_cast<int>(newNum), 0, 4));
        } else if (column >= ColBone0 && column <= ColBone3) {
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
        if (r >= 0 && static_cast<size_t>(r) < d.vertices.size()) {
            applyValueToVertex(d.vertices[r], c, val);
        }
    };

    auto notify = [this](int minR, int maxR, int c) {
        emit dataChanged(index(minR, c), index(maxR, c), {Qt::DisplayRole, Qt::EditRole});
    };

    QString cmdText = QStringLiteral("Batch Edit Vertex Col %1 (%2 %3)")
                          .arg(column)
                          .arg(opType)
                          .arg(operand);

    m_controller->pushCommand(new PmxBatchOperationCommand(
        m_controller, column, deltas, applier, notify, cmdText));

    return true;
}

void PmxVertexTableModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxVertexTableModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
}

} // namespace pmx
