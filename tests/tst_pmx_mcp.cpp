#include "core/PmxDocument.h"
#include "core/PmxReader.h"
#include "core/PmxTypes.h"
#include "core/PmxWriter.h"
#include "mcp/PmxMcpServer.h"
#include "models/PmxDocumentController.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

using namespace pmx;

class TstPmxMcp : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testInitializeHandshake();
    void testToolsListSchema();
    void testGetModelSummaryPmx20();
    void testGetModelSummaryPmx21();
    void testRenameBoneViaUndoStack();
    void testUafLifetimeAfterServerDestruction();
    void testAdversarialMalformedJson();
    void testAdversarialUnknownMethod();
    void testAdversarialUnknownTool();
    void testAdversarialNegativeBoneIndex();
    void testAdversarialOutOfRangeBoneIndex();
    void testAdversarialMissingRequiredParams();
    void testAdversarialMissingJsonRpc();
    void testSocketCommunication();

private:
    PmxDocument createSamplePmx20();
    PmxDocument createSamplePmx21();

    QTemporaryDir m_tempDir;
    QString m_pmx20Path;
    QString m_pmx21Path;
};

void TstPmxMcp::initTestCase() {
    QVERIFY(m_tempDir.isValid());

    m_pmx20Path = m_tempDir.filePath(QStringLiteral("sample_20.pmx"));
    PmxDocument doc20 = createSamplePmx20();
    QString err;
    QVERIFY2(PmxWriter::writeToFile(m_pmx20Path, doc20, &err), qPrintable(err));

    m_pmx21Path = m_tempDir.filePath(QStringLiteral("sample_21.pmx"));
    PmxDocument doc21 = createSamplePmx21();
    QVERIFY2(PmxWriter::writeToFile(m_pmx21Path, doc21, &err), qPrintable(err));
}

void TstPmxMcp::cleanupTestCase() {
}

PmxDocument TstPmxMcp::createSamplePmx20() {
    PmxDocument doc;
    doc.header.version = 2.0f;
    doc.header.textEncoding = 0; // UTF-16LE
    doc.header.additionalVec4Count = 0;
    doc.header.vertexIndexSize = 1;
    doc.header.textureIndexSize = 1;
    doc.header.materialIndexSize = 1;
    doc.header.boneIndexSize = 1;
    doc.header.morphIndexSize = 1;
    doc.header.rigidBodyIndexSize = 1;

    doc.modelInfo.modelNameJp = QStringLiteral("PMX20モデル");
    doc.modelInfo.modelNameEn = QStringLiteral("PMX20 Model");
    doc.modelInfo.commentJp = QStringLiteral("PMX20 コメント");
    doc.modelInfo.commentEn = QStringLiteral("PMX20 Comment");

    for (int i = 0; i < 3; ++i) {
        PmxVertex v;
        v.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.uv = glm::vec2(0.0f, 0.0f);
        v.weightType = WeightType::BDEF1;
        v.boneIndices[0] = 0;
        v.boneWeights[0] = 1.0f;
        doc.vertices.push_back(v);
        doc.faceIndices.push_back(i);
    }

    doc.textures.push_back(QStringLiteral("tex1.png"));

    PmxMaterial mat;
    mat.nameJp = QStringLiteral("材質1");
    mat.nameEn = QStringLiteral("Mat1");
    mat.diffuse = glm::vec4(1.0f);
    mat.specular = glm::vec3(0.0f);
    mat.ambient = glm::vec3(0.5f);
    mat.drawFlags = 0x1F;
    mat.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.edgeSize = 1.0f;
    mat.textureIndex = 0;
    mat.faceVertexCount = 3;
    doc.materials.push_back(mat);

    PmxBone b0;
    b0.nameJp = QStringLiteral("全ての親");
    b0.nameEn = QStringLiteral("Root");
    b0.position = glm::vec3(0.0f, 0.0f, 0.0f);
    b0.parentBoneIndex = -1;
    doc.bones.push_back(b0);

    PmxBone b1;
    b1.nameJp = QStringLiteral("センター");
    b1.nameEn = QStringLiteral("Center");
    b1.position = glm::vec3(0.0f, 10.0f, 0.0f);
    b1.parentBoneIndex = 0;
    doc.bones.push_back(b1);

    PmxMorph m;
    m.nameJp = QStringLiteral("モーフ1");
    m.nameEn = QStringLiteral("Morph1");
    m.panelCategory = 1;
    m.morphType = 1;
    VertexMorphOffset vmo;
    vmo.vertexIndex = 0;
    vmo.positionOffset = glm::vec3(1.0f, 0.0f, 0.0f);
    m.vertexOffsets.push_back(vmo);
    doc.morphs.push_back(m);

    PmxDisplayFrame df;
    df.nameJp = QStringLiteral("RootFrame");
    df.nameEn = QStringLiteral("Root Frame");
    df.specialFlag = 1;
    PmxDisplayElement elem;
    elem.targetType = 0;
    elem.index = 0;
    df.elements.push_back(elem);
    doc.displayFrames.push_back(df);

    PmxRigidBody rb;
    rb.nameJp = QStringLiteral("剛体1");
    rb.nameEn = QStringLiteral("Body1");
    rb.boneIndex = 0;
    rb.shape = 0;
    doc.rigidBodies.push_back(rb);

    PmxJoint j;
    j.nameJp = QStringLiteral("ジョイント1");
    j.nameEn = QStringLiteral("Joint1");
    j.rigidBodyIndexA = 0;
    j.rigidBodyIndexB = -1;
    doc.joints.push_back(j);

    return doc;
}

