#include "viewport/PmxCamera.h"
#include "viewport/PmxViewportItem.h"
#include "models/PmxDocumentController.h"
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

#include "viewport/PmxTextureCache.h"

#include <QSignalSpy>
#include <QTest>
#include <QUndoCommand>
#include <QFile>
#include <rhi/qshader.h>
#include <cmath>
#include <limits>

using namespace pmx;

class tst_pmx_viewport : public QObject {
    Q_OBJECT

private slots:
    void testCameraDefaults();
    void testCameraYawWrapping();
    void testCameraPitchClamping();
    void testCameraNearFarPlaneClamping();
    void testCameraPanMath();
    void testHandednessConversions();
    void testFilterValidTriangleIndices();
    void testViewportItemControllerAttachment();
    void testAppBridgeModelExposure();
    void testAppBridgeBatchOperation();
    void testAppBridgeBatchOperationBoundaries();
    void testAppBridgeLanguageToggle();
    void testShaderResourcesLoaded();
    void testEdgeExtrusionMath();
    void testMeshVertexEdgeScalePropagation();
    void testSphereUVCalculation();
    void testToonLookupMath();
    void testTextureCacheFallbacks();
    void testMaterialBatchPartitioning();
    void testEdgeMaterialBatchFiltering();
};

void tst_pmx_viewport::testCameraDefaults() {
    PmxCamera camera;
    QCOMPARE(camera.distance(), 35.0f);
    QCOMPARE(camera.yaw(), 0.0f);
    QCOMPARE(camera.pitch(), 0.0f);
    QCOMPARE(camera.fov(), 45.0f);
    QCOMPARE(camera.nearPlane(), 0.1f);
    QCOMPARE(camera.farPlane(), 1000.0f);
    QVERIFY(camera.farPlane() > camera.nearPlane());

    glm::mat4 view = camera.viewMatrix();
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            QVERIFY(std::isfinite(view[col][row]));
        }
    }

    glm::mat4 proj = camera.projectionMatrix(16.0f / 9.0f);
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            QVERIFY(std::isfinite(proj[col][row]));
        }
    }
}

void tst_pmx_viewport::testCameraYawWrapping() {
    PmxCamera camera;
    camera.setYaw(370.0f);
    // std::remainder(370.0f, 360.0f) is 10.0f
    QVERIFY(std::abs(camera.yaw() - 10.0f) < 0.001f);

    camera.setYaw(720.0f);
    QVERIFY(std::abs(camera.yaw()) < 0.001f);

    camera.setYaw(-190.0f);
    // std::remainder(-190.0f, 360.0f) is 170.0f
    QVERIFY(std::abs(camera.yaw() - 170.0f) < 0.001f);

    camera.orbit(365.0f, 0.0f);
    QVERIFY(std::isfinite(camera.yaw()));

    // Verify NaN rejection
    float oldYaw = camera.yaw();
    camera.setYaw(std::numeric_limits<float>::quiet_NaN());
    QCOMPARE(camera.yaw(), oldYaw);
}

void tst_pmx_viewport::testCameraPitchClamping() {
    PmxCamera camera;
    camera.setPitch(120.0f);
    QCOMPARE(camera.pitch(), 89.0f);

    camera.setPitch(-150.0f);
    QCOMPARE(camera.pitch(), -89.0f);

    camera.setPitch(45.0f);
    QCOMPARE(camera.pitch(), 45.0f);

    camera.orbit(0.0f, 100.0f);
    QCOMPARE(camera.pitch(), 89.0f);

    // Verify NaN rejection
    camera.setPitch(std::numeric_limits<float>::quiet_NaN());
    QCOMPARE(camera.pitch(), 89.0f);
}

void tst_pmx_viewport::testCameraNearFarPlaneClamping() {
    PmxCamera camera;
    float initialNear = camera.nearPlane();
    float initialFar = camera.farPlane();

    // Invalid near plane (negative, zero, or >= farPlane)
    camera.setNearPlane(-5.0f);
    QCOMPARE(camera.nearPlane(), initialNear);

    camera.setNearPlane(0.0f);
    QCOMPARE(camera.nearPlane(), initialNear);

    camera.setNearPlane(initialFar + 10.0f);
    QCOMPARE(camera.nearPlane(), initialNear);

    // Valid near plane
    camera.setNearPlane(0.5f);
    QCOMPARE(camera.nearPlane(), 0.5f);

    // Invalid far plane (<= nearPlane or negative)
    camera.setFarPlane(0.2f);
    QCOMPARE(camera.farPlane(), initialFar);

    camera.setFarPlane(-10.0f);
    QCOMPARE(camera.farPlane(), initialFar);

    // Valid far plane
    camera.setFarPlane(2000.0f);
    QCOMPARE(camera.farPlane(), 2000.0f);

    // Safe clamps in projectionMatrix
    glm::mat4 proj = camera.projectionMatrix(1.0f);
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            QVERIFY(std::isfinite(proj[col][row]));
        }
    }
}

void tst_pmx_viewport::testCameraPanMath() {
    PmxCamera camera;
    camera.setTarget(glm::vec3(0.0f));
    camera.setYaw(0.0f);
    camera.setPitch(0.0f);
    camera.setDistance(100.0f);

    camera.pan(10.0f, 5.0f);
    QVERIFY(camera.target().x > 0.0f);
    QVERIFY(camera.target().y > 0.0f);

    camera.pan(-10.0f, -5.0f);
    QVERIFY(std::abs(camera.target().x) < 0.001f);
    QVERIFY(std::abs(camera.target().y) < 0.001f);
}

