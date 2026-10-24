#include "core/PmxReader.h"
#include "core/PmxWriter.h"
#include "models/PmxBoneTableModel.h"
#include "models/PmxDisplayFrameModel.h"
#include "models/PmxDocumentController.h"
#include "models/PmxInfoModel.h"
#include "models/PmxJointTableModel.h"
#include "models/PmxMaterialTableModel.h"
#include "models/PmxMorphTableModel.h"
#include "models/PmxRigidBodyTableModel.h"
#include "models/PmxSelectionManager.h"
#include "models/PmxUndoCommands.h"
#include "models/PmxVertexTableModel.h"

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
    void testVertexModelCellEditUndoRedo();
    void testMorphModelCellEditUndoRedo();
    void testDisplayFrameModelCellEditUndoRedo();
    void testRigidBodyModelCellEditUndoRedo();
    void testJointModelCellEditUndoRedo();
    void testContinuousEditCoalescing();
    void testBatchMaterialOperations();
    void testBatchBoneOperations();
    void testBatchVertexOperations();
    void testBatchRigidBodyOperations();
    void testBatchJointOperations();
    void testSetDataIdenticalValueNoOp();
    void testOutOfBoundsAccess();
    void testAllModelsOutOfBoundsAccess();
    void testDynamicLanguageSwitching();
    void testLanguageSwitchingOnAllModels();
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

    // Morphs
    {
        pmx::PmxMorph m0;
        m0.nameJp = QStringLiteral("まばたき");
        m0.nameEn = QStringLiteral("Blink");
        m0.panelCategory = 2; // Eye
        m0.morphType = 1; // Vertex
        pmx::VertexMorphOffset voff;
        voff.vertexIndex = 0;
        voff.positionOffset = glm::vec3(0.0f, -0.5f, 0.0f);
        m0.vertexOffsets.push_back(voff);
        doc.morphs.push_back(m0);

        pmx::PmxMorph m1;
        m1.nameJp = QStringLiteral("あ");
        m1.nameEn = QStringLiteral("A");
        m1.panelCategory = 3; // Lip
        m1.morphType = 1; // Vertex
        pmx::VertexMorphOffset voff1;
        voff1.vertexIndex = 1;
        voff1.positionOffset = glm::vec3(0.0f, 0.2f, 0.0f);
        m1.vertexOffsets.push_back(voff1);
        doc.morphs.push_back(m1);
    }

    // Display Frames
    {
        pmx::PmxDisplayFrame f0;
        f0.nameJp = QStringLiteral("Root");
        f0.nameEn = QStringLiteral("Root");
        f0.specialFlag = 1;
        pmx::PmxDisplayElement elem0;
        elem0.targetType = 0; // Bone
        elem0.index = 0;
        f0.elements.push_back(elem0);
        doc.displayFrames.push_back(f0);

        pmx::PmxDisplayFrame f1;
        f1.nameJp = QStringLiteral("表情");
        f1.nameEn = QStringLiteral("Facial");
        f1.specialFlag = 0;
        pmx::PmxDisplayElement elem1;
        elem1.targetType = 1; // Morph
        elem1.index = 0;
        f1.elements.push_back(elem1);
        doc.displayFrames.push_back(f1);
    }

    // Rigid Bodies
    {
        pmx::PmxRigidBody rb0;
        rb0.nameJp = QStringLiteral("頭");
        rb0.nameEn = QStringLiteral("Head");
        rb0.boneIndex = 2;
        rb0.groupIndex = 0;
        rb0.nonCollisionMask = 0xFFFE;
        rb0.shape = 0; // Sphere
        rb0.size = glm::vec3(1.0f);
        rb0.position = glm::vec3(0.0f, 16.0f, 0.0f);
        rb0.rotation = glm::vec3(0.0f);
        rb0.mass = 1.0f;
        rb0.linearDamping = 0.5f;
        rb0.angularDamping = 0.5f;
        rb0.restitution = 0.0f;
        rb0.friction = 0.5f;
        rb0.physicsMode = 0; // FollowBone
        doc.rigidBodies.push_back(rb0);

        pmx::PmxRigidBody rb1;
        rb1.nameJp = QStringLiteral("髪");
        rb1.nameEn = QStringLiteral("Hair");
        rb1.boneIndex = -1;
        rb1.groupIndex = 1;
        rb1.nonCollisionMask = 0xFFFD;
        rb1.shape = 1; // Box
        rb1.size = glm::vec3(0.5f, 1.0f, 0.5f);
        rb1.position = glm::vec3(0.0f, 15.0f, -1.0f);
        rb1.rotation = glm::vec3(0.1f, 0.0f, 0.0f);
        rb1.mass = 0.5f;
        rb1.linearDamping = 0.5f;
        rb1.angularDamping = 0.5f;
        rb1.restitution = 0.0f;
        rb1.friction = 0.5f;
        rb1.physicsMode = 1; // Dynamic
        doc.rigidBodies.push_back(rb1);
    }

    // Joints
    {
        pmx::PmxJoint j0;
        j0.nameJp = QStringLiteral("ジョイント0");
        j0.nameEn = QStringLiteral("Joint0");
        j0.jointType = 0;
        j0.rigidBodyIndexA = 0;
        j0.rigidBodyIndexB = 1;
        j0.position = glm::vec3(0.0f, 15.5f, -0.5f);
        j0.rotation = glm::vec3(0.0f);
        j0.linearLowerLimit = glm::vec3(-0.1f);
        j0.linearUpperLimit = glm::vec3(0.1f);
        j0.angularLowerLimit = glm::vec3(-0.5f);
        j0.angularUpperLimit = glm::vec3(0.5f);
        j0.springLinearStiffness = glm::vec3(10.0f);
        j0.springAngularStiffness = glm::vec3(100.0f);
        doc.joints.push_back(j0);
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
    QCOMPARE(controller.document().vertices.size(), static_cast<size_t>(3));
    QCOMPARE(controller.document().morphs.size(), static_cast<size_t>(2));
    QCOMPARE(controller.document().displayFrames.size(), static_cast<size_t>(2));
    QCOMPARE(controller.document().rigidBodies.size(), static_cast<size_t>(2));
    QCOMPARE(controller.document().joints.size(), static_cast<size_t>(1));

    // Verify model row counts
    QCOMPARE(controller.materialModel()->rowCount(), 3);
    QCOMPARE(controller.boneModel()->rowCount(), 3);
    QCOMPARE(controller.infoModel()->rowCount(), static_cast<int>(PmxInfoModel::RowCount));
    QCOMPARE(controller.vertexModel()->rowCount(), 3);
    QCOMPARE(controller.morphModel()->rowCount(), 2);
    QCOMPARE(controller.displayFrameModel()->rowCount(), 2);
    QCOMPARE(controller.rigidBodyModel()->rowCount(), 2);
    QCOMPARE(controller.jointModel()->rowCount(), 1);

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
    QCOMPARE(roundTripDoc.vertices.size(), static_cast<size_t>(3));
    QCOMPARE(roundTripDoc.morphs.size(), static_cast<size_t>(2));
    QCOMPARE(roundTripDoc.displayFrames.size(), static_cast<size_t>(2));
    QCOMPARE(roundTripDoc.rigidBodies.size(), static_cast<size_t>(2));
    QCOMPARE(roundTripDoc.joints.size(), static_cast<size_t>(1));
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

    // 4. VertexModel: setting identical posX returns false, stack untouched
    auto* vertexModel = controller.vertexModel();
    edited = vertexModel->setData(vertexModel->index(0, PmxVertexTableModel::ColPosX), 0.0f);
    QCOMPARE(edited, false);
    QCOMPARE(controller.undoStack()->index(), initialUndoIndex);

    // 5. MorphModel: setting identical name returns false, stack untouched
    auto* morphModel = controller.morphModel();
    edited = morphModel->setData(morphModel->index(0, PmxMorphTableModel::ColName), QStringLiteral("まばたき"));
    QCOMPARE(edited, false);
    QCOMPARE(controller.undoStack()->index(), initialUndoIndex);

    // 6. DisplayFrameModel: setting identical name returns false, stack untouched
    auto* frameModel = controller.displayFrameModel();
    edited = frameModel->setData(frameModel->index(0, PmxDisplayFrameModel::ColName), QStringLiteral("Root"));
    QCOMPARE(edited, false);
    QCOMPARE(controller.undoStack()->index(), initialUndoIndex);

    // 7. RigidBodyModel: setting identical name returns false, stack untouched
    auto* rigidBodyModel = controller.rigidBodyModel();
    edited = rigidBodyModel->setData(rigidBodyModel->index(0, PmxRigidBodyTableModel::ColName), QStringLiteral("頭"));
    QCOMPARE(edited, false);
    QCOMPARE(controller.undoStack()->index(), initialUndoIndex);

    // 8. JointModel: setting identical name returns false, stack untouched
    auto* jointModel = controller.jointModel();
    edited = jointModel->setData(jointModel->index(0, PmxJointTableModel::ColName), QStringLiteral("ジョイント0"));
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

void TstPmxUndoModels::testVertexModelCellEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.vertexModel();
    QCOMPARE(model->rowCount(), 3);
    QCOMPARE(model->columnCount(), 19);

    // 1. PosX/Y/Z
    QModelIndex posXIndex = model->index(0, PmxVertexTableModel::ColPosX);
    QCOMPARE(model->data(posXIndex).toFloat(), 0.0f);
    QVERIFY(model->setData(posXIndex, 5.5f));
    QCOMPARE(controller.document().vertices[0].position.x, 5.5f);
    QCOMPARE(model->data(posXIndex).toFloat(), 5.5f);

    controller.undo();
    QCOMPARE(controller.document().vertices[0].position.x, 0.0f);
    QCOMPARE(model->data(posXIndex).toFloat(), 0.0f);

    controller.redo();
    QCOMPARE(controller.document().vertices[0].position.x, 5.5f);

    QModelIndex posYIndex = model->index(0, PmxVertexTableModel::ColPosY);
    QVERIFY(model->setData(posYIndex, 2.5f));
    QCOMPARE(controller.document().vertices[0].position.y, 2.5f);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].position.y, 0.0f);

    QModelIndex posZIndex = model->index(0, PmxVertexTableModel::ColPosZ);
    QVERIFY(model->setData(posZIndex, -1.5f));
    QCOMPARE(controller.document().vertices[0].position.z, -1.5f);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].position.z, 0.0f);

    // 2. NormX/Y/Z
    QModelIndex normXIndex = model->index(0, PmxVertexTableModel::ColNormX);
    QVERIFY(model->setData(normXIndex, 0.1f));
    QCOMPARE(controller.document().vertices[0].normal.x, 0.1f);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].normal.x, 0.0f);

    QModelIndex normYIndex = model->index(0, PmxVertexTableModel::ColNormY);
    QCOMPARE(model->data(normYIndex).toFloat(), 1.0f);
    QVERIFY(model->setData(normYIndex, 0.8f));
    QCOMPARE(controller.document().vertices[0].normal.y, 0.8f);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].normal.y, 1.0f);

    QModelIndex normZIndex = model->index(0, PmxVertexTableModel::ColNormZ);
    QVERIFY(model->setData(normZIndex, 0.2f));
    QCOMPARE(controller.document().vertices[0].normal.z, 0.2f);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].normal.z, 0.0f);

    // 3. UVs
    QModelIndex uvUIndex = model->index(0, PmxVertexTableModel::ColUvU);
    QModelIndex uvVIndex = model->index(0, PmxVertexTableModel::ColUvV);
    QVERIFY(model->setData(uvUIndex, 0.25f));
    QVERIFY(model->setData(uvVIndex, 0.75f));
    QCOMPARE(controller.document().vertices[0].uv.x, 0.25f);
    QCOMPARE(controller.document().vertices[0].uv.y, 0.75f);

    controller.undo(); // undo V
    QCOMPARE(controller.document().vertices[0].uv.y, 0.0f);
    controller.undo(); // undo U
    QCOMPARE(controller.document().vertices[0].uv.x, 0.0f);

    // 4. Bone and Weights
    QModelIndex boneIndex = model->index(0, PmxVertexTableModel::ColBone0);
    QVERIFY(model->setData(boneIndex, 2));
    QCOMPARE(controller.document().vertices[0].boneIndices[0], 2);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].boneIndices[0], 0);

    QModelIndex weightIndex = model->index(0, PmxVertexTableModel::ColWeight0);
    QVERIFY(model->setData(weightIndex, 0.6f));
    QCOMPARE(controller.document().vertices[0].boneWeights[0], 0.6f);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].boneWeights[0], 1.0f);

    // 5. EdgeScale
    QModelIndex edgeIndex = model->index(0, PmxVertexTableModel::ColEdgeScale);
    QVERIFY(model->setData(edgeIndex, 3.0f));
    QCOMPARE(controller.document().vertices[0].edgeScale, 3.0f);
    controller.undo();
    QCOMPARE(controller.document().vertices[0].edgeScale, 1.0f);

    // 6. ColIndex is read-only
    QModelIndex indexCol = model->index(0, PmxVertexTableModel::ColIndex);
    QCOMPARE(model->data(indexCol).toInt(), 0);
    QVERIFY(!(model->flags(indexCol) & Qt::ItemIsEditable));
    QVERIFY(!model->setData(indexCol, 99));
}

