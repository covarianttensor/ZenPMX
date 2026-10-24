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

#include <QSignalSpy>
#include <QTest>
#include <QUndoCommand>
#include <QFile>
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
    QVERIFY(QFile::exists(QStringLiteral(":/shaders/mesh.vert.qsb")));
    QVERIFY(QFile::exists(QStringLiteral(":/shaders/mesh.frag.qsb")));
    QVERIFY(QFile::exists(QStringLiteral(":/shaders/lines.vert.qsb")));
    QVERIFY(QFile::exists(QStringLiteral(":/shaders/lines.frag.qsb")));

    QFile vertFile(QStringLiteral(":/shaders/mesh.vert.qsb"));
    QVERIFY(vertFile.open(QIODevice::ReadOnly));
    QVERIFY(vertFile.size() > 0);
}

QTEST_MAIN(tst_pmx_viewport)
#include "tst_pmx_viewport.moc"