PmxDocument TstPmxMcp::createSamplePmx21() {
    PmxDocument doc;
    doc.header.version = 2.1f;
    doc.header.textEncoding = 1; // UTF-8
    doc.header.additionalVec4Count = 0;
    doc.header.vertexIndexSize = 1;
    doc.header.textureIndexSize = 1;
    doc.header.materialIndexSize = 1;
    doc.header.boneIndexSize = 1;
    doc.header.morphIndexSize = 1;
    doc.header.rigidBodyIndexSize = 1;

    doc.modelInfo.modelNameJp = QStringLiteral("PMX21モデル");
    doc.modelInfo.modelNameEn = QStringLiteral("PMX21 Model");
    doc.modelInfo.commentJp = QStringLiteral("PMX21 コメント");
    doc.modelInfo.commentEn = QStringLiteral("PMX21 Comment");

    for (int i = 0; i < 3; ++i) {
        PmxVertex v;
        v.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.uv = glm::vec2(0.0f, 0.0f);
        v.weightType = WeightType::BDEF1;
        v.boneIndices[0] = 0;
        v.boneWeights[0] = 1.0f;
        doc.vertices.push_back(v);
        doc.faceIndices.push_back(i);
    }

    PmxMaterial mat;
    mat.nameJp = QStringLiteral("材質21");
    mat.nameEn = QStringLiteral("Mat21");
    mat.faceVertexCount = 3;
    doc.materials.push_back(mat);

    PmxBone b0;
    b0.nameJp = QStringLiteral("ボーン0");
    b0.nameEn = QStringLiteral("Bone0");
    doc.bones.push_back(b0);

    PmxSoftBody sb;
    sb.nameJp = QStringLiteral("ソフトボディ1");
    sb.nameEn = QStringLiteral("SoftBody1");
    sb.shape = 0;
    sb.materialIndex = 0;
    doc.softBodies.push_back(sb);

    return doc;
}