void TstPmxUndoModels::testMorphModelCellEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.morphModel();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->columnCount(), 4);

    // 1. Name JP edit
    QModelIndex nameIndex = model->index(0, PmxMorphTableModel::ColName);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("まばたき"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("ウィンク")));
    QCOMPARE(controller.document().morphs[0].nameJp, QStringLiteral("ウィンク"));
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("ウィンク"));

    controller.undo();
    QCOMPARE(controller.document().morphs[0].nameJp, QStringLiteral("まばたき"));
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("まばたき"));

    controller.redo();
    QCOMPARE(controller.document().morphs[0].nameJp, QStringLiteral("ウィンク"));

    // 2. Name EN edit
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::English);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("Blink"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("Wink")));
    QCOMPARE(controller.document().morphs[0].nameEn, QStringLiteral("Wink"));
    QCOMPARE(controller.document().morphs[0].nameJp, QStringLiteral("ウィンク")); // JP preserved

    controller.undo();
    QCOMPARE(controller.document().morphs[0].nameEn, QStringLiteral("Blink"));
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::Japanese);

    // 3. Panel
    QModelIndex panelIndex = model->index(0, PmxMorphTableModel::ColPanel);
    QCOMPARE(model->data(panelIndex).toInt(), 2);
    QVERIFY(model->setData(panelIndex, 4));
    QCOMPARE(controller.document().morphs[0].panelCategory, static_cast<uint8_t>(4));
    controller.undo();
    QCOMPARE(controller.document().morphs[0].panelCategory, static_cast<uint8_t>(2));

    // Bounds check for panel: [0, 4]
    QVERIFY(!model->setData(panelIndex, -1));
    QVERIFY(!model->setData(panelIndex, 5));

    // 4. MorphType
    QModelIndex typeIndex = model->index(0, PmxMorphTableModel::ColMorphType);
    QCOMPARE(model->data(typeIndex).toInt(), 1);
    QVERIFY(model->setData(typeIndex, 0));
    QCOMPARE(controller.document().morphs[0].morphType, static_cast<uint8_t>(0));
    controller.undo();
    QCOMPARE(controller.document().morphs[0].morphType, static_cast<uint8_t>(1));

    // Bounds check for morphType: [0, 10]
    QVERIFY(!model->setData(typeIndex, -1));
    QVERIFY(!model->setData(typeIndex, 11));

    // 5. OffsetCount is read-only
    QModelIndex offsetIndex = model->index(0, PmxMorphTableModel::ColOffsetCount);
    QCOMPARE(model->data(offsetIndex).toInt(), 1);
    QVERIFY(!(model->flags(offsetIndex) & Qt::ItemIsEditable));
    QVERIFY(!model->setData(offsetIndex, 99));
}