void tst_pmx_viewport::testHandednessConversions() {
    // 1. Position conversion
    glm::vec3 pLH(1.5f, 2.5f, 3.5f);
    glm::vec3 pRH = PmxCamera::toRightHanded(pLH);
    QCOMPARE(pRH.x, 1.5f);
    QCOMPARE(pRH.y, 2.5f);
    QCOMPARE(pRH.z, -3.5f);

    glm::vec3 pRoundTrip = PmxCamera::toLeftHanded(pRH);
    QCOMPARE(pRoundTrip, pLH);

    // 2. Euler rotation conversion
    glm::vec3 rLH(0.1f, 0.2f, 0.3f);
    glm::vec3 rRH = PmxCamera::toRightHandedEuler(rLH);
    QCOMPARE(rRH.x, -0.1f);
    QCOMPARE(rRH.y, -0.2f);
    QCOMPARE(rRH.z, 0.3f);

    glm::vec3 rRoundTrip = PmxCamera::toLeftHandedEuler(rRH);
    QCOMPARE(rRoundTrip, rLH);

    // 3. Quaternion conversion
    glm::quat qLH(0.7071f, 0.1f, 0.2f, 0.3f); // (w, x, y, z)
    glm::quat qRH = PmxCamera::toRightHandedQuat(qLH);
    QCOMPARE(qRH.w, qLH.w);
    QCOMPARE(qRH.x, -qLH.x);
    QCOMPARE(qRH.y, -qLH.y);
    QCOMPARE(qRH.z, qLH.z);

    glm::quat qRoundTrip = PmxCamera::toLeftHandedQuat(qRH);
    QCOMPARE(qRoundTrip.w, qLH.w);
    QCOMPARE(qRoundTrip.x, qLH.x);
    QCOMPARE(qRoundTrip.y, qLH.y);
    QCOMPARE(qRoundTrip.z, qLH.z);
}

void tst_pmx_viewport::testFilterValidTriangleIndices() {
    // 1. Valid indices
    std::vector<int32_t> rawValid = { 0, 1, 2, 2, 1, 3 };
    auto filteredValid = PmxViewportItem::filterValidTriangleIndices(rawValid, 4);
    QCOMPARE(filteredValid.size(), 6u);
    QCOMPARE(filteredValid[0], 0u);
    QCOMPARE(filteredValid[1], 1u);
    QCOMPARE(filteredValid[2], 2u);
    QCOMPARE(filteredValid[3], 2u);
    QCOMPARE(filteredValid[4], 1u);
    QCOMPARE(filteredValid[5], 3u);

    // 2. Corrupted indices: negative index in first triangle
    std::vector<int32_t> rawWithNegative = { 0, -1, 2, 1, 2, 0 };
    auto filteredNeg = PmxViewportItem::filterValidTriangleIndices(rawWithNegative, 3);
    QCOMPARE(filteredNeg.size(), 3u);
    QCOMPARE(filteredNeg[0], 1u);
    QCOMPARE(filteredNeg[1], 2u);
    QCOMPARE(filteredNeg[2], 0u);

    // 3. Out-of-bounds index: index >= vertexCount
    std::vector<int32_t> rawWithOob = { 0, 1, 2, 0, 10, 2 };
    auto filteredOob = PmxViewportItem::filterValidTriangleIndices(rawWithOob, 5);
    QCOMPARE(filteredOob.size(), 3u);
    QCOMPARE(filteredOob[0], 0u);
    QCOMPARE(filteredOob[1], 1u);
    QCOMPARE(filteredOob[2], 2u);

    // 4. Incomplete trailing triplet
    std::vector<int32_t> rawTrailing = { 0, 1, 2, 1, 2 };
    auto filteredTrailing = PmxViewportItem::filterValidTriangleIndices(rawTrailing, 3);
    QCOMPARE(filteredTrailing.size(), 3u);

    // 5. Zero vertex count
    auto filteredZero = PmxViewportItem::filterValidTriangleIndices(rawValid, 0);
    QVERIFY(filteredZero.empty());
}

void tst_pmx_viewport::testViewportItemControllerAttachment() {
    PmxDocumentController controller;
    PmxViewportItem item;

    quint64 rev0 = item.modelRevision();
    item.setController(&controller);
    quint64 rev1 = item.modelRevision();
    QVERIFY(rev1 > rev0);

    // Push an undo command and verify modelRevision increments
    controller.undoStack()->push(new QUndoCommand());
    quint64 rev2 = item.modelRevision();
    QVERIFY(rev2 > rev1);

    // Another command
    controller.undoStack()->push(new QUndoCommand());
    quint64 rev3 = item.modelRevision();
    QVERIFY(rev3 > rev2);

    // Undo operation increments revision as well
    controller.undoStack()->undo();
    quint64 rev4 = item.modelRevision();
    QVERIFY(rev4 > rev3);

    // Redo operation increments revision
    controller.undoStack()->redo();
    quint64 rev5 = item.modelRevision();
    QVERIFY(rev5 > rev4);
}

void tst_pmx_viewport::testAppBridgeModelExposure() {
    PmxAppBridge bridge;
    QVERIFY(bridge.controller() != nullptr);

    // Verify all 8 models are exposed and non-null
    QVERIFY(bridge.infoModel() != nullptr);
    QVERIFY(bridge.vertexModel() != nullptr);
    QVERIFY(bridge.materialModel() != nullptr);
    QVERIFY(bridge.boneModel() != nullptr);
    QVERIFY(bridge.morphModel() != nullptr);
    QVERIFY(bridge.displayFrameModel() != nullptr);
    QVERIFY(bridge.rigidBodyModel() != nullptr);
    QVERIFY(bridge.jointModel() != nullptr);

    // Verify they are correctly attached to the controller
    QCOMPARE(bridge.infoModel(), bridge.controller()->infoModel());
    QCOMPARE(bridge.vertexModel(), bridge.controller()->vertexModel());
    QCOMPARE(bridge.materialModel(), bridge.controller()->materialModel());
    QCOMPARE(bridge.boneModel(), bridge.controller()->boneModel());
    QCOMPARE(bridge.morphModel(), bridge.controller()->morphModel());
    QCOMPARE(bridge.displayFrameModel(), bridge.controller()->displayFrameModel());
    QCOMPARE(bridge.rigidBodyModel(), bridge.controller()->rigidBodyModel());
    QCOMPARE(bridge.jointModel(), bridge.controller()->jointModel());
}