void TstPmxMcp::testInitializeHandshake() {
    PmxMcpServer server;
    QByteArray request = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": { "name": "test-client", "version": "1.0.0" }
        }
    })";

    QByteArray response = server.processMessage(request);
    QVERIFY(!response.isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(response);
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();

    QCOMPARE(obj[QStringLiteral("jsonrpc")].toString(), QStringLiteral("2.0"));
    QCOMPARE(obj[QStringLiteral("id")].toInt(), 1);

    QJsonObject res = obj[QStringLiteral("result")].toObject();
    QCOMPARE(res[QStringLiteral("protocolVersion")].toString(), QStringLiteral("2024-11-05"));

    QJsonObject serverInfo = res[QStringLiteral("serverInfo")].toObject();
    QCOMPARE(serverInfo[QStringLiteral("name")].toString(), QStringLiteral("pmx-mcp"));
    QCOMPARE(serverInfo[QStringLiteral("version")].toString(), QStringLiteral("1.0.0"));

    QVERIFY(res.contains(QStringLiteral("capabilities")));
    QVERIFY(res[QStringLiteral("capabilities")].toObject().contains(QStringLiteral("tools")));
}

void TstPmxMcp::testToolsListSchema() {
    PmxMcpServer server;
    QByteArray request = R"({
        "jsonrpc": "2.0",
        "id": 2,
        "method": "tools/list",
        "params": {}
    })";

    QByteArray response = server.processMessage(request);
    QVERIFY(!response.isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(response);
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();

    QCOMPARE(obj[QStringLiteral("jsonrpc")].toString(), QStringLiteral("2.0"));
    QCOMPARE(obj[QStringLiteral("id")].toInt(), 2);

    QJsonObject res = obj[QStringLiteral("result")].toObject();
    QVERIFY(res.contains(QStringLiteral("tools")));
    QJsonArray tools = res[QStringLiteral("tools")].toArray();
    QVERIFY(tools.size() >= 2);

    bool foundSummary = false;
    bool foundRename = false;

    for (const auto& item : tools) {
        QJsonObject t = item.toObject();
        QString name = t[QStringLiteral("name")].toString();
        QVERIFY(!name.isEmpty());
        QVERIFY(!t[QStringLiteral("description")].toString().isEmpty());
        QVERIFY(t[QStringLiteral("inputSchema")].isObject());
        QJsonObject schema = t[QStringLiteral("inputSchema")].toObject();
        QCOMPARE(schema[QStringLiteral("type")].toString(), QStringLiteral("object"));

        if (name == QStringLiteral("get_model_summary")) {
            foundSummary = true;
            QJsonObject props = schema[QStringLiteral("properties")].toObject();
            QVERIFY(props.contains(QStringLiteral("filePath")));
        } else if (name == QStringLiteral("rename_bone")) {
            foundRename = true;
            QJsonObject props = schema[QStringLiteral("properties")].toObject();
            QVERIFY(props.contains(QStringLiteral("boneIndex")));
            QVERIFY(props.contains(QStringLiteral("nameJp")));
            QVERIFY(props.contains(QStringLiteral("nameEn")));
            QJsonArray req = schema[QStringLiteral("required")].toArray();
            QVERIFY(req.contains(QStringLiteral("boneIndex")));
        }
    }

    QVERIFY(foundSummary);
    QVERIFY(foundRename);
}