void TstPmxUndoModels::testDisplayFrameModelCellEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.displayFrameModel();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->columnCount(), 3);

    // 1. Name JP edit
    QModelIndex nameIndex = model->index(1, PmxDisplayFrameModel::ColName);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("表情"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("目表情")));
    QCOMPARE(controller.document().displayFrames[1].nameJp, QStringLiteral("目表情"));

    controller.undo();
    QCOMPARE(controller.document().displayFrames[1].nameJp, QStringLiteral("表情"));

    controller.redo();
    QCOMPARE(controller.document().displayFrames[1].nameJp, QStringLiteral("目表情"));

    // 2. Name EN edit
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::English);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("Facial"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("EyesFacial")));
    QCOMPARE(controller.document().displayFrames[1].nameEn, QStringLiteral("EyesFacial"));
    QCOMPARE(controller.document().displayFrames[1].nameJp, QStringLiteral("目表情"));

    controller.undo();
    QCOMPARE(controller.document().displayFrames[1].nameEn, QStringLiteral("Facial"));
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::Japanese);

    // 3. Special Flag
    QModelIndex specialIndex = model->index(1, PmxDisplayFrameModel::ColSpecial);
    QCOMPARE(model->data(specialIndex).toInt(), 0);
    QVERIFY(model->setData(specialIndex, 1));
    QCOMPARE(controller.document().displayFrames[1].specialFlag, static_cast<uint8_t>(1));
    controller.undo();
    QCOMPARE(controller.document().displayFrames[1].specialFlag, static_cast<uint8_t>(0));

    // 4. ItemCount is read-only
    QModelIndex countIndex = model->index(1, PmxDisplayFrameModel::ColItemCount);
    QCOMPARE(model->data(countIndex).toInt(), 1);
    QVERIFY(!(model->flags(countIndex) & Qt::ItemIsEditable));
    QVERIFY(!model->setData(countIndex, 50));
}

