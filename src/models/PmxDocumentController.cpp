#include "models/PmxDocumentController.h"

#include "core/PmxMerger.h"
#include "core/PmxReader.h"
#include "core/PmxWriter.h"
#include "models/PmxUndoCommands.h"
#include "models/PmxBoneTableModel.h"
#include "models/PmxDisplayFrameModel.h"
#include "models/PmxInfoModel.h"
#include "models/PmxJointTableModel.h"
#include "models/PmxMaterialTableModel.h"
#include "models/PmxMorphTableModel.h"
#include "models/PmxRigidBodyTableModel.h"
#include "models/PmxVertexTableModel.h"

namespace pmx {

PmxDocumentController::PmxDocumentController(QObject* parent)
    : QObject(parent)
    , m_selectionManager(this)
{
    m_infoModel = new PmxInfoModel(this, this);
    m_materialModel = new PmxMaterialTableModel(this, this);
    m_boneModel = new PmxBoneTableModel(this, this);
    m_vertexModel = new PmxVertexTableModel(this, this);
    m_morphModel = new PmxMorphTableModel(this, this);
    m_displayFrameModel = new PmxDisplayFrameModel(this, this);
    m_rigidBodyModel = new PmxRigidBodyTableModel(this, this);
    m_jointModel = new PmxJointTableModel(this, this);

    connect(&m_undoStack, &QUndoStack::cleanChanged, this, [this](bool clean) {
        emit isModifiedChanged(!clean);
    });

    m_undoStack.setClean();
}

PmxDocumentController::~PmxDocumentController() = default;

bool PmxDocumentController::loadFromFile(const QString& filePath, QString* errorMessage) {
    PmxDocument newDoc;
    if (!PmxReader::readFromFile(filePath, newDoc, errorMessage)) {
        return false;
    }

    m_document = std::move(newDoc);
    m_document.sourceFilePath = filePath;
    m_undoStack.clear();
    m_selectionManager.clearAll();
    resetAllModels();

    emit documentLoaded(filePath);
    emit documentReset();
    emit isModifiedChanged(false);
    return true;
}

bool PmxDocumentController::saveToFile(const QString& filePath, QString* errorMessage) {
    QString targetPath = filePath.isEmpty() ? m_document.sourceFilePath : filePath;
    if (targetPath.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("No destination file path specified for saving.");
        }
        return false;
    }

    if (!PmxWriter::writeToFile(targetPath, m_document, errorMessage)) {
        return false;
    }

    m_document.sourceFilePath = targetPath;
    m_undoStack.setClean();
    emit documentSaved(targetPath);
    return true;
}

bool PmxDocumentController::mergeModel(const PmxDocument& incomingDoc, const MergeOptions& options, QString* errorMessage) {
    PmxDocument mergedDoc = m_document;
    if (!PmxMerger::merge(mergedDoc, incomingDoc, options, errorMessage)) {
        return false;
    }

    auto* command = new PmxDocumentSnapshotCommand(this, std::move(mergedDoc), QStringLiteral("Merge Model (追加選択)"));
    m_undoStack.push(command);
    resetAllModels();
    return true;
}

const PmxDocument& PmxDocumentController::document() const {
    return m_document;
}

PmxDocument& PmxDocumentController::documentInternal() {
    return m_document;
}

void PmxDocumentController::setDocumentInternal(const PmxDocument& doc) {
    m_document = doc;
    resetAllModels();
    emit documentReset();
}

QUndoStack* PmxDocumentController::undoStack() {
    return &m_undoStack;
}

const QUndoStack* PmxDocumentController::undoStack() const {
    return &m_undoStack;
}

void PmxDocumentController::undo() {
    m_undoStack.undo();
}

void PmxDocumentController::redo() {
    m_undoStack.redo();
}

bool PmxDocumentController::isModified() const {
    return !m_undoStack.isClean();
}

void PmxDocumentController::pushCommand(QUndoCommand* command) {
    if (command) {
        m_undoStack.push(command);
    }
}

PmxDocumentController::DisplayLanguage PmxDocumentController::displayLanguage() const {
    return m_displayLanguage;
}

void PmxDocumentController::setDisplayLanguage(DisplayLanguage language) {
    if (m_displayLanguage == language) {
        return;
    }
    m_displayLanguage = language;
    emit displayLanguageChanged(m_displayLanguage);
}

PmxSelectionManager* PmxDocumentController::selectionManager() {
    return &m_selectionManager;
}

const PmxSelectionManager* PmxDocumentController::selectionManager() const {
    return &m_selectionManager;
}

PmxInfoModel* PmxDocumentController::infoModel() const {
    return m_infoModel;
}

PmxMaterialTableModel* PmxDocumentController::materialModel() const {
    return m_materialModel;
}

PmxBoneTableModel* PmxDocumentController::boneModel() const {
    return m_boneModel;
}

PmxVertexTableModel* PmxDocumentController::vertexModel() const {
    return m_vertexModel;
}

PmxMorphTableModel* PmxDocumentController::morphModel() const {
    return m_morphModel;
}

PmxDisplayFrameModel* PmxDocumentController::displayFrameModel() const {
    return m_displayFrameModel;
}

PmxRigidBodyTableModel* PmxDocumentController::rigidBodyModel() const {
    return m_rigidBodyModel;
}

PmxJointTableModel* PmxDocumentController::jointModel() const {
    return m_jointModel;
}

void PmxDocumentController::resetAllModels() {
    if (m_infoModel) {
        m_infoModel->resetModel();
    }
    if (m_materialModel) {
        m_materialModel->resetModel();
    }
    if (m_boneModel) {
        m_boneModel->resetModel();
    }
    if (m_vertexModel) {
        m_vertexModel->resetModel();
    }
    if (m_morphModel) {
        m_morphModel->resetModel();
    }
    if (m_displayFrameModel) {
        m_displayFrameModel->resetModel();
    }
    if (m_rigidBodyModel) {
        m_rigidBodyModel->resetModel();
    }
    if (m_jointModel) {
        m_jointModel->resetModel();
    }
}

} // namespace pmx
