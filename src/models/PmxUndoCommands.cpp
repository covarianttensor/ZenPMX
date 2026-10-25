#include "models/PmxUndoCommands.h"
#include "models/PmxVertexTableModel.h"
#include "models/PmxBoneTableModel.h"

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

// PmxVertexTransformCommand
PmxVertexTransformCommand::PmxVertexTransformCommand(
    PmxDocumentController* controller,
    std::vector<VertexTransformDelta> deltas,
    int commandId,
    const QString& text,
    QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_controller(controller)
    , m_deltas(std::move(deltas))
    , m_commandId(commandId)
{
}

bool PmxVertexTransformCommand::mergeWith(const QUndoCommand* other) {
    if (!other || m_commandId == -1 || other->id() != m_commandId) {
        return false;
    }
    const auto* next = dynamic_cast<const PmxVertexTransformCommand*>(other);
    if (!next || next->m_deltas.size() != m_deltas.size()) {
        return false;
    }
    for (size_t i = 0; i < m_deltas.size(); ++i) {
        if (m_deltas[i].vertexIndex != next->m_deltas[i].vertexIndex) {
            return false;
        }
    }
    for (size_t i = 0; i < m_deltas.size(); ++i) {
        m_deltas[i].newPos = next->m_deltas[i].newPos;
        m_deltas[i].newNormal = next->m_deltas[i].newNormal;
    }
    return true;
}

void PmxVertexTransformCommand::undo() {
    apply(false);
}

void PmxVertexTransformCommand::redo() {
    apply(true);
}

void PmxVertexTransformCommand::apply(bool useNew) {
    if (!m_controller) return;
    auto& doc = m_controller->documentInternal();
    for (const auto& d : m_deltas) {
        if (d.vertexIndex >= 0 && static_cast<size_t>(d.vertexIndex) < doc.vertices.size()) {
            doc.vertices[d.vertexIndex].position = useNew ? d.newPos : d.oldPos;
            doc.vertices[d.vertexIndex].normal = useNew ? d.newNormal : d.oldNormal;
        }
    }
    if (m_controller->vertexModel()) {
        m_controller->vertexModel()->resetModel();
    }
}

// PmxBoneTransformCommand
PmxBoneTransformCommand::PmxBoneTransformCommand(
    PmxDocumentController* controller,
    std::vector<BoneTransformDelta> deltas,
    int commandId,
    const QString& text,
    QUndoCommand* parent)
    : QUndoCommand(text, parent)
    , m_controller(controller)
    , m_deltas(std::move(deltas))
    , m_commandId(commandId)
{
}

bool PmxBoneTransformCommand::mergeWith(const QUndoCommand* other) {
    if (!other || m_commandId == -1 || other->id() != m_commandId) {
        return false;
    }
    const auto* next = dynamic_cast<const PmxBoneTransformCommand*>(other);
    if (!next || next->m_deltas.size() != m_deltas.size()) {
        return false;
    }
    for (size_t i = 0; i < m_deltas.size(); ++i) {
        if (m_deltas[i].boneIndex != next->m_deltas[i].boneIndex) {
            return false;
        }
    }
    for (size_t i = 0; i < m_deltas.size(); ++i) {
        m_deltas[i].newPos = next->m_deltas[i].newPos;
    }
    return true;
}

void PmxBoneTransformCommand::undo() {
    apply(false);
}

void PmxBoneTransformCommand::redo() {
    apply(true);
}

void PmxBoneTransformCommand::apply(bool useNew) {
    if (!m_controller) return;
    auto& doc = m_controller->documentInternal();
    for (const auto& d : m_deltas) {
        if (d.boneIndex >= 0 && static_cast<size_t>(d.boneIndex) < doc.bones.size()) {
            doc.bones[d.boneIndex].position = useNew ? d.newPos : d.oldPos;
        }
    }
    if (m_controller->boneModel()) {
        m_controller->boneModel()->resetModel();
    }
}

} // namespace pmx