void TstPmxUndoModels::testRigidBodyModelCellEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.rigidBodyModel();
    QCOMPARE(model->rowCount(), 2);
    QCOMPARE(model->columnCount(), 20);

    // 1. Name JP / EN
    QModelIndex nameIndex = model->index(0, PmxRigidBodyTableModel::ColName);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("頭"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("頭部")));
    QCOMPARE(controller.document().rigidBodies[0].nameJp, QStringLiteral("頭部"));

    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].nameJp, QStringLiteral("頭"));

    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::English);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("Head"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("HeadBoneRigid")));
    QCOMPARE(controller.document().rigidBodies[0].nameEn, QStringLiteral("HeadBoneRigid"));
    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].nameEn, QStringLiteral("Head"));
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::Japanese);

    // 2. Shape
    QModelIndex shapeIndex = model->index(0, PmxRigidBodyTableModel::ColShape);
    QCOMPARE(model->data(shapeIndex).toInt(), 0);
    QVERIFY(model->setData(shapeIndex, 2));
    QCOMPARE(controller.document().rigidBodies[0].shape, static_cast<uint8_t>(2));
    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].shape, static_cast<uint8_t>(0));

    // 3. Size
    QModelIndex sizeIndex = model->index(0, PmxRigidBodyTableModel::ColSizeX);
    QCOMPARE(model->data(sizeIndex).toFloat(), 1.0f);
    QVERIFY(model->setData(sizeIndex, 2.5f));
    QCOMPARE(controller.document().rigidBodies[0].size.x, 2.5f);
    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].size.x, 1.0f);

    // 4. Position
    QModelIndex posIndex = model->index(0, PmxRigidBodyTableModel::ColPosY);
    QCOMPARE(model->data(posIndex).toFloat(), 16.0f);
    QVERIFY(model->setData(posIndex, 18.0f));
    QCOMPARE(controller.document().rigidBodies[0].position.y, 18.0f);
    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].position.y, 16.0f);

    // 5. PhysicsMode
    QModelIndex modeIndex = model->index(0, PmxRigidBodyTableModel::ColPhysicsMode);
    QCOMPARE(model->data(modeIndex).toInt(), 0);
    QVERIFY(model->setData(modeIndex, 1));
    QCOMPARE(controller.document().rigidBodies[0].physicsMode, static_cast<uint8_t>(1));
    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].physicsMode, static_cast<uint8_t>(0));
}

