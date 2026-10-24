#include "app/PmxAppBridge.h"
#include "models/PmxInfoModel.h"
#include "models/PmxVertexTableModel.h"
#include "models/PmxMaterialTableModel.h"
#include "models/PmxBoneTableModel.h"
#include "models/PmxMorphTableModel.h"
#include "models/PmxDisplayFrameModel.h"
#include "models/PmxRigidBodyTableModel.h"
#include "models/PmxJointTableModel.h"
#include "models/PmxUndoCommands.h"

#include <QFileInfo>
#include <QUrl>

namespace pmx {

PmxAppBridge::PmxAppBridge(QObject* parent)
    : QObject(parent)
{
    connect(&m_controller, &PmxDocumentController::isModifiedChanged, this, &PmxAppBridge::stateChanged);
    connect(&m_controller, &PmxDocumentController::displayLanguageChanged, this, &PmxAppBridge::languageChanged);

    if (m_controller.undoStack()) {
        connect(m_controller.undoStack(), &QUndoStack::canUndoChanged, this, &PmxAppBridge::undoRedoChanged);
        connect(m_controller.undoStack(), &QUndoStack::canRedoChanged, this, &PmxAppBridge::undoRedoChanged);
        connect(m_controller.undoStack(), &QUndoStack::indexChanged, this, &PmxAppBridge::stateChanged);
    }

    connect(&m_controller, &PmxDocumentController::documentLoaded, this, [this](const QString& path) {
        m_currentFilePath = path;
        emit filePathChanged();
        emit modelsChanged();
        const auto& doc = m_controller.document();
        m_statusMessage = QStringLiteral("Opened: %1 (%2 vertices, %3 materials, %4 bones)")
                              .arg(QFileInfo(path).fileName())
                              .arg(doc.vertices.size())
                              .arg(doc.materials.size())
                              .arg(doc.bones.size());
        emit statusMessageChanged();
    });

    connect(&m_controller, &PmxDocumentController::documentSaved, this, [this](const QString& path) {
        m_currentFilePath = path;
        emit filePathChanged();
        m_statusMessage = QStringLiteral("Saved: %1").arg(QFileInfo(path).fileName());
        emit statusMessageChanged();
    });

    connect(&m_controller, &PmxDocumentController::documentReset, this, [this]() {
        emit modelsChanged();
    });
}

QAbstractTableModel* PmxAppBridge::infoModel() const {
    return m_controller.infoModel();
}

QAbstractTableModel* PmxAppBridge::vertexModel() const {
    return m_controller.vertexModel();
}

QAbstractTableModel* PmxAppBridge::materialModel() const {
    return m_controller.materialModel();
}

QAbstractTableModel* PmxAppBridge::boneModel() const {
    return m_controller.boneModel();
}

QAbstractTableModel* PmxAppBridge::morphModel() const {
    return m_controller.morphModel();
}

QAbstractTableModel* PmxAppBridge::displayFrameModel() const {
    return m_controller.displayFrameModel();
}

QAbstractTableModel* PmxAppBridge::rigidBodyModel() const {
    return m_controller.rigidBodyModel();
}

QAbstractTableModel* PmxAppBridge::jointModel() const {
    return m_controller.jointModel();
}

QString PmxAppBridge::displayLanguage() const {
    return m_controller.displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese
               ? QStringLiteral("Japanese")
               : QStringLiteral("English");
}

bool PmxAppBridge::openFile(const QString& filePath) {
    QString localPath = filePath;
    QUrl url(filePath);
    if (url.isValid() && url.isLocalFile()) {
        localPath = url.toLocalFile();
    }

    QString errorMsg;
    bool ok = m_controller.loadFromFile(localPath, &errorMsg);
    if (!ok) {
        m_statusMessage = QStringLiteral("Error loading: %1").arg(errorMsg);
        emit statusMessageChanged();
    }
    return ok;
}

bool PmxAppBridge::saveFile(const QString& filePath) {
    QString savePath = filePath;
    if (savePath.isEmpty()) {
        savePath = m_currentFilePath;
    } else {
        QUrl url(filePath);
        if (url.isValid() && url.isLocalFile()) {
            savePath = url.toLocalFile();
        }
    }

    QString errorMsg;
    bool ok = m_controller.saveToFile(savePath, &errorMsg);
    if (!ok) {
        m_statusMessage = QStringLiteral("Error saving: %1").arg(errorMsg);
        emit statusMessageChanged();
    }
    return ok;
}

void PmxAppBridge::undo() {
    m_controller.undo();
    emit undoRedoChanged();
}

void PmxAppBridge::redo() {
    m_controller.redo();
    emit undoRedoChanged();
}

void PmxAppBridge::toggleLanguage() {
    auto current = m_controller.displayLanguage();
    auto next = (current == PmxDocumentController::DisplayLanguage::Japanese)
                    ? PmxDocumentController::DisplayLanguage::English
                    : PmxDocumentController::DisplayLanguage::Japanese;
    m_controller.setDisplayLanguage(next);
}

bool PmxAppBridge::applyBatchOperation(int tableId, const QList<int>& rows, int column, int operation, double operand) {
    QString opStr;
    switch (static_cast<PmxBatchOperationCommand::BatchOp>(operation)) {
    case PmxBatchOperationCommand::BatchOp::Add:
        opStr = QStringLiteral("+");
        break;
    case PmxBatchOperationCommand::BatchOp::Subtract:
        opStr = QStringLiteral("-");
        break;
    case PmxBatchOperationCommand::BatchOp::Multiply:
        opStr = QStringLiteral("*");
        break;
    case PmxBatchOperationCommand::BatchOp::Assign:
        opStr = QStringLiteral("=");
        break;
    default:
        return false;
    }

    switch (tableId) {
    case 2: // Material
        if (auto* m = m_controller.materialModel()) {
            return m->applyBatchOperation(rows, column, opStr, operand);
        }
        break;
    case 3: // Bone
        if (auto* m = m_controller.boneModel()) {
            return m->applyBatchOperation(rows, column, opStr, operand);
        }
        break;
    case 4: // Vertex
        if (auto* m = m_controller.vertexModel()) {
            return m->applyBatchOperation(rows, column, opStr, operand);
        }
        break;
    case 7: // RigidBody
        if (auto* m = m_controller.rigidBodyModel()) {
            return m->applyBatchOperation(rows, column, opStr, operand);
        }
        break;
    case 8: // Joint
        if (auto* m = m_controller.jointModel()) {
            return m->applyBatchOperation(rows, column, opStr, operand);
        }
        break;
    default:
        break;
    }
    return false;
}

int PmxAppBridge::rowCount(int tableId) const {
    switch (tableId) {
    case 0:
    case 1:
        return m_controller.infoModel() ? m_controller.infoModel()->rowCount() : 0;
    case 2:
        return m_controller.materialModel() ? m_controller.materialModel()->rowCount() : 0;
    case 3:
        return m_controller.boneModel() ? m_controller.boneModel()->rowCount() : 0;
    case 4:
        return m_controller.vertexModel() ? m_controller.vertexModel()->rowCount() : 0;
    case 5:
        return m_controller.morphModel() ? m_controller.morphModel()->rowCount() : 0;
    case 6:
        return m_controller.displayFrameModel() ? m_controller.displayFrameModel()->rowCount() : 0;
    case 7:
        return m_controller.rigidBodyModel() ? m_controller.rigidBodyModel()->rowCount() : 0;
    case 8:
        return m_controller.jointModel() ? m_controller.jointModel()->rowCount() : 0;
    default:
        return 0;
    }
}

} // namespace pmx
