#include "models/PmxMaterialTableModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

#include <QColor>
#include <algorithm>
#include <cmath>

namespace pmx {

PmxMaterialTableModel::PmxMaterialTableModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxMaterialTableModel::onDisplayLanguageChanged);
    }
}

int PmxMaterialTableModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_controller) return 0;
    return static_cast<int>(m_controller->document().materials.size());
}

int PmxMaterialTableModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant PmxMaterialTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        switch (section) {
        case ColName:               return isJp ? QStringLiteral("材質名") : QStringLiteral("Material Name");
        case ColDiffuseR:           return isJp ? QStringLiteral("拡散色 R") : QStringLiteral("Diffuse R");
        case ColDiffuseG:           return isJp ? QStringLiteral("拡散色 G") : QStringLiteral("Diffuse G");
        case ColDiffuseB:           return isJp ? QStringLiteral("拡散色 B") : QStringLiteral("Diffuse B");
        case ColDiffuseA:           return isJp ? QStringLiteral("非透過度") : QStringLiteral("Diffuse Alpha");
        case ColSpecularR:          return isJp ? QStringLiteral("反射色 R") : QStringLiteral("Specular R");
        case ColSpecularG:          return isJp ? QStringLiteral("反射色 G") : QStringLiteral("Specular G");
        case ColSpecularB:          return isJp ? QStringLiteral("反射色 B") : QStringLiteral("Specular B");
        case ColSpecularPower:      return isJp ? QStringLiteral("反射強度") : QStringLiteral("Specular Power");
        case ColAmbientR:           return isJp ? QStringLiteral("環境色 R") : QStringLiteral("Ambient R");
        case ColAmbientG:           return isJp ? QStringLiteral("環境色 G") : QStringLiteral("Ambient G");
        case ColAmbientB:           return isJp ? QStringLiteral("環境色 B") : QStringLiteral("Ambient B");
        case ColDrawFlags:          return isJp ? QStringLiteral("描画フラグ") : QStringLiteral("Draw Flags");
        case ColEdgeR:              return isJp ? QStringLiteral("輪郭色 R") : QStringLiteral("Edge R");
        case ColEdgeG:              return isJp ? QStringLiteral("輪郭色 G") : QStringLiteral("Edge G");
        case ColEdgeB:              return isJp ? QStringLiteral("輪郭色 B") : QStringLiteral("Edge B");
        case ColEdgeA:              return isJp ? QStringLiteral("輪郭色 A") : QStringLiteral("Edge A");
        case ColEdgeSize:           return isJp ? QStringLiteral("輪郭サイズ") : QStringLiteral("Edge Size");
        case ColTextureIndex:       return isJp ? QStringLiteral("テクスチャ") : QStringLiteral("Texture Index");
        case ColSphereTextureIndex: return isJp ? QStringLiteral("スフィア") : QStringLiteral("Sphere Index");
        case ColSphereMode:         return isJp ? QStringLiteral("スフィアモード") : QStringLiteral("Sphere Mode");
        case ColSharedToonFlag:     return isJp ? QStringLiteral("共有Toon") : QStringLiteral("Shared Toon");
        case ColToonIndex:          return isJp ? QStringLiteral("Toon") : QStringLiteral("Toon Index");
        case ColMemo:               return isJp ? QStringLiteral("メモ") : QStringLiteral("Memo");
        case ColFaceVertexCount:    return isJp ? QStringLiteral("面頂点数") : QStringLiteral("Face Vertex Count");
        default: break;
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxMaterialTableModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant PmxMaterialTableModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }

    const auto& materials = m_controller->document().materials;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= materials.size() || col < 0 || col >= ColumnCount) {
        return {};
    }

    const auto& mat = materials[row];

    if (role == NameJpRole) return mat.nameJp;
    if (role == NameEnRole) return mat.nameEn;
    if (role == DiffuseColorRole) {
        return QColor::fromRgbF(std::clamp(mat.diffuse.r, 0.0f, 1.0f),
                               std::clamp(mat.diffuse.g, 0.0f, 1.0f),
                               std::clamp(mat.diffuse.b, 0.0f, 1.0f),
                               std::clamp(mat.diffuse.a, 0.0f, 1.0f));
    }
    if (role == SpecularColorRole) {
        return QColor::fromRgbF(std::clamp(mat.specular.r, 0.0f, 1.0f),
                               std::clamp(mat.specular.g, 0.0f, 1.0f),
                               std::clamp(mat.specular.b, 0.0f, 1.0f));
    }
    if (role == AmbientColorRole) {
        return QColor::fromRgbF(std::clamp(mat.ambient.r, 0.0f, 1.0f),
                               std::clamp(mat.ambient.g, 0.0f, 1.0f),
                               std::clamp(mat.ambient.b, 0.0f, 1.0f));
    }
    if (role == EdgeColorRole) {
        return QColor::fromRgbF(std::clamp(mat.edgeColor.r, 0.0f, 1.0f),
                               std::clamp(mat.edgeColor.g, 0.0f, 1.0f),
                               std::clamp(mat.edgeColor.b, 0.0f, 1.0f),
                               std::clamp(mat.edgeColor.a, 0.0f, 1.0f));
    }

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        if (col == ColName) {
            bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
            return isJp ? mat.nameJp : mat.nameEn;
        }
        return getMaterialFieldValue(mat, col);
    }

    return {};
}