void TstPmxUndoModels::testJointModelCellEditUndoRedo() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.jointModel();
    QCOMPARE(model->rowCount(), 1);
    QCOMPARE(model->columnCount(), 28);

    // 1. Name JP / EN
    QModelIndex nameIndex = model->index(0, PmxJointTableModel::ColName);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("ジョイント0"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("首ジョイント")));
    QCOMPARE(controller.document().joints[0].nameJp, QStringLiteral("首ジョイント"));
    controller.undo();
    QCOMPARE(controller.document().joints[0].nameJp, QStringLiteral("ジョイント0"));

    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::English);
    QCOMPARE(model->data(nameIndex).toString(), QStringLiteral("Joint0"));
    QVERIFY(model->setData(nameIndex, QStringLiteral("NeckJoint")));
    QCOMPARE(controller.document().joints[0].nameEn, QStringLiteral("NeckJoint"));
    controller.undo();
    QCOMPARE(controller.document().joints[0].nameEn, QStringLiteral("Joint0"));
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::Japanese);

    // 2. Limits
    QModelIndex minXIndex = model->index(0, PmxJointTableModel::ColPosMinX);
    QVERIFY(model->setData(minXIndex, -2.0f));
    QCOMPARE(controller.document().joints[0].linearLowerLimit.x, -2.0f);
    controller.undo();
    QCOMPARE(controller.document().joints[0].linearLowerLimit.x, -0.1f);

    QModelIndex maxXIndex = model->index(0, PmxJointTableModel::ColPosMaxX);
    QVERIFY(model->setData(maxXIndex, 2.0f));
    QCOMPARE(controller.document().joints[0].linearUpperLimit.x, 2.0f);
    controller.undo();
    QCOMPARE(controller.document().joints[0].linearUpperLimit.x, 0.1f);

    QModelIndex rotMinIndex = model->index(0, PmxJointTableModel::ColRotMinX);
    QVERIFY(model->setData(rotMinIndex, -1.0f));
    QCOMPARE(controller.document().joints[0].angularLowerLimit.x, -1.0f);
    controller.undo();
    QCOMPARE(controller.document().joints[0].angularLowerLimit.x, -0.5f);

    // 3. Springs
    QModelIndex springPosIndex = model->index(0, PmxJointTableModel::ColSpringPosX);
    QVERIFY(model->setData(springPosIndex, 50.0f));
    QCOMPARE(controller.document().joints[0].springLinearStiffness.x, 50.0f);
    controller.undo();
    QCOMPARE(controller.document().joints[0].springLinearStiffness.x, 10.0f);

    QModelIndex springRotIndex = model->index(0, PmxJointTableModel::ColSpringRotX);
    QVERIFY(model->setData(springRotIndex, 250.0f));
    QCOMPARE(controller.document().joints[0].springAngularStiffness.x, 250.0f);
    controller.undo();
    QCOMPARE(controller.document().joints[0].springAngularStiffness.x, 100.0f);
}

void TstPmxUndoModels::testBatchVertexOperations() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.vertexModel();
    QCOMPARE(model->rowCount(), 3);

    // 1. Add 10.0 to PosX on rows 0, 1, 2
    bool ok = model->applyBatchOperation({0, 1, 2}, PmxVertexTableModel::ColPosX, QStringLiteral("+"), 10.0);
    QVERIFY(ok);
    QCOMPARE(controller.document().vertices[0].position.x, 10.0f);
    QCOMPARE(controller.document().vertices[1].position.x, 11.0f);
    QCOMPARE(controller.document().vertices[2].position.x, 12.0f);

    // Undo
    controller.undo();
    QCOMPARE(controller.document().vertices[0].position.x, 0.0f);
    QCOMPARE(controller.document().vertices[1].position.x, 1.0f);
    QCOMPARE(controller.document().vertices[2].position.x, 2.0f);

    // Redo
    controller.redo();
    QCOMPARE(controller.document().vertices[0].position.x, 10.0f);
    QCOMPARE(controller.document().vertices[1].position.x, 11.0f);
    QCOMPARE(controller.document().vertices[2].position.x, 12.0f);

    // 2. Multiply PosX by 2.0 on rows 0, 1
    ok = model->applyBatchOperation({0, 1}, PmxVertexTableModel::ColPosX, QStringLiteral("*"), 2.0);
    QVERIFY(ok);
    QCOMPARE(controller.document().vertices[0].position.x, 20.0f);
    QCOMPARE(controller.document().vertices[1].position.x, 22.0f);
    QCOMPARE(controller.document().vertices[2].position.x, 12.0f); // untouched

    controller.undo();
    QCOMPARE(controller.document().vertices[0].position.x, 10.0f);
    QCOMPARE(controller.document().vertices[1].position.x, 11.0f);

    // 3. Batch protection tests:
    // Reject NaN
    QVERIFY(!model->applyBatchOperation({0}, PmxVertexTableModel::ColPosX, QStringLiteral("+"),
                                        std::numeric_limits<double>::quiet_NaN()));
    // Reject Inf
    QVERIFY(!model->applyBatchOperation({0}, PmxVertexTableModel::ColPosX, QStringLiteral("+"),
                                        std::numeric_limits<double>::infinity()));
    // Reject ColIndex
    QVERIFY(!model->applyBatchOperation({0}, PmxVertexTableModel::ColIndex, QStringLiteral("+"), 1.0));
    // Reject out of bounds column
    QVERIFY(!model->applyBatchOperation({0}, -1, QStringLiteral("+"), 1.0));
    QVERIFY(!model->applyBatchOperation({0}, 99, QStringLiteral("+"), 1.0));
    // Reject no-op
    QVERIFY(!model->applyBatchOperation({0}, PmxVertexTableModel::ColPosX, QStringLiteral("*"), 1.0));
}

