#include "models/PmxUndoCommands.h"

#include <algorithm>
#include <limits>

namespace pmx {

// PmxDocumentSnapshotCommand
PmxDocumentSnapshotCommand::PmxDocumentSnapshotCommand(PmxDocumentController* controller,
                                                       pmx::PmxDocument newDoc,
                                                       const QString& text,
                                                       QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_controller(controller)
    , m_oldDoc(controller ? controller->document() : pmx::PmxDocument{})
    , m_newDoc(std::move(newDoc))
{
}

void PmxDocumentSnapshotCommand::undo() {
    if (m_controller) {
        m_controller->setDocumentInternal(m_oldDoc);
    }
}

void PmxDocumentSnapshotCommand::redo() {
    if (m_controller) {
        m_controller->setDocumentInternal(m_newDoc);
    }
}

// PmxBatchOperationCommand
PmxBatchOperationCommand::PmxBatchOperationCommand(
    PmxDocumentController* controller,
    int column,
    QList<Delta> deltas,
    std::function<void(pmx::PmxDocument&, int row, int col, const QVariant&)> fieldApplier,
    std::function<void(int minRow, int maxRow, int col)> notifyCallback,
    const QString& text,
    QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_controller(controller)
    , m_column(column)
    , m_deltas(std::move(deltas))
    , m_fieldApplier(std::move(fieldApplier))
    , m_notifyCallback(std::move(notifyCallback))
{
}

void PmxBatchOperationCommand::undo() {
    apply(false);
}

void PmxBatchOperationCommand::redo() {
    apply(true);
}

void PmxBatchOperationCommand::apply(bool useNew) {
    if (!m_controller || !m_fieldApplier) {
        return;
    }
    auto& doc = m_controller->documentInternal();
    int minRow = std::numeric_limits<int>::max();
    int maxRow = -1;

    for (const auto& delta : m_deltas) {
        if (delta.row >= 0) {
            const QVariant& val = useNew ? delta.newValue : delta.oldValue;
            m_fieldApplier(doc, delta.row, m_column, val);
            minRow = std::min(minRow, delta.row);
            maxRow = std::max(maxRow, delta.row);
        }
    }

    if (maxRow >= minRow && m_notifyCallback) {
        m_notifyCallback(minRow, maxRow, m_column);
    }
}

} // namespace pmx
