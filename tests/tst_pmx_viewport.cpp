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
#include "viewport/PmxGizmo.h"
#include "viewport/PmxWireframeRenderer.h"
#include "viewport/PmxWeightPaintBrush.h"

#include <QSignalSpy>
#include <QTest>
#include <QUndoCommand>
#include <QFile>
#include <rhi/qshader.h>
#include <cmath>
#include <limits>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

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
    void testScreenPointToRay();
    void testGizmoHitTest();
    void testGizmoTranslationDeltaAndSnapping();
    void testGizmoRotationAndScaleDelta();
    void testSymmetricVertexAndMirroring();
    void testSymmetricBoneMatching();
    void testVertexTransformCommandUndoRedo();
    void testBoneTransformCommandUndoRedo();
    void testViewportGizmoIntegration();
    void testBoneWireframeOctahedron();
    void testRigidBodyWireframeShapesAndColors();
    void testJointWireframeCrossLimitArcsAndAnchors();
    void testWireframeRendererRobustness();
    void testViewportWireframeToggles();
    void testWeightToColorGradients();
    void testBrushFalloffMath();
    void testApplyBrushToVertexWeights();
    void testVertexWeightCommandUndoRedo();
    void testWeightPaintingBoundaryConditions();
    void testViewportWeightPaintProperties();
    void testBrushCursorGeneration();
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

void tst_pmx_viewport::testScreenPointToRay() {
    PmxCamera camera;
    camera.setTarget(glm::vec3(0.0f, 10.0f, 0.0f));
    camera.setDistance(20.0f);
    camera.setYaw(0.0f);
    camera.setPitch(0.0f);

    float width = 800.0f;
    float height = 600.0f;

    // Center screen ray
    QPointF centerPoint(400.0f, 300.0f);
    Ray centerRay = camera.screenPointToRay(centerPoint, width, height);

    // Ray origin must equal camera eye position
    glm::vec3 eye = camera.eyePosition();
    QVERIFY(glm::distance(centerRay.origin, eye) < 0.001f);

    // Ray direction must be normalized
    QVERIFY(std::abs(glm::length(centerRay.direction) - 1.0f) < 0.001f);

    // For center ray, direction should point towards target: normalize(target - eye)
    glm::vec3 expectedDir = glm::normalize(camera.target() - eye);
    QVERIFY(glm::distance(centerRay.direction, expectedDir) < 0.005f);

    // Top-left vs bottom-right rays should diverge
    Ray tlRay = camera.screenPointToRay(QPointF(0.0f, 0.0f), width, height);
    Ray brRay = camera.screenPointToRay(QPointF(800.0f, 600.0f), width, height);
    QVERIFY(tlRay.direction.x < brRay.direction.x);
    QVERIFY(tlRay.direction.y > brRay.direction.y);

    // Boundary test: width = 0.0f, height = -10.0f
    Ray badDimRay = camera.screenPointToRay(QPointF(100.0f, 100.0f), 0.0f, -10.0f);
    QVERIFY(!std::isnan(badDimRay.direction.x));
    QVERIFY(!std::isnan(badDimRay.direction.y));
    QVERIFY(!std::isnan(badDimRay.direction.z));
    QVERIFY(std::abs(glm::length(badDimRay.direction) - 1.0f) < 0.001f);
}

void tst_pmx_viewport::testGizmoHitTest() {
    PmxGizmo gizmo;
    gizmo.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    gizmo.setMode(GizmoMode::Translate);
    float scale = 1.0f;

    // Ray along +X axis towards origin offset: origin (0.5, 0, 5), dir (0, 0, -1) -> hits AxisX
    GizmoAxis hitX = gizmo.hitTest(glm::vec3(0.5f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), scale);
    QCOMPARE(hitX, GizmoAxis::AxisX);

    // Ray at (0, 0.5, 5), dir (0, 0, -1) -> hits AxisY
    GizmoAxis hitY = gizmo.hitTest(glm::vec3(0.0f, 0.5f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), scale);
    QCOMPARE(hitY, GizmoAxis::AxisY);

    // Ray at (5, 0, 0.5), dir (-1, 0, 0) -> hits AxisZ
    GizmoAxis hitZ = gizmo.hitTest(glm::vec3(5.0f, 0.0f, 0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), scale);
    QCOMPARE(hitZ, GizmoAxis::AxisZ);

    // Plane handle test: plane XY handle is near (0.25, 0.25, 0)
    GizmoAxis hitXY = gizmo.hitTest(glm::vec3(0.25f, 0.25f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), scale);
    QCOMPARE(hitXY, GizmoAxis::PlaneXY);

    // Missing ray
    GizmoAxis hitMiss = gizmo.hitTest(glm::vec3(10.0f, 10.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), scale);
    QCOMPARE(hitMiss, GizmoAxis::None);

    // Boundary tests: gizmoScale = 0.0f and GizmoMode::None
    GizmoAxis hitZeroScale = gizmo.hitTest(glm::vec3(0.5f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), 0.0f);
    QCOMPARE(hitZeroScale, GizmoAxis::None);

    gizmo.setMode(GizmoMode::None);
    GizmoAxis hitModeNone = gizmo.hitTest(glm::vec3(0.5f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), 1.0f);
    QCOMPARE(hitModeNone, GizmoAxis::None);

    // Rotation mode
    gizmo.setMode(GizmoMode::Rotate);
    // Z ring lies in XY plane at radius ~0.85
    GizmoAxis hitRotZ = gizmo.hitTest(glm::vec3(0.85f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), scale);
    QCOMPARE(hitRotZ, GizmoAxis::AxisZ);

    // Scale mode
    gizmo.setMode(GizmoMode::Scale);
    GizmoAxis hitScaleX = gizmo.hitTest(glm::vec3(0.5f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, -1.0f), scale);
    QCOMPARE(hitScaleX, GizmoAxis::AxisX);
}

void tst_pmx_viewport::testGizmoTranslationDeltaAndSnapping() {
    PmxGizmo gizmo;
    gizmo.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    gizmo.setTranslationSnap(0.5f);
    float scale = 1.0f;

    glm::vec3 ray0Origin(0.0f, 0.0f, 10.0f);
    glm::vec3 ray0Dir(0.0f, 0.0f, -1.0f);

    glm::vec3 ray1Origin(1.2f, 0.0f, 10.0f);
    glm::vec3 ray1Dir(0.0f, 0.0f, -1.0f);

    // Without snap
    glm::vec3 deltaNoSnap = gizmo.computeTranslationDelta(
        GizmoAxis::AxisX,
        ray1Origin, ray1Dir,
        ray0Origin, ray0Dir,
        scale, false
    );
    QVERIFY(std::abs(deltaNoSnap.x - 1.2f) < 0.01f);
    QCOMPARE(deltaNoSnap.y, 0.0f);
    QCOMPARE(deltaNoSnap.z, 0.0f);

    // With snap (0.5 snap increment, 1.2 rounds to 1.0)
    glm::vec3 deltaSnap = gizmo.computeTranslationDelta(
        GizmoAxis::AxisX,
        ray1Origin, ray1Dir,
        ray0Origin, ray0Dir,
        scale, true
    );
    QVERIFY(std::abs(deltaSnap.x - 1.0f) < 0.001f);

    // Plane translation
    glm::vec3 rayPlane1Origin(0.6f, 1.4f, 10.0f);
    glm::vec3 deltaPlaneSnap = gizmo.computeTranslationDelta(
        GizmoAxis::PlaneXY,
        rayPlane1Origin, ray1Dir,
        ray0Origin, ray0Dir,
        scale, true
    );
    // 0.6 snaps to 0.5, 1.4 snaps to 1.5
    QVERIFY(std::abs(deltaPlaneSnap.x - 0.5f) < 0.01f);
    QVERIFY(std::abs(deltaPlaneSnap.y - 1.5f) < 0.01f);
    QCOMPARE(deltaPlaneSnap.z, 0.0f);

    // Boundary snapping tests: snap = 0.0f, snap = -1.0f, snap = Inf
    gizmo.setTranslationSnap(0.0f);
    glm::vec3 dZeroSnap = gizmo.computeTranslationDelta(GizmoAxis::AxisX, ray1Origin, ray1Dir, ray0Origin, ray0Dir, scale, true);
    QVERIFY(std::isfinite(dZeroSnap.x));
    QVERIFY(std::isfinite(dZeroSnap.y));
    QVERIFY(std::isfinite(dZeroSnap.z));

    gizmo.setTranslationSnap(-1.0f);
    glm::vec3 dNegSnap = gizmo.computeTranslationDelta(GizmoAxis::AxisX, ray1Origin, ray1Dir, ray0Origin, ray0Dir, scale, true);
    QVERIFY(std::isfinite(dNegSnap.x));
    QVERIFY(std::isfinite(dNegSnap.y));
    QVERIFY(std::isfinite(dNegSnap.z));

    gizmo.setTranslationSnap(std::numeric_limits<float>::infinity());
    glm::vec3 dInfSnap = gizmo.computeTranslationDelta(GizmoAxis::AxisX, ray1Origin, ray1Dir, ray0Origin, ray0Dir, scale, true);
    QVERIFY(std::isfinite(dInfSnap.x));
    QVERIFY(std::isfinite(dInfSnap.y));
    QVERIFY(std::isfinite(dInfSnap.z));
}