void TstPmxUndoModels::testBatchRigidBodyOperations() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.rigidBodyModel();
    QCOMPARE(model->rowCount(), 2);

    // Initial values:
    // rb0: size=(1,1,1), mass=1.0, linearDamping=0.5
    // rb1: size=(0.5,1,0.5), mass=0.5, linearDamping=0.5
    QCOMPARE(controller.document().rigidBodies[0].size.x, 1.0f);
    QCOMPARE(controller.document().rigidBodies[1].size.x, 0.5f);
    QCOMPARE(controller.document().rigidBodies[0].mass, 1.0f);
    QCOMPARE(controller.document().rigidBodies[1].mass, 0.5f);
    QCOMPARE(controller.document().rigidBodies[0].linearDamping, 0.5f);
    QCOMPARE(controller.document().rigidBodies[1].linearDamping, 0.5f);

    // 1. Validation: Reject non-numeric column (ColName)
    bool failName = model->applyBatchOperation({0, 1}, PmxRigidBodyTableModel::ColName, QStringLiteral("="), 5.0);
    QCOMPARE(failName, false);
    QCOMPARE(controller.document().rigidBodies[0].nameJp, QStringLiteral("頭"));
    QCOMPARE(controller.document().rigidBodies[1].nameJp, QStringLiteral("髪"));

    // 2. Validation: Reject NaN and Inf
    bool failNan = model->applyBatchOperation({0, 1}, PmxRigidBodyTableModel::ColMass, QStringLiteral("+"),
                                              std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(failNan, false);
    bool failInf = model->applyBatchOperation({0, 1}, PmxRigidBodyTableModel::ColMass, QStringLiteral("+"),
                                              std::numeric_limits<double>::infinity());
    QCOMPARE(failInf, false);

    // 3. Validation: Reject unknown operator
    bool failOp = model->applyBatchOperation({0, 1}, PmxRigidBodyTableModel::ColMass, QStringLiteral("?"), 2.0);
    QCOMPARE(failOp, false);

    // 4. Validation: Reject no-op (e.g. multiply by 1.0)
    bool failNoChange = model->applyBatchOperation({0}, PmxRigidBodyTableModel::ColMass, QStringLiteral("*"), 1.0);
    QCOMPARE(failNoChange, false);
    QCOMPARE(controller.undoStack()->count(), 0);

    // 5. Batch addition on SizeX across rows {0, 1} (+2.0)
    bool ok = model->applyBatchOperation({0, 1}, PmxRigidBodyTableModel::ColSizeX, QStringLiteral("+"), 2.0);
    QVERIFY(ok);
    QCOMPARE(controller.undoStack()->count(), 1);
    QCOMPARE(controller.document().rigidBodies[0].size.x, 3.0f);
    QCOMPARE(controller.document().rigidBodies[1].size.x, 2.5f);

    // Undo batch addition
    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].size.x, 1.0f);
    QCOMPARE(controller.document().rigidBodies[1].size.x, 0.5f);
    QCOMPARE(controller.isModified(), false);

    // Redo batch addition
    controller.redo();
    QCOMPARE(controller.document().rigidBodies[0].size.x, 3.0f);
    QCOMPARE(controller.document().rigidBodies[1].size.x, 2.5f);

    // 6. Batch multiplication on Mass across rows {0, 1} (*3.0)
    ok = model->applyBatchOperation({0, 1}, PmxRigidBodyTableModel::ColMass, QStringLiteral("*"), 3.0);
    QVERIFY(ok);
    QCOMPARE(controller.document().rigidBodies[0].mass, 3.0f);
    QCOMPARE(controller.document().rigidBodies[1].mass, 1.5f);

    // Undo batch multiplication
    controller.undo();
    QCOMPARE(controller.document().rigidBodies[0].mass, 1.0f);
    QCOMPARE(controller.document().rigidBodies[1].mass, 0.5f);

    // Redo batch multiplication
    controller.redo();
    QCOMPARE(controller.document().rigidBodies[0].mass, 3.0f);
    QCOMPARE(controller.document().rigidBodies[1].mass, 1.5f);

    // 7. Batch addition on LinearDamping across rows {0, 1} (+0.25)
    ok = model->applyBatchOperation({0, 1}, PmxRigidBodyTableModel::ColLinearDamping, QStringLiteral("+"), 0.25);
    QVERIFY(ok);
    QCOMPARE(controller.document().rigidBodies[0].linearDamping, 0.75f);
    QCOMPARE(controller.document().rigidBodies[1].linearDamping, 0.75f);

    // Undo all back to clean state
    controller.undo(); // undo damping
    QCOMPARE(controller.document().rigidBodies[0].linearDamping, 0.5f);
    QCOMPARE(controller.document().rigidBodies[1].linearDamping, 0.5f);

    controller.undo(); // undo mass
    QCOMPARE(controller.document().rigidBodies[0].mass, 1.0f);
    QCOMPARE(controller.document().rigidBodies[1].mass, 0.5f);

    controller.undo(); // undo size
    QCOMPARE(controller.document().rigidBodies[0].size.x, 1.0f);
    QCOMPARE(controller.document().rigidBodies[1].size.x, 0.5f);
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testBatchJointOperations() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    auto* model = controller.jointModel();
    QCOMPARE(model->rowCount(), 1);

    // Initial values for j0:
    // linearLowerLimit = vec3(-0.1f)
    // springLinearStiffness = vec3(10.0f)
    // angularUpperLimit = vec3(0.5f)
    QCOMPARE(controller.document().joints[0].linearLowerLimit.x, -0.1f);
    QCOMPARE(controller.document().joints[0].springLinearStiffness.x, 10.0f);
    QCOMPARE(controller.document().joints[0].angularUpperLimit.x, 0.5f);

    // 1. Validation: Reject non-numeric column (ColName)
    bool failName = model->applyBatchOperation({0}, PmxJointTableModel::ColName, QStringLiteral("="), 5.0);
    QCOMPARE(failName, false);
    QCOMPARE(controller.document().joints[0].nameJp, QStringLiteral("ジョイント0"));

    // 2. Validation: Reject NaN and Inf
    bool failNan = model->applyBatchOperation({0}, PmxJointTableModel::ColSpringPosX, QStringLiteral("+"),
                                              std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(failNan, false);
    bool failInf = model->applyBatchOperation({0}, PmxJointTableModel::ColSpringPosX, QStringLiteral("+"),
                                              std::numeric_limits<double>::infinity());
    QCOMPARE(failInf, false);

    // 3. Validation: Reject unknown operator
    bool failOp = model->applyBatchOperation({0}, PmxJointTableModel::ColSpringPosX, QStringLiteral("?"), 2.0);
    QCOMPARE(failOp, false);

    // 4. Validation: Reject no-op (e.g. multiply by 1.0)
    bool failNoChange = model->applyBatchOperation({0}, PmxJointTableModel::ColSpringPosX, QStringLiteral("*"), 1.0);
    QCOMPARE(failNoChange, false);
    QCOMPARE(controller.undoStack()->count(), 0);

    // 5. Batch operation on linear limit column (ColPosMinX) (+0.5)
    bool ok = model->applyBatchOperation({0}, PmxJointTableModel::ColPosMinX, QStringLiteral("+"), 0.5);
    QVERIFY(ok);
    QCOMPARE(controller.undoStack()->count(), 1);
    QCOMPARE(controller.document().joints[0].linearLowerLimit.x, 0.4f);

    // Undo linear limit
    controller.undo();
    QCOMPARE(controller.document().joints[0].linearLowerLimit.x, -0.1f);
    QCOMPARE(controller.isModified(), false);

    // Redo linear limit
    controller.redo();
    QCOMPARE(controller.document().joints[0].linearLowerLimit.x, 0.4f);

    // 6. Batch operation on spring column (ColSpringPosX) (*2.5)
    ok = model->applyBatchOperation({0}, PmxJointTableModel::ColSpringPosX, QStringLiteral("*"), 2.5);
    QVERIFY(ok);
    QCOMPARE(controller.document().joints[0].springLinearStiffness.x, 25.0f);

    // Undo spring
    controller.undo();
    QCOMPARE(controller.document().joints[0].springLinearStiffness.x, 10.0f);

    // Redo spring
    controller.redo();
    QCOMPARE(controller.document().joints[0].springLinearStiffness.x, 25.0f);

    // 7. Batch operation on angular limit column (ColRotMaxX) (+0.2)
    ok = model->applyBatchOperation({0}, PmxJointTableModel::ColRotMaxX, QStringLiteral("+"), 0.2);
    QVERIFY(ok);
    QCOMPARE(controller.document().joints[0].angularUpperLimit.x, 0.7f);

    // Undo all back to clean state
    controller.undo(); // undo angular limit
    QCOMPARE(controller.document().joints[0].angularUpperLimit.x, 0.5f);

    controller.undo(); // undo spring
    QCOMPARE(controller.document().joints[0].springLinearStiffness.x, 10.0f);

    controller.undo(); // undo linear limit
    QCOMPARE(controller.document().joints[0].linearLowerLimit.x, -0.1f);
    QCOMPARE(controller.isModified(), false);
}