QVariant PmxMaterialTableModel::getMaterialFieldValue(const PmxMaterial& mat, int column) {
    switch (column) {
    case ColName:               return mat.nameJp;
    case ColDiffuseR:           return mat.diffuse.r;
    case ColDiffuseG:           return mat.diffuse.g;
    case ColDiffuseB:           return mat.diffuse.b;
    case ColDiffuseA:           return mat.diffuse.a;
    case ColSpecularR:          return mat.specular.r;
    case ColSpecularG:          return mat.specular.g;
    case ColSpecularB:          return mat.specular.b;
    case ColSpecularPower:      return mat.specularPower;
    case ColAmbientR:           return mat.ambient.r;
    case ColAmbientG:           return mat.ambient.g;
    case ColAmbientB:           return mat.ambient.b;
    case ColDrawFlags:          return static_cast<int>(mat.drawFlags);
    case ColEdgeR:              return mat.edgeColor.r;
    case ColEdgeG:              return mat.edgeColor.g;
    case ColEdgeB:              return mat.edgeColor.b;
    case ColEdgeA:              return mat.edgeColor.a;
    case ColEdgeSize:           return mat.edgeSize;
    case ColTextureIndex:       return mat.textureIndex;
    case ColSphereTextureIndex: return mat.sphereTextureIndex;
    case ColSphereMode:         return static_cast<int>(mat.sphereMode);
    case ColSharedToonFlag:     return static_cast<int>(mat.sharedToonFlag);
    case ColToonIndex:          return mat.toonIndex;
    case ColMemo:               return mat.memo;
    case ColFaceVertexCount:    return mat.faceVertexCount;
    default: break;
    }
    return {};
}

void PmxMaterialTableModel::applyValueToMaterial(PmxMaterial& mat, int column, const QVariant& value) {
    switch (column) {
    case ColName:
        mat.nameJp = value.toString();
        break;
    case ColDiffuseR:           mat.diffuse.r = value.toFloat(); break;
    case ColDiffuseG:           mat.diffuse.g = value.toFloat(); break;
    case ColDiffuseB:           mat.diffuse.b = value.toFloat(); break;
    case ColDiffuseA:           mat.diffuse.a = value.toFloat(); break;
    case ColSpecularR:          mat.specular.r = value.toFloat(); break;
    case ColSpecularG:          mat.specular.g = value.toFloat(); break;
    case ColSpecularB:          mat.specular.b = value.toFloat(); break;
    case ColSpecularPower:      mat.specularPower = value.toFloat(); break;
    case ColAmbientR:           mat.ambient.r = value.toFloat(); break;
    case ColAmbientG:           mat.ambient.g = value.toFloat(); break;
    case ColAmbientB:           mat.ambient.b = value.toFloat(); break;
    case ColDrawFlags:          mat.drawFlags = static_cast<uint8_t>(value.toInt()); break;
    case ColEdgeR:              mat.edgeColor.r = value.toFloat(); break;
    case ColEdgeG:              mat.edgeColor.g = value.toFloat(); break;
    case ColEdgeB:              mat.edgeColor.b = value.toFloat(); break;
    case ColEdgeA:              mat.edgeColor.a = value.toFloat(); break;
    case ColEdgeSize:           mat.edgeSize = value.toFloat(); break;
    case ColTextureIndex:       mat.textureIndex = value.toInt(); break;
    case ColSphereTextureIndex: mat.sphereTextureIndex = value.toInt(); break;
    case ColSphereMode:         mat.sphereMode = static_cast<uint8_t>(value.toInt()); break;
    case ColSharedToonFlag:     mat.sharedToonFlag = static_cast<uint8_t>(value.toInt()); break;
    case ColToonIndex:          mat.toonIndex = value.toInt(); break;
    case ColMemo:               mat.memo = value.toString(); break;
    case ColFaceVertexCount:    mat.faceVertexCount = value.toInt(); break;
    default: break;
    }
}

