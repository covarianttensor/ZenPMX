#pragma once

#include "core/PmxDocument.h"
#include "models/PmxDocumentController.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <memory>

class QLocalServer;
class QLocalSocket;

namespace pmx {

class PmxMcpServer : public QObject {
    Q_OBJECT

public:
    explicit PmxMcpServer(PmxDocumentController* controller = nullptr, QObject* parent = nullptr);
    ~PmxMcpServer() override;

    // Document controller management
    void setController(PmxDocumentController* controller);
    PmxDocumentController* controller() const;

    // Core in-memory MCP JSON-RPC 2.0 message processor
    QByteArray processMessage(const QByteArray& requestJson);

    // Live GUI Socket Server Mode
    bool startLocalServer(const QString& socketPath = QString());
    void stopLocalServer();
    bool isServerListening() const;
    QString serverSocketPath() const;

    // Live GUI Socket Client Mode
    bool connectToGui(const QString& socketPath = QString(), int timeoutMs = 1000);
    void disconnectFromGui();
    bool isConnectedToGui() const;

    // Standard socket path ($XDG_RUNTIME_DIR/pmx-editor-mcp.sock)
    static QString defaultSocketPath();

signals:
    void messageProcessed(const QByteArray& request, const QByteArray& response);

private slots:
    void onNewLocalConnection();

private:
    QJsonObject handleRequest(const QJsonObject& reqObj);
    QJsonObject handleInitialize(const QJsonObject& params);
    QJsonObject handleToolsList();
    QJsonObject handleToolsCall(const QJsonObject& params, int* outErrorCode, QString* outErrorMsg);
    QJsonObject handleGetModelSummary(const QJsonObject& args, int* outErrorCode, QString* outErrorMsg);
    QJsonObject handleRenameBone(const QJsonObject& args, int* outErrorCode, QString* outErrorMsg);

    static QByteArray makeErrorResponse(const QJsonValue& id, int code, const QString& message, const QJsonValue& data = QJsonValue());
    static QByteArray makeSuccessResponse(const QJsonValue& id, const QJsonObject& result);

    PmxDocumentController* m_controller = nullptr;
    std::unique_ptr<PmxDocumentController> m_ownedController;

    QLocalServer* m_localServer = nullptr;
    QString m_serverSocketPath;

    QLocalSocket* m_guiClientSocket = nullptr;
    bool m_connectedToGui = false;
};

} // namespace pmx
