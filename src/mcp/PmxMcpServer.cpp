#include "mcp/PmxMcpServer.h"

#include "models/PmxBoneTableModel.h"
#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPointer>
#include <cmath>

namespace pmx {

PmxMcpServer::PmxMcpServer(PmxDocumentController* controller, QObject* parent)
    : QObject(parent)
{
    setController(controller);
}

PmxMcpServer::~PmxMcpServer() {
    stopLocalServer();
    disconnectFromGui();
}

void PmxMcpServer::setController(PmxDocumentController* controller) {
    if (controller) {
        m_ownedController.reset();
        m_controller = controller;
    } else {
        m_ownedController = std::make_unique<PmxDocumentController>();
        m_controller = m_ownedController.get();
    }
}

PmxDocumentController* PmxMcpServer::controller() const {
    return m_controller;
}

QString PmxMcpServer::defaultSocketPath() {
    QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (runtimeDir.isEmpty()) {
        runtimeDir = QDir::tempPath();
    }
    return QDir(runtimeDir).filePath(QStringLiteral("pmx-editor-mcp.sock"));
}

bool PmxMcpServer::startLocalServer(const QString& socketPath) {
    stopLocalServer();

    QString path = socketPath.isEmpty() ? defaultSocketPath() : socketPath;
    m_serverSocketPath = path;

    // Remove any stale socket file left from previous process
    QLocalServer::removeServer(path);

    m_localServer = new QLocalServer(this);
    if (!m_localServer->listen(path)) {
        delete m_localServer;
        m_localServer = nullptr;
        return false;
    }

    connect(m_localServer, &QLocalServer::newConnection, this, &PmxMcpServer::onNewLocalConnection);
    return true;
}

void PmxMcpServer::stopLocalServer() {
    if (m_localServer) {
        m_localServer->close();
        delete m_localServer;
        m_localServer = nullptr;
        if (!m_serverSocketPath.isEmpty()) {
            QLocalServer::removeServer(m_serverSocketPath);
        }
    }
}

bool PmxMcpServer::isServerListening() const {
    return m_localServer && m_localServer->isListening();
}

QString PmxMcpServer::serverSocketPath() const {
    return m_serverSocketPath;
}

bool PmxMcpServer::connectToGui(const QString& socketPath, int timeoutMs) {
    disconnectFromGui();

    QString path = socketPath.isEmpty() ? defaultSocketPath() : socketPath;
    m_guiClientSocket = new QLocalSocket(this);
    m_guiClientSocket->connectToServer(path);
    if (!m_guiClientSocket->waitForConnected(timeoutMs)) {
        delete m_guiClientSocket;
        m_guiClientSocket = nullptr;
        m_connectedToGui = false;
        return false;
    }

    m_connectedToGui = true;
    return true;
}

void PmxMcpServer::disconnectFromGui() {
    if (m_guiClientSocket) {
        m_guiClientSocket->disconnectFromServer();
        delete m_guiClientSocket;
        m_guiClientSocket = nullptr;
    }
    m_connectedToGui = false;
}

bool PmxMcpServer::isConnectedToGui() const {
    return m_connectedToGui && m_guiClientSocket && m_guiClientSocket->state() == QLocalSocket::ConnectedState;
}

void PmxMcpServer::onNewLocalConnection() {
    while (m_localServer && m_localServer->hasPendingConnections()) {
        QLocalSocket* socket = m_localServer->nextPendingConnection();
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            while (socket->canReadLine()) {
                QByteArray line = socket->readLine().trimmed();
                if (line.isEmpty()) {
                    continue;
                }
                QByteArray response = processMessage(line);
                if (!response.isEmpty()) {
                    socket->write(response + '\n');
                    socket->flush();
                }
            }
        });
        connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
    }
}