bool PmxMaterialTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_controller) {
        return false;
    }

    const auto& materials = m_controller->document().materials;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= materials.size() || col < 0 || col >= ColumnCount) {
        return false;
    }

    const auto& mat = materials[row];
    int cmdId = makeCommandId(2, row, col);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    if (col == ColName) {
        bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        int nameCmdId = isJp ? makeCommandId(2, row, col) : makeCommandId(2, row, col + 32);
        if (isJp) {
            QString oldVal = mat.nameJp;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.materials.size()) {
                        d.materials[row].nameJp = v;
                    }
                },
                notify, QStringLiteral("Edit Material Name (JP)")));
            return true;
        } else {
            QString oldVal = mat.nameEn;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.materials.size()) {
                        d.materials[row].nameEn = v;
                    }
                },
                notify, QStringLiteral("Edit Material Name (EN)")));
            return true;
        }
    }

    // Float columns
    if ((col >= ColDiffuseR && col <= ColSpecularPower) ||
        (col >= ColAmbientR && col <= ColAmbientB) ||
        (col >= ColEdgeR && col <= ColEdgeSize)) {
        float oldVal = getMaterialFieldValue(mat, col).toFloat();
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const float& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.materials.size()) {
                    applyValueToMaterial(d.materials[row], col, v);
                }
            },
            notify, QStringLiteral("Edit Material Float Property")));
        return true;
    }

    // Int32 columns
    if (col == ColTextureIndex || col == ColSphereTextureIndex ||
        col == ColToonIndex || col == ColFaceVertexCount) {
        int32_t oldVal = getMaterialFieldValue(mat, col).toInt();
        int32_t newVal = value.toInt();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<int32_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const int32_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.materials.size()) {
                    applyValueToMaterial(d.materials[row], col, v);
                }
            },
            notify, QStringLiteral("Edit Material Int Property")));
        return true;
    }

    // Uint8 columns
    if (col == ColDrawFlags || col == ColSphereMode || col == ColSharedToonFlag) {
        uint8_t oldVal = static_cast<uint8_t>(getMaterialFieldValue(mat, col).toInt());
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [row, col](pmx::PmxDocument& d, const uint8_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.materials.size()) {
                    applyValueToMaterial(d.materials[row], col, v);
                }
            },
            notify, QStringLiteral("Edit Material Flag Property")));
        return true;
    }

    // Memo column
    if (col == ColMemo) {
        QString oldVal = mat.memo;
        QString newVal = value.toString();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const QString& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.materials.size()) {
                    d.materials[row].memo = v;
                }
            },
            notify, QStringLiteral("Edit Material Memo")));
        return true;
    }

    return false;
}

double PmxMaterialTableModel::applyMathOp(double current, const QString& op, double operand) {
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

bool PmxMaterialTableModel::applyBatchOperation(const QList<int>& rowIndices,
                                               int column,
                                               const QString& opType,
                                               double operand)
{
    if (!m_controller || rowIndices.isEmpty()) {
        return false;
    }

    // Reject non-numeric columns
    if (column == ColName || column == ColMemo || column < 0 || column >= ColumnCount) {
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

    const auto& materials = m_controller->document().materials;
    int matCount = static_cast<int>(materials.size());

    QList<PmxBatchOperationCommand::Delta> deltas;
    deltas.reserve(rowIndices.size());
    bool hasAnyChange = false;

    for (int r : rowIndices) {
        if (r < 0 || r >= matCount) {
            continue;
        }
        const auto& mat = materials[r];
        QVariant oldVal = getMaterialFieldValue(mat, column);
        double currentNum = oldVal.toDouble();
        double newNum = applyMathOp(currentNum, opType, operand);
        QVariant newVal;

        // Maintain type consistency
        if (column == ColDrawFlags || column == ColSphereMode || column == ColSharedToonFlag) {
            newVal = static_cast<int>(std::clamp(static_cast<int>(newNum), 0, 255));
        } else if (column == ColTextureIndex || column == ColSphereTextureIndex ||
                   column == ColToonIndex || column == ColFaceVertexCount) {
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
        if (r >= 0 && static_cast<size_t>(r) < d.materials.size()) {
            applyValueToMaterial(d.materials[r], c, val);
        }
    };

    auto notify = [this](int minR, int maxR, int c) {
        emit dataChanged(index(minR, c), index(maxR, c), {Qt::DisplayRole, Qt::EditRole});
    };

    QString cmdText = QStringLiteral("Batch Edit Material Col %1 (%2 %3)")
                          .arg(column)
                          .arg(opType)
                          .arg(operand);

    m_controller->pushCommand(new PmxBatchOperationCommand(
        m_controller, column, deltas, applier, notify, cmdText));

    return true;
}

void PmxMaterialTableModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxMaterialTableModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    if (rowCount() > 0) {
        emit dataChanged(index(0, ColName), index(rowCount() - 1, ColName),
                         {Qt::DisplayRole, Qt::EditRole});
    }
}

} // namespace pmx