void tst_pmx_viewport::testGizmoRotationAndScaleDelta() {
    PmxGizmo gizmo;
    gizmo.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    gizmo.setRotationSnap(glm::radians(15.0f)); // 15 degrees snap
    gizmo.setScaleSnap(0.1f);
    float scale = 1.0f;

    // Rotation around Z axis: ray in XY plane
    glm::vec3 r0Orig(1.0f, 0.0f, 10.0f);
    glm::vec3 r0Dir(0.0f, 0.0f, -1.0f);

    // Rotate ~45 degrees: (cos45, sin45) = (0.7071, 0.7071)
    glm::vec3 r1Orig(0.7071f, 0.7071f, 10.0f);
    glm::vec3 r1Dir(0.0f, 0.0f, -1.0f);

    glm::quat qRotNoSnap = gizmo.computeRotationDelta(
        GizmoAxis::AxisZ,
        r1Orig, r1Dir,
        r0Orig, r0Dir,
        scale, false
    );
    float angle = glm::angle(qRotNoSnap);
    QVERIFY(std::abs(glm::degrees(angle) - 45.0f) < 1.0f);

    // With 15 deg snap: 45 degrees is an exact multiple (3 * 15)
    glm::quat qRotSnap = gizmo.computeRotationDelta(
        GizmoAxis::AxisZ,
        r1Orig, r1Dir,
        r0Orig, r0Dir,
        scale, true
    );
    float angleSnap = glm::angle(qRotSnap);
    QVERIFY(std::abs(glm::degrees(angleSnap) - 45.0f) < 0.1f);

    // Scale delta along AxisX
    glm::vec3 s0Orig(0.0f, 0.0f, 10.0f);
    glm::vec3 s0Dir(0.0f, 0.0f, -1.0f);
    glm::vec3 s1Orig(0.24f, 0.0f, 10.0f); // +0.24 delta -> factor = 1.24
    glm::vec3 s1Dir(0.0f, 0.0f, -1.0f);

    glm::vec3 scaleNoSnap = gizmo.computeScaleDelta(
        GizmoAxis::AxisX,
        s1Orig, s1Dir,
        s0Orig, s0Dir,
        scale, false
    );
    QVERIFY(std::abs(scaleNoSnap.x - 1.24f) < 0.02f);
    QCOMPARE(scaleNoSnap.y, 1.0f);
    QCOMPARE(scaleNoSnap.z, 1.0f);

    // With snap (0.1 snap): 1.24 -> 1.2
    glm::vec3 scaleSnap = gizmo.computeScaleDelta(
        GizmoAxis::AxisX,
        s1Orig, s1Dir,
        s0Orig, s0Dir,
        scale, true
    );
    QVERIFY(std::abs(scaleSnap.x - 1.2f) < 0.01f);
}

void tst_pmx_viewport::testSymmetricVertexAndMirroring() {
    PmxDocument doc;
    PmxVertex v0;
    v0.position = glm::vec3(2.0f, 5.0f, 3.0f);
    doc.vertices.push_back(v0);

    PmxVertex v1;
    v1.position = glm::vec3(-2.0f, 5.0f, 3.0f);
    doc.vertices.push_back(v1);

    PmxVertex v2; // centerline vertex
    v2.position = glm::vec3(0.001f, 10.0f, 1.0f);
    doc.vertices.push_back(v2);

    PmxVertex v3; // asymmetric vertex
    v3.position = glm::vec3(7.0f, 1.0f, 2.0f);
    doc.vertices.push_back(v3);

    // v0 symmetric partner should be v1
    QCOMPARE(PmxGizmo::findSymmetricVertex(doc, 0), 1);
    // v1 symmetric partner should be v0
    QCOMPARE(PmxGizmo::findSymmetricVertex(doc, 1), 0);
    // Centerline vertex v2 should pair with itself
    QCOMPARE(PmxGizmo::findSymmetricVertex(doc, 2), 2);
    // Asymmetric vertex v3 should return -1
    QCOMPARE(PmxGizmo::findSymmetricVertex(doc, 3), -1);
    // Boundary tests: negative index (-1) and out-of-bounds index (999)
    QCOMPARE(PmxGizmo::findSymmetricVertex(doc, -1), -1);
    QCOMPARE(PmxGizmo::findSymmetricVertex(doc, 999), -1);

    // Mirror delta
    glm::vec3 delta(1.5f, -2.0f, 3.0f);
    glm::vec3 mirrored = PmxGizmo::mirrorDelta(delta);
    QCOMPARE(mirrored.x, -1.5f);
    QCOMPARE(mirrored.y, -2.0f);
    QCOMPARE(mirrored.z, 3.0f);
}

void tst_pmx_viewport::testSymmetricBoneMatching() {
    PmxDocument doc;
    // Japanese pairs
    PmxBone b0; b0.nameJp = QString::fromUtf8("左腕"); doc.bones.push_back(b0);
    PmxBone b1; b1.nameJp = QString::fromUtf8("右腕"); doc.bones.push_back(b1);

    // English pairs
    PmxBone b2; b2.nameEn = QStringLiteral("Arm_L"); doc.bones.push_back(b2);
    PmxBone b3; b3.nameEn = QStringLiteral("Arm_R"); doc.bones.push_back(b3);

    PmxBone b4; b4.nameEn = QStringLiteral("LeftLeg"); doc.bones.push_back(b4);
    PmxBone b5; b5.nameEn = QStringLiteral("RightLeg"); doc.bones.push_back(b5);

    // Center bone
    PmxBone b6; b6.nameJp = QString::fromUtf8("センター"); doc.bones.push_back(b6);

    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 0), 1);
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 1), 0);
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 2), 3);
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 3), 2);
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 4), 5);
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 5), 4);
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 6), -1);
    // Boundary tests: negative index (-1) and out-of-bounds index (999)
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, -1), -1);
    QCOMPARE(PmxGizmo::findSymmetricBone(doc, 999), -1);

    // Mirror rotation across X=0 plane
    glm::quat q = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qMirrored = PmxGizmo::mirrorRotation(q);
    QVERIFY(std::abs(qMirrored.w - q.w) < 0.001f);
    QVERIFY(std::abs(qMirrored.y + q.y) < 0.001f);
}

void tst_pmx_viewport::testVertexTransformCommandUndoRedo() {
    PmxDocument doc;
    PmxVertex v0;
    v0.position = glm::vec3(1.0f, 2.0f, 3.0f);
    v0.normal = glm::vec3(0.0f, 1.0f, 0.0f);
    doc.vertices.push_back(v0);

    PmxDocumentController controller;
    controller.setDocumentInternal(doc);

    std::vector<VertexTransformDelta> deltas;
    VertexTransformDelta d;
    d.vertexIndex = 0;
    d.oldPos = v0.position;
    d.newPos = glm::vec3(5.0f, 6.0f, 7.0f);
    d.oldNormal = v0.normal;
    d.newNormal = glm::vec3(1.0f, 0.0f, 0.0f);
    deltas.push_back(d);

    auto* cmd = new PmxVertexTransformCommand(&controller, deltas, 1001);
    controller.pushCommand(cmd);

    // Verify redo applied
    QCOMPARE(controller.document().vertices[0].position, glm::vec3(5.0f, 6.0f, 7.0f));
    QCOMPARE(controller.document().vertices[0].normal, glm::vec3(1.0f, 0.0f, 0.0f));
    QCOMPARE(controller.undoStack()->count(), 1);

    // Verify mergeWith within same drag session (same commandId)
    std::vector<VertexTransformDelta> deltas2;
    VertexTransformDelta d2;
    d2.vertexIndex = 0;
    d2.oldPos = v0.position;
    d2.newPos = glm::vec3(8.0f, 9.0f, 10.0f);
    d2.oldNormal = v0.normal;
    d2.newNormal = glm::vec3(0.0f, 0.0f, 1.0f);
    deltas2.push_back(d2);

    auto* cmd2 = new PmxVertexTransformCommand(&controller, deltas2, 1001);
    controller.pushCommand(cmd2);

    QCOMPARE(controller.document().vertices[0].position, glm::vec3(8.0f, 9.0f, 10.0f));
    // Count must still be 1 because it merged!
    QCOMPARE(controller.undoStack()->count(), 1);

    // Undo should revert to initial v0.position
    controller.undo();
    QCOMPARE(controller.document().vertices[0].position, glm::vec3(1.0f, 2.0f, 3.0f));
    QCOMPARE(controller.document().vertices[0].normal, glm::vec3(0.0f, 1.0f, 0.0f));

    // Redo should restore to merged 8.0f, 9.0f, 10.0f
    controller.redo();
    QCOMPARE(controller.document().vertices[0].position, glm::vec3(8.0f, 9.0f, 10.0f));

    // Boundary tests: null controller, out-of-bounds index, and mergeWith rejection
    PmxVertexTransformCommand nullCmd(nullptr, deltas, 1001);
    nullCmd.redo();
    nullCmd.undo();

    std::vector<VertexTransformDelta> oobDeltas;
    VertexTransformDelta oobD;
    oobD.vertexIndex = 999;
    oobD.oldPos = glm::vec3(0.0f);
    oobD.newPos = glm::vec3(1.0f);
    oobDeltas.push_back(oobD);
    PmxVertexTransformCommand* oobCmd = new PmxVertexTransformCommand(&controller, oobDeltas, 1001);
    controller.pushCommand(oobCmd);

    PmxVertexTransformCommand baseCmd(&controller, deltas, 1001);
    QVERIFY(!baseCmd.mergeWith(nullptr));

    PmxVertexTransformCommand diffIdCmd(&controller, deltas, 9999);
    QVERIFY(!baseCmd.mergeWith(&diffIdCmd));

    std::vector<VertexTransformDelta> diffCountDeltas = deltas;
    diffCountDeltas.push_back(d);
    PmxVertexTransformCommand diffCountCmd(&controller, diffCountDeltas, 1001);
    QVERIFY(!baseCmd.mergeWith(&diffCountCmd));

    std::vector<VertexTransformDelta> diffIdxDeltas = deltas;
    diffIdxDeltas[0].vertexIndex = 123;
    PmxVertexTransformCommand diffIdxCmd(&controller, diffIdxDeltas, 1001);
    QVERIFY(!baseCmd.mergeWith(&diffIdxCmd));
}