void TstPmxUndoModels::testAllModelsOutOfBoundsAccess() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    QList<QAbstractTableModel*> models = {
        controller.materialModel(),
        controller.boneModel(),
        controller.vertexModel(),
        controller.morphModel(),
        controller.displayFrameModel(),
        controller.rigidBodyModel(),
        controller.jointModel()
    };

    for (auto* m : models) {
        QVERIFY(m != nullptr);
        QVERIFY(!m->data(m->index(-1, 0)).isValid());
        QVERIFY(!m->data(m->index(9999, 0)).isValid());
        QVERIFY(!m->data(m->index(0, -1)).isValid());
        QVERIFY(!m->data(m->index(0, 9999)).isValid());
        QCOMPARE(m->flags(m->index(-1, 0)), Qt::NoItemFlags);
        QCOMPARE(m->flags(m->index(9999, 0)), Qt::NoItemFlags);
        QVERIFY(!m->setData(m->index(-1, 0), 1));
        QVERIFY(!m->setData(m->index(9999, 0), 1));
    }

    auto* infoModel = controller.infoModel();
    QVERIFY(!infoModel->data(infoModel->index(-1, 0)).isValid());
    QVERIFY(!infoModel->data(infoModel->index(9999, 0)).isValid());
    QVERIFY(!infoModel->data(infoModel->index(0, -1)).isValid());
    QVERIFY(!infoModel->data(infoModel->index(0, 9999)).isValid());
}

