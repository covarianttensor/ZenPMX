#include "core/PmxReader.h"
#include "core/PmxWriter.h"
#include "models/PmxBoneTableModel.h"
#include "models/PmxDocumentController.h"
#include "models/PmxInfoModel.h"
#include "models/PmxMaterialTableModel.h"
#include "models/PmxSelectionManager.h"
#include "models/PmxUndoCommands.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <cmath>
#include <limits>

using namespace pmx;

class TstPmxUndoModels : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testCommandIdAllocation();
    void testLoadAndSaveModel();
    void testInfoModelEditUndoRedo();
    void testMaterialModelCellEditUndoRedo();
    void testBoneModelCellEditUndoRedo();
    void testContinuousEditCoalescing();
    void testBatchMaterialOperations();
    void testBatchBoneOperations();
    void testSetDataIdenticalValueNoOp();
    void testOutOfBoundsAccess();
    void testDynamicLanguageSwitching();
    void testSelectionManager();
    void testSnapshotCommand();

private:
    pmx::PmxDocument createSampleDocument();
    QTemporaryDir m_tempDir;
    QString m_sampleFilePath;
};

pmx::PmxDocument TstPmxUndoModels::createSampleDocument() {
    pmx::PmxDocument doc;
    doc.header.version = 2.0f;
    doc.header.textEncoding = 0; // UTF-16LE
    doc.header.additionalVec4Count = 0;
    doc.header.vertexIndexSize = 1;
    doc.header.textureIndexSize = 1;
    doc.header.materialIndexSize = 1;
    doc.header.boneIndexSize = 1;
    doc.header.morphIndexSize = 1;
    doc.header.rigidBodyIndexSize = 1;

    doc.modelInfo.modelNameJp = QStringLiteral("テストモデル");
    doc.modelInfo.modelNameEn = QStringLiteral("Test Model");
    doc.modelInfo.commentJp = QStringLiteral("コメント日本語");
    doc.modelInfo.commentEn = QStringLiteral("Comment English");

    // Vertices (one triangle)
    for (int i = 0; i < 3; ++i) {
        pmx::PmxVertex v;
        v.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.uv = glm::vec2(0.0f, 0.0f);
        v.weightType = pmx::WeightType::BDEF1;
        v.boneIndices[0] = 0;
        v.boneWeights[0] = 1.0f;
        doc.vertices.push_back(v);
        doc.faceIndices.push_back(i);
    }

    // Textures
    doc.textures.push_back(QStringLiteral("tex1.png"));
    doc.textures.push_back(QStringLiteral("tex2.png"));

    // Materials
    {
        pmx::PmxMaterial m0;
        m0.nameJp = QStringLiteral("材質0");
        m0.nameEn = QStringLiteral("Mat0");
        m0.diffuse = glm::vec4(0.8f, 0.2f, 0.3f, 1.0f);
        m0.specular = glm::vec3(0.5f);
        m0.specularPower = 5.0f;
        m0.ambient = glm::vec3(0.5f);
        m0.drawFlags = 0x1F;
        m0.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        m0.edgeSize = 1.0f;
        m0.textureIndex = 0;
        m0.sphereTextureIndex = -1;
        m0.sphereMode = 0;
        m0.sharedToonFlag = 0;
        m0.toonIndex = 1;
        m0.memo = QStringLiteral("Material 0 memo");
        m0.faceVertexCount = 3;
        doc.materials.push_back(m0);

        pmx::PmxMaterial m1;
        m1.nameJp = QStringLiteral("材質1");
        m1.nameEn = QStringLiteral("Mat1");
        m1.diffuse = glm::vec4(0.1f, 0.5f, 0.9f, 0.8f);
        m1.specular = glm::vec3(0.2f);
        m1.specularPower = 10.0f;
        m1.ambient = glm::vec3(0.4f);
        m1.drawFlags = 0x1F;
        m1.edgeColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
        m1.edgeSize = 2.0f;
        m1.textureIndex = -1;
        m1.sphereTextureIndex = -1;
        m1.sphereMode = 0;
        m1.sharedToonFlag = 1;
        m1.toonIndex = 2;
        m1.memo = QStringLiteral("Material 1 memo");
        m1.faceVertexCount = 0;
        doc.materials.push_back(m1);

        pmx::PmxMaterial m2;
        m2.nameJp = QStringLiteral("材質2");
        m2.nameEn = QStringLiteral("Mat2");
        m2.diffuse = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
        m2.specular = glm::vec3(0.0f);
        m2.specularPower = 1.0f;
        m2.ambient = glm::vec3(0.2f);
        m2.drawFlags = 0x1F;
        m2.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        m2.edgeSize = 3.0f;
        m2.textureIndex = 1;
        m2.sphereTextureIndex = -1;
        m2.sphereMode = 0;
        m2.sharedToonFlag = 0;
        m2.toonIndex = 0;
        m2.memo = QStringLiteral("Material 2 memo");
        m2.faceVertexCount = 0;
        doc.materials.push_back(m2);
    }

    // Bones
    {
        pmx::PmxBone b0;
        b0.nameJp = QStringLiteral("全ての親");
        b0.nameEn = QStringLiteral("Root");
        b0.position = glm::vec3(0.0f, 0.0f, 0.0f);
        b0.parentBoneIndex = -1;
        b0.deformTier = 0;
        b0.boneFlags = 0x0002;
        doc.bones.push_back(b0);

        pmx::PmxBone b1;
        b1.nameJp = QStringLiteral("センター");
        b1.nameEn = QStringLiteral("Center");
        b1.position = glm::vec3(0.0f, 10.0f, 0.0f);
        b1.parentBoneIndex = 0;
        b1.deformTier = 0;
        b1.boneFlags = 0x0003;
        doc.bones.push_back(b1);

        pmx::PmxBone b2;
        b2.nameJp = QStringLiteral("上半身");
        b2.nameEn = QStringLiteral("UpperBody");
        b2.position = glm::vec3(0.0f, 14.0f, 0.0f);
        b2.parentBoneIndex = 1;
        b2.deformTier = 1;
        b2.boneFlags = 0x0002;
        doc.bones.push_back(b2);
    }

    return doc;
}

