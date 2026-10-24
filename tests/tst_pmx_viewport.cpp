#include "viewport/PmxCamera.h"
#include "viewport/PmxViewportItem.h"
#include "models/PmxDocumentController.h"

#include <QTest>
#include <QUndoCommand>
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

QTEST_MAIN(tst_pmx_viewport)
#include "tst_pmx_viewport.moc"
