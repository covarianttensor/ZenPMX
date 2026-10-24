#include "models/PmxDisplayFrameModel.h"

#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

namespace pmx {

PmxDisplayFrameModel::PmxDisplayFrameModel(PmxDocumentController* controller, QObject* parent)
    : QAbstractTableModel(parent)
    , m_controller(controller)
{
    if (m_controller) {
        connect(m_controller, &PmxDocumentController::displayLanguageChanged,
                this, &PmxDisplayFrameModel::onDisplayLanguageChanged);
    }
}

int PmxDisplayFrameModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() || !m_controller) return 0;
    return static_cast<int>(m_controller->document().displayFrames.size());
}

int PmxDisplayFrameModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant PmxDisplayFrameModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        bool isJp = m_controller && m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        switch (section) {
        case ColName:      return isJp ? QStringLiteral("表示枠名") : QStringLiteral("Frame Name");
        case ColSpecial:   return isJp ? QStringLiteral("特殊枠") : QStringLiteral("Special Flag");
        case ColItemCount: return isJp ? QStringLiteral("要素数") : QStringLiteral("Item Count");
        default: break;
        }
    } else {
        return section;
    }
    return {};
}

Qt::ItemFlags PmxDisplayFrameModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    if (index.column() == ColItemCount) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
}

QVariant PmxDisplayFrameModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_controller) {
        return {};
    }

    const auto& frames = m_controller->document().displayFrames;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= frames.size() || col < 0 || col >= ColumnCount) {
        return {};
    }

    const auto& frame = frames[row];

    if (role == NameJpRole) return frame.nameJp;
    if (role == NameEnRole) return frame.nameEn;
    if (role == SpecialFlagRole) return static_cast<int>(frame.specialFlag);
    if (role == ItemCountRole) return static_cast<int>(frame.elements.size());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (col) {
        case ColName: {
            bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
            return isJp ? frame.nameJp : frame.nameEn;
        }
        case ColSpecial:   return static_cast<int>(frame.specialFlag);
        case ColItemCount: return static_cast<int>(frame.elements.size());
        default: break;
        }
    }

    return {};
}

bool PmxDisplayFrameModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole || !m_controller) {
        return false;
    }

    const auto& frames = m_controller->document().displayFrames;
    int row = index.row();
    int col = index.column();
    if (row < 0 || static_cast<size_t>(row) >= frames.size() || col < 0 || col >= ColumnCount) {
        return false;
    }

    const auto& frame = frames[row];
    int cmdId = makeCommandId(6, row, col);
    auto notify = [this, index]() {
        emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    };

    if (col == ColName) {
        bool isJp = m_controller->displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese;
        int nameCmdId = isJp ? makeCommandId(6, row, col) : makeCommandId(6, row, col + 32);
        if (isJp) {
            QString oldVal = frame.nameJp;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.displayFrames.size()) {
                        d.displayFrames[row].nameJp = v;
                    }
                },
                notify, QStringLiteral("Edit Display Frame Name (JP)")));
            return true;
        } else {
            QString oldVal = frame.nameEn;
            QString newVal = value.toString();
            if (oldVal == newVal) return false;
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller, nameCmdId, oldVal, newVal,
                [row](pmx::PmxDocument& d, const QString& v) {
                    if (row >= 0 && static_cast<size_t>(row) < d.displayFrames.size()) {
                        d.displayFrames[row].nameEn = v;
                    }
                },
                notify, QStringLiteral("Edit Display Frame Name (EN)")));
            return true;
        }
    }

    if (col == ColSpecial) {
        uint8_t oldVal = frame.specialFlag;
        uint8_t newVal = static_cast<uint8_t>(value.toInt());
        if (oldVal == newVal) return false;
        m_controller->pushCommand(new PmxPropertyEditCommand<uint8_t>(
            m_controller, cmdId, oldVal, newVal,
            [row](pmx::PmxDocument& d, const uint8_t& v) {
                if (row >= 0 && static_cast<size_t>(row) < d.displayFrames.size()) {
                    d.displayFrames[row].specialFlag = v;
                }
            },
            notify, QStringLiteral("Edit Display Frame Special Flag")));
        return true;
    }

    return false;
}

void PmxDisplayFrameModel::resetModel() {
    beginResetModel();
    endResetModel();
}

void PmxDisplayFrameModel::onDisplayLanguageChanged() {
    emit headerDataChanged(Qt::Horizontal, 0, ColumnCount - 1);
    if (rowCount() > 0) {
        emit dataChanged(index(0, ColName), index(rowCount() - 1, ColName),
                         {Qt::DisplayRole, Qt::EditRole});
    }
}

} // namespace pmx