void TstPmxUndoModels::initTestCase() {
    QVERIFY(m_tempDir.isValid());
    m_sampleFilePath = m_tempDir.filePath(QStringLiteral("sample.pmx"));

    pmx::PmxDocument doc = createSampleDocument();
    QString error;
    bool written = pmx::PmxWriter::writeToFile(m_sampleFilePath, doc, &error);
    QVERIFY2(written, qPrintable(error));
}

void TstPmxUndoModels::cleanupTestCase() {
    // QTemporaryDir cleans up on destruction
}

void TstPmxUndoModels::testCommandIdAllocation() {
    // Ensure makeCommandId does not collide at row >= 4096
    int idRow0 = makeCommandId(0, 0, 1);
    int idRow4096 = makeCommandId(0, 4096, 1);
    QVERIFY(idRow0 != idRow4096);

    // Verify bit partitioning: tableId, col, row are distinct
    int idTable0 = makeCommandId(0, 10, 5);
    int idTable1 = makeCommandId(1, 10, 5);
    QVERIFY(idTable0 != idTable1);

    int idCol5 = makeCommandId(1, 10, 5);
    int idCol6 = makeCommandId(1, 10, 6);
    QVERIFY(idCol5 != idCol6);
}

void TstPmxUndoModels::testLoadAndSaveModel() {
    pmx::PmxDocumentController controller;
    QCOMPARE(controller.isModified(), false);
    QCOMPARE(controller.undoStack()->count(), 0);

    QString error;
    bool loaded = controller.loadFromFile(m_sampleFilePath, &error);
    QVERIFY2(loaded, qPrintable(error));

    QCOMPARE(controller.isModified(), false);
    QCOMPARE(controller.document().materials.size(), static_cast<size_t>(3));
    QCOMPARE(controller.document().bones.size(), static_cast<size_t>(3));

    // Verify model row counts
    QCOMPARE(controller.materialModel()->rowCount(), 3);
    QCOMPARE(controller.boneModel()->rowCount(), 3);
    QCOMPARE(controller.infoModel()->rowCount(), static_cast<int>(PmxInfoModel::RowCount));

    // Save test
    QString savePath = m_tempDir.filePath(QStringLiteral("saved.pmx"));
    bool saved = controller.saveToFile(savePath, &error);
    QVERIFY2(saved, qPrintable(error));
    QCOMPARE(controller.isModified(), false);

    // Verify saved file can be read back losslessly
    pmx::PmxDocument roundTripDoc;
    bool readBack = pmx::PmxReader::readFromFile(savePath, roundTripDoc, &error);
    QVERIFY2(readBack, qPrintable(error));
    QCOMPARE(roundTripDoc.materials.size(), static_cast<size_t>(3));
    QCOMPARE(roundTripDoc.bones.size(), static_cast<size_t>(3));
}