void tst_pmx_viewport::testBoneTransformCommandUndoRedo() {
    PmxDocument doc;
    PmxBone b0;
    b0.nameJp = QStringLiteral("Bone0");
    b0.nameEn = QStringLiteral("Bone0");
    b0.position = glm::vec3(10.0f, 20.0f, 30.0f);
    doc.bones.push_back(b0);

    PmxDocumentController controller;
    controller.setDocumentInternal(doc);

    std::vector<BoneTransformDelta> deltas;
    BoneTransformDelta d;
    d.boneIndex = 0;
    d.oldPos = b0.position;
    d.newPos = glm::vec3(15.0f, 25.0f, 35.0f);
    deltas.push_back(d);

    auto* cmd = new PmxBoneTransformCommand(&controller, deltas, 1002);
    controller.pushCommand(cmd);

    QCOMPARE(controller.document().bones[0].position, glm::vec3(15.0f, 25.0f, 35.0f));
    QCOMPARE(controller.undoStack()->count(), 1);

    // Test mergeWith
    std::vector<BoneTransformDelta> deltas2;
    BoneTransformDelta d2;
    d2.boneIndex = 0;
    d2.oldPos = b0.position;
    d2.newPos = glm::vec3(50.0f, 60.0f, 70.0f);
    deltas2.push_back(d2);

    auto* cmd2 = new PmxBoneTransformCommand(&controller, deltas2, 1002);
    controller.pushCommand(cmd2);

    QCOMPARE(controller.document().bones[0].position, glm::vec3(50.0f, 60.0f, 70.0f));
    QCOMPARE(controller.undoStack()->count(), 1);

    controller.undo();
    QCOMPARE(controller.document().bones[0].position, glm::vec3(10.0f, 20.0f, 30.0f));

    controller.redo();
    QCOMPARE(controller.document().bones[0].position, glm::vec3(50.0f, 60.0f, 70.0f));

    // Boundary tests: null controller, out-of-bounds index, and mergeWith rejection
    PmxBoneTransformCommand nullBoneCmd(nullptr, deltas, 1002);
    nullBoneCmd.redo();
    nullBoneCmd.undo();

    std::vector<BoneTransformDelta> oobBoneDeltas;
    BoneTransformDelta oobBoneD;
    oobBoneD.boneIndex = 999;
    oobBoneD.oldPos = glm::vec3(0.0f);
    oobBoneD.newPos = glm::vec3(1.0f);
    oobBoneDeltas.push_back(oobBoneD);
    PmxBoneTransformCommand* oobBoneCmd = new PmxBoneTransformCommand(&controller, oobBoneDeltas, 1002);
    controller.pushCommand(oobBoneCmd);

    PmxBoneTransformCommand baseBoneCmd(&controller, deltas, 1002);
    QVERIFY(!baseBoneCmd.mergeWith(nullptr));

    PmxBoneTransformCommand diffIdBoneCmd(&controller, deltas, 9999);
    QVERIFY(!baseBoneCmd.mergeWith(&diffIdBoneCmd));

    std::vector<BoneTransformDelta> diffCountBoneDeltas = deltas;
    diffCountBoneDeltas.push_back(d);
    PmxBoneTransformCommand diffCountBoneCmd(&controller, diffCountBoneDeltas, 1002);
    QVERIFY(!baseBoneCmd.mergeWith(&diffCountBoneCmd));

    std::vector<BoneTransformDelta> diffIdxBoneDeltas = deltas;
    diffIdxBoneDeltas[0].boneIndex = 123;
    PmxBoneTransformCommand diffIdxBoneCmd(&controller, diffIdxBoneDeltas, 1002);
    QVERIFY(!baseBoneCmd.mergeWith(&diffIdxBoneCmd));
}

void tst_pmx_viewport::testViewportGizmoIntegration() {
    PmxDocument doc;
    PmxBone b0;
    b0.position = glm::vec3(10.0f, 20.0f, 30.0f); // PMX LH
    doc.bones.push_back(b0);

    PmxDocumentController controller;
    controller.setDocumentInternal(doc);

    PmxViewportItem viewport;
    viewport.setController(&controller);

    viewport.setGizmoMode(GizmoMode::Translate);
    QCOMPARE(viewport.gizmoMode(), GizmoMode::Translate);
    QCOMPARE(viewport.gizmoModeInt(), 1);

    viewport.setCoordSpace(CoordSpace::Local);
    QCOMPARE(viewport.coordSpace(), CoordSpace::Local);
    QCOMPARE(viewport.coordSpaceInt(), 1);

    viewport.setSymmetricEditing(true);
    QVERIFY(viewport.symmetricEditing());

    viewport.setSnapEnabled(true);
    QVERIFY(viewport.snapEnabled());

    // Select bone 0 -> gizmo should update position to RH position: (10, 20, -30)
    controller.selectionManager()->setSelectedBones({ 0 });
    viewport.updateGizmoPosition();

    glm::vec3 expectedRH = PmxCamera::toRightHanded(b0.position);
    QCOMPARE(viewport.gizmo()->position(), expectedRH);
    QCOMPARE(viewport.gizmo()->position().z, -30.0f);

    // Empty selection handling: clear all selections and update gizmo position without crash
    controller.selectionManager()->clearAll();
    viewport.updateGizmoPosition();
    QVERIFY(true);

    // Test selection of rigid body and joint triggers updateGizmoPosition without crash
    controller.selectionManager()->setSelectedRigidBodies({ 0 });
    viewport.updateGizmoPosition();
    QCOMPARE(viewport.gizmo()->position(), expectedRH);

    controller.selectionManager()->setSelectedJoints({ 0 });
    viewport.updateGizmoPosition();
    QCOMPARE(viewport.gizmo()->position(), expectedRH);
}

