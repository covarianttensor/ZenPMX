#pragma once

#include "core/PmxDocument.h"
#include "core/PmxMerger.h"
#include "core/PmxTypes.h"
#include "models/PmxSelectionManager.h"

#include <QObject>
#include <QString>
#include <QUndoStack>

namespace pmx {

class PmxInfoModel;
class PmxMaterialTableModel;
class PmxBoneTableModel;
class PmxVertexTableModel;
class PmxMorphTableModel;
class PmxDisplayFrameModel;
class PmxRigidBodyTableModel;
class PmxJointTableModel;
class PmxDocumentSnapshotCommand;
class PmxBatchOperationCommand;
class PmxVertexTransformCommand;
class PmxBoneTransformCommand;
template <typename T> class PmxPropertyEditCommand;

class PmxDocumentController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isModified READ isModified NOTIFY isModifiedChanged)
    Q_PROPERTY(DisplayLanguage displayLanguage READ displayLanguage WRITE setDisplayLanguage NOTIFY displayLanguageChanged)

public:
    enum class DisplayLanguage {
        Japanese,
        English
    };
    Q_ENUM(DisplayLanguage)

    explicit PmxDocumentController(QObject* parent = nullptr);
    ~PmxDocumentController() override;

    // Document loading / saving / merging
    bool loadFromFile(const QString& filePath, QString* errorMessage = nullptr);
    bool saveToFile(const QString& filePath = QString(), QString* errorMessage = nullptr);
    bool mergeModel(const PmxDocument& incomingDoc, const MergeOptions& options = {}, QString* errorMessage = nullptr);

    // Document state (const access only for public callers - enforces State Mutation Law)
    const pmx::PmxDocument& document() const;

    // Undo / Redo
    QUndoStack* undoStack();
    const QUndoStack* undoStack() const;
    void undo();
    void redo();
    bool isModified() const;
    void pushCommand(QUndoCommand* command);

    // Language
    DisplayLanguage displayLanguage() const;
    void setDisplayLanguage(DisplayLanguage language);

    // Selection
    PmxSelectionManager* selectionManager();
    const PmxSelectionManager* selectionManager() const;

    // Table Models
    PmxInfoModel* infoModel() const;
    PmxMaterialTableModel* materialModel() const;
    PmxBoneTableModel* boneModel() const;
    PmxVertexTableModel* vertexModel() const;
    PmxMorphTableModel* morphModel() const;
    PmxDisplayFrameModel* displayFrameModel() const;
    PmxRigidBodyTableModel* rigidBodyModel() const;
    PmxJointTableModel* jointModel() const;

    // Internal mutation helpers (accessible ONLY to internal friend commands and models)
    pmx::PmxDocument& documentInternal();
    void setDocumentInternal(const pmx::PmxDocument& doc);
    void resetAllModels();

signals:
    void documentLoaded(const QString& filePath);
    void documentSaved(const QString& filePath);
    void documentReset();
    void isModifiedChanged(bool modified);
    void displayLanguageChanged(DisplayLanguage language);

private:
    friend class PmxDocumentSnapshotCommand;
    friend class PmxBatchOperationCommand;
    friend class PmxVertexTransformCommand;
    friend class PmxBoneTransformCommand;
    friend class PmxMaterialTableModel;
    friend class PmxBoneTableModel;
    friend class PmxInfoModel;
    friend class PmxVertexTableModel;
    friend class PmxMorphTableModel;
    friend class PmxDisplayFrameModel;
    friend class PmxRigidBodyTableModel;
    friend class PmxJointTableModel;
    template <typename T> friend class PmxPropertyEditCommand;

    pmx::PmxDocument m_document;
    QUndoStack m_undoStack;
    PmxSelectionManager m_selectionManager;
    DisplayLanguage m_displayLanguage = DisplayLanguage::Japanese;

    PmxInfoModel* m_infoModel = nullptr;
    PmxMaterialTableModel* m_materialModel = nullptr;
    PmxBoneTableModel* m_boneModel = nullptr;
    PmxVertexTableModel* m_vertexModel = nullptr;
    PmxMorphTableModel* m_morphModel = nullptr;
    PmxDisplayFrameModel* m_displayFrameModel = nullptr;
    PmxRigidBodyTableModel* m_rigidBodyModel = nullptr;
    PmxJointTableModel* m_jointModel = nullptr;
};

} // namespace pmx
