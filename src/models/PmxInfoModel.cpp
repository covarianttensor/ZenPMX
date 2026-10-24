#include "models/PmxInfoModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

namespace pmx {

PmxInfoModel::PmxInfoModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxInfoModel::onDisplayLanguageChanged);
    }
}

int PmxInfoModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return RowCount;
}

int PmxInfoModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return 2;
}

QVariant PmxInfoModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        if (section == 0) {
            return isJp ? QStringLiteral("項目") : QStringLiteral("Property");
        } else if (section == 1) {
            return isJp ? QStringLiteral("値") : QStringLiteral("Value");
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxInfoModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    Qt::ItemFlags f = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (index.column() == 1) {
        f |= Qt::ItemIsEditable;
    }
    return f;
}

QVariant PmxInfoModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }
    if (index.row() < 0 || index.row() >= RowCount || index.column() < 0 || index.column() >= 2) {
        return {};
    }

    const auto& doc = m_controller->document();
    bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;

    if (index.column() == 0) {
        if (role == Qt::DisplayRole) {
            switch (index.row()) {
            case RowModelNameJp:        return isJp ? QStringLiteral("モデル名 (日)") : QStringLiteral("Model Name (JP)");
            case RowModelNameEn:        return isJp ? QStringLiteral("モデル名 (英)") : QStringLiteral("Model Name (EN)");
            case RowCommentJp:          return isJp ? QStringLiteral("コメント (日)") : QStringLiteral("Comment (JP)");
            case RowCommentEn:          return isJp ? QStringLiteral("コメント (英)") : QStringLiteral("Comment (EN)");
            case RowVersion:            return isJp ? QStringLiteral("バージョン") : QStringLiteral("Version");
            case RowTextEncoding:       return isJp ? QStringLiteral("エンコード") : QStringLiteral("Text Encoding");
            case RowAdditionalUVs:      return isJp ? QStringLiteral("追加UV数") : QStringLiteral("Additional UV Count");
            case RowVertexIndexSize:    return isJp ? QStringLiteral("頂点Indexサイズ") : QStringLiteral("Vertex Index Size");
            case RowTextureIndexSize:   return isJp ? QStringLiteral("テクスチャIndexサイズ") : QStringLiteral("Texture Index Size");
            case RowMaterialIndexSize:  return isJp ? QStringLiteral("材質Indexサイズ") : QStringLiteral("Material Index Size");
            case RowBoneIndexSize:      return isJp ? QStringLiteral("ボーンIndexサイズ") : QStringLiteral("Bone Index Size");
            case RowMorphIndexSize:     return isJp ? QStringLiteral("モーフIndexサイズ") : QStringLiteral("Morph Index Size");
            case RowRigidBodyIndexSize: return isJp ? QStringLiteral("剛体Indexサイズ") : QStringLiteral("Rigid Body Index Size");
            default: break;
            }
        }
        return {};
    }

    if (index.column() == 1) {
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            switch (index.row()) {
            case RowModelNameJp:        return doc.modelInfo.modelNameJp;
            case RowModelNameEn:        return doc.modelInfo.modelNameEn;
            case RowCommentJp:          return doc.modelInfo.commentJp;
            case RowCommentEn:          return doc.modelInfo.commentEn;
            case RowVersion:            return doc.header.version;
            case RowTextEncoding:       return static_cast<int>(doc.header.textEncoding);
            case RowAdditionalUVs:      return static_cast<int>(doc.header.additionalVec4Count);
            case RowVertexIndexSize:    return static_cast<int>(doc.header.vertexIndexSize);
            case RowTextureIndexSize:   return static_cast<int>(doc.header.textureIndexSize);
            case RowMaterialIndexSize:  return static_cast<int>(doc.header.materialIndexSize);
            case RowBoneIndexSize:      return static_cast<int>(doc.header.boneIndexSize);
            case RowMorphIndexSize:     return static_cast<int>(doc.header.morphIndexSize);
            case RowRigidBodyIndexSize: return static_cast<int>(doc.header.rigidBodyIndexSize);
            default: break;
            }
        }
    }

    return {};
}