void tst_pmx_viewport::testBoneWireframeOctahedron() {
    PmxDocument doc;
    // Bone 0: PMX LH pos (0, 0, 5), TailIsBone to Bone 1
    PmxBone b0;
    b0.position = glm::vec3(0.0f, 0.0f, 5.0f);
    b0.boneFlags = 0x0001; // Tail is bone
    b0.tailBoneIndex = 1;
    b0.parentBoneIndex = -1;
    doc.bones.push_back(b0);

    // Bone 1: pos (0, 10.0f, 5.0f), unlinked (tailOffset = 0), parent is Bone 0
    PmxBone b1;
    b1.position = glm::vec3(0.0f, 10.0f, 5.0f);
    b1.boneFlags = 0x0000;
    b1.tailOffset = glm::vec3(0.0f);
    b1.parentBoneIndex = 0;
    doc.bones.push_back(b1);

    // Bone 2: pos (5.0f, 0.0f, 0.0f), tailOffset = (0, 4.0f, 0), parent = -1
    PmxBone b2;
    b2.position = glm::vec3(5.0f, 0.0f, 0.0f);
    b2.boneFlags = 0x0000;
    b2.tailOffset = glm::vec3(0.0f, 4.0f, 0.0f);
    b2.parentBoneIndex = -1;
    doc.bones.push_back(b2);

    // Unselected generation
    auto lines = PmxWireframeRenderer::buildBoneWireframes(doc, -1);
    // b0: 24 vertices (octahedral pyramid)
    // b1: 2 vertices (parent line to b0) + 24 vertices (localized octahedron) = 26 vertices
    // b2: 24 vertices (octahedral pyramid)
    // Total = 24 + 26 + 24 = 74 vertices (37 line segments)
    QCOMPARE(lines.size(), size_t(74));

    // Verify all vertices are finite and RH handedness applied (+Z in PMX -> -Z in RH)
    for (const auto& v : lines) {
        QVERIFY(std::isfinite(v.position.x));
        QVERIFY(std::isfinite(v.position.y));
        QVERIFY(std::isfinite(v.position.z));
    }

    // Exact coordinate checks for Bone 0 diamond pyramid:
    // pHead = (0, 0, -5), pTail = (0, 10, -5)
    // v = (0, 10, 0), L = 10, dir = (0, 1, 0), up = (1, 0, 0)
    // cross(dir, up) = (0, 0, -1) -> u = (0, 0, -0.25)
    // w = cross(dir, u) = (-0.25, 0, 0)
    // pBase = pHead + 0.2 * v = (0, 2, -5)
    glm::vec3 pHead0(0.0f, 0.0f, -5.0f);
    glm::vec3 pTail0(0.0f, 10.0f, -5.0f);
    glm::vec3 d0(0.0f, 2.0f, -5.25f);
    glm::vec3 d1(-0.25f, 2.0f, -5.0f);
    glm::vec3 d2(0.0f, 2.0f, -4.75f);
    glm::vec3 d3(0.25f, 2.0f, -5.0f);

    // Lines from pHead to base diamond points
    QCOMPARE(lines[0].position, pHead0);
    QCOMPARE(lines[1].position, d0);
    QCOMPARE(lines[2].position, pHead0);
    QCOMPARE(lines[3].position, d1);
    QCOMPARE(lines[4].position, pHead0);
    QCOMPARE(lines[5].position, d2);
    QCOMPARE(lines[6].position, pHead0);
    QCOMPARE(lines[7].position, d3);

    // Lines connecting diamond base ring
    QCOMPARE(lines[8].position, d0);
    QCOMPARE(lines[9].position, d1);
    QCOMPARE(lines[10].position, d1);
    QCOMPARE(lines[11].position, d2);
    QCOMPARE(lines[12].position, d2);
    QCOMPARE(lines[13].position, d3);
    QCOMPARE(lines[14].position, d3);
    QCOMPARE(lines[15].position, d0);

    // Lines from base diamond points to pTail
    QCOMPARE(lines[16].position, d0);
    QCOMPARE(lines[17].position, pTail0);
    QCOMPARE(lines[18].position, d1);
    QCOMPARE(lines[19].position, pTail0);
    QCOMPARE(lines[20].position, d2);
    QCOMPARE(lines[21].position, pTail0);
    QCOMPARE(lines[22].position, d3);
    QCOMPARE(lines[23].position, pTail0);

    // Exact coordinate checks for Bone 1:
    // Parent bone connecting line (b0 head to b1 head)
    glm::vec3 pHead1(0.0f, 10.0f, -5.0f);
    QCOMPARE(lines[24].position, pHead0);
    QCOMPARE(lines[25].position, pHead1);

    // Fallback localized octahedron of radius 0.2f centered at pHead1
    const float rOct = 0.2f;
    glm::vec3 octTop = pHead1 + glm::vec3(0.0f, rOct, 0.0f);
    glm::vec3 octBot = pHead1 - glm::vec3(0.0f, rOct, 0.0f);
    glm::vec3 octR0 = pHead1 + glm::vec3(rOct, 0.0f, 0.0f);
    glm::vec3 octR1 = pHead1 + glm::vec3(0.0f, 0.0f, rOct);
    glm::vec3 octR2 = pHead1 - glm::vec3(rOct, 0.0f, 0.0f);
    glm::vec3 octR3 = pHead1 - glm::vec3(0.0f, 0.0f, rOct);

    // Top to ring
    QCOMPARE(lines[26].position, octTop); QCOMPARE(lines[27].position, octR0);
    QCOMPARE(lines[28].position, octTop); QCOMPARE(lines[29].position, octR1);
    QCOMPARE(lines[30].position, octTop); QCOMPARE(lines[31].position, octR2);
    QCOMPARE(lines[32].position, octTop); QCOMPARE(lines[33].position, octR3);

    // Ring lines
    QCOMPARE(lines[34].position, octR0); QCOMPARE(lines[35].position, octR1);
    QCOMPARE(lines[36].position, octR1); QCOMPARE(lines[37].position, octR2);
    QCOMPARE(lines[38].position, octR2); QCOMPARE(lines[39].position, octR3);
    QCOMPARE(lines[40].position, octR3); QCOMPARE(lines[41].position, octR0);

    // Bottom to ring
    QCOMPARE(lines[42].position, octBot); QCOMPARE(lines[43].position, octR0);
    QCOMPARE(lines[44].position, octBot); QCOMPARE(lines[45].position, octR1);
    QCOMPARE(lines[46].position, octBot); QCOMPARE(lines[47].position, octR2);
    QCOMPARE(lines[48].position, octBot); QCOMPARE(lines[49].position, octR3);

    // Unselected bone color
    QCOMPARE(lines[0].color, glm::vec4(0.2f, 0.8f, 1.0f, 0.9f));

    // Test selection of bone 0: bone 0 lines should be highlighted yellow/orange
    auto selectedLines = PmxWireframeRenderer::buildBoneWireframes(doc, 0);
    QCOMPARE(selectedLines.size(), size_t(74));
    QCOMPARE(selectedLines[0].color, glm::vec4(1.0f, 0.8f, 0.1f, 1.0f));
    // Bone 1 lines should still be unselected color
    QCOMPARE(selectedLines[24].color, glm::vec4(0.2f, 0.8f, 1.0f, 0.9f));
}

