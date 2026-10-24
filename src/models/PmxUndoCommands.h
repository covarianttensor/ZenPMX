#pragma once

#include "core/PmxDocument.h"
#include "models/PmxDocumentController.h"

#include <QList>
#include <QString>
#include <QUndoCommand>
#include <QVariant>
#include <functional>
#include <utility>

namespace pmx {

inline int makeCommandId(int tableId, int row, int col) {
    return ((tableId & 0xF) << 27) | ((col & 0x3F) << 21) | (row & 0x1FFFFF);
}

// Reversible property edit command supporting id() and mergeWith() so rapid continuous
// edits (e.g. dragging a color slider or spinbox) coalesce into a single undo step.
template <typename T>
class PmxPropertyEditCommand : public QUndoCommand {
public:
    using Setter = std::function<void(pmx::PmxDocument&, const T&)>;
    using Notify = std::function<void()>;

    PmxPropertyEditCommand(PmxDocumentController* controller,
                           int commandId,
                           const T& oldValue,
                           const T& newValue,
                           Setter setter,
                           Notify notify = nullptr,
                           const QString& text = QString(),
                           QUndoCommand* parent = nullptr)
        : QUndoCommand(text, parent)
        , m_controller(controller)
        , m_commandId(commandId)
        , m_oldValue(oldValue)
        , m_newValue(newValue)
        , m_setter(std::move(setter))
        , m_notify(std::move(notify))
    {
    }

    int id() const override {
        return m_commandId;
    }

    bool mergeWith(const QUndoCommand* other) override {
        if (!other || m_commandId == -1 || other->id() != m_commandId) {
            return false;
        }
        const auto* next = dynamic_cast<const PmxPropertyEditCommand<T>*>(other);
        if (!next || next->m_commandId != m_commandId) {
            return false;
        }
        m_newValue = next->m_newValue;
        return true;
    }

    void redo() override {
        applyValue(m_newValue);
    }

    void undo() override {
        applyValue(m_oldValue);
    }

    const T& oldValue() const { return m_oldValue; }
    const T& newValue() const { return m_newValue; }

private:
    void applyValue(const T& val) {
        if (m_controller && m_setter) {
            m_setter(m_controller->documentInternal(), val);
            if (m_notify) {
                m_notify();
            }
        }
    }

    PmxDocumentController* m_controller;
    int m_commandId;
    T m_oldValue;
    T m_newValue;
    Setter m_setter;
    Notify m_notify;
};

// Complete undoable state snapshot swap for large structural or batch edits.
class PmxDocumentSnapshotCommand : public QUndoCommand {
public:
    PmxDocumentSnapshotCommand(PmxDocumentController* controller,
                               pmx::PmxDocument newDoc,
                               const QString& text = QStringLiteral("Snapshot Edit"),
                               QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    PmxDocumentController* m_controller;
    pmx::PmxDocument m_oldDoc;
    pmx::PmxDocument m_newDoc;
};

// Batch operation command for multi-row edits with single-step undo across table models.
class PmxBatchOperationCommand : public QUndoCommand {
public:
    enum class BatchOp {
        Add = 0,
        Subtract = 1,
        Multiply = 2,
        Assign = 3
    };

    struct Delta {
        int row = -1;
        QVariant oldValue;
        QVariant newValue;
    };

    PmxBatchOperationCommand(
        PmxDocumentController* controller,
        int column,
        QList<Delta> deltas,
        std::function<void(pmx::PmxDocument&, int row, int col, const QVariant&)> fieldApplier,
        std::function<void(int minRow, int maxRow, int col)> notifyCallback,
        const QString& text = QStringLiteral("Batch Edit"),
        QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    void apply(bool useNew);

    PmxDocumentController* m_controller;
    int m_column;
    QList<Delta> m_deltas;
    std::function<void(pmx::PmxDocument&, int row, int col, const QVariant&)> m_fieldApplier;
    std::function<void(int minRow, int maxRow, int col)> m_notifyCallback;
};

using PmxBatchMaterialOperationCommand = PmxBatchOperationCommand;

} // namespace pmx
