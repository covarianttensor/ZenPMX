#pragma once

#include "models/PmxDocumentController.h"
#include <QObject>
#include <QString>
#include <QUrl>
#include <QAbstractTableModel>

namespace pmx {

class PmxAppBridge : public QObject {
    Q_OBJECT
    Q_PROPERTY(pmx::PmxDocumentController* controller READ controller CONSTANT)
    Q_PROPERTY(QAbstractTableModel* infoModel READ infoModel NOTIFY modelsChanged)
    Q_PROPERTY(QAbstractTableModel* materialModel READ materialModel NOTIFY modelsChanged)
    Q_PROPERTY(QAbstractTableModel* boneModel READ boneModel NOTIFY modelsChanged)
    Q_PROPERTY(bool isModified READ isModified NOTIFY stateChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoRedoChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoRedoChanged)
    Q_PROPERTY(QString currentFilePath READ currentFilePath NOTIFY filePathChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QString displayLanguage READ displayLanguage NOTIFY languageChanged)

public:
    explicit PmxAppBridge(QObject* parent = nullptr);
    ~PmxAppBridge() override = default;

    pmx::PmxDocumentController* controller() { return &m_controller; }
    const pmx::PmxDocumentController* controller() const { return &m_controller; }

    QAbstractTableModel* infoModel() const;
    QAbstractTableModel* materialModel() const;
    QAbstractTableModel* boneModel() const;

    bool isModified() const { return m_controller.isModified(); }
    bool canUndo() const { return m_controller.undoStack() && m_controller.undoStack()->canUndo(); }
    bool canRedo() const { return m_controller.undoStack() && m_controller.undoStack()->canRedo(); }

    QString currentFilePath() const { return m_currentFilePath; }
    QString statusMessage() const { return m_statusMessage; }
    QString displayLanguage() const;

    Q_INVOKABLE bool openFile(const QString& filePath);
    Q_INVOKABLE bool saveFile(const QString& filePath = QString());
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void toggleLanguage();

signals:
    void modelsChanged();
    void stateChanged();
    void undoRedoChanged();
    void filePathChanged();
    void statusMessageChanged();
    void languageChanged();

private:
    pmx::PmxDocumentController m_controller;
    QString m_currentFilePath;
    QString m_statusMessage = QStringLiteral("Ready");
};

} // namespace pmx