void tst_pmx_viewport::testRigidBodyWireframeShapesAndColors() {
    PmxDocument doc;

    // RB 0: Sphere (shape 0), size.x = 2.0f, mode 0 (Static/FollowBone)
    PmxRigidBody rbSphere;
    rbSphere.shape = 0;
    rbSphere.position = glm::vec3(0.0f, 10.0f, 2.0f);
    rbSphere.rotation = glm::vec3(0.0f);
    rbSphere.size = glm::vec3(2.0f, 0.0f, 0.0f);
    rbSphere.physicsMode = 0;
    doc.rigidBodies.push_back(rbSphere);

    // RB 1: Box (shape 1), size = (1.0f, 2.0f, 3.0f), mode 1 (Dynamic)
    PmxRigidBody rbBox;
    rbBox.shape = 1;
    rbBox.position = glm::vec3(5.0f, 0.0f, 0.0f);
    rbBox.rotation = glm::vec3(0.0f);
    rbBox.size = glm::vec3(1.0f, 2.0f, 3.0f);
    rbBox.physicsMode = 1;
    doc.rigidBodies.push_back(rbBox);

    // RB 2: Capsule (shape 2), size = (1.0f, 4.0f, 0.0f), mode 2 (Dynamic+Bone)
    PmxRigidBody rbCapsule;
    rbCapsule.shape = 2;
    rbCapsule.position = glm::vec3(-5.0f, 0.0f, 0.0f);
    rbCapsule.rotation = glm::vec3(0.0f);
    rbCapsule.size = glm::vec3(1.0f, 4.0f, 0.0f);
    rbCapsule.physicsMode = 2;
    doc.rigidBodies.push_back(rbCapsule);

    // Unselected build
    auto lines = PmxWireframeRenderer::buildRigidBodyWireframes(doc, -1);
    // Sphere: 3 circles * 24 segments * 2 = 144 vertices
    // Box: 12 edges * 2 = 24 vertices
    // Capsule: 2 rings (48 each = 96) + 4 vertical sides (8) + 4 dome arcs (12 segs each * 2 = 96) = 200 vertices
    // Total = 144 + 24 + 200 = 368 vertices
    QCOMPARE(lines.size(), size_t(368));

    // Verify finite coordinates
    for (const auto& v : lines) {
        QVERIFY(std::isfinite(v.position.x));
        QVERIFY(std::isfinite(v.position.y));
        QVERIFY(std::isfinite(v.position.z));
    }

    // Verify mode colors
    // Mode 0: Sky Blue (0.2, 0.55, 1.0, 0.85)
    QCOMPARE(lines[0].color, glm::vec4(0.2f, 0.55f, 1.0f, 0.85f));
    // Mode 1: Bright Orange (1.0, 0.5, 0.05, 0.85)
    QCOMPARE(lines[144].color, glm::vec4(1.0f, 0.5f, 0.05f, 0.85f));
    // Mode 2: Golden Yellow (1.0, 0.85, 0.1, 0.85)
    QCOMPARE(lines[144 + 24].color, glm::vec4(1.0f, 0.85f, 0.1f, 0.85f));

    // Test selection of RB 1 (Box)
    auto selectedLines = PmxWireframeRenderer::buildRigidBodyWireframes(doc, 1);
    QCOMPARE(selectedLines.size(), size_t(368));
    QCOMPARE(selectedLines[144].color, glm::vec4(1.0f, 0.95f, 0.3f, 1.0f));
    // Sphere should remain Mode 0 color
    QCOMPARE(selectedLines[0].color, glm::vec4(0.2f, 0.55f, 1.0f, 0.85f));

    // Exact sphere radius points for unrotated sphere at p = (0, 10, -2), r = 2.0f
    glm::vec3 pSph(0.0f, 10.0f, -2.0f);
    float rSph = 2.0f;
    QVERIFY(glm::distance(lines[0].position, pSph + glm::vec3(rSph, 0.0f, 0.0f)) < 1e-4f);
    QVERIFY(glm::distance(lines[12].position, pSph + glm::vec3(0.0f, rSph, 0.0f)) < 1e-4f);
    QVERIFY(glm::distance(lines[24].position, pSph - glm::vec3(rSph, 0.0f, 0.0f)) < 1e-4f);
    QVERIFY(glm::distance(lines[36].position, pSph - glm::vec3(0.0f, rSph, 0.0f)) < 1e-4f);
    QVERIFY(glm::distance(lines[48 + 12].position, pSph + glm::vec3(0.0f, 0.0f, rSph)) < 1e-4f);
    QVERIFY(glm::distance(lines[48 + 36].position, pSph - glm::vec3(0.0f, 0.0f, rSph)) < 1e-4f);

    // Handedness check: Sphere pos PMX (0, 10, 2) -> RH (0, 10, -2)
    glm::vec3 sphereCenter(0.0f);
    for (size_t i = 0; i < 144; ++i) {
        sphereCenter += lines[i].position;
    }
    sphereCenter /= 144.0f;
    QVERIFY(std::abs(sphereCenter.x - 0.0f) < 0.01f);
    QVERIFY(std::abs(sphereCenter.y - 10.0f) < 0.01f);
    QVERIFY(std::abs(sphereCenter.z - (-2.0f)) < 0.01f);

    // =========================================================================
    // Non-zero Euler Rotation Tests on Rigid Bodies (pitch = 45 deg, yaw = 30 deg)
    // =========================================================================
    PmxDocument rotDoc;
    glm::vec3 rotEuler(glm::radians(45.0f), glm::radians(30.0f), 0.0f);

    // Rotated Sphere
    PmxRigidBody rbSphRot;
    rbSphRot.shape = 0;
    rbSphRot.position = glm::vec3(1.0f, 2.0f, 3.0f);
    rbSphRot.rotation = rotEuler;
    rbSphRot.size = glm::vec3(1.5f, 0.0f, 0.0f);
    rotDoc.rigidBodies.push_back(rbSphRot);

    // Rotated Box
    PmxRigidBody rbBoxRot;
    rbBoxRot.shape = 1;
    rbBoxRot.position = glm::vec3(-2.0f, 4.0f, 1.0f);
    rbBoxRot.rotation = rotEuler;
    rbBoxRot.size = glm::vec3(1.0f, 2.0f, 0.5f);
    rotDoc.rigidBodies.push_back(rbBoxRot);

    // Rotated Capsule
    PmxRigidBody rbCapRot;
    rbCapRot.shape = 2;
    rbCapRot.position = glm::vec3(3.0f, 0.0f, -2.0f);
    rbCapRot.rotation = rotEuler;
    rbCapRot.size = glm::vec3(0.7f, 3.0f, 0.0f);
    rotDoc.rigidBodies.push_back(rbCapRot);

    // Expected rotation matrix R in right-handed space
    glm::vec3 rRH = PmxCamera::toRightHandedEuler(rotEuler);
    glm::mat3 rotZ = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.z, glm::vec3(0.0f, 0.0f, 1.0f)));
    glm::mat3 rotX = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.x, glm::vec3(1.0f, 0.0f, 0.0f)));
    glm::mat3 rotY = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.y, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::mat3 R = rotY * rotX * rotZ;

    auto rotLines = PmxWireframeRenderer::buildRigidBodyWireframes(rotDoc, -1);
    QCOMPARE(rotLines.size(), size_t(368));

    // 1. Assert exact rotated sphere radius points p ± R*(r, 0, 0), p ± R*(0, r, 0), p ± R*(0, 0, r)
    glm::vec3 pSphRot(1.0f, 2.0f, -3.0f);
    float rRot = 1.5f;
    glm::vec3 expPx = pSphRot + R * glm::vec3(rRot, 0.0f, 0.0f);
    glm::vec3 expNx = pSphRot + R * glm::vec3(-rRot, 0.0f, 0.0f);
    glm::vec3 expPy = pSphRot + R * glm::vec3(0.0f, rRot, 0.0f);
    glm::vec3 expNy = pSphRot + R * glm::vec3(0.0f, -rRot, 0.0f);
    glm::vec3 expPz = pSphRot + R * glm::vec3(0.0f, 0.0f, rRot);
    glm::vec3 expNz = pSphRot + R * glm::vec3(0.0f, 0.0f, -rRot);

    QVERIFY(glm::distance(rotLines[0].position, expPx) < 1e-4f);
    QVERIFY(glm::distance(rotLines[12].position, expPy) < 1e-4f);
    QVERIFY(glm::distance(rotLines[24].position, expNx) < 1e-4f);
    QVERIFY(glm::distance(rotLines[36].position, expNy) < 1e-4f);
    QVERIFY(glm::distance(rotLines[48 + 12].position, expPz) < 1e-4f);
    QVERIFY(glm::distance(rotLines[48 + 36].position, expNz) < 1e-4f);

    // 2. Assert box corner vertices rotated by R
    glm::vec3 pBoxRot(-2.0f, 4.0f, -1.0f);
    float ex = 1.0f, ey = 2.0f, ez = 0.5f;
    glm::vec3 cRot[8] = {
        pBoxRot + R * glm::vec3(-ex, -ey, -ez),
        pBoxRot + R * glm::vec3( ex, -ey, -ez),
        pBoxRot + R * glm::vec3( ex,  ey, -ez),
        pBoxRot + R * glm::vec3(-ex,  ey, -ez),
        pBoxRot + R * glm::vec3(-ex, -ey,  ez),
        pBoxRot + R * glm::vec3( ex, -ey,  ez),
        pBoxRot + R * glm::vec3( ex,  ey,  ez),
        pBoxRot + R * glm::vec3(-ex,  ey,  ez),
    };

    const size_t boxStart = 144;
    // Edge (0,1)
    QVERIFY(glm::distance(rotLines[boxStart + 0].position, cRot[0]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 1].position, cRot[1]) < 1e-4f);
    // Edge (1,2)
    QVERIFY(glm::distance(rotLines[boxStart + 2].position, cRot[1]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 3].position, cRot[2]) < 1e-4f);
    // Edge (2,3)
    QVERIFY(glm::distance(rotLines[boxStart + 4].position, cRot[2]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 5].position, cRot[3]) < 1e-4f);
    // Edge (3,0)
    QVERIFY(glm::distance(rotLines[boxStart + 6].position, cRot[3]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 7].position, cRot[0]) < 1e-4f);
    // Edge (4,5)
    QVERIFY(glm::distance(rotLines[boxStart + 8].position, cRot[4]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 9].position, cRot[5]) < 1e-4f);
    // Edge (5,6)
    QVERIFY(glm::distance(rotLines[boxStart + 10].position, cRot[5]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 11].position, cRot[6]) < 1e-4f);
    // Edge (6,7)
    QVERIFY(glm::distance(rotLines[boxStart + 12].position, cRot[6]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 13].position, cRot[7]) < 1e-4f);
    // Edge (7,4)
    QVERIFY(glm::distance(rotLines[boxStart + 14].position, cRot[7]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 15].position, cRot[4]) < 1e-4f);
    // Vertical pillars (0,4), (1,5), (2,6), (3,7)
    QVERIFY(glm::distance(rotLines[boxStart + 16].position, cRot[0]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 17].position, cRot[4]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 18].position, cRot[1]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 19].position, cRot[5]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 20].position, cRot[2]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 21].position, cRot[6]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 22].position, cRot[3]) < 1e-4f);
    QVERIFY(glm::distance(rotLines[boxStart + 23].position, cRot[7]) < 1e-4f);

    // 3. Assert capsule dome apex points at ±(h/2 + r) rotated by R
    glm::vec3 pCapRot(3.0f, 0.0f, 2.0f);
    float rCap = 0.7f;
    float hCap = 3.0f;
    float hhCap = hCap * 0.5f;

    glm::vec3 expTopApex = pCapRot + R * glm::vec3(0.0f, hhCap + rCap, 0.0f);
    glm::vec3 expBotApex = pCapRot + R * glm::vec3(0.0f, -(hhCap + rCap), 0.0f);

    const size_t capStart = 144 + 24;
    // Each of the 12 arc iterations pushes 8 vertices: top XY (2), top ZY (2), bot XY (2), bot ZY (2).
    // Segment s=6 (a0 = pi/2) top apex is at offset 104 + 6*8 = 152.
    // Segment s=6 (a0 = pi/2) bot apex is at offset 104 + 6*8 + 4 = 156.
    QVERIFY(glm::distance(rotLines[capStart + 152].position, expTopApex) < 1e-4f);
    QVERIFY(glm::distance(rotLines[capStart + 156].position, expBotApex) < 1e-4f);
}