bool PmxInfoModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || index.column() != 1 || role != Qt::EditRole || !m_controller) {
        return false;
    }
    if (index.row() < 0 || index.row() >= RowCount) {
        return false;
    }

    const auto& doc = m_controller->document();
    int cmdId = makeCommandId(1, index.row(), 1);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    switch (index.row()) {
    case RowModelNameJp: {
        QString oldVal = doc.modelInfo.modelNameJp;
        QString newVal = value.toString();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const QString& v) { d.modelInfo.modelNameJp = v; },
            notify, QStringLiteral("Edit Model Name (JP)")));
        return true;
    }
    case RowModelNameEn: {
        QString oldVal = doc.modelInfo.modelNameEn;
        QString newVal = value.toString();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const QString& v) { d.modelInfo.modelNameEn = v; },
            notify, QStringLiteral("Edit Model Name (EN)")));
        return true;
    }
    case RowCommentJp: {
        QString oldVal = doc.modelInfo.commentJp;
        QString newVal = value.toString();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const QString& v) { d.modelInfo.commentJp = v; },
            notify, QStringLiteral("Edit Comment (JP)")));
        return true;
    }
    case RowCommentEn: {
        QString oldVal = doc.modelInfo.commentEn;
        QString newVal = value.toString();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const QString& v) { d.modelInfo.commentEn = v; },
            notify, QStringLiteral("Edit Comment (EN)")));
        return true;
    }
    case RowVersion: {
        float oldVal = doc.header.version;
        float newVal = value.toFloat();
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<float>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const float& v) { d.header.version = v; },
            notify, QStringLiteral("Edit Version")));
        return true;
    }
    case RowTextEncoding: {
        uint8_t oldVal = doc.header.textEncoding;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.textEncoding = v; },
            notify, QStringLiteral("Edit Text Encoding")));
        return true;
    }
    case RowAdditionalUVs: {
        uint8_t oldVal = doc.header.additionalVec4Count;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.additionalVec4Count = v; },
            notify, QStringLiteral("Edit Additional UV Count")));
        return true;
    }
    case RowVertexIndexSize: {
        uint8_t oldVal = doc.header.vertexIndexSize;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.vertexIndexSize = v; },
            notify, QStringLiteral("Edit Vertex Index Size")));
        return true;
    }
    case RowTextureIndexSize: {
        uint8_t oldVal = doc.header.textureIndexSize;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.textureIndexSize = v; },
            notify, QStringLiteral("Edit Texture Index Size")));
        return true;
    }
    case RowMaterialIndexSize: {
        uint8_t oldVal = doc.header.materialIndexSize;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.materialIndexSize = v; },
            notify, QStringLiteral("Edit Material Index Size")));
        return true;
    }
    case RowBoneIndexSize: {
        uint8_t oldVal = doc.header.boneIndexSize;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.boneIndexSize = v; },
            notify, QStringLiteral("Edit Bone Index Size")));
        return true;
    }
    case RowMorphIndexSize: {
        uint8_t oldVal = doc.header.morphIndexSize;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.morphIndexSize = v; },
            notify, QStringLiteral("Edit Morph Index Size")));
        return true;
    }
    case RowRigidBodyIndexSize: {
        uint8_t oldVal = doc.header.rigidBodyIndexSize;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [](pmx::PmxDocument& d, const uint8_t& v) { d.header.rigidBodyIndexSize = v; },
            notify, QStringLiteral("Edit Rigid Body Index Size")));
        return true;
    }
    default:
        break;
    }

    return false;
}

void PmxInfoModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxInfoModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, 1);
    emit dataChanged(index(0, 0), index(RowCount - 1, 0), {Qt::DisplayRole});
}

} // namespace pmx