void TstPmxMcp::testGetModelSummaryPmx20() {
    PmxDocumentController controller;
    PmxMcpServer server(&controller);

    // 1. Call via filePath argument
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 3;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("get_model_summary");
        QJsonObject args;
        args[QStringLiteral("filePath")] = m_pmx20Path;
        params[QStringLiteral("arguments")] = args;
        req[QStringLiteral("params")] = params;

        QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QJsonDocument doc = QJsonDocument::fromJson(resp);
        QVERIFY(doc.isObject());
        QJsonObject obj = doc.object();
        QVERIFY(!obj.contains(QStringLiteral("error")));
        QJsonObject res = obj[QStringLiteral("result")].toObject();

        QCOMPARE(res[QStringLiteral("modelNameJp")].toString(), QStringLiteral("PMX20モデル"));
        QCOMPARE(res[QStringLiteral("modelNameEn")].toString(), QStringLiteral("PMX20 Model"));
        QCOMPARE(res[QStringLiteral("commentJp")].toString(), QStringLiteral("PMX20 コメント"));
        QCOMPARE(res[QStringLiteral("commentEn")].toString(), QStringLiteral("PMX20 Comment"));
        QCOMPARE(res[QStringLiteral("version")].toDouble(), 2.0);
        QCOMPARE(res[QStringLiteral("textEncoding")].toString(), QStringLiteral("UTF-16LE"));
        QCOMPARE(res[QStringLiteral("vertexCount")].toInt(), 3);
        QCOMPARE(res[QStringLiteral("faceCount")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("textureCount")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("materialCount")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("boneCount")].toInt(), 2);
        QCOMPARE(res[QStringLiteral("morphCount")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("displayFrameCount")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("rigidBodyCount")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("jointCount")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("softBodyCount")].toInt(), 0);

        // Verify MCP content array
        QVERIFY(res.contains(QStringLiteral("content")));
        QJsonArray contentArr = res[QStringLiteral("content")].toArray();
        QVERIFY(!contentArr.isEmpty());
        QCOMPARE(contentArr[0].toObject()[QStringLiteral("type")].toString(), QStringLiteral("text"));
    }

    // 2. Call with empty arguments against the already loaded model
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 4;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("get_model_summary");
        req[QStringLiteral("params")] = params;

        QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QJsonDocument doc = QJsonDocument::fromJson(resp);
        QJsonObject obj = doc.object();
        QJsonObject res = obj[QStringLiteral("result")].toObject();

        QCOMPARE(res[QStringLiteral("modelNameJp")].toString(), QStringLiteral("PMX20モデル"));
        QCOMPARE(res[QStringLiteral("boneCount")].toInt(), 2);
    }
}

void TstPmxMcp::testGetModelSummaryPmx21() {
    PmxDocumentController controller;
    PmxMcpServer server(&controller);

    QJsonObject req;
    req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    req[QStringLiteral("id")] = 5;
    req[QStringLiteral("method")] = QStringLiteral("tools/call");
    QJsonObject params;
    params[QStringLiteral("name")] = QStringLiteral("get_model_summary");
    QJsonObject args;
    args[QStringLiteral("filePath")] = m_pmx21Path;
    params[QStringLiteral("arguments")] = args;
    req[QStringLiteral("params")] = params;

    QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    QJsonObject obj = doc.object();
    QVERIFY(!obj.contains(QStringLiteral("error")));
    QJsonObject res = obj[QStringLiteral("result")].toObject();

    QCOMPARE(res[QStringLiteral("modelNameJp")].toString(), QStringLiteral("PMX21モデル"));
    QCOMPARE(res[QStringLiteral("version")].toDouble(), 2.1);
    QCOMPARE(res[QStringLiteral("textEncoding")].toString(), QStringLiteral("UTF-8"));
    QCOMPARE(res[QStringLiteral("softBodyCount")].toInt(), 1);
}