void tst_pmx_viewport::testJointWireframeCrossLimitArcsAndAnchors() {
    PmxDocument doc;

    PmxRigidBody rbA;
    rbA.position = glm::vec3(0.0f, 0.0f, 2.0f);
    doc.rigidBodies.push_back(rbA);

    PmxRigidBody rbB;
    rbB.position = glm::vec3(0.0f, 10.0f, 2.0f);
    doc.rigidBodies.push_back(rbB);

    PmxJoint joint;
    joint.position = glm::vec3(0.0f, 5.0f, 2.0f);
    // Non-zero Euler rotation on joint cross: pitch = 45 deg, yaw = 30 deg
    joint.rotation = glm::vec3(glm::radians(45.0f), glm::radians(30.0f), 0.0f);
    joint.rigidBodyIndexA = 0;
    joint.rigidBodyIndexB = 1;
    // Asymmetric angular limits (radians)
    joint.angularLowerLimit = glm::vec3(0.0f, 0.1f, -0.3f);
    joint.angularUpperLimit = glm::vec3(0.8f, 0.7f, 0.4f);
    doc.joints.push_back(joint);

    auto lines = PmxWireframeRenderer::buildJointWireframes(doc, -1);
    // 3 axis lines * 2 = 6 vertices
    // 3 limit arcs * 12 segs * 2 = 72 vertices
    // 2 anchor lines * 2 = 4 vertices
    // Total = 6 + 72 + 4 = 82 vertices
    QCOMPARE(lines.size(), size_t(82));

    for (const auto& v : lines) {
        QVERIFY(std::isfinite(v.position.x));
        QVERIFY(std::isfinite(v.position.y));
        QVERIFY(std::isfinite(v.position.z));
    }

    // Axes: X = Red, Y = Green, Z = Blue
    QCOMPARE(lines[0].color, glm::vec4(1.0f, 0.15f, 0.15f, 1.0f)); // X
    QCOMPARE(lines[2].color, glm::vec4(0.15f, 0.9f, 0.15f, 1.0f)); // Y
    QCOMPARE(lines[4].color, glm::vec4(0.2f, 0.4f, 1.0f, 1.0f));   // Z

    // Compute expected rotation matrix R in right-handed space
    glm::vec3 rRH = PmxCamera::toRightHandedEuler(joint.rotation);
    glm::mat3 rotZ = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.z, glm::vec3(0.0f, 0.0f, 1.0f)));
    glm::mat3 rotX = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.x, glm::vec3(1.0f, 0.0f, 0.0f)));
    glm::mat3 rotY = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.y, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::mat3 R = rotY * rotX * rotZ;

    glm::vec3 p = PmxCamera::toRightHanded(joint.position);
    QCOMPARE(p, glm::vec3(0.0f, 5.0f, -2.0f));

    // Non-zero rotated coordinate cross endpoints
    glm::vec3 expCrossX = p + R * glm::vec3(0.4f, 0.0f, 0.0f);
    glm::vec3 expCrossY = p + R * glm::vec3(0.0f, 0.4f, 0.0f);
    glm::vec3 expCrossZ = p + R * glm::vec3(0.0f, 0.0f, 0.4f);

    QCOMPARE(lines[0].position, p);
    QVERIFY(glm::distance(lines[1].position, expCrossX) < 1e-4f);
    QCOMPARE(lines[2].position, p);
    QVERIFY(glm::distance(lines[3].position, expCrossY) < 1e-4f);
    QCOMPARE(lines[4].position, p);
    QVERIFY(glm::distance(lines[5].position, expCrossZ) < 1e-4f);

    // Verify asymmetric rotational limit arcs in RH space:
    // MMD to RH: rx_RH = -rx_LH, ry_RH = -ry_LH, rz_RH = rz_LH
    // minX = -upperX = -0.8f, maxX = -lowerX = 0.0f
    // minY = -upperY = -0.7f, maxY = -lowerY = -0.1f
    // minZ = lowerZ = -0.3f, maxZ = upperZ = 0.4f
    const float rArc = 0.5f;

    // X limit arc (YZ plane): s=0 (minX = -0.8f) and s=11 segment end (maxX = 0.0f)
    glm::vec3 expXArcStart = p + R * glm::vec3(0.0f, rArc * std::cos(-0.8f), rArc * std::sin(-0.8f));
    glm::vec3 expXArcEnd = p + R * glm::vec3(0.0f, rArc * std::cos(0.0f), rArc * std::sin(0.0f));
    QVERIFY(glm::distance(lines[6].position, expXArcStart) < 1e-4f);
    QVERIFY(glm::distance(lines[6 + 23].position, expXArcEnd) < 1e-4f);

    // Y limit arc (ZX plane): s=0 (minY = -0.7f) and s=11 segment end (maxY = -0.1f)
    glm::vec3 expYArcStart = p + R * glm::vec3(rArc * std::sin(-0.7f), 0.0f, rArc * std::cos(-0.7f));
    glm::vec3 expYArcEnd = p + R * glm::vec3(rArc * std::sin(-0.1f), 0.0f, rArc * std::cos(-0.1f));
    QVERIFY(glm::distance(lines[30].position, expYArcStart) < 1e-4f);
    QVERIFY(glm::distance(lines[30 + 23].position, expYArcEnd) < 1e-4f);

    // Z limit arc (XY plane): s=0 (minZ = -0.3f) and s=11 segment end (maxZ = 0.4f)
    glm::vec3 expZArcStart = p + R * glm::vec3(rArc * std::cos(-0.3f), rArc * std::sin(-0.3f), 0.0f);
    glm::vec3 expZArcEnd = p + R * glm::vec3(rArc * std::cos(0.4f), rArc * std::sin(0.4f), 0.0f);
    QVERIFY(glm::distance(lines[54].position, expZArcStart) < 1e-4f);
    QVERIFY(glm::distance(lines[54 + 23].position, expZArcEnd) < 1e-4f);

    // Anchor line color: Magenta/Purple (0.85, 0.2, 0.85, 0.8)
    const size_t anchorStart = 6 + 72;
    QCOMPARE(lines[anchorStart].color, glm::vec4(0.85f, 0.2f, 0.85f, 0.8f));

    // Selection highlight on joint
    auto selectedLines = PmxWireframeRenderer::buildJointWireframes(doc, 0);
    QCOMPARE(selectedLines.size(), size_t(82));
    QCOMPARE(selectedLines[anchorStart].color, glm::vec4(1.0f, 0.4f, 1.0f, 1.0f));
}

void tst_pmx_viewport::testWireframeRendererRobustness() {
    PmxDocument emptyDoc;
    QVERIFY(PmxWireframeRenderer::buildBoneWireframes(emptyDoc).empty());
    QVERIFY(PmxWireframeRenderer::buildRigidBodyWireframes(emptyDoc).empty());
    QVERIFY(PmxWireframeRenderer::buildJointWireframes(emptyDoc).empty());

    // Document with NaN/Inf values
    PmxDocument badDoc;
    PmxBone badBone;
    badBone.position = glm::vec3(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f);
    badDoc.bones.push_back(badBone);

    PmxRigidBody badRb;
    badRb.position = glm::vec3(std::numeric_limits<float>::infinity(), 0.0f, 0.0f);
    badDoc.rigidBodies.push_back(badRb);

    PmxJoint badJoint;
    badJoint.position = glm::vec3(0.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f);
    badJoint.rigidBodyIndexA = -99;
    badJoint.rigidBodyIndexB = 99999;
    badDoc.joints.push_back(badJoint);

    // None of these should crash or emit non-finite vertices
    auto boneLines = PmxWireframeRenderer::buildBoneWireframes(badDoc);
    QVERIFY(boneLines.empty());

    auto rbLines = PmxWireframeRenderer::buildRigidBodyWireframes(badDoc);
    QVERIFY(rbLines.empty());

    auto jointLines = PmxWireframeRenderer::buildJointWireframes(badDoc);
    QVERIFY(jointLines.empty());

    // Rigid body with negative size
    PmxDocument negDoc;
    PmxRigidBody negRb;
    negRb.shape = 1; // Box
    negRb.position = glm::vec3(0.0f);
    negRb.rotation = glm::vec3(0.0f);
    negRb.size = glm::vec3(-1.0f, -2.0f, -3.0f);
    negDoc.rigidBodies.push_back(negRb);

    auto negLines = PmxWireframeRenderer::buildRigidBodyWireframes(negDoc);
    QCOMPARE(negLines.size(), size_t(24));
    for (const auto& v : negLines) {
        QVERIFY(std::isfinite(v.position.x));
        QVERIFY(std::isfinite(v.position.y));
        QVERIFY(std::isfinite(v.position.z));
    }
}