void TstPmxUndoModels::testInfoModelEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.infoModel();
    QModelIndex nameIndex = model->index(PmxInfoModel::RowModelNameJp, 1);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("テストモデル"));

    // Edit model name
    bool edited = model->setData(nameIndex, QStringLiteral("新モデル名"));
    QVERIFY(edited);
    QCOMPARE(controller.document().modelInfo.modelNameJp, QStringLiteral("新モデル名"));
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("新モデル名"));
    QCOMPARE(controller.isModified(), true);
    QCOMPARE(controller.undoStack()->count(), 1);

    // Undo
    controller.undo();
    QCOMPARE(controller.document().modelInfo.modelNameJp, QStringLiteral("テストモデル"));
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("テストモデル"));
    QCOMPARE(controller.isModified(), false);

    // Redo
    controller.redo();
    QCOMPARE(controller.document().modelInfo.modelNameJp, QStringLiteral("新モデル名"));
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("新モデル名"));
    QCOMPARE(controller.isModified(), true);

    // Roll back to clean state
    controller.undo();
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testMaterialModelCellEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.materialModel();
    QModelIndex edgeIndex = model->index(0, PmxMaterialTableModel::ColEdgeSize);
    QCOMPARE(model->data(edgeIndex).toFloat(), 1.0f);

    // Edit edgeSize
    bool edited = model->setData(edgeIndex, 5.5f);
    QVERIFY(edited);
    QCOMPARE(controller.document().materials[0].edgeSize, 5.5f);
    QCOMPARE(controller.undoStack()->count(), 1);
    QCOMPARE(controller.isModified(), true);

    // Undo
    controller.undo();
    QCOMPARE(controller.document().materials[0].edgeSize, 1.0f);
    QCOMPARE(controller.isModified(), false);

    // Redo
    controller.redo();
    QCOMPARE(controller.document().materials[0].edgeSize, 5.5f);
    QCOMPARE(controller.isModified(), true);

    controller.undo();
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testBoneModelCellEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.boneModel();
    QModelIndex posIndex = model->index(1, PmxBoneTableModel::ColPosX);
    QCOMPARE(model->data(posIndex).toFloat(), 0.0f);

    // Edit posX
    bool edited = model->setData(posIndex, 12.5f);
    QVERIFY(edited);
    QCOMPARE(controller.document().bones[1].position.x, 12.5f);
    QCOMPARE(controller.undoStack()->count(), 1);

    // Edit deformTier
    QModelIndex tierIndex = model->index(1, PmxBoneTableModel::ColDeformTier);
    QCOMPARE(model->data(tierIndex).toInt(), 0);
    edited = model->setData(tierIndex, 3);
    QVERIFY(edited);
    QCOMPARE(controller.document().bones[1].deformTier, 3);
    QCOMPARE(controller.undoStack()->count(), 2);

    // Undo tier
    controller.undo();
    QCOMPARE(controller.document().bones[1].deformTier, 0);
    QCOMPARE(controller.document().bones[1].position.x, 12.5f);

    // Undo posX
    controller.undo();
    QCOMPARE(controller.document().bones[1].position.x, 0.0f);
    QCOMPARE(controller.isModified(), false);

    // Redo both
    controller.redo();
    QCOMPARE(controller.document().bones[1].position.x, 12.5f);
    controller.redo();
    QCOMPARE(controller.document().bones[1].deformTier, 3);

    controller.undo();
    controller.undo();
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testContinuousEditCoalescing() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.boneModel();
    QModelIndex posYIndex = model->index(0, PmxBoneTableModel::ColPosY);
    QCOMPARE(model->data(posYIndex).toFloat(), 0.0f);

    // Simulate dragging a slider or spinbox across multiple continuous events:
    // 0.0 -> 1.0 -> 2.5 -> 4.0 -> 7.8
    QVERIFY(model->setData(posYIndex, 1.0f));
    QVERIFY(model->setData(posYIndex, 2.5f));
    QVERIFY(model->setData(posYIndex, 4.0f));
    QVERIFY(model->setData(posYIndex, 7.8f));

    // Verify all consecutive edits coalesced into exactly ONE command on the stack
    QCOMPARE(controller.undoStack()->count(), 1);
    QCOMPARE(controller.document().bones[0].position.y, 7.8f);

    // Calling undo MUST roll back to the pre-drag initial value (0.0f) in a single step!
    controller.undo();
    QCOMPARE(controller.document().bones[0].position.y, 0.0f);
    QCOMPARE(controller.isModified(), false);

    // Calling redo MUST roll forward directly to the final dragged value (7.8f)!
    controller.redo();
    QCOMPARE(controller.document().bones[0].position.y, 7.8f);
    QCOMPARE(controller.isModified(), true);

    controller.undo();
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testBatchMaterialOperations() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.materialModel();
    // Initial edgeSize values: Mat0=1.0, Mat1=2.0, Mat2=3.0
    QCOMPARE(controller.document().materials[0].edgeSize, 1.0f);
    QCOMPARE(controller.document().materials[1].edgeSize, 2.0f);
    QCOMPARE(controller.document().materials[2].edgeSize, 3.0f);

    // Validation: Reject non-numeric columns without modifying strings
    bool failName = model->applyBatchOperation({0, 1}, PmxMaterialTableModel::ColName, QStringLiteral("="), 5.0);
    QCOMPARE(failName, false);
    QCOMPARE(controller.document().materials[0].nameJp, QStringLiteral("材質0"));
    QCOMPARE(controller.document().materials[1].nameJp, QStringLiteral("材質1"));

    bool failMemo = model->applyBatchOperation({0, 1}, PmxMaterialTableModel::ColMemo, QStringLiteral("="), 5.0);
    QCOMPARE(failMemo, false);

    // Validation: Reject NaN and Inf
    bool failNan = model->applyBatchOperation({0, 1}, PmxMaterialTableModel::ColEdgeSize, QStringLiteral("*"),
                                              std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(failNan, false);

    bool failInf = model->applyBatchOperation({0, 1}, PmxMaterialTableModel::ColEdgeSize, QStringLiteral("*"),
                                              std::numeric_limits<double>::infinity());
    QCOMPARE(failInf, false);

    // Validation: Reject unknown operator
    bool failOp = model->applyBatchOperation({0, 1}, PmxMaterialTableModel::ColEdgeSize, QStringLiteral("?"), 2.0);
    QCOMPARE(failOp, false);

    // Validation: Reject operations where no values change
    bool failNoChange = model->applyBatchOperation({0}, PmxMaterialTableModel::ColEdgeSize, QStringLiteral("*"), 1.0);
    QCOMPARE(failNoChange, false);
    QCOMPARE(controller.undoStack()->count(), 0);

    // 1. Batch multiply by 0.5 across all 3 materials
    bool ok = model->applyBatchOperation({0, 1, 2}, PmxMaterialTableModel::ColEdgeSize, QStringLiteral("*"), 0.5);
    QVERIFY(ok);
    QCOMPARE(controller.undoStack()->count(), 1);
    QCOMPARE(controller.document().materials[0].edgeSize, 0.5f);
    QCOMPARE(controller.document().materials[1].edgeSize, 1.0f);
    QCOMPARE(controller.document().materials[2].edgeSize, 1.5f);

    // 2. Batch add +10.0 to materials 0 and 2
    ok = model->applyBatchOperation({0, 2}, PmxMaterialTableModel::ColEdgeSize, QStringLiteral("+"), 10.0);
    QVERIFY(ok);
    QCOMPARE(controller.undoStack()->count(), 2);
    QCOMPARE(controller.document().materials[0].edgeSize, 10.5f);
    QCOMPARE(controller.document().materials[1].edgeSize, 1.0f);
    QCOMPARE(controller.document().materials[2].edgeSize, 11.5f);

    // 3. Batch assign = 99.0 to material 1
    ok = model->applyBatchOperation({1}, PmxMaterialTableModel::ColEdgeSize, QStringLiteral("="), 99.0);
    QVERIFY(ok);
    QCOMPARE(controller.undoStack()->count(), 3);
    QCOMPARE(controller.document().materials[1].edgeSize, 99.0f);

    // Step-by-step undo
    // Undo assignment
    controller.undo();
    QCOMPARE(controller.document().materials[1].edgeSize, 1.0f);

    // Undo addition
    controller.undo();
    QCOMPARE(controller.document().materials[0].edgeSize, 0.5f);
    QCOMPARE(controller.document().materials[2].edgeSize, 1.5f);

    // Undo multiplication -> back to pristine initial state
    controller.undo();
    QCOMPARE(controller.document().materials[0].edgeSize, 1.0f);
    QCOMPARE(controller.document().materials[1].edgeSize, 2.0f);
    QCOMPARE(controller.document().materials[2].edgeSize, 3.0f);
    QCOMPARE(controller.isModified(), false);

    // Redo all three back
    controller.redo();
    QCOMPARE(controller.document().materials[0].edgeSize, 0.5f);
    controller.redo();
    QCOMPARE(controller.document().materials[0].edgeSize, 10.5f);
    controller.redo();
    QCOMPARE(controller.document().materials[1].edgeSize, 99.0f);

    // Undo back to clean
    controller.undo();
    controller.undo();
    controller.undo();
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testBatchBoneOperations() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* boneModel = controller.boneModel();
    // Initial Pos Y: Root=0.0, Center=10.0, UpperBody=14.0
    QCOMPARE(controller.document().bones[0].position.y, 0.0f);
    QCOMPARE(controller.document().bones[1].position.y, 10.0f);
    QCOMPARE(controller.document().bones[2].position.y, 14.0f);

    // Validation: Reject non-numeric ColName
    bool failName = boneModel->applyBatchOperation({0, 1}, PmxBoneTableModel::ColName, QStringLiteral("="), 10.0);
    QCOMPARE(failName, false);
    QCOMPARE(controller.document().bones[0].nameJp, QStringLiteral("全ての親"));

    // Validation: Reject NaN
    bool failNan = boneModel->applyBatchOperation({0, 1}, PmxBoneTableModel::ColPosY, QStringLiteral("+"),
                                                  std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(failNan, false);

    // Batch add +5.0 to Pos Y across all 3 bones
    bool ok = boneModel->applyBatchOperation({0, 1, 2}, PmxBoneTableModel::ColPosY, QStringLiteral("+"), 5.0);
    QVERIFY(ok);
    QCOMPARE(controller.undoStack()->count(), 1);
    QCOMPARE(controller.document().bones[0].position.y, 5.0f);
    QCOMPARE(controller.document().bones[1].position.y, 15.0f);
    QCOMPARE(controller.document().bones[2].position.y, 19.0f);

    // Undo batch addition
    controller.undo();
    QCOMPARE(controller.document().bones[0].position.y, 0.0f);
    QCOMPARE(controller.document().bones[1].position.y, 10.0f);
    QCOMPARE(controller.document().bones[2].position.y, 14.0f);
    QCOMPARE(controller.isModified(), false);

    // Redo batch addition
    controller.redo();
    QCOMPARE(controller.document().bones[0].position.y, 5.0f);
    QCOMPARE(controller.document().bones[1].position.y, 15.0f);
    QCOMPARE(controller.document().bones[2].position.y, 19.0f);

    controller.undo();
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testSetDataIdenticalValueNoOp() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    int initialUndoIndex = controller.undoStack()->index();

    // 1. InfoModel: setting identical model name returns false, stack untouched
    auto* infoModel = controller.infoModel();
    bool edited = infoModel->setData(infoModel->index(PmxInfoModel::RowModelNameJp, 1), QStringLiteral("テストモデル"));
    QCOMPARE(edited, false);
    QCOMPARE(controller.undoStack()->index(), initialUndoIndex);

    // 2. MaterialModel: setting identical edgeSize returns false, stack untouched
    auto* matModel = controller.materialModel();
    edited = matModel->setData(matModel->index(0, PmxMaterialTableModel::ColEdgeSize), 1.0f);
    QCOMPARE(edited, false);
    QCOMPARE(controller.undoStack()->index(), initialUndoIndex);

    // 3. BoneModel: setting identical posX returns false, stack untouched
    auto* boneModel = controller.boneModel();
    edited = boneModel->setData(boneModel->index(1, PmxBoneTableModel::ColPosX), 0.0f);
    QCOMPARE(edited, false);
    QCOMPARE(controller.undoStack()->index(), initialUndoIndex);
}

void TstPmxUndoModels::testOutOfBoundsAccess() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* matModel = controller.materialModel();
    QVERIFY(!matModel->data(matModel->index(-1, 0)).isValid());
    QVERIFY(!matModel->data(matModel->index(9999, 0)).isValid());
    QVERIFY(!matModel->data(matModel->index(0, -1)).isValid());
    QVERIFY(!matModel->data(matModel->index(0, 9999)).isValid());

    auto* boneModel = controller.boneModel();
    QVERIFY(!boneModel->data(boneModel->index(-1, 0)).isValid());
    QVERIFY(!boneModel->data(boneModel->index(9999, 0)).isValid());
    QVERIFY(!boneModel->data(boneModel->index(0, -1)).isValid());
    QVERIFY(!boneModel->data(boneModel->index(0, 9999)).isValid());

    auto* infoModel = controller.infoModel();
    QVERIFY(!infoModel->data(infoModel->index(-1, 0)).isValid());
    QVERIFY(!infoModel->data(infoModel->index(9999, 0)).isValid());
    QVERIFY(!infoModel->data(infoModel->index(0, -1)).isValid());
    QVERIFY(!infoModel->data(infoModel->index(0, 9999)).isValid());
}