QByteArray PmxMcpServer::processMessage(const QByteArray& requestJson) {
    if (requestJson.trimmed().isEmpty()) {
        return QByteArray();
    }

    // Live GUI client forwarding mode
    if (m_connectedToGui && m_guiClientSocket && m_guiClientSocket->state() == QLocalSocket::ConnectedState) {
        QByteArray trimmed = requestJson.trimmed();
        m_guiClientSocket->write(trimmed + '\n');
        if (!m_guiClientSocket->waitForBytesWritten(1000)) {
            return makeErrorResponse(QJsonValue(), -32603, QStringLiteral("Timeout sending message to GUI"));
        }
        if (!m_guiClientSocket->waitForReadyRead(3000)) {
            return makeErrorResponse(QJsonValue(), -32603, QStringLiteral("Timeout waiting for response from GUI"));
        }
        QByteArray respLine = m_guiClientSocket->readLine().trimmed();
        emit messageProcessed(requestJson, respLine);
        return respLine;
    }

    // Direct local in-memory processing
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(requestJson, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return makeErrorResponse(QJsonValue(), -32700, QStringLiteral("Parse error"));
    }

    QJsonObject reqObj = doc.object();
    QJsonObject respObj = handleRequest(reqObj);
    if (respObj.isEmpty()) {
        return QByteArray();
    }

    QByteArray respData = QJsonDocument(respObj).toJson(QJsonDocument::Compact);
    emit messageProcessed(requestJson, respData);
    return respData;
}