void tst_pmx_viewport::testViewportWireframeToggles() {
    PmxViewportItem viewport;
    QCOMPARE(viewport.showRigidBodies(), true);
    QCOMPARE(viewport.showJoints(), true);

    QSignalSpy rbSpy(&viewport, &PmxViewportItem::showRigidBodiesChanged);
    QSignalSpy jointSpy(&viewport, &PmxViewportItem::showJointsChanged);

    viewport.setShowRigidBodies(false);
    QCOMPARE(viewport.showRigidBodies(), false);
    QCOMPARE(rbSpy.count(), 1);

    // No-op set should not fire signal
    viewport.setShowRigidBodies(false);
    QCOMPARE(rbSpy.count(), 1);

    viewport.setShowJoints(false);
    QCOMPARE(viewport.showJoints(), false);
    QCOMPARE(jointSpy.count(), 1);

    viewport.setShowJoints(false);
    QCOMPARE(jointSpy.count(), 1);

    viewport.setShowRigidBodies(true);
    viewport.setShowJoints(true);
    QCOMPARE(viewport.showRigidBodies(), true);
    QCOMPARE(viewport.showJoints(), true);
    QCOMPARE(rbSpy.count(), 2);
    QCOMPARE(jointSpy.count(), 2);
}

void tst_pmx_viewport::testWeightToColorGradients() {
    auto c0 = PmxWeightPaintBrush::weightToColor(0.0f);
    QCOMPARE(c0, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    auto c0125 = PmxWeightPaintBrush::weightToColor(0.125f);
    QCOMPARE(c0125, glm::vec4(0.0f, 0.5f, 1.0f, 1.0f));

    auto c025 = PmxWeightPaintBrush::weightToColor(0.25f);
    QCOMPARE(c025, glm::vec4(0.0f, 1.0f, 1.0f, 1.0f));

    auto c0375 = PmxWeightPaintBrush::weightToColor(0.375f);
    QCOMPARE(c0375, glm::vec4(0.0f, 1.0f, 0.5f, 1.0f));

    auto c05 = PmxWeightPaintBrush::weightToColor(0.5f);
    QCOMPARE(c05, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));

    auto c0625 = PmxWeightPaintBrush::weightToColor(0.625f);
    QCOMPARE(c0625, glm::vec4(0.5f, 1.0f, 0.0f, 1.0f));

    auto c075 = PmxWeightPaintBrush::weightToColor(0.75f);
    QCOMPARE(c075, glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));

    auto c0875 = PmxWeightPaintBrush::weightToColor(0.875f);
    QCOMPARE(c0875, glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));

    auto c10 = PmxWeightPaintBrush::weightToColor(1.0f);
    QCOMPARE(c10, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    // Out-of-bounds clamp tests
    auto cNeg = PmxWeightPaintBrush::weightToColor(-0.5f);
    QCOMPARE(cNeg, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    auto cOver = PmxWeightPaintBrush::weightToColor(1.5f);
    QCOMPARE(cOver, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));

    auto cNaN = PmxWeightPaintBrush::weightToColor(std::numeric_limits<float>::quiet_NaN());
    QCOMPARE(cNaN, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));

    auto cInf = PmxWeightPaintBrush::weightToColor(std::numeric_limits<float>::infinity());
    QCOMPARE(cInf, glm::vec4(0.0f, 0.0f, 1.0f, 1.0f));
}

void tst_pmx_viewport::testBrushFalloffMath() {
    float r = 2.0f;

    // Center (d = 0)
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.0f, r, BrushFalloff::Linear), 1.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.0f, r, BrushFalloff::Gaussian), 1.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.0f, r, BrushFalloff::Constant), 1.0f);

    // Midpoint (d = r / 2 = 1.0f)
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(1.0f, r, BrushFalloff::Linear), 0.5f);
    float expGaussianMid = std::exp(-3.0f * 0.25f);
    QVERIFY(std::abs(PmxWeightPaintBrush::computeFalloff(1.0f, r, BrushFalloff::Gaussian) - expGaussianMid) < 1e-5f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(1.0f, r, BrushFalloff::Constant), 1.0f);

    // Boundary (d >= r)
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(r, r, BrushFalloff::Linear), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(r + 0.5f, r, BrushFalloff::Linear), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(r + 0.5f, r, BrushFalloff::Gaussian), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(r + 0.5f, r, BrushFalloff::Constant), 0.0f);

    // Invalid radius / distance
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.5f, 0.0f, BrushFalloff::Linear), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.5f, -1.0f, BrushFalloff::Linear), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(-0.5f, r, BrushFalloff::Linear), 0.0f);
}

void tst_pmx_viewport::testApplyBrushToVertexWeights() {
    // 1. BDEF1: single bone elevation
    PmxVertex vBdef1;
    vBdef1.weightType = WeightType::BDEF1;
    vBdef1.boneIndices = { 10, -1, -1, -1 };
    vBdef1.boneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };

    // Same bone Add does nothing (already 100%)
    PmxWeightPaintBrush::applyBrushToVertex(vBdef1, 10, 1.0f, 0.5f, BrushMode::Add);
    QCOMPARE(vBdef1.weightType, WeightType::BDEF1);
    QCOMPARE(vBdef1.boneWeights[0], 1.0f);

    // Different bone Add elevates to BDEF2
    PmxWeightPaintBrush::applyBrushToVertex(vBdef1, 20, 1.0f, 0.3f, BrushMode::Add);
    QCOMPARE(vBdef1.weightType, WeightType::BDEF2);
    QCOMPARE(vBdef1.boneIndices[0], 10);
    QCOMPARE(vBdef1.boneIndices[1], 20);
    QVERIFY(std::abs(vBdef1.boneWeights[0] - 0.7f) < 1e-5f);
    QVERIFY(std::abs(vBdef1.boneWeights[1] - 0.3f) < 1e-5f);
    QVERIFY(std::abs((vBdef1.boneWeights[0] + vBdef1.boneWeights[1]) - 1.0f) < 1e-6f);

    // 2. BDEF2: Add, Subtract, Set
    PmxWeightPaintBrush::applyBrushToVertex(vBdef1, 20, 0.5f, 0.2f, BrushMode::Add);
    QVERIFY(std::abs(vBdef1.boneWeights[1] - 0.4f) < 1e-5f);
    QVERIFY(std::abs(vBdef1.boneWeights[0] - 0.6f) < 1e-5f);
    QVERIFY(std::abs((vBdef1.boneWeights[0] + vBdef1.boneWeights[1]) - 1.0f) < 1e-6f);

    PmxWeightPaintBrush::applyBrushToVertex(vBdef1, 20, 1.0f, 0.1f, BrushMode::Subtract);
    QVERIFY(std::abs(vBdef1.boneWeights[1] - 0.3f) < 1e-5f);
    QVERIFY(std::abs(vBdef1.boneWeights[0] - 0.7f) < 1e-5f);
    QVERIFY(std::abs((vBdef1.boneWeights[0] + vBdef1.boneWeights[1]) - 1.0f) < 1e-6f);

    PmxWeightPaintBrush::applyBrushToVertex(vBdef1, 20, 1.0f, 0.5f, BrushMode::Set);
    QVERIFY(std::abs(vBdef1.boneWeights[1] - 0.5f) < 1e-5f);
    QVERIFY(std::abs(vBdef1.boneWeights[0] - 0.5f) < 1e-5f);
    QVERIFY(std::abs((vBdef1.boneWeights[0] + vBdef1.boneWeights[1]) - 1.0f) < 1e-6f);

    // 3. SDEF: preserves sdef params
    PmxVertex vSdef;
    vSdef.weightType = WeightType::SDEF;
    vSdef.boneIndices = { 5, 8, -1, -1 };
    vSdef.boneWeights = { 0.6f, 0.4f, 0.0f, 0.0f };
    vSdef.sdefC = glm::vec3(1.0f, 2.0f, 3.0f);
    vSdef.sdefR0 = glm::vec3(0.0f, 1.0f, 0.0f);
    vSdef.sdefR1 = glm::vec3(0.0f, 0.0f, 1.0f);

    PmxWeightPaintBrush::applyBrushToVertex(vSdef, 5, 1.0f, 0.2f, BrushMode::Add);
    QCOMPARE(vSdef.weightType, WeightType::SDEF);
    QVERIFY(std::abs(vSdef.boneWeights[0] - 0.8f) < 1e-5f);
    QVERIFY(std::abs(vSdef.boneWeights[1] - 0.2f) < 1e-5f);
    QCOMPARE(vSdef.sdefC, glm::vec3(1.0f, 2.0f, 3.0f));
    QCOMPARE(vSdef.sdefR0, glm::vec3(0.0f, 1.0f, 0.0f));
    QCOMPARE(vSdef.sdefR1, glm::vec3(0.0f, 0.0f, 1.0f));

    // 4. BDEF4: normalization sum check
    PmxVertex vBdef4;
    vBdef4.weightType = WeightType::BDEF4;
    vBdef4.boneIndices = { 1, 2, 3, 4 };
    vBdef4.boneWeights = { 0.4f, 0.3f, 0.2f, 0.1f };

    PmxWeightPaintBrush::applyBrushToVertex(vBdef4, 2, 1.0f, 0.1f, BrushMode::Add);
    float sumWeights = vBdef4.boneWeights[0] + vBdef4.boneWeights[1] + vBdef4.boneWeights[2] + vBdef4.boneWeights[3];
    QVERIFY(std::abs(sumWeights - 1.0f) < 1e-6f);

    // Smooth mode on BDEF4
    PmxWeightPaintBrush::applyBrushToVertex(vBdef4, 1, 1.0f, 0.5f, BrushMode::Smooth);
    sumWeights = vBdef4.boneWeights[0] + vBdef4.boneWeights[1] + vBdef4.boneWeights[2] + vBdef4.boneWeights[3];
    QVERIFY(std::abs(sumWeights - 1.0f) < 1e-6f);
}