void TstPmxMcp::testRenameBoneViaUndoStack() {
    PmxDocumentController controller;
    PmxDocument doc = createSamplePmx20();
    controller.setDocumentInternal(doc);
    controller.undoStack()->setClean();

    PmxMcpServer server(&controller);

    const QString origNameJp = controller.document().bones[1].nameJp; // "センター"
    const QString origNameEn = controller.document().bones[1].nameEn; // "Center"
    QCOMPARE(origNameJp, QStringLiteral("センター"));
    QCOMPARE(origNameEn, QStringLiteral("Center"));
    QVERIFY(!controller.isModified());

    // 1. Rename both JP and EN
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 6;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("rename_bone");
        QJsonObject args;
        args[QStringLiteral("boneIndex")] = 1;
        args[QStringLiteral("nameJp")] = QStringLiteral("センター改");
        args[QStringLiteral("nameEn")] = QStringLiteral("CenterNew");
        params[QStringLiteral("arguments")] = args;
        req[QStringLiteral("params")] = params;

        QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QJsonDocument resDoc = QJsonDocument::fromJson(resp);
        QJsonObject obj = resDoc.object();
        QVERIFY(!obj.contains(QStringLiteral("error")));
        QJsonObject res = obj[QStringLiteral("result")].toObject();

        QCOMPARE(res[QStringLiteral("boneIndex")].toInt(), 1);
        QCOMPARE(res[QStringLiteral("nameJp")].toString(), QStringLiteral("センター改"));
        QCOMPARE(res[QStringLiteral("nameEn")].toString(), QStringLiteral("CenterNew"));
        QCOMPARE(res[QStringLiteral("bone")].toObject()[QStringLiteral("nameJp")].toString(), QStringLiteral("センター改"));

        // Verify controller document was updated
        QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("センター改"));
        QCOMPARE(controller.document().bones[1].nameEn, QStringLiteral("CenterNew"));
        QVERIFY(controller.isModified());

        // 2. Undo: MUST restore previous bone names bit-identically
        controller.undoStack()->undo();
        QCOMPARE(controller.document().bones[1].nameJp, origNameJp);
        QCOMPARE(controller.document().bones[1].nameEn, origNameEn);
        QVERIFY(!controller.isModified());

        // 3. Redo
        controller.undoStack()->redo();
        QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("センター改"));
        QCOMPARE(controller.document().bones[1].nameEn, QStringLiteral("CenterNew"));
        QVERIFY(controller.isModified());
    }

    // 4. Rename only nameJp
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 7;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("rename_bone");
        QJsonObject args;
        args[QStringLiteral("boneIndex")] = 1;
        args[QStringLiteral("nameJp")] = QStringLiteral("センター再改");
        params[QStringLiteral("arguments")] = args;
        req[QStringLiteral("params")] = params;

        QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QJsonDocument resDoc = QJsonDocument::fromJson(resp);
        QJsonObject res = resDoc.object()[QStringLiteral("result")].toObject();

        QCOMPARE(res[QStringLiteral("nameJp")].toString(), QStringLiteral("センター再改"));
        QCOMPARE(res[QStringLiteral("nameEn")].toString(), QStringLiteral("CenterNew"));

        QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("センター再改"));
        QCOMPARE(controller.document().bones[1].nameEn, QStringLiteral("CenterNew"));

        controller.undoStack()->undo();
        QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("センター改"));
        QCOMPARE(controller.document().bones[1].nameEn, QStringLiteral("CenterNew"));
    }

    // 5. Rename only nameEn
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 8;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("rename_bone");
        QJsonObject args;
        args[QStringLiteral("boneIndex")] = 1;
        args[QStringLiteral("nameEn")] = QStringLiteral("CenterFinal");
        params[QStringLiteral("arguments")] = args;
        req[QStringLiteral("params")] = params;

        QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QJsonDocument resDoc = QJsonDocument::fromJson(resp);
        QJsonObject res = resDoc.object()[QStringLiteral("result")].toObject();

        QCOMPARE(res[QStringLiteral("nameJp")].toString(), QStringLiteral("センター改"));
        QCOMPARE(res[QStringLiteral("nameEn")].toString(), QStringLiteral("CenterFinal"));

        controller.undoStack()->undo();
        QCOMPARE(controller.document().bones[1].nameEn, QStringLiteral("CenterNew"));
    }
}

void TstPmxMcp::testUafLifetimeAfterServerDestruction() {
    PmxDocumentController controller;
    PmxDocument doc = createSamplePmx20();
    controller.setDocumentInternal(doc);
    controller.undoStack()->setClean();

    const QString origName = controller.document().bones[1].nameJp;

    {
        PmxMcpServer localServer(&controller);
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 200;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("rename_bone");
        QJsonObject args;
        args[QStringLiteral("boneIndex")] = 1;
        args[QStringLiteral("nameJp")] = QStringLiteral("サーバー破棄後テスト");
        params[QStringLiteral("arguments")] = args;
        req[QStringLiteral("params")] = params;

        QByteArray resp = localServer.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QVERIFY(!resp.isEmpty());
        // localServer goes out of scope and is destroyed here
    }

    QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("サーバー破棄後テスト"));

    // Invoke undo() on controller - must NOT crash or trigger UAF from destroyed localServer!
    controller.undoStack()->undo();
    QCOMPARE(controller.document().bones[1].nameJp, origName);

    controller.undoStack()->redo();
    QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("サーバー破棄後テスト"));

    controller.undoStack()->undo();
    QCOMPARE(controller.document().bones[1].nameJp, origName);
}