QJsonObject PmxMcpServer::handleRequest(const QJsonObject& reqObj) {
    if (!reqObj.contains(QStringLiteral("jsonrpc")) || reqObj[QStringLiteral("jsonrpc")].toString() != QStringLiteral("2.0")) {
        QJsonObject err;
        err[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        err[QStringLiteral("id")] = reqObj.value(QStringLiteral("id"));
        QJsonObject errorObj;
        errorObj[QStringLiteral("code")] = -32600;
        errorObj[QStringLiteral("message")] = QStringLiteral("Invalid Request: jsonrpc must be '2.0'");
        err[QStringLiteral("error")] = errorObj;
        return err;
    }

    bool hasId = reqObj.contains(QStringLiteral("id"));
    QJsonValue idVal = reqObj.value(QStringLiteral("id"));

    if (!reqObj.contains(QStringLiteral("method")) || !reqObj[QStringLiteral("method")].isString()) {
        QJsonObject err;
        err[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        err[QStringLiteral("id")] = idVal;
        QJsonObject errorObj;
        errorObj[QStringLiteral("code")] = -32600;
        errorObj[QStringLiteral("message")] = QStringLiteral("Invalid Request: method is missing or not a string");
        err[QStringLiteral("error")] = errorObj;
        return err;
    }

    QString method = reqObj[QStringLiteral("method")].toString();

    // Standard MCP notifications
    if (method == QStringLiteral("notifications/initialized") || method == QStringLiteral("$/cancelRequest")) {
        if (!hasId) {
            return QJsonObject(); // No response for notifications without ID
        }
    }

    QJsonObject resp;
    resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    resp[QStringLiteral("id")] = idVal;

    QJsonObject params;
    if (reqObj.contains(QStringLiteral("params")) && reqObj[QStringLiteral("params")].isObject()) {
        params = reqObj[QStringLiteral("params")].toObject();
    }

    if (method == QStringLiteral("initialize")) {
        resp[QStringLiteral("result")] = handleInitialize(params);
        return resp;
    } else if (method == QStringLiteral("ping")) {
        resp[QStringLiteral("result")] = QJsonObject();
        return resp;
    } else if (method == QStringLiteral("tools/list")) {
        resp[QStringLiteral("result")] = handleToolsList();
        return resp;
    } else if (method == QStringLiteral("tools/call")) {
        int errorCode = 0;
        QString errorMsg;
        QJsonObject result = handleToolsCall(params, &errorCode, &errorMsg);
        if (errorCode != 0) {
            QJsonObject errResp;
            errResp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
            errResp[QStringLiteral("id")] = idVal;
            QJsonObject errorObj;
            errorObj[QStringLiteral("code")] = errorCode;
            errorObj[QStringLiteral("message")] = errorMsg;
            errResp[QStringLiteral("error")] = errorObj;
            return errResp;
        }
        resp[QStringLiteral("result")] = result;
        return resp;
    } else {
        // Unknown method
        QJsonObject errResp;
        errResp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        errResp[QStringLiteral("id")] = idVal;
        QJsonObject errorObj;
        errorObj[QStringLiteral("code")] = -32601;
        errorObj[QStringLiteral("message")] = QStringLiteral("Method not found: ") + method;
        errResp[QStringLiteral("error")] = errorObj;
        return errResp;
    }
}

QJsonObject PmxMcpServer::handleInitialize(const QJsonObject& /*params*/) {
    QJsonObject result;
    result[QStringLiteral("protocolVersion")] = QStringLiteral("2024-11-05");

    QJsonObject capabilities;
    QJsonObject toolsCap;
    toolsCap[QStringLiteral("listChanged")] = false;
    capabilities[QStringLiteral("tools")] = toolsCap;
    result[QStringLiteral("capabilities")] = capabilities;

    QJsonObject serverInfo;
    serverInfo[QStringLiteral("name")] = QStringLiteral("pmx-mcp");
    serverInfo[QStringLiteral("version")] = QStringLiteral("1.0.0");
    result[QStringLiteral("serverInfo")] = serverInfo;

    return result;
}

QJsonObject PmxMcpServer::handleToolsList() {
    QJsonObject result;
    QJsonArray toolsArray;

    // Tool 1: get_model_summary
    {
        QJsonObject tool;
        tool[QStringLiteral("name")] = QStringLiteral("get_model_summary");
        tool[QStringLiteral("description")] = QStringLiteral("Returns model summary with names, comments, version, encodings, and table counts.");

        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        QJsonObject props;
        QJsonObject filePathProp;
        filePathProp[QStringLiteral("type")] = QStringLiteral("string");
        filePathProp[QStringLiteral("description")] = QStringLiteral("Optional file path of PMX model to load and inspect.");
        props[QStringLiteral("filePath")] = filePathProp;
        schema[QStringLiteral("properties")] = props;

        tool[QStringLiteral("inputSchema")] = schema;
        toolsArray.append(tool);
    }

    // Tool 2: rename_bone
    {
        QJsonObject tool;
        tool[QStringLiteral("name")] = QStringLiteral("rename_bone");
        tool[QStringLiteral("description")] = QStringLiteral("Renames a bone by index and pushes the edit to QUndoStack via PmxPropertyEditCommand.");

        QJsonObject schema;
        schema[QStringLiteral("type")] = QStringLiteral("object");
        QJsonObject props;

        QJsonObject boneIndexProp;
        boneIndexProp[QStringLiteral("type")] = QStringLiteral("integer");
        boneIndexProp[QStringLiteral("description")] = QStringLiteral("0-based index of the bone to rename.");
        props[QStringLiteral("boneIndex")] = boneIndexProp;

        QJsonObject nameJpProp;
        nameJpProp[QStringLiteral("type")] = QStringLiteral("string");
        nameJpProp[QStringLiteral("description")] = QStringLiteral("New Japanese name for the bone.");
        props[QStringLiteral("nameJp")] = nameJpProp;

        QJsonObject nameEnProp;
        nameEnProp[QStringLiteral("type")] = QStringLiteral("string");
        nameEnProp[QStringLiteral("description")] = QStringLiteral("New English name for the bone.");
        props[QStringLiteral("nameEn")] = nameEnProp;

        schema[QStringLiteral("properties")] = props;

        QJsonArray required;
        required.append(QStringLiteral("boneIndex"));
        schema[QStringLiteral("required")] = required;

        tool[QStringLiteral("inputSchema")] = schema;
        toolsArray.append(tool);
    }

    result[QStringLiteral("tools")] = toolsArray;
    return result;
}

QJsonObject PmxMcpServer::handleToolsCall(const QJsonObject& params, int* outErrorCode, QString* outErrorMsg) {
    if (!params.contains(QStringLiteral("name")) || !params[QStringLiteral("name")].isString()) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Missing required parameter: tool name ('name')");
        return QJsonObject();
    }

    QString toolName = params[QStringLiteral("name")].toString();
    QJsonObject args;
    if (params.contains(QStringLiteral("arguments"))) {
        if (params[QStringLiteral("arguments")].isObject()) {
            args = params[QStringLiteral("arguments")].toObject();
        } else if (params[QStringLiteral("arguments")].isNull()) {
            args = QJsonObject();
        } else {
            *outErrorCode = -32602;
            *outErrorMsg = QStringLiteral("Parameter 'arguments' must be a JSON object");
            return QJsonObject();
        }
    } else {
        args = params;
        args.remove(QStringLiteral("name"));
    }

    if (toolName == QStringLiteral("get_model_summary")) {
        return handleGetModelSummary(args, outErrorCode, outErrorMsg);
    } else if (toolName == QStringLiteral("rename_bone")) {
        return handleRenameBone(args, outErrorCode, outErrorMsg);
    } else {
        *outErrorCode = -32601;
        *outErrorMsg = QStringLiteral("Unknown tool: ") + toolName;
        return QJsonObject();
    }
}

QJsonObject PmxMcpServer::handleGetModelSummary(const QJsonObject& args, int* outErrorCode, QString* outErrorMsg) {
    if (args.contains(QStringLiteral("filePath")) && !args[QStringLiteral("filePath")].isNull()) {
        QString path = args[QStringLiteral("filePath")].toString();
        if (!path.isEmpty()) {
            QString loadErr;
            if (!m_controller->loadFromFile(path, &loadErr)) {
                *outErrorCode = -32602;
                *outErrorMsg = QStringLiteral("Failed to load model file '%1': %2").arg(path, loadErr);
                return QJsonObject();
            }
        }
    }

    const auto& doc = m_controller->document();
    int vertexCount = static_cast<int>(doc.vertices.size());
    int faceCount = static_cast<int>(doc.faceIndices.size() / 3);
    int textureCount = static_cast<int>(doc.textures.size());
    int materialCount = static_cast<int>(doc.materials.size());
    int boneCount = static_cast<int>(doc.bones.size());
    int morphCount = static_cast<int>(doc.morphs.size());
    int frameCount = static_cast<int>(doc.displayFrames.size());
    int rigidBodyCount = static_cast<int>(doc.rigidBodies.size());
    int jointCount = static_cast<int>(doc.joints.size());
    int softBodyCount = static_cast<int>(doc.softBodies.size());

    QJsonObject summary;
    summary[QStringLiteral("modelNameJp")] = doc.modelInfo.modelNameJp;
    summary[QStringLiteral("modelNameEn")] = doc.modelInfo.modelNameEn;
    summary[QStringLiteral("nameJp")] = doc.modelInfo.modelNameJp;
    summary[QStringLiteral("nameEn")] = doc.modelInfo.modelNameEn;
    summary[QStringLiteral("commentJp")] = doc.modelInfo.commentJp;
    summary[QStringLiteral("commentEn")] = doc.modelInfo.commentEn;
    summary[QStringLiteral("comment")] = doc.modelInfo.commentJp;
    summary[QStringLiteral("version")] = std::round(static_cast<double>(doc.header.version) * 100.0) / 100.0;
    summary[QStringLiteral("textEncoding")] = doc.header.textEncoding == 0 ? QStringLiteral("UTF-16LE") : QStringLiteral("UTF-8");
    summary[QStringLiteral("encoding")] = doc.header.textEncoding == 0 ? QStringLiteral("UTF-16LE") : QStringLiteral("UTF-8");
    summary[QStringLiteral("textEncodingValue")] = doc.header.textEncoding;
    summary[QStringLiteral("vertexCount")] = vertexCount;
    summary[QStringLiteral("faceCount")] = faceCount;
    summary[QStringLiteral("faceVertexCount")] = static_cast<int>(doc.faceIndices.size());
    summary[QStringLiteral("textureCount")] = textureCount;
    summary[QStringLiteral("materialCount")] = materialCount;
    summary[QStringLiteral("boneCount")] = boneCount;
    summary[QStringLiteral("morphCount")] = morphCount;
    summary[QStringLiteral("displayFrameCount")] = frameCount;
    summary[QStringLiteral("rigidBodyCount")] = rigidBodyCount;
    summary[QStringLiteral("jointCount")] = jointCount;
    summary[QStringLiteral("softBodyCount")] = softBodyCount;

    // Plural aliases
    summary[QStringLiteral("vertices")] = vertexCount;
    summary[QStringLiteral("faces")] = faceCount;
    summary[QStringLiteral("textures")] = textureCount;
    summary[QStringLiteral("materials")] = materialCount;
    summary[QStringLiteral("bones")] = boneCount;
    summary[QStringLiteral("morphs")] = morphCount;
    summary[QStringLiteral("displayFrames")] = frameCount;
    summary[QStringLiteral("rigidBodies")] = rigidBodyCount;
    summary[QStringLiteral("joints")] = jointCount;
    summary[QStringLiteral("softBodies")] = softBodyCount;

    QJsonObject result = summary;
    QJsonArray contentArr;
    QJsonObject textObj;
    textObj[QStringLiteral("type")] = QStringLiteral("text");
    textObj[QStringLiteral("text")] = QString::fromUtf8(QJsonDocument(summary).toJson(QJsonDocument::Indented));
    contentArr.append(textObj);

    result[QStringLiteral("content")] = contentArr;
    result[QStringLiteral("isError")] = false;
    return result;
}

QJsonObject PmxMcpServer::handleRenameBone(const QJsonObject& args, int* outErrorCode, QString* outErrorMsg) {
    if (!args.contains(QStringLiteral("boneIndex"))) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Missing required parameter: boneIndex");
        return QJsonObject();
    }

    QJsonValue boneIndexVal = args.value(QStringLiteral("boneIndex"));
    if (!boneIndexVal.isDouble()) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Parameter 'boneIndex' must be an integer");
        return QJsonObject();
    }

    double boneIndexDbl = boneIndexVal.toDouble();
    if (std::isnan(boneIndexDbl) || std::isinf(boneIndexDbl) || boneIndexDbl != std::floor(boneIndexDbl)) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Parameter 'boneIndex' must be an integer (received %1)").arg(boneIndexDbl);
        return QJsonObject();
    }

    if (boneIndexDbl < 0.0) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Parameter 'boneIndex' must be non-negative (received %1)").arg(boneIndexDbl);
        return QJsonObject();
    }

    const auto& doc = m_controller->document();
    if (boneIndexDbl >= static_cast<double>(doc.bones.size())) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Parameter 'boneIndex' %1 out of range (model has %2 bones)")
                           .arg(static_cast<qint64>(boneIndexDbl))
                           .arg(doc.bones.size());
        return QJsonObject();
    }

    int boneIndex = static_cast<int>(boneIndexDbl);

    bool hasJp = args.contains(QStringLiteral("nameJp")) && !args[QStringLiteral("nameJp")].isNull();
    bool hasEn = args.contains(QStringLiteral("nameEn")) && !args[QStringLiteral("nameEn")].isNull();

    if (hasJp && !args[QStringLiteral("nameJp")].isString()) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Parameter 'nameJp' must be a string");
        return QJsonObject();
    }

    if (hasEn && !args[QStringLiteral("nameEn")].isString()) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Parameter 'nameEn' must be a string");
        return QJsonObject();
    }

    if (!hasJp && !hasEn) {
        *outErrorCode = -32602;
        *outErrorMsg = QStringLiteral("Missing required parameter: at least one of 'nameJp' or 'nameEn' must be provided");
        return QJsonObject();
    }

    const auto& currentBone = doc.bones[boneIndex];
    QString newNameJp = hasJp ? args[QStringLiteral("nameJp")].toString() : currentBone.nameJp;
    QString newNameEn = hasEn ? args[QStringLiteral("nameEn")].toString() : currentBone.nameEn;

    bool changeJp = hasJp && (newNameJp != currentBone.nameJp);
    bool changeEn = hasEn && (newNameEn != currentBone.nameEn);

    if (changeJp || changeEn) {
        bool useMacro = (changeJp && changeEn);
        if (useMacro) {
            m_controller->undoStack()->beginMacro(QStringLiteral("Rename Bone"));
        }

        QPointer<PmxDocumentController> ctrl = m_controller;

        if (changeJp) {
            QString oldVal = currentBone.nameJp;
            int cmdId = makeCommandId(3, boneIndex, PmxBoneTableModel::ColName);
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller,
                cmdId,
                oldVal,
                newNameJp,
                [boneIndex](PmxDocument& d, const QString& v) {
                    if (boneIndex >= 0 && static_cast<size_t>(boneIndex) < d.bones.size()) {
                        d.bones[boneIndex].nameJp = v;
                    }
                },
                [ctrl]() {
                    if (ctrl && ctrl->boneModel()) {
                        ctrl->boneModel()->resetModel();
                    }
                },
                QStringLiteral("Edit Bone Name (JP)")
            ));
        }

        if (changeEn) {
            QString oldVal = currentBone.nameEn;
            int cmdId = makeCommandId(3, boneIndex, PmxBoneTableModel::ColName + 50);
            m_controller->pushCommand(new PmxPropertyEditCommand<QString>(
                m_controller,
                cmdId,
                oldVal,
                newNameEn,
                [boneIndex](PmxDocument& d, const QString& v) {
                    if (boneIndex >= 0 && static_cast<size_t>(boneIndex) < d.bones.size()) {
                        d.bones[boneIndex].nameEn = v;
                    }
                },
                [ctrl]() {
                    if (ctrl && ctrl->boneModel()) {
                        ctrl->boneModel()->resetModel();
                    }
                },
                QStringLiteral("Edit Bone Name (EN)")
            ));
        }

        if (useMacro) {
            m_controller->undoStack()->endMacro();
        }
    }

    const auto& updatedBone = m_controller->document().bones[boneIndex];
    QJsonObject boneObj;
    boneObj[QStringLiteral("boneIndex")] = boneIndex;
    boneObj[QStringLiteral("nameJp")] = updatedBone.nameJp;
    boneObj[QStringLiteral("nameEn")] = updatedBone.nameEn;
    boneObj[QStringLiteral("parentBoneIndex")] = updatedBone.parentBoneIndex;
    boneObj[QStringLiteral("deformTier")] = updatedBone.deformTier;
    boneObj[QStringLiteral("boneFlags")] = static_cast<int>(updatedBone.boneFlags);

    QJsonObject posObj;
    posObj[QStringLiteral("x")] = static_cast<double>(updatedBone.position.x);
    posObj[QStringLiteral("y")] = static_cast<double>(updatedBone.position.y);
    posObj[QStringLiteral("z")] = static_cast<double>(updatedBone.position.z);
    boneObj[QStringLiteral("position")] = posObj;

    QJsonObject result = boneObj;
    result[QStringLiteral("bone")] = boneObj;

    QJsonArray contentArr;
    QJsonObject textObj;
    textObj[QStringLiteral("type")] = QStringLiteral("text");
    textObj[QStringLiteral("text")] = QStringLiteral("Bone %1 renamed successfully: JP='%2', EN='%3'")
                                         .arg(boneIndex)
                                         .arg(updatedBone.nameJp, updatedBone.nameEn);
    contentArr.append(textObj);

    result[QStringLiteral("content")] = contentArr;
    result[QStringLiteral("isError")] = false;

    return result;
}

QByteArray PmxMcpServer::makeErrorResponse(const QJsonValue& id, int code, const QString& message, const QJsonValue& data) {
    QJsonObject resp;
    resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    resp[QStringLiteral("id")] = id.isUndefined() ? QJsonValue(QJsonValue::Null) : id;

    QJsonObject err;
    err[QStringLiteral("code")] = code;
    err[QStringLiteral("message")] = message;
    if (!data.isUndefined() && !data.isNull()) {
        err[QStringLiteral("data")] = data;
    }
    resp[QStringLiteral("error")] = err;

    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

QByteArray PmxMcpServer::makeSuccessResponse(const QJsonValue& id, const QJsonObject& result) {
    QJsonObject resp;
    resp[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    resp[QStringLiteral("id")] = id.isUndefined() ? QJsonValue(QJsonValue::Null) : id;
    resp[QStringLiteral("result")] = result;

    return QJsonDocument(resp).toJson(QJsonDocument::Compact);
}

} // namespace pmx