void tst_pmx_viewport::testVertexWeightCommandUndoRedo() {
    PmxDocument doc;
    PmxVertex v0;
    v0.weightType = WeightType::BDEF1;
    v0.boneIndices = { 0, -1, -1, -1 };
    v0.boneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };

    PmxVertex v1;
    v1.weightType = WeightType::BDEF2;
    v1.boneIndices = { 0, 1, -1, -1 };
    v1.boneWeights = { 0.5f, 0.5f, 0.0f, 0.0f };

    doc.vertices = { v0, v1 };

    PmxDocumentController controller;
    controller.setDocumentInternal(doc);

    VertexWeightDelta d0;
    d0.vertexIndex = 0;
    d0.oldWeightType = WeightType::BDEF1;
    d0.oldBoneIndices = { 0, -1, -1, -1 };
    d0.oldBoneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };
    d0.newWeightType = WeightType::BDEF2;
    d0.newBoneIndices = { 0, 1, -1, -1 };
    d0.newBoneWeights = { 0.7f, 0.3f, 0.0f, 0.0f };

    auto* cmd = new PmxVertexWeightCommand(&controller, { d0 }, 2001);
    controller.undoStack()->push(cmd);

    QCOMPARE(controller.document().vertices[0].weightType, WeightType::BDEF2);
    QVERIFY(std::abs(controller.document().vertices[0].boneWeights[0] - 0.7f) < 1e-5f);

    controller.undoStack()->undo();
    QCOMPARE(controller.document().vertices[0].weightType, WeightType::BDEF1);
    QCOMPARE(controller.document().vertices[0].boneWeights[0], 1.0f);

    controller.undoStack()->redo();
    QCOMPARE(controller.document().vertices[0].weightType, WeightType::BDEF2);
    QVERIFY(std::abs(controller.document().vertices[0].boneWeights[0] - 0.7f) < 1e-5f);

    // Test mergeWith on same drag session ID
    VertexWeightDelta d0_second;
    d0_second.vertexIndex = 0;
    d0_second.oldWeightType = WeightType::BDEF2;
    d0_second.oldBoneIndices = { 0, 1, -1, -1 };
    d0_second.oldBoneWeights = { 0.7f, 0.3f, 0.0f, 0.0f };
    d0_second.newWeightType = WeightType::BDEF2;
    d0_second.newBoneIndices = { 0, 1, -1, -1 };
    d0_second.newBoneWeights = { 0.4f, 0.6f, 0.0f, 0.0f };

    auto* cmd2 = new PmxVertexWeightCommand(&controller, { d0_second }, 2001);
    controller.undoStack()->push(cmd2);

    // Merged: undo restores back to original d0.old (BDEF1)
    controller.undoStack()->undo();
    QCOMPARE(controller.document().vertices[0].weightType, WeightType::BDEF1);
    QCOMPARE(controller.document().vertices[0].boneWeights[0], 1.0f);

    controller.undoStack()->redo();
    QCOMPARE(controller.document().vertices[0].weightType, WeightType::BDEF2);
    QVERIFY(std::abs(controller.document().vertices[0].boneWeights[0] - 0.4f) < 1e-5f);
}

void tst_pmx_viewport::testWeightPaintingBoundaryConditions() {
    // 1. Zero and negative radius
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.5f, 0.0f, BrushFalloff::Linear), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.5f, -2.0f, BrushFalloff::Linear), 0.0f);

    // 2. Strength > 1.0 clamped safely
    PmxVertex v;
    v.weightType = WeightType::BDEF2;
    v.boneIndices = { 1, 2, -1, -1 };
    v.boneWeights = { 0.5f, 0.5f, 0.0f, 0.0f };
    PmxWeightPaintBrush::applyBrushToVertex(v, 1, 1.0f, 5.0f, BrushMode::Add);
    QVERIFY(v.boneWeights[0] <= 1.0f);
    QVERIFY(std::abs(v.boneWeights[0] + v.boneWeights[1] - 1.0f) < 1e-6f);

    // 3. Unselected bone (-1)
    QCOMPARE(PmxWeightPaintBrush::getVertexBoneWeight(v, -1), 0.0f);
    float wBefore = v.boneWeights[0];
    PmxWeightPaintBrush::applyBrushToVertex(v, -1, 1.0f, 0.5f, BrushMode::Add);
    QCOMPARE(v.boneWeights[0], wBefore);

    // 4. Empty document raycasting
    PmxViewportItem viewport;
    Ray ray;
    ray.origin = glm::vec3(0.0f, 0.0f, 10.0f);
    ray.direction = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 hitPoint(0.0f), hitNormal(0.0f);
    QVERIFY(!viewport.raycastMesh(ray, hitPoint, hitNormal));

    // 5. NaN and Inf handling
    float nanVal = std::numeric_limits<float>::quiet_NaN();
    float infVal = std::numeric_limits<float>::infinity();

    QCOMPARE(PmxWeightPaintBrush::computeFalloff(nanVal, 1.0f, BrushFalloff::Linear), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(0.5f, nanVal, BrushFalloff::Linear), 0.0f);
    QCOMPARE(PmxWeightPaintBrush::computeFalloff(infVal, 1.0f, BrushFalloff::Linear), 0.0f);

    PmxVertex vNaN = v;
    PmxWeightPaintBrush::applyBrushToVertex(vNaN, 1, nanVal, 0.5f, BrushMode::Add);
    QCOMPARE(vNaN.boneWeights[0], v.boneWeights[0]);

    PmxWeightPaintBrush::applyBrushToVertex(vNaN, 1, 0.5f, nanVal, BrushMode::Add);
    QCOMPARE(vNaN.boneWeights[0], v.boneWeights[0]);
}

void tst_pmx_viewport::testViewportWeightPaintProperties() {
    PmxViewportItem viewport;

    // Viewport Mode
    QCOMPARE(viewport.viewportMode(), 0);
    QSignalSpy modeSpy(&viewport, &PmxViewportItem::viewportModeChanged);
    viewport.setViewportMode(2);
    QCOMPARE(viewport.viewportMode(), 2);
    QCOMPARE(modeSpy.count(), 1);

    // Brush Radius
    QCOMPARE(viewport.brushRadius(), 1.0f);
    QSignalSpy radiusSpy(&viewport, &PmxViewportItem::brushRadiusChanged);
    viewport.setBrushRadius(3.5f);
    QCOMPARE(viewport.brushRadius(), 3.5f);
    QCOMPARE(radiusSpy.count(), 1);

    // Brush Strength
    QCOMPARE(viewport.brushStrength(), 0.2f);
    QSignalSpy strengthSpy(&viewport, &PmxViewportItem::brushStrengthChanged);
    viewport.setBrushStrength(0.85f);
    QCOMPARE(viewport.brushStrength(), 0.85f);
    QCOMPARE(strengthSpy.count(), 1);

    // Brush Mode
    QCOMPARE(viewport.brushMode(), 0);
    QSignalSpy brushModeSpy(&viewport, &PmxViewportItem::brushModeChanged);
    viewport.setBrushMode(1);
    QCOMPARE(viewport.brushMode(), 1);
    QCOMPARE(brushModeSpy.count(), 1);

    // Brush Falloff
    QCOMPARE(viewport.brushFalloff(), 0);
    QSignalSpy falloffSpy(&viewport, &PmxViewportItem::brushFalloffChanged);
    viewport.setBrushFalloff(2);
    QCOMPARE(viewport.brushFalloff(), 2);
    QCOMPARE(falloffSpy.count(), 1);
}

void tst_pmx_viewport::testBrushCursorGeneration() {
    // Valid circle
    glm::vec3 center(0.0f, 10.0f, -5.0f);
    glm::vec3 normal(0.0f, 1.0f, 0.0f);
    float radius = 2.0f;
    int segments = 32;

    auto cursorLines = PmxWeightPaintBrush::buildBrushCursor(center, normal, radius, segments);
    QCOMPARE(cursorLines.size(), size_t(segments * 2));

    for (const auto& v : cursorLines) {
        QVERIFY(std::isfinite(v.position.x));
        QVERIFY(std::isfinite(v.position.y));
        QVERIFY(std::isfinite(v.position.z));
        float dist = glm::distance(v.position, center);
        QVERIFY(std::abs(dist - radius) < 1e-4f);
    }

    // Invalid radius returns empty
    QVERIFY(PmxWeightPaintBrush::buildBrushCursor(center, normal, 0.0f, segments).empty());
    QVERIFY(PmxWeightPaintBrush::buildBrushCursor(center, normal, -1.0f, segments).empty());
    QVERIFY(PmxWeightPaintBrush::buildBrushCursor(center, normal, std::numeric_limits<float>::quiet_NaN(), segments).empty());
}

QTEST_MAIN(tst_pmx_viewport)
#include "tst_pmx_viewport.moc"