void TstPmxMcp::testAdversarialMalformedJson() {
    PmxMcpServer server;
    QByteArray malformed = "{ this is not json at all }}}";

    QByteArray resp = server.processMessage(malformed);
    QVERIFY(!resp.isEmpty());

    QJsonDocument doc = QJsonDocument::fromJson(resp);
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();

    QVERIFY(obj.contains(QStringLiteral("error")));
    QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32700);
}

void TstPmxMcp::testAdversarialUnknownMethod() {
    PmxMcpServer server;
    QByteArray req = R"({
        "jsonrpc": "2.0",
        "id": 99,
        "method": "foo/bar",
        "params": {}
    })";

    QByteArray resp = server.processMessage(req);
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    QJsonObject obj = doc.object();

    QVERIFY(obj.contains(QStringLiteral("error")));
    QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32601);
}

void TstPmxMcp::testAdversarialUnknownTool() {
    PmxMcpServer server;
    QByteArray req = R"({
        "jsonrpc": "2.0",
        "id": 100,
        "method": "tools/call",
        "params": {
            "name": "non_existent_tool_xyz",
            "arguments": {}
        }
    })";

    QByteArray resp = server.processMessage(req);
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    QJsonObject obj = doc.object();

    QVERIFY(obj.contains(QStringLiteral("error")));
    QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32601);
}

void TstPmxMcp::testAdversarialNegativeBoneIndex() {
    PmxDocumentController controller;
    PmxDocument doc = createSamplePmx20();
    controller.setDocumentInternal(doc);
    PmxMcpServer server(&controller);

    auto sendRenameWithIndex = [&](const QJsonValue& idxVal) -> int {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 101;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("rename_bone");
        QJsonObject args;
        args[QStringLiteral("boneIndex")] = idxVal;
        args[QStringLiteral("nameJp")] = QStringLiteral("不正インデックス");
        params[QStringLiteral("arguments")] = args;
        req[QStringLiteral("params")] = params;

        QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QJsonObject obj = QJsonDocument::fromJson(resp).object();
        return obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt();
    };

    // Negative integer
    QCOMPARE(sendRenameWithIndex(-5), -32602);
    // Negative fractional: -0.5 must NOT be truncated to 0
    QCOMPARE(sendRenameWithIndex(-0.5), -32602);
    // Positive fractional: 1.5 must be rejected
    QCOMPARE(sendRenameWithIndex(1.5), -32602);
}

void TstPmxMcp::testAdversarialOutOfRangeBoneIndex() {
    PmxDocumentController controller;
    PmxDocument doc = createSamplePmx20(); // Has 2 bones (indices 0 and 1)
    controller.setDocumentInternal(doc);
    PmxMcpServer server(&controller);

    QByteArray req = R"({
        "jsonrpc": "2.0",
        "id": 102,
        "method": "tools/call",
        "params": {
            "name": "rename_bone",
            "arguments": {
                "boneIndex": 999999,
                "nameJp": "存在しないボーン"
            }
        }
    })";

    QByteArray resp = server.processMessage(req);
    QJsonDocument docObj = QJsonDocument::fromJson(resp);
    QJsonObject obj = docObj.object();

    QVERIFY(obj.contains(QStringLiteral("error")));
    QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32602);
}

