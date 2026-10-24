#include "app/PmxAppBridge.h"
#include "models/PmxInfoModel.h"
#include "models/PmxMaterialTableModel.h"
#include "models/PmxBoneTableModel.h"

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

QAbstractTableModel* PmxAppBridge::materialModel() const {
    return m_controller.materialModel();
}

QAbstractTableModel* PmxAppBridge::boneModel() const {
    return m_controller.boneModel();
}

QString PmxAppBridge::displayLanguage() const {
    return m_controller.displayLanguage() == PmxDocumentController::DisplayLanguage::Japanese
               ? QStringLiteral("JP")
               : QStringLiteral("EN");
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

} // namespace pmx