void tst_pmx_viewport::testAppBridgeBatchOperation() {
    PmxAppBridge bridge;

    // Create a sample document with vertices
    pmx::PmxDocument doc;
    for (int i = 0; i < 3; ++i) {
        pmx::PmxVertex v;
        v.position = glm::vec3(static_cast<float>(i * 10), static_cast<float>(i * 5), 0.0f);
        v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v.uv = glm::vec2(0.0f, 0.0f);
        v.weightType = pmx::WeightType::BDEF1;
        v.boneIndices[0] = 0;
        v.boneWeights[0] = 1.0f;
        doc.vertices.push_back(v);
        doc.faceIndices.push_back(i);
    }

    bridge.controller()->setDocumentInternal(doc);
    bridge.controller()->resetAllModels();

    QCOMPARE(bridge.rowCount(4 /* Vertex */), 3);

    // Verify initial positions
    auto* vModel = bridge.vertexModel();
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 0.0);
    QCOMPARE(vModel->data(vModel->index(1, PmxVertexTableModel::ColPosX)).toDouble(), 10.0);
    QCOMPARE(vModel->data(vModel->index(2, PmxVertexTableModel::ColPosX)).toDouble(), 20.0);

    // Batch Add (+ 5.0) to row 0 and 1 on ColPosX
    bool ok = bridge.applyBatchOperation(
        4, /* tableId = 4 (Vertex) */
        {0, 1},
        PmxVertexTableModel::ColPosX,
        static_cast<int>(PmxBatchOperationCommand::BatchOp::Add), // 0
        5.0
    );
    QVERIFY(ok);

    // Verify positions changed
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 5.0);
    QCOMPARE(vModel->data(vModel->index(1, PmxVertexTableModel::ColPosX)).toDouble(), 15.0);
    QCOMPARE(vModel->data(vModel->index(2, PmxVertexTableModel::ColPosX)).toDouble(), 20.0); // unchanged

    // Verify Undo
    QVERIFY(bridge.canUndo());
    bridge.undo();
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 0.0);
    QCOMPARE(vModel->data(vModel->index(1, PmxVertexTableModel::ColPosX)).toDouble(), 10.0);
    QCOMPARE(vModel->data(vModel->index(2, PmxVertexTableModel::ColPosX)).toDouble(), 20.0);

    // Verify Redo
    QVERIFY(bridge.canRedo());
    bridge.redo();
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 5.0);
    QCOMPARE(vModel->data(vModel->index(1, PmxVertexTableModel::ColPosX)).toDouble(), 15.0);
    QCOMPARE(vModel->data(vModel->index(2, PmxVertexTableModel::ColPosX)).toDouble(), 20.0);

    // Also test Batch Subtract (- 2.0)
    bool okSub = bridge.applyBatchOperation(
        4,
        {0, 1},
        PmxVertexTableModel::ColPosX,
        static_cast<int>(PmxBatchOperationCommand::BatchOp::Subtract), // 1
        2.0
    );
    QVERIFY(okSub);
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 3.0);
    QCOMPARE(vModel->data(vModel->index(1, PmxVertexTableModel::ColPosX)).toDouble(), 13.0);

    // Also test Multiply (* 2.0)
    bool okMul = bridge.applyBatchOperation(
        4,
        {0},
        PmxVertexTableModel::ColPosX,
        static_cast<int>(PmxBatchOperationCommand::BatchOp::Multiply), // 2
        2.0
    );
    QVERIFY(okMul);
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 6.0);

    // Also test Assign (= 42.0)
    bool okAssign = bridge.applyBatchOperation(
        4,
        {2},
        PmxVertexTableModel::ColPosX,
        static_cast<int>(PmxBatchOperationCommand::BatchOp::Assign), // 3
        42.0
    );
    QVERIFY(okAssign);
    QCOMPARE(vModel->data(vModel->index(2, PmxVertexTableModel::ColPosX)).toDouble(), 42.0);
}