void TstPmxUndoModels::testLanguageSwitchingOnAllModels() {
    pmx::PmxDocumentController controller;
    QVERIFY(controller.loadFromFile(m_sampleFilePath));

    // 1. Japanese
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::Japanese);

    QCOMPARE(controller.materialModel()->headerData(PmxMaterialTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("材質名"));
    QCOMPARE(controller.boneModel()->headerData(PmxBoneTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("ボーン名"));
    QCOMPARE(controller.vertexModel()->headerData(PmxVertexTableModel::ColIndex, Qt::Horizontal).toString(),
             QStringLiteral("頂点Index"));
    QCOMPARE(controller.vertexModel()->headerData(PmxVertexTableModel::ColPosX, Qt::Horizontal).toString(),
             QStringLiteral("位置 X"));
    QCOMPARE(controller.morphModel()->headerData(PmxMorphTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("モーフ名"));
    QCOMPARE(controller.morphModel()->headerData(PmxMorphTableModel::ColPanel, Qt::Horizontal).toString(),
             QStringLiteral("パネル"));
    QCOMPARE(controller.displayFrameModel()->headerData(PmxDisplayFrameModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("表示枠名"));
    QCOMPARE(controller.displayFrameModel()->headerData(PmxDisplayFrameModel::ColSpecial, Qt::Horizontal).toString(),
             QStringLiteral("特殊枠"));
    QCOMPARE(controller.rigidBodyModel()->headerData(PmxRigidBodyTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("剛体名"));
    QCOMPARE(controller.rigidBodyModel()->headerData(PmxRigidBodyTableModel::ColBoneIndex, Qt::Horizontal).toString(),
             QStringLiteral("関連ボーン"));
    QCOMPARE(controller.jointModel()->headerData(PmxJointTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Joint名"));
    QCOMPARE(controller.jointModel()->headerData(PmxJointTableModel::ColJointType, Qt::Horizontal).toString(),
             QStringLiteral("接続方式"));

    // Check Japanese data values
    QCOMPARE(controller.morphModel()->data(controller.morphModel()->index(0, PmxMorphTableModel::ColName)).toString(),
             QStringLiteral("まばたき"));
    QCOMPARE(controller.displayFrameModel()->data(controller.displayFrameModel()->index(1, PmxDisplayFrameModel::ColName)).toString(),
             QStringLiteral("表情"));
    QCOMPARE(controller.rigidBodyModel()->data(controller.rigidBodyModel()->index(0, PmxRigidBodyTableModel::ColName)).toString(),
             QStringLiteral("頭"));
    QCOMPARE(controller.jointModel()->data(controller.jointModel()->index(0, PmxJointTableModel::ColName)).toString(),
             QStringLiteral("ジョイント0"));

    // 2. English
    controller.setDisplayLanguage(PmxDocumentController::DisplayLanguage::English);

    QCOMPARE(controller.materialModel()->headerData(PmxMaterialTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Material Name"));
    QCOMPARE(controller.boneModel()->headerData(PmxBoneTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Bone Name"));
    QCOMPARE(controller.vertexModel()->headerData(PmxVertexTableModel::ColIndex, Qt::Horizontal).toString(),
             QStringLiteral("Index"));
    QCOMPARE(controller.vertexModel()->headerData(PmxVertexTableModel::ColPosX, Qt::Horizontal).toString(),
             QStringLiteral("Position X"));
    QCOMPARE(controller.morphModel()->headerData(PmxMorphTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Morph Name"));
    QCOMPARE(controller.morphModel()->headerData(PmxMorphTableModel::ColPanel, Qt::Horizontal).toString(),
             QStringLiteral("Panel"));
    QCOMPARE(controller.displayFrameModel()->headerData(PmxDisplayFrameModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Frame Name"));
    QCOMPARE(controller.displayFrameModel()->headerData(PmxDisplayFrameModel::ColSpecial, Qt::Horizontal).toString(),
             QStringLiteral("Special Flag"));
    QCOMPARE(controller.rigidBodyModel()->headerData(PmxRigidBodyTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Rigid Body Name"));
    QCOMPARE(controller.rigidBodyModel()->headerData(PmxRigidBodyTableModel::ColBoneIndex, Qt::Horizontal).toString(),
             QStringLiteral("Bone Index"));
    QCOMPARE(controller.jointModel()->headerData(PmxJointTableModel::ColName, Qt::Horizontal).toString(),
             QStringLiteral("Joint Name"));
    QCOMPARE(controller.jointModel()->headerData(PmxJointTableModel::ColJointType, Qt::Horizontal).toString(),
             QStringLiteral("Joint Type"));

    // Check English data values
    QCOMPARE(controller.morphModel()->data(controller.morphModel()->index(0, PmxMorphTableModel::ColName)).toString(),
             QStringLiteral("Blink"));
    QCOMPARE(controller.displayFrameModel()->data(controller.displayFrameModel()->index(1, PmxDisplayFrameModel::ColName)).toString(),
             QStringLiteral("Facial"));
    QCOMPARE(controller.rigidBodyModel()->data(controller.rigidBodyModel()->index(0, PmxRigidBodyTableModel::ColName)).toString(),
             QStringLiteral("Head"));
    QCOMPARE(controller.jointModel()->data(controller.jointModel()->index(0, PmxJointTableModel::ColName)).toString(),
             QStringLiteral("Joint0"));
}

QTEST_MAIN(TstPmxUndoModels)
#include "tst_pmx_undo_models.moc"