void TstPmxUndoModels::testDynamicLanguageSwitching() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* boneModel = controller.boneModel();
    auto* matModel = controller.materialModel();

    // 1. Initial Japanese mode
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::Japanese);
    QCOMPARE(controller.displayLanguage(), PmxDocumentController::DisplayLanguage::Japanese);

    QCOMPARE(boneModel->headerData(PmxBoneTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("ボーン名"));
    QCOMPARE(boneModel->headerData(PmxBoneTableModel::ColDeformTier, Qt::Horizontal).toString(),
             QStringLiteral("変形階層"));
    QCOMPARE(boneModel->data(boneModel->index(1, PmxBoneTableModel::ColName)).toString(),
             QStringLiteral("センター"));

    QCOMPARE(matModel->headerData(PmxMaterialTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("材質名"));
    QCOMPARE(matModel->headerData(PmxMaterialTableModel::ColEdgeSize, Qt::Horizontal).toString(),
             QStringLiteral("輪郭サイズ"));
    QCOMPARE(matModel->data(matModel->index(0, PmxMaterialTableModel::ColName)).toString(),
             QStringLiteral("材質0"));

    // 2. Switch to English mode
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::English);
    QCOMPARE(controller.displayLanguage(), PmxDocumentController::DisplayLanguage::English);

    QCOMPARE(boneModel->headerData(PmxBoneTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Bone Name"));
    QCOMPARE(boneModel->headerData(PmxBoneTableModel::ColDeformTier, Qt::Horizontal).toString(),
             QStringLiteral("Deform Tier"));
    QCOMPARE(boneModel->data(boneModel->index(1, PmxBoneTableModel::ColName)).toString(),
             QStringLiteral("Center"));

    QCOMPARE(matModel->headerData(PmxMaterialTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Material Name"));
    QCOMPARE(matModel->headerData(PmxMaterialTableModel::ColEdgeSize, Qt::Horizontal).toString(),
             QStringLiteral("Edge Size"));
    QCOMPARE(matModel->data(matModel->index(0, PmxMaterialTableModel::ColName)).toString(),
             QStringLiteral("Mat0"));

    // 3. Edit in English mode updates nameEn without mutating nameJp
    bool edited = boneModel->setData(boneModel->index(1, PmxBoneTableModel::ColName), QStringLiteral("Center_Renamed"));
    QVERIFY(edited);
    QCOMPARE(controller.document().bones[1].nameEn, QStringLiteral("Center_Renamed"));
    QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("センター"));

    // Undo edit
    controller.undo();
    QCOMPARE(controller.document().bones[1].nameEn, QStringLiteral("Center"));
    QCOMPARE(controller.document().bones[1].nameJp, QStringLiteral("センター"));
}

void TstPmxUndoModels::testSelectionManager() {
    pmx::PmxSelectionManager sm;
    QSignalSpy vertSpy(&sm, &PmxSelectionManager::selectionChanged);
    QSignalSpy anySpy(&sm, &PmxSelectionManager::anySelectionChanged);

    sm.select(PmxSelectionManager::SelectionType::Vertex, 5);
    sm.select(PmxSelectionManager::SelectionType::Vertex, 10);
    QCOMPARE(vertSpy.count(), 2);
    QCOMPARE(anySpy.count(), 2);
    QCOMPARE(sm.selectionCount(PmxSelectionManager::SelectionType::Vertex), 2);
    QVERIFY(sm.isSelected(PmxSelectionManager::SelectionType::Vertex, 5));
    QVERIFY(sm.isSelected(PmxSelectionManager::SelectionType::Vertex, 10));
    QVERIFY(!sm.isSelected(PmxSelectionManager::SelectionType::Vertex, 7));

    // Toggle
    sm.toggle(PmxSelectionManager::SelectionType::Vertex, 5);
    QVERIFY(!sm.isSelected(PmxSelectionManager::SelectionType::Vertex, 5));
    QCOMPARE(sm.selectionCount(PmxSelectionManager::SelectionType::Vertex), 1);

    // List ordering
    sm.setSelection(PmxSelectionManager::SelectionType::Bone, QList<int>{8, 2, 4});
    QCOMPARE(sm.selectedBones(), (QList<int>{2, 4, 8}));

    // Clear
    sm.clearAll();
    QCOMPARE(sm.hasSelection(PmxSelectionManager::SelectionType::Vertex), false);
    QCOMPARE(sm.hasSelection(PmxSelectionManager::SelectionType::Bone), false);
}

void TstPmxUndoModels::testSnapshotCommand() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    pmx::PmxDocument modifiedDoc = controller.document();
    modifiedDoc.modelInfo.modelNameJp = QStringLiteral("スナップショットモデル");
    modifiedDoc.bones.clear(); // Structural change
    modifiedDoc.materials[0].edgeSize = 88.0f;

    controller.pushCommand(new PmxDocumentSnapshotCommand(&controller, modifiedDoc, QStringLiteral("Bulk Snapshot")));
    QCOMPARE(controller.document().modelInfo.modelNameJp, QStringLiteral("スナップショットモデル"));
    QCOMPARE(controller.boneModel()->rowCount(), 0);
    QCOMPARE(controller.document().materials[0].edgeSize, 88.0f);
    QCOMPARE(controller.undoStack()->count(), 1);

    // Undo snapshot
    controller.undo();
    QCOMPARE(controller.document().modelInfo.modelNameJp, QStringLiteral("テストモデル"));
    QCOMPARE(controller.boneModel()->rowCount(), 3);
    QCOMPARE(controller.document().materials[0].edgeSize, 1.0f);

    // Redo snapshot
    controller.redo();
    QCOMPARE(controller.document().modelInfo.modelNameJp, QStringLiteral("スナップショットモデル"));
    QCOMPARE(controller.boneModel()->rowCount(), 0);
    QCOMPARE(controller.document().materials[0].edgeSize, 88.0f);

    controller.undo();
    QCOMPARE(controller.isModified(), false);
}

QTEST_MAIN(TstPmxUndoModels)
#include "tst_pmx_undo_models.moc"