void TstPmxMcp::testAdversarialMissingRequiredParams() {
    PmxDocumentController controller;
    PmxDocument doc = createSamplePmx20();
    controller.setDocumentInternal(doc);
    PmxMcpServer server(&controller);

    // 1. rename_bone without boneIndex
    {
        QByteArray req = R"({
            "jsonrpc": "2.0",
            "id": 103,
            "method": "tools/call",
            "params": {
                "name": "rename_bone",
                "arguments": {
                    "nameJp": "名前のみ"
                }
            }
        })";
        QByteArray resp = server.processMessage(req);
        QJsonObject obj = QJsonDocument::fromJson(resp).object();
        QVERIFY(obj.contains(QStringLiteral("error")));
        QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32602);
    }

    // 2. rename_bone with boneIndex but neither nameJp nor nameEn
    {
        QByteArray req = R"({
            "jsonrpc": "2.0",
            "id": 104,
            "method": "tools/call",
            "params": {
                "name": "rename_bone",
                "arguments": {
                    "boneIndex": 0
                }
            }
        })";
        QByteArray resp = server.processMessage(req);
        QJsonObject obj = QJsonDocument::fromJson(resp).object();
        QVERIFY(obj.contains(QStringLiteral("error")));
        QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32602);
    }

    // 3. tools/call missing tool name
    {
        QByteArray req = R"({
            "jsonrpc": "2.0",
            "id": 105,
            "method": "tools/call",
            "params": {}
        })";
        QByteArray resp = server.processMessage(req);
        QJsonObject obj = QJsonDocument::fromJson(resp).object();
        QVERIFY(obj.contains(QStringLiteral("error")));
        QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32602);
    }

    // 4. get_model_summary with nonexistent file
    {
        QByteArray req = R"({
            "jsonrpc": "2.0",
            "id": 106,
            "method": "tools/call",
            "params": {
                "name": "get_model_summary",
                "arguments": {
                    "filePath": "/nonexistent_pmx_file_dir/does_not_exist.pmx"
                }
            }
        })";
        QByteArray resp = server.processMessage(req);
        QJsonObject obj = QJsonDocument::fromJson(resp).object();
        QVERIFY(obj.contains(QStringLiteral("error")));
        QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32602);
    }

    // 5. rename_bone with non-string nameJp (e.g. integer 123)
    {
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 107;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("rename_bone");
        QJsonObject args;
        args[QStringLiteral("boneIndex")] = 0;
        args[QStringLiteral("nameJp")] = 123;
        params[QStringLiteral("arguments")] = args;
        req[QStringLiteral("params")] = params;

        QByteArray resp = server.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));
        QJsonObject obj = QJsonDocument::fromJson(resp).object();
        QVERIFY(obj.contains(QStringLiteral("error")));
        QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32602);
    }
}

void TstPmxMcp::testAdversarialMissingJsonRpc() {
    PmxMcpServer server;

    // Missing jsonrpc field entirely
    QByteArray req = R"({
        "id": 150,
        "method": "initialize",
        "params": {}
    })";

    QByteArray resp = server.processMessage(req);
    QJsonObject obj = QJsonDocument::fromJson(resp).object();
    QVERIFY(obj.contains(QStringLiteral("error")));
    QCOMPARE(obj[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32600);

    // jsonrpc field is wrong version (not "2.0")
    QByteArray reqWrong = R"({
        "jsonrpc": "1.0",
        "id": 151,
        "method": "initialize",
        "params": {}
    })";
    QByteArray respWrong = server.processMessage(reqWrong);
    QJsonObject objWrong = QJsonDocument::fromJson(respWrong).object();
    QVERIFY(objWrong.contains(QStringLiteral("error")));
    QCOMPARE(objWrong[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(), -32600);
}