void tst_pmx_viewport::testAppBridgeBatchOperationBoundaries() {
    PmxAppBridge bridge;

    // Set up a document populated with 2 vertices, 2 materials, 2 bones, 2 rigid bodies, 2 joints
    pmx::PmxDocument doc;

    // Vertices
    for (int i = 0; i < 2; ++i) {
        pmx::PmxVertex v;
        v.position = glm::vec3(static_cast<float>(i * 10), 0.0f, 0.0f);
        doc.vertices.push_back(v);
        doc.faceIndices.push_back(i);
    }

    // Materials
    for (int i = 0; i < 2; ++i) {
        pmx::PmxMaterial m;
        m.nameJp = QStringLiteral("Mat%1").arg(i);
        m.edgeSize = 1.0f + static_cast<float>(i);
        doc.materials.push_back(m);
    }

    // Bones
    for (int i = 0; i < 2; ++i) {
        pmx::PmxBone b;
        b.nameJp = QStringLiteral("Bone%1").arg(i);
        b.position = glm::vec3(0.0f, static_cast<float>(i * 10), 0.0f);
        doc.bones.push_back(b);
    }

    // Rigid bodies
    for (int i = 0; i < 2; ++i) {
        pmx::PmxRigidBody rb;
        rb.nameJp = QStringLiteral("Body%1").arg(i);
        rb.mass = 1.0f + static_cast<float>(i);
        doc.rigidBodies.push_back(rb);
    }

    // Joints
    for (int i = 0; i < 2; ++i) {
        pmx::PmxJoint j;
        j.nameJp = QStringLiteral("Joint%1").arg(i);
        j.springLinearStiffness = glm::vec3(10.0f * (i + 1), 0.0f, 0.0f);
        doc.joints.push_back(j);
    }

    bridge.controller()->setDocumentInternal(doc);
    bridge.controller()->resetAllModels();

    // Verify rowCount for all table IDs
    QCOMPARE(bridge.rowCount(0), 13); // Info rows (RowCount = 13)
    QCOMPARE(bridge.rowCount(1), 13); // Info rows (RowCount = 13)
    QCOMPARE(bridge.rowCount(2), 2);  // Material
    QCOMPARE(bridge.rowCount(3), 2);  // Bone
    QCOMPARE(bridge.rowCount(4), 2);  // Vertex
    QCOMPARE(bridge.rowCount(5), 0);  // Morph
    QCOMPARE(bridge.rowCount(6), 0);  // DisplayFrame
    QCOMPARE(bridge.rowCount(7), 2);  // RigidBody
    QCOMPARE(bridge.rowCount(8), 2);  // Joint
    QCOMPARE(bridge.rowCount(-1), 0); // Invalid
    QCOMPARE(bridge.rowCount(99), 0); // Invalid

    // 1. Boundary: Empty row selection list
    QVERIFY(!bridge.applyBatchOperation(4, {}, PmxVertexTableModel::ColPosX, 0, 5.0));

    // 2. Boundary: Invalid table IDs (not in {2, 3, 4, 7, 8})
    QVERIFY(!bridge.applyBatchOperation(-1, {0}, 1, 0, 5.0));
    QVERIFY(!bridge.applyBatchOperation(0, {0}, 1, 0, 5.0));
    QVERIFY(!bridge.applyBatchOperation(1, {0}, 1, 0, 5.0)); // Info table does not support batch
    QVERIFY(!bridge.applyBatchOperation(5, {0}, 1, 0, 5.0)); // Morph table does not support batch
    QVERIFY(!bridge.applyBatchOperation(6, {0}, 1, 0, 5.0)); // DisplayFrame table does not support batch
    QVERIFY(!bridge.applyBatchOperation(99, {0}, 1, 0, 5.0));

    // 3. Boundary: Invalid operation enum
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColPosX, -1, 5.0));
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColPosX, 4, 5.0));
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColPosX, 100, 5.0));

    // 4. Boundary: Invalid columns
    QVERIFY(!bridge.applyBatchOperation(4, {0}, -1, 0, 5.0));
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColIndex, 0, 5.0)); // Read-only index col
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColumnCount, 0, 5.0));
    QVERIFY(!bridge.applyBatchOperation(4, {0}, 999, 0, 5.0));

    // 5. Boundary: NaN and Inf operands
    double qnan = std::numeric_limits<double>::quiet_NaN();
    double pinf = std::numeric_limits<double>::infinity();
    double ninf = -std::numeric_limits<double>::infinity();
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColPosX, 0, qnan));
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColPosX, 0, pinf));
    QVERIFY(!bridge.applyBatchOperation(4, {0}, PmxVertexTableModel::ColPosX, 0, ninf));

    // 6. Boundary: Purely out-of-bounds rows
    QVERIFY(!bridge.applyBatchOperation(4, {-5, -1, 10, 50}, PmxVertexTableModel::ColPosX, 0, 5.0));

    // 7. Boundary: Mixed valid and out-of-bounds rows
    // Only row 0 should be updated; invalid rows skipped safely without crash or memory corruption
    auto* vModel = bridge.vertexModel();
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 0.0);
    bool okMixed = bridge.applyBatchOperation(4, {-1, 0, 100}, PmxVertexTableModel::ColPosX, 0, 7.5);
    QVERIFY(okMixed);
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 7.5);
    QCOMPARE(vModel->data(vModel->index(1, PmxVertexTableModel::ColPosX)).toDouble(), 10.0); // row 1 untouched

    // Undo mixed operation
    bridge.undo();
    QCOMPARE(vModel->data(vModel->index(0, PmxVertexTableModel::ColPosX)).toDouble(), 0.0);

    // 8. Functional test: Material table (tableId = 2) batch operation
    auto* mModel = bridge.materialModel();
    QCOMPARE(mModel->data(mModel->index(0, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 1.0);
    QCOMPARE(mModel->data(mModel->index(1, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 2.0);
    bool okMat = bridge.applyBatchOperation(2, {0, 1}, PmxMaterialTableModel::ColEdgeSize, 0 /* Add */, 0.5);
    QVERIFY(okMat);
    QCOMPARE(mModel->data(mModel->index(0, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 1.5);
    QCOMPARE(mModel->data(mModel->index(1, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 2.5);
    bridge.undo();
    QCOMPARE(mModel->data(mModel->index(0, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 1.0);
    QCOMPARE(mModel->data(mModel->index(1, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 2.0);
    bridge.redo();
    QCOMPARE(mModel->data(mModel->index(0, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 1.5);
    QCOMPARE(mModel->data(mModel->index(1, PmxMaterialTableModel::ColEdgeSize)).toDouble(), 2.5);

    // 9. Functional test: Bone table (tableId = 3) batch operation
    auto* bModel = bridge.boneModel();
    QCOMPARE(bModel->data(bModel->index(0, PmxBoneTableModel::ColPosY)).toDouble(), 0.0);
    bool okBone = bridge.applyBatchOperation(3, {0}, PmxBoneTableModel::ColPosY, 0 /* Add */, 3.0);
    QVERIFY(okBone);
    QCOMPARE(bModel->data(bModel->index(0, PmxBoneTableModel::ColPosY)).toDouble(), 3.0);
    bridge.undo();
    QCOMPARE(bModel->data(bModel->index(0, PmxBoneTableModel::ColPosY)).toDouble(), 0.0);

    // 10. Functional test: RigidBody table (tableId = 7) batch operation
    auto* rbModel = bridge.rigidBodyModel();
    QCOMPARE(rbModel->data(rbModel->index(1, PmxRigidBodyTableModel::ColMass)).toDouble(), 2.0);
    bool okRb = bridge.applyBatchOperation(7, {1}, PmxRigidBodyTableModel::ColMass, 2 /* Multiply */, 3.0);
    QVERIFY(okRb);
    QCOMPARE(rbModel->data(rbModel->index(1, PmxRigidBodyTableModel::ColMass)).toDouble(), 6.0);
    bridge.undo();
    QCOMPARE(rbModel->data(rbModel->index(1, PmxRigidBodyTableModel::ColMass)).toDouble(), 2.0);

    // 11. Functional test: Joint table (tableId = 8) batch operation
    auto* jModel = bridge.jointModel();
    QCOMPARE(jModel->data(jModel->index(0, PmxJointTableModel::ColSpringPosX)).toDouble(), 10.0);
    bool okJoint = bridge.applyBatchOperation(8, {0}, PmxJointTableModel::ColSpringPosX, 0 /* Add */, 5.0);
    QVERIFY(okJoint);
    QCOMPARE(jModel->data(jModel->index(0, PmxJointTableModel::ColSpringPosX)).toDouble(), 15.0);
    bridge.undo();
    QCOMPARE(jModel->data(jModel->index(0, PmxJointTableModel::ColSpringPosX)).toDouble(), 10.0);
}

void tst_pmx_viewport::testAppBridgeLanguageToggle() {
    PmxAppBridge bridge;
    QSignalSpy spy(&bridge, &PmxAppBridge::languageChanged);

    // Initial language defaults to Japanese
    QCOMPARE(bridge.displayLanguage(), QStringLiteral("Japanese"));

    // Toggle once -> English
    bridge.toggleLanguage();
    QCOMPARE(bridge.displayLanguage(), QStringLiteral("English"));
    QCOMPARE(spy.count(), 1);

    // Toggle again -> Japanese
    bridge.toggleLanguage();
    QCOMPARE(bridge.displayLanguage(), QStringLiteral("Japanese"));
    QCOMPARE(spy.count(), 2);
}

void tst_pmx_viewport::testShaderResourcesLoaded() {
    PmxViewportItem item;
    const QStringList shaderPaths = {
        QStringLiteral(":/shaders/mesh.vert.qsb"),
        QStringLiteral(":/shaders/mesh.frag.qsb"),
        QStringLiteral(":/shaders/lines.vert.qsb"),
        QStringLiteral(":/shaders/lines.frag.qsb"),
        QStringLiteral(":/shaders/pmx_edge.vert.qsb"),
        QStringLiteral(":/shaders/pmx_edge.frag.qsb")
    };

    for (const QString& path : shaderPaths) {
        QVERIFY2(QFile::exists(path), qPrintable(path));
        QFile f(path);
        QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(path));
        QByteArray data = f.readAll();
        QVERIFY2(data.size() > 0, qPrintable(path));
        QShader shader = QShader::fromSerialized(data);
        QVERIFY2(shader.isValid(), qPrintable(QString("Failed to deserialize valid QShader from: %1").arg(path)));
    }
}

void tst_pmx_viewport::testEdgeExtrusionMath() {
    // Math formula: extrudedPos = inPosition + inNormal * (edgeSize * edgeScale * 0.01f)

    // 1. Origin vertex with unit normal along +Y, standard 1.0 size and 1.0 scale
    glm::vec3 pos1(0.0f, 0.0f, 0.0f);
    glm::vec3 norm1(0.0f, 1.0f, 0.0f);
    glm::vec3 ext1 = PmxViewportItem::calculateEdgeExtrusion(pos1, norm1, 1.0f, 1.0f);
    QCOMPARE(ext1.x, 0.0f);
    QCOMPARE(ext1.y, 0.01f);
    QCOMPARE(ext1.z, 0.0f);

    // 2. Translated vertex with +Z normal, edgeSize=2.0, edgeScale=1.5
    // displacement = 2.0 * 1.5 * 0.01 = 0.03
    glm::vec3 pos2(10.0f, 20.0f, 30.0f);
    glm::vec3 norm2(0.0f, 0.0f, 1.0f);
    glm::vec3 ext2 = PmxViewportItem::calculateEdgeExtrusion(pos2, norm2, 2.0f, 1.5f);
    QCOMPARE(ext2.x, 10.0f);
    QCOMPARE(ext2.y, 20.0f);
    QVERIFY(std::abs(ext2.z - 30.03f) < 1e-5f);

    // 3. Zero edgeScale suppresses extrusion completely
    glm::vec3 extZeroScale = PmxViewportItem::calculateEdgeExtrusion(pos2, norm2, 2.0f, 0.0f);
    QCOMPARE(extZeroScale, pos2);

    // 4. Zero edgeSize suppresses extrusion completely
    glm::vec3 extZeroSize = PmxViewportItem::calculateEdgeExtrusion(pos2, norm2, 0.0f, 1.5f);
    QCOMPARE(extZeroSize, pos2);

    // 5. Diagonal normal vector
    glm::vec3 diagNorm = glm::normalize(glm::vec3(1.0f, 2.0f, -2.0f));
    glm::vec3 pos5(1.0f, 2.0f, 3.0f);
    float edgeSize = 2.5f;
    float edgeScale = 0.8f;
    glm::vec3 extDiag = PmxViewportItem::calculateEdgeExtrusion(pos5, diagNorm, edgeSize, edgeScale);
    glm::vec3 expectedDiag = pos5 + diagNorm * (2.5f * 0.8f * 0.01f);
    QVERIFY(glm::distance(extDiag, expectedDiag) < 1e-5f);

    // 6. Negative edgeScale (inversion along normal)
    // displacement = 2.0 * (-1.0) * 0.01 = -0.02
    glm::vec3 extNegScale = PmxViewportItem::calculateEdgeExtrusion(pos2, norm2, 2.0f, -1.0f);
    QCOMPARE(extNegScale.x, 10.0f);
    QCOMPARE(extNegScale.y, 20.0f);
    QVERIFY(std::abs(extNegScale.z - 29.98f) < 1e-5f);
}

void tst_pmx_viewport::testMeshVertexEdgeScalePropagation() {
    PmxDocument doc;

    PmxVertex v0;
    v0.position = glm::vec3(1.0f, 2.0f, 3.0f);
    v0.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    v0.uv = glm::vec2(0.2f, 0.4f);
    v0.edgeScale = 1.0f; // Default

    PmxVertex v1;
    v1.position = glm::vec3(-5.0f, 10.0f, -15.0f);
    v1.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    v1.uv = glm::vec2(0.5f, 0.5f);
    v1.edgeScale = 0.0f; // Suppressed

    PmxVertex v2;
    v2.position = glm::vec3(100.0f, 200.0f, 300.0f);
    v2.normal = glm::vec3(1.0f, 0.0f, 0.0f);
    v2.uv = glm::vec2(0.9f, 0.1f);
    v2.edgeScale = 2.75f; // Thickened

    PmxVertex v3;
    v3.position = glm::vec3(2.0f, 4.0f, 6.0f);
    v3.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    v3.uv = glm::vec2(0.1f, 0.2f);
    v3.edgeScale = -1.0f; // Negative edgeScale

    doc.vertices = { v0, v1, v2, v3 };

    auto staged = PmxViewportItem::stageVertices(doc);
    QCOMPARE(staged.size(), 4u);

    // Verify edgeScale correctly propagated for all vertices
    QCOMPARE(staged[0].edgeScale, 1.0f);
    QCOMPARE(staged[1].edgeScale, 0.0f);
    QCOMPARE(staged[2].edgeScale, 2.75f);
    QCOMPARE(staged[3].edgeScale, -1.0f);

    // Also verify coordinate handedness conversion is preserved
    QCOMPARE(staged[0].position, glm::vec3(1.0f, 2.0f, -3.0f));
    QCOMPARE(staged[0].normal, glm::vec3(0.0f, 1.0f, 0.0f));
    QCOMPARE(staged[0].uv, glm::vec2(0.2f, 0.4f));

    QCOMPARE(staged[1].position, glm::vec3(-5.0f, 10.0f, 15.0f));
    QCOMPARE(staged[1].normal, glm::vec3(0.0f, 0.0f, -1.0f));

    QCOMPARE(staged[2].position, glm::vec3(100.0f, 200.0f, -300.0f));

    QCOMPARE(staged[3].position, glm::vec3(2.0f, 4.0f, -6.0f));
    QCOMPARE(staged[3].normal, glm::vec3(0.0f, 1.0f, 0.0f));
    QCOMPARE(staged[3].uv, glm::vec2(0.1f, 0.2f));
}

void tst_pmx_viewport::testSphereUVCalculation() {
    // MMD Sphere UV formula:
    // u = 0.5 + 0.5 * normalView.x
    // v = 0.5 - 0.5 * normalView.y

    // Center / facing camera: normal = (0, 0, 1) or (0, 0, -1)
    glm::vec2 uvCenter = PmxTextureCache::calculateSphereUV(glm::vec3(0.0f, 0.0f, 1.0f));
    QCOMPARE(uvCenter.x, 0.5f);
    QCOMPARE(uvCenter.y, 0.5f);

    // Right: normal = (1, 0, 0)
    glm::vec2 uvRight = PmxTextureCache::calculateSphereUV(glm::vec3(1.0f, 0.0f, 0.0f));
    QCOMPARE(uvRight.x, 1.0f);
    QCOMPARE(uvRight.y, 0.5f);

    // Left: normal = (-1, 0, 0)
    glm::vec2 uvLeft = PmxTextureCache::calculateSphereUV(glm::vec3(-1.0f, 0.0f, 0.0f));
    QCOMPARE(uvLeft.x, 0.0f);
    QCOMPARE(uvLeft.y, 0.5f);

    // Up: normal = (0, 1, 0) -> v = 0.5 - 0.5(1) = 0.0 (top)
    glm::vec2 uvUp = PmxTextureCache::calculateSphereUV(glm::vec3(0.0f, 1.0f, 0.0f));
    QCOMPARE(uvUp.x, 0.5f);
    QCOMPARE(uvUp.y, 0.0f);

    // Down: normal = (0, -1, 0) -> v = 0.5 - 0.5(-1) = 1.0 (bottom)
    glm::vec2 uvDown = PmxTextureCache::calculateSphereUV(glm::vec3(0.0f, -1.0f, 0.0f));
    QCOMPARE(uvDown.x, 0.5f);
    QCOMPARE(uvDown.y, 1.0f);

    // Diagonal: normal = normalize(1, 1, 0)
    glm::vec3 diag = glm::normalize(glm::vec3(1.0f, 1.0f, 0.0f));
    glm::vec2 uvDiag = PmxTextureCache::calculateSphereUV(diag);
    QVERIFY(std::abs(uvDiag.x - (0.5f + 0.5f * diag.x)) < 1e-5f);
    QVERIFY(std::abs(uvDiag.y - (0.5f - 0.5f * diag.y)) < 1e-5f);
}

void tst_pmx_viewport::testToonLookupMath() {
    // Formula: v = clamp(0.5 - 0.5 * (N . L), 0.0, 1.0)

    // N . L = 1.0 (facing light directly, fully lit) -> v = 0.0
    float vLit = PmxTextureCache::calculateToonV(1.0f);
    QCOMPARE(vLit, 0.0f);
    QCOMPARE(PmxTextureCache::calculateToonCoordinate(1.0f), 0.0f);

    // N . L = 0.0 (perpendicular, terminator) -> v = 0.5
    float vTerm = PmxTextureCache::calculateToonV(0.0f);
    QCOMPARE(vTerm, 0.5f);
    QCOMPARE(PmxTextureCache::calculateToonCoordinate(0.0f), 0.5f);

    // N . L = -1.0 (facing completely away, shadow) -> v = 1.0
    float vShadow = PmxTextureCache::calculateToonV(-1.0f);
    QCOMPARE(vShadow, 1.0f);
    QCOMPARE(PmxTextureCache::calculateToonCoordinate(-1.0f), 1.0f);

    // Intermediate angles
    float vHalfLit = PmxTextureCache::calculateToonV(0.5f);
    QCOMPARE(vHalfLit, 0.25f);

    float vHalfShadow = PmxTextureCache::calculateToonV(-0.5f);
    QCOMPARE(vHalfShadow, 0.75f);

    // Clamping on extreme values (overshoot / undershoot)
    float vOver = PmxTextureCache::calculateToonV(2.5f);
    QCOMPARE(vOver, 0.0f);

    float vUnder = PmxTextureCache::calculateToonV(-3.0f);
    QCOMPARE(vUnder, 1.0f);
}

void tst_pmx_viewport::testTextureCacheFallbacks() {
    // 1. Verify default white image
    QImage white = PmxTextureCache::createWhiteImage();
    QVERIFY(!white.isNull());
    QCOMPARE(white.width(), 1);
    QCOMPARE(white.height(), 1);
    QCOMPARE(white.pixelColor(0, 0), QColor(255, 255, 255, 255));

    // 2. Verify all 10 programmatic toon textures (presets 0..9 corresponding to toon01..toon10)
    std::vector<QImage> toonImages;
    toonImages.reserve(10);
    for (int t = 0; t < 10; ++t) {
        QImage toon = PmxTextureCache::createToonImage(t);
        QVERIFY(!toon.isNull());
        QCOMPARE(toon.width(), 16);
        QCOMPARE(toon.height(), 16);
        QVERIFY(toon.bits() != nullptr);

        // Verify vertical gradient: lit side at top (y=0) is lighter or equal to shadow side at bottom (y=15)
        QColor litPixel = toon.pixelColor(8, 0);
        QColor shadowPixel = toon.pixelColor(8, 15);
        int litLum = litPixel.red() + litPixel.green() + litPixel.blue();
        int shadowLum = shadowPixel.red() + shadowPixel.green() + shadowPixel.blue();
        QVERIFY(litLum >= shadowLum);

        toonImages.push_back(toon);
    }

    // 3. Specifically verify preset 0 (toon01) and preset 1 (toon02) are distinct (zero collision)
    QVERIFY(toonImages[0] != toonImages[1]);
    QCOMPARE(toonImages[0].pixelColor(8, 15), QColor(170, 170, 170)); // Neutral gray shadow
    QCOMPARE(toonImages[1].pixelColor(8, 15), QColor(230, 195, 185)); // Warm skin shadow

    // Verify all 10 presets (0..9) are distinct
    for (size_t i = 0; i < toonImages.size(); ++i) {
        for (size_t j = i + 1; j < toonImages.size(); ++j) {
            QVERIFY2(toonImages[i] != toonImages[j],
                     qPrintable(QString("Presets %1 and %2 must be distinct").arg(i).arg(j)));
        }
    }

    // 4. Verify boundary fallbacks (out of 0..9 range falls back to preset 0)
    QImage toonUnderflow = PmxTextureCache::createToonImage(-5);
    QCOMPARE(toonUnderflow, toonImages[0]);

    QImage toonOverflow = PmxTextureCache::createToonImage(10);
    QCOMPARE(toonOverflow, toonImages[0]);

    QImage toonLargeOverflow = PmxTextureCache::createToonImage(99);
    QCOMPARE(toonLargeOverflow, toonImages[0]);
}

void tst_pmx_viewport::testMaterialBatchPartitioning() {
    PmxDocument doc;
    // 4 Vertices
    for (int i = 0; i < 4; ++i) {
        PmxVertex v;
        v.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
        doc.vertices.push_back(v);
    }

    // 12 Face Indices (4 triangles)
    // Triangle 0: (0, 1, 2)
    // Triangle 1: (2, 1, 3)
    // Triangle 2: (0, 2, 3)
    // Triangle 3: (1, 2, 0)
    doc.faceIndices = { 0, 1, 2,  2, 1, 3,  0, 2, 3,  1, 2, 0 };

    // Material 0: 6 indices (2 triangles)
    PmxMaterial m0;
    m0.nameJp = QStringLiteral("Mat0");
    m0.faceVertexCount = 6;
    m0.drawFlags = 0x01; // DoubleSided
    m0.sphereMode = 1;   // Mul
    m0.sharedToonFlag = 1;
    m0.toonIndex = 0;
    doc.materials.push_back(m0);

    // Material 1: 6 indices (2 triangles)
    PmxMaterial m1;
    m1.nameJp = QStringLiteral("Mat1");
    m1.faceVertexCount = 6;
    m1.drawFlags = 0x00; // Back cull
    m1.sphereMode = 2;   // Add
    m1.sharedToonFlag = 0;
    m1.toonIndex = -1;
    doc.materials.push_back(m1);

    std::vector<uint32_t> stagedIndices;
    auto batches = PmxViewportItem::partitionMaterialBatches(doc, stagedIndices);

    QCOMPARE(batches.size(), 2u);
    QCOMPARE(stagedIndices.size(), 12u);

    // Batch 0
    QCOMPARE(batches[0].indexOffset, 0u);
    QCOMPARE(batches[0].indexCount, 6u);
    QVERIFY(batches[0].doubleSided);
    QCOMPARE(batches[0].sphereMode, 1);
    QVERIFY(batches[0].hasToon);
    QCOMPARE(batches[0].sharedToonIndex, 0);
    QVERIFY(batches[0].indexOffset + batches[0].indexCount <= stagedIndices.size());

    // Batch 1
    QCOMPARE(batches[1].indexOffset, 6u);
    QCOMPARE(batches[1].indexCount, 6u);
    QVERIFY(!batches[1].doubleSided);
    QCOMPARE(batches[1].sphereMode, 2);
    QVERIFY(!batches[1].hasToon);
    QVERIFY(batches[1].indexOffset + batches[1].indexCount <= stagedIndices.size());

    // Test corrupted index filtering within material partitions:
    // Make index 4 out-of-bounds (vertex index 99) in triangle 1 of Material 0
    doc.faceIndices[4] = 99;
    std::vector<uint32_t> stagedCorrupt;
    auto batchesCorrupt = PmxViewportItem::partitionMaterialBatches(doc, stagedCorrupt);

    QCOMPARE(batchesCorrupt.size(), 2u);
    // Material 0 lost 1 invalid triangle (3 indices), keeping 3 valid indices
    QCOMPARE(batchesCorrupt[0].indexOffset, 0u);
    QCOMPARE(batchesCorrupt[0].indexCount, 3u);

    // Material 1 still has its 6 valid indices starting right after Material 0
    QCOMPARE(batchesCorrupt[1].indexOffset, 3u);
    QCOMPARE(batchesCorrupt[1].indexCount, 6u);
    QCOMPARE(stagedCorrupt.size(), 9u);

    // Index buffer bounds invariant holds strictly:
    for (const auto& b : batchesCorrupt) {
        QVERIFY(b.indexOffset + b.indexCount <= stagedCorrupt.size());
    }

    // Boundary: excess materials with no remaining indices
    PmxMaterial mExcess;
    mExcess.nameJp = QStringLiteral("MatExcess");
    mExcess.faceVertexCount = 100;
    doc.materials.push_back(mExcess);

    std::vector<uint32_t> stagedExcess;
    auto batchesExcess = PmxViewportItem::partitionMaterialBatches(doc, stagedExcess);
    QCOMPARE(batchesExcess.size(), 3u);
    QCOMPARE(batchesExcess[2].indexCount, 0u);
    for (const auto& b : batchesExcess) {
        QVERIFY(b.indexOffset + b.indexCount <= stagedExcess.size());
    }
}

void tst_pmx_viewport::testEdgeMaterialBatchFiltering() {
    PmxDocument doc;
    for (int i = 0; i < 3; ++i) {
        PmxVertex v;
        v.position = glm::vec3(static_cast<float>(i), 0.0f, 0.0f);
        doc.vertices.push_back(v);
    }
    // 1 triangle per material (3 indices each, total 18 indices for 6 materials)
    doc.faceIndices = {
        0, 1, 2,
        0, 1, 2,
        0, 1, 2,
        0, 1, 2,
        0, 1, 2,
        0, 1, 2
    };

    // Mat 0: Standard valid edge: drawFlags 0x10, edgeSize > 0, edgeColor.a > 0 -> hasEdge true
    PmxMaterial m0;
    m0.faceVertexCount = 3;
    m0.drawFlags = 0x1F; // Bit 4 (0x10) set
    m0.edgeSize = 1.0f;
    m0.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    doc.materials.push_back(m0);

    // Mat 1: Bit 4 cleared (0x0F) -> hasEdge false
    PmxMaterial m1;
    m1.faceVertexCount = 3;
    m1.drawFlags = 0x0F; // No 0x10 bit
    m1.edgeSize = 1.0f;
    m1.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    doc.materials.push_back(m1);

    // Mat 2: edgeSize == 0.0f -> hasEdge false
    PmxMaterial m2;
    m2.faceVertexCount = 3;
    m2.drawFlags = 0x10;
    m2.edgeSize = 0.0f;
    m2.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    doc.materials.push_back(m2);

    // Mat 3: edgeSize < 0.0f -> hasEdge false
    PmxMaterial m3;
    m3.faceVertexCount = 3;
    m3.drawFlags = 0x10;
    m3.edgeSize = -1.0f;
    m3.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    doc.materials.push_back(m3);

    // Mat 4: edgeColor.a == 0.0f -> hasEdge false
    PmxMaterial m4;
    m4.faceVertexCount = 3;
    m4.drawFlags = 0x10;
    m4.edgeSize = 1.0f;
    m4.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
    doc.materials.push_back(m4);

    // Mat 5: edgeColor.a < 0.0f -> hasEdge false
    PmxMaterial m5;
    m5.faceVertexCount = 3;
    m5.drawFlags = 0x10;
    m5.edgeSize = 1.0f;
    m5.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, -0.5f);
    doc.materials.push_back(m5);

    // Mat 6: faceVertexCount == 0 with edge flag 0x10 enabled -> hasEdge true, indexCount 0
    PmxMaterial m6;
    m6.faceVertexCount = 0;
    m6.drawFlags = 0x10;
    m6.edgeSize = 1.0f;
    m6.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    doc.materials.push_back(m6);

    // Mat 7: Non-finite edgeSize (NaN) -> hasEdge false
    PmxMaterial m7;
    m7.faceVertexCount = 3;
    m7.drawFlags = 0x10;
    m7.edgeSize = std::numeric_limits<float>::quiet_NaN();
    m7.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    doc.materials.push_back(m7);

    // Mat 8: Non-finite edgeColor.a (Inf) -> hasEdge false
    PmxMaterial m8;
    m8.faceVertexCount = 3;
    m8.drawFlags = 0x10;
    m8.edgeSize = 1.0f;
    m8.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, std::numeric_limits<float>::infinity());
    doc.materials.push_back(m8);

    std::vector<uint32_t> stagedIndices;
    auto batches = PmxViewportItem::partitionMaterialBatches(doc, stagedIndices);

    QCOMPARE(batches.size(), 9u);
    QVERIFY(batches[0].hasEdge);
    QVERIFY(!batches[1].hasEdge);
    QVERIFY(!batches[2].hasEdge);
    QVERIFY(!batches[3].hasEdge);
    QVERIFY(!batches[4].hasEdge);
    QVERIFY(!batches[5].hasEdge);
    QVERIFY(batches[6].hasEdge);
    QCOMPARE(batches[6].indexCount, 0u);
    QVERIFY(!batches[7].hasEdge);
    QVERIFY(!batches[8].hasEdge);

    // Also verify empty materials doc produces hasEdge == false
    PmxDocument emptyMatDoc;
    emptyMatDoc.vertices = doc.vertices;
    emptyMatDoc.faceIndices = { 0, 1, 2 };
    std::vector<uint32_t> emptyIndices;
    auto emptyBatches = PmxViewportItem::partitionMaterialBatches(emptyMatDoc, emptyIndices);
    QCOMPARE(emptyBatches.size(), 1u);
    QVERIFY(!emptyBatches[0].hasEdge);
}

QTEST_MAIN(tst_pmx_viewport)
#include "tst_pmx_viewport.moc"