void TstPmxMcp::testSocketCommunication() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString sockPath = tempDir.filePath(QStringLiteral("test_mcp.sock"));

    PmxDocumentController serverController;
    PmxDocument doc = createSamplePmx20();
    serverController.setDocumentInternal(doc);

    PmxMcpServer server(&serverController);
    QVERIFY(server.startLocalServer(sockPath));
    QVERIFY(server.isServerListening());
    QCOMPARE(server.serverSocketPath(), sockPath);

    QByteArray responseBytes;
    QByteArray renameResponseBytes;
    bool clientSuccess = false;

    // Run client connection in a worker thread so server can process events in main thread
    QThread* thread = QThread::create([&]() {
        PmxMcpServer clientServer;
        if (!clientServer.connectToGui(sockPath, 3000)) {
            return;
        }
        if (!clientServer.isConnectedToGui()) {
            return;
        }

        // 1. get_model_summary over socket
        QJsonObject req;
        req[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        req[QStringLiteral("id")] = 42;
        req[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject params;
        params[QStringLiteral("name")] = QStringLiteral("get_model_summary");
        req[QStringLiteral("params")] = params;

        responseBytes = clientServer.processMessage(QJsonDocument(req).toJson(QJsonDocument::Compact));

        // 2. rename_bone over socket
        QJsonObject reqRename;
        reqRename[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
        reqRename[QStringLiteral("id")] = 43;
        reqRename[QStringLiteral("method")] = QStringLiteral("tools/call");
        QJsonObject paramsRename;
        paramsRename[QStringLiteral("name")] = QStringLiteral("rename_bone");
        QJsonObject argsRename;
        argsRename[QStringLiteral("boneIndex")] = 1;
        argsRename[QStringLiteral("nameJp")] = QStringLiteral("ソケット経由変更");
        argsRename[QStringLiteral("nameEn")] = QStringLiteral("SocketChanged");
        paramsRename[QStringLiteral("arguments")] = argsRename;
        reqRename[QStringLiteral("params")] = paramsRename;

        renameResponseBytes = clientServer.processMessage(QJsonDocument(reqRename).toJson(QJsonDocument::Compact));
        clientSuccess = true;
    });

    thread->start();

    int elapsed = 0;
    while (!thread->isFinished() && elapsed < 4000) {
        QCoreApplication::processEvents();
        QThread::msleep(10);
        elapsed += 10;
    }
    thread->wait();
    delete thread;

    QVERIFY(clientSuccess);
    QVERIFY(!responseBytes.isEmpty());
    QVERIFY(!renameResponseBytes.isEmpty());

    QJsonDocument respDoc = QJsonDocument::fromJson(responseBytes);
    QVERIFY(respDoc.isObject());
    QJsonObject respObj = respDoc.object();
    QCOMPARE(respObj[QStringLiteral("id")].toInt(), 42);
    QJsonObject res = respObj[QStringLiteral("result")].toObject();
    QCOMPARE(res[QStringLiteral("modelNameJp")].toString(), QStringLiteral("PMX20モデル"));

    QJsonDocument renameDoc = QJsonDocument::fromJson(renameResponseBytes);
    QVERIFY(renameDoc.isObject());
    QJsonObject renameRes = renameDoc.object()[QStringLiteral("result")].toObject();
    QCOMPARE(renameRes[QStringLiteral("nameJp")].toString(), QStringLiteral("ソケット経由変更"));
    QCOMPARE(renameRes[QStringLiteral("nameEn")].toString(), QStringLiteral("SocketChanged"));

    // Verify bone in server controller was updated via socket
    QCOMPARE(serverController.document().bones[1].nameJp, QStringLiteral("ソケット経由変更"));
    QCOMPARE(serverController.document().bones[1].nameEn, QStringLiteral("SocketChanged"));

    // Verify undo restores original state bit-identically
    serverController.undoStack()->undo();
    QCOMPARE(serverController.document().bones[1].nameJp, QStringLiteral("センター"));
    QCOMPARE(serverController.document().bones[1].nameEn, QStringLiteral("Center"));

    server.stopLocalServer();
    QVERIFY(!server.isServerListening());
}

QTEST_MAIN(TstPmxMcp)
#include "tst_pmx_mcp.moc"
