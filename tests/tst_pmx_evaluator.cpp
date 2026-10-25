#include "core/PmxDocument.h"
#include "core/PmxTypes.h"
#include "evaluator/PmxRuntimeScene.h"
#include "evaluator/MmdCcdIkSolver.h"

#include <QtTest>
#include <cmath>

using namespace pmx;

class TstPmxEvaluator : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Required tests per specification:
    void testDeformTierOrder();
    void testChildTransformPropagation();
    void testAppendRotationInheritance();
    void testFixedAxisConstraint();
    void testGroupMorphRecursion();
    void testNonDestructiveInvariant();

    // Additional coverage tests:
    void testMaterialMorphEvaluation();
    void testUvMorphAccumulation();
    void testCyclicGroupMorphSafety();
    void testResetTransformsRestPose();
    void testCoordinateConversions();

    // CCDIK Solver Tests (Ticket 4.2):
    void testCcdIk2LinkReachable();
    void testCcdIkKneeAngleLimits();
    void testCcdIkIterationClamping();
    void testCcdIkUnreachableTarget();
    void testCcdIkEnableToggle();
    void testEulerAngleDecomposition();
};

void TstPmxEvaluator::initTestCase() {
}

void TstPmxEvaluator::cleanupTestCase() {
}

void TstPmxEvaluator::testDeformTierOrder() {
    // Construct 3 bones with reverse tier order: Tier 2 (index 0), Tier 0 (index 1), Tier 1 (index 2)
    PmxDocument doc;

    PmxBone boneTier2;
    boneTier2.nameJp = "Tier2Bone";
    boneTier2.deformTier = 2;
    boneTier2.parentBoneIndex = 2; // Child of Tier 1
    boneTier2.position = {0.0f, 2.0f, 0.0f};

    PmxBone boneTier0;
    boneTier0.nameJp = "Tier0Bone";
    boneTier0.deformTier = 0;
    boneTier0.parentBoneIndex = -1; // Root
    boneTier0.position = {0.0f, 0.0f, 0.0f};

    PmxBone boneTier1;
    boneTier1.nameJp = "Tier1Bone";
    boneTier1.deformTier = 1;
    boneTier1.parentBoneIndex = 1; // Child of Tier 0
    boneTier1.position = {0.0f, 1.0f, 0.0f};

    doc.bones = {boneTier2, boneTier0, boneTier1};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Verify evaluation order executes Tier 0 -> Tier 1 -> Tier 2
    const auto& order = scene.evaluationOrder();
    QCOMPARE(order.size(), 3u);
    QCOMPARE(order[0], 1); // Bone at index 1 is Tier 0
    QCOMPARE(order[1], 2); // Bone at index 2 is Tier 1
    QCOMPARE(order[2], 0); // Bone at index 0 is Tier 2

    // Apply translation to Tier 0 root and assert transform propagates through Tier 1 to Tier 2
    scene.setBoneTransform(1, glm::vec3(0.0f, 5.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    // Root Tier 0: rest (0, 0, 0) + trans (0, 5, 0) = (0, 5, 0)
    QVERIFY(glm::distance(scene.worldPosition(1), glm::vec3(0.0f, 5.0f, 0.0f)) < 1e-5f);
    // Child Tier 1: rest (0, 1, 0) + trans (0, 5, 0) = (0, 6, 0)
    QVERIFY(glm::distance(scene.worldPosition(2), glm::vec3(0.0f, 6.0f, 0.0f)) < 1e-5f);
    // Grandchild Tier 2: rest (0, 2, 0) + trans (0, 5, 0) = (0, 7, 0)
    QVERIFY(glm::distance(scene.worldPosition(0), glm::vec3(0.0f, 7.0f, 0.0f)) < 1e-5f);
}

void TstPmxEvaluator::testChildTransformPropagation() {
    PmxDocument doc;

    PmxBone parent;
    parent.nameJp = "Parent";
    parent.parentBoneIndex = -1;
    parent.position = {0.0f, 1.0f, 0.0f}; // LH (0, 1, 0) -> RH (0, 1, 0)

    PmxBone child;
    child.nameJp = "Child";
    child.parentBoneIndex = 0;
    child.position = {2.0f, 1.0f, 0.0f}; // LH (2, 1, 0) -> RH (2, 1, 0)

    doc.bones = {parent, child};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Initial rest positions
    QVERIFY(glm::distance(scene.worldPosition(0), glm::vec3(0.0f, 1.0f, 0.0f)) < 1e-5f);
    QVERIFY(glm::distance(scene.worldPosition(1), glm::vec3(2.0f, 1.0f, 0.0f)) < 1e-5f);

    // Parent rotated 90 degrees around Z axis (+Z in RH)
    glm::quat rotZ90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    scene.setBoneTransform(0, glm::vec3(0.0f), rotZ90);
    scene.evaluate();

    // Parent position remains (0, 1, 0)
    QVERIFY(glm::distance(scene.worldPosition(0), glm::vec3(0.0f, 1.0f, 0.0f)) < 1e-4f);

    // Relative rest offset is (2, 0, 0).
    // Rotated 90 deg around Z gives (0, 2, 0).
    // Child world position should be (0, 1, 0) + (0, 2, 0) = (0, 3, 0).
    glm::vec3 childPos = scene.worldPosition(1);
    QVERIFY(glm::distance(childPos, glm::vec3(0.0f, 3.0f, 0.0f)) < 1e-4f);

    // Verify child skinning matrix transforms a vertex at child rest position to current world position
    glm::vec4 restVertex(2.0f, 1.0f, 0.0f, 1.0f);
    glm::vec4 deformedVertex = scene.skinningMatrix(1) * restVertex;
    QVERIFY(glm::distance(glm::vec3(deformedVertex), glm::vec3(0.0f, 3.0f, 0.0f)) < 1e-4f);
}

void TstPmxEvaluator::testAppendRotationInheritance() {
    PmxDocument doc;

    // Bone 0: source bone (e.g. waist)
    PmxBone waist;
    waist.nameJp = "Waist";
    waist.parentBoneIndex = -1;
    waist.position = {0.0f, 0.0f, 0.0f};

    // Bone 1: half-rotation bone (weight = 0.5)
    PmxBone halfBone;
    halfBone.nameJp = "HalfBone";
    halfBone.parentBoneIndex = -1;
    halfBone.position = {0.0f, 1.0f, 0.0f};
    halfBone.boneFlags = 0x0100; // InheritRotation
    halfBone.inheritParentIndex = 0;
    halfBone.inheritWeight = 0.5f;

    // Bone 2: waist cancel bone (weight = -1.0, child of waist)
    PmxBone cancelBone;
    cancelBone.nameJp = "CancelBone";
    cancelBone.parentBoneIndex = 0;
    cancelBone.position = {0.0f, 2.0f, 0.0f};
    cancelBone.boneFlags = 0x0100; // InheritRotation
    cancelBone.inheritParentIndex = 0;
    cancelBone.inheritWeight = -1.0f;

    doc.bones = {waist, halfBone, cancelBone};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Rotate waist by 60 degrees around Y axis
    glm::quat rotY60 = glm::angleAxis(glm::radians(60.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    scene.setBoneTransform(0, glm::vec3(0.0f), rotY60);
    scene.evaluate();

    // 1. Verify half bone inherited 30 degrees around Y axis (weight 0.5)
    glm::quat expectedHalf = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 vX(1.0f, 0.0f, 0.0f);
    glm::vec3 halfTransformed = glm::vec3(scene.localMatrix(1) * glm::vec4(vX, 0.0f));
    glm::vec3 expectedHalfVec = expectedHalf * vX;
    QVERIFY(glm::distance(halfTransformed, expectedHalfVec) < 1e-4f);

    // 2. Verify cancel bone pattern:
    // Append rotation is -60 degrees around Y.
    // Since cancelBone is a child of waist (which has +60 deg rotation),
    // waist's +60 deg and cancelBone's -60 deg cancel out in world space:
    // Total world rotation for cancelBone is identity!
    glm::vec3 cancelWorldX = glm::vec3(scene.worldMatrix(2) * glm::vec4(vX, 0.0f));
    QVERIFY(glm::distance(cancelWorldX, vX) < 1e-4f);
}

void TstPmxEvaluator::testFixedAxisConstraint() {
    PmxDocument doc;

    PmxBone bone;
    bone.nameJp = "FixedAxisBone";
    bone.parentBoneIndex = -1;
    bone.position = {0.0f, 0.0f, 0.0f};
    bone.boneFlags = 0x0400; // FixedAxis
    bone.fixedAxisVector = {0.0f, 1.0f, 0.0f}; // Fixed along Y axis

    doc.bones = {bone};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Apply rotation containing both pitch (45 deg around X) and yaw (30 deg around Y)
    glm::quat pitch = glm::angleAxis(glm::radians(45.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat yaw = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat combined = pitch * yaw;

    scene.setBoneTransform(0, glm::vec3(0.0f), combined);
    scene.evaluate();

    // Fixed axis constraint must strip pitch and preserve ONLY yaw around Y
    glm::vec3 vX(1.0f, 0.0f, 0.0f);
    glm::vec3 transformedX = glm::vec3(scene.localMatrix(0) * glm::vec4(vX, 0.0f));

    // Vector must remain strictly in the XZ plane (Y == 0)
    QVERIFY(std::abs(transformedX.y) < 1e-5f);

    // Vector direction must match pure yaw around Y
    glm::vec3 expectedYawX = yaw * vX;
    QVERIFY(glm::distance(transformedX, expectedYawX) < 1e-4f);

    // The fixed axis vector itself (0, 1, 0) must remain completely invariant
    glm::vec3 vY(0.0f, 1.0f, 0.0f);
    glm::vec3 transformedY = glm::vec3(scene.localMatrix(0) * glm::vec4(vY, 0.0f));
    QVERIFY(glm::distance(transformedY, vY) < 1e-5f);
}

void TstPmxEvaluator::testGroupMorphRecursion() {
    PmxDocument doc;

    // Single vertex and single bone
    PmxVertex v0;
    v0.position = {0.0f, 0.0f, 0.0f};
    doc.vertices = {v0};

    PmxBone b0;
    b0.nameJp = "Root";
    b0.position = {0.0f, 0.0f, 0.0f};
    doc.bones = {b0};

    // Morph 0: Vertex morph (offset X=1, Y=2, Z=3 in LH -> Z=-3 in RH)
    PmxMorph mVertex;
    mVertex.nameJp = "VertexMorph";
    mVertex.morphType = 1;
    mVertex.vertexOffsets.push_back({0, {1.0f, 2.0f, 3.0f}});

    // Morph 1: Bone morph (translation Y=4.0)
    PmxMorph mBone;
    mBone.nameJp = "BoneMorph";
    mBone.morphType = 2;
    mBone.boneOffsets.push_back({0, {0.0f, 4.0f, 0.0f}, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)});

    // Morph 2: Group morph containing Morph 0 (weight 0.5) and Morph 1 (weight 0.4)
    PmxMorph mGroupChild;
    mGroupChild.nameJp = "ChildGroup";
    mGroupChild.morphType = 0;
    mGroupChild.groupOffsets.push_back({0, 0.5f});
    mGroupChild.groupOffsets.push_back({1, 0.4f});

    // Morph 3: Parent Group morph containing Morph 2 (weight 0.8)
    PmxMorph mGroupParent;
    mGroupParent.nameJp = "ParentGroup";
    mGroupParent.morphType = 0;
    mGroupParent.groupOffsets.push_back({2, 0.8f});

    doc.morphs = {mVertex, mBone, mGroupChild, mGroupParent};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Activate Parent Group morph with weight 0.5
    scene.setMorphWeight(3, 0.5f);
    scene.evaluate();

    // Calculation:
    // Morph 3 weight = 0.5
    // Morph 2 effective weight = 0.5 * 0.8 = 0.4
    // Morph 0 (Vertex) effective weight = 0.4 * 0.5 = 0.2
    // Morph 1 (Bone) effective weight = 0.4 * 0.4 = 0.16

    // Expected vertex offset: 0.2 * (1.0, 2.0, -3.0) = (0.2, 0.4, -0.6)
    glm::vec3 expectedVertexOffset(0.2f, 0.4f, -0.6f);
    QVERIFY(glm::distance(scene.vertexOffset(0), expectedVertexOffset) < 1e-5f);

    // Expected bone morph translation: 0.16 * (0.0, 4.0, 0.0) = (0.0, 0.64, 0.0)
    glm::vec3 expectedBoneTrans(0.0f, 0.64f, 0.0f);
    QVERIFY(glm::distance(scene.bone(0).morphTranslation, expectedBoneTrans) < 1e-5f);
}

void TstPmxEvaluator::testNonDestructiveInvariant() {
    PmxDocument doc;
    doc.header.version = 2.0f;
    doc.header.vertexIndexSize = 2;
    doc.modelInfo.modelNameJp = "ImmutableModel";

    PmxVertex v;
    v.position = {1.0f, 2.0f, 3.0f};
    v.normal = {0.0f, 1.0f, 0.0f};
    doc.vertices = {v};

    PmxBone b;
    b.nameJp = "Bone0";
    b.position = {4.0f, 5.0f, 6.0f};
    doc.bones = {b};

    PmxMaterial m;
    m.nameJp = "Material0";
    m.diffuse = {0.8f, 0.8f, 0.8f, 1.0f};
    doc.materials = {m};

    PmxMorph morph;
    morph.nameJp = "Morph0";
    morph.morphType = 1;
    morph.vertexOffsets.push_back({0, {0.1f, 0.2f, 0.3f}});
    doc.morphs = {morph};

    // Create a pristine snapshot copy
    const PmxDocument docSnapshot = doc;

    // Perform intensive runtime evaluations
    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    scene.setBoneTransform(0, glm::vec3(10.0f, 20.0f, 30.0f), glm::angleAxis(1.5f, glm::vec3(0, 1, 0)));
    scene.setMorphWeight(0, 0.75f);
    scene.evaluate();

    scene.resetTransforms();
    scene.evaluate();

    // Verify doc remains 100% bitwise identical to docSnapshot
    QCOMPARE(doc.header.version, docSnapshot.header.version);
    QCOMPARE(doc.header.vertexIndexSize, docSnapshot.header.vertexIndexSize);
    QCOMPARE(doc.modelInfo.modelNameJp, docSnapshot.modelInfo.modelNameJp);

    QCOMPARE(doc.vertices.size(), docSnapshot.vertices.size());
    QCOMPARE(doc.vertices[0].position.x, docSnapshot.vertices[0].position.x);
    QCOMPARE(doc.vertices[0].position.y, docSnapshot.vertices[0].position.y);
    QCOMPARE(doc.vertices[0].position.z, docSnapshot.vertices[0].position.z);
    QCOMPARE(doc.vertices[0].normal.y, docSnapshot.vertices[0].normal.y);

    QCOMPARE(doc.bones.size(), docSnapshot.bones.size());
    QCOMPARE(doc.bones[0].nameJp, docSnapshot.bones[0].nameJp);
    QCOMPARE(doc.bones[0].position.x, docSnapshot.bones[0].position.x);
    QCOMPARE(doc.bones[0].position.y, docSnapshot.bones[0].position.y);
    QCOMPARE(doc.bones[0].position.z, docSnapshot.bones[0].position.z);

    QCOMPARE(doc.materials.size(), docSnapshot.materials.size());
    QCOMPARE(doc.materials[0].diffuse.r, docSnapshot.materials[0].diffuse.r);

    QCOMPARE(doc.morphs.size(), docSnapshot.morphs.size());
    QCOMPARE(doc.morphs[0].vertexOffsets.size(), docSnapshot.morphs[0].vertexOffsets.size());
    QCOMPARE(doc.morphs[0].vertexOffsets[0].positionOffset.x, docSnapshot.morphs[0].vertexOffsets[0].positionOffset.x);
}

void TstPmxEvaluator::testMaterialMorphEvaluation() {
    PmxDocument doc;

    PmxMaterial mat0;
    mat0.nameJp = "Mat0";
    mat0.diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
    mat0.ambient = {0.4f, 0.4f, 0.4f};
    mat0.specular = {0.8f, 0.8f, 0.8f};
    mat0.specularPower = 10.0f;

    PmxMaterial mat1;
    mat1.nameJp = "Mat1";
    mat1.diffuse = {0.6f, 0.6f, 0.6f, 1.0f};
    mat1.ambient = {0.2f, 0.2f, 0.2f};
    mat1.specular = {0.5f, 0.5f, 0.5f};

    doc.materials = {mat0, mat1};

    // Morph 0: Multiply on Mat0: diffuse * 0.5
    PmxMorph morphMul;
    morphMul.morphType = 8;
    MaterialMorphOffset offMul;
    offMul.materialIndex = 0;
    offMul.opType = 0; // Multiply
    offMul.diffuse = {0.5f, 0.5f, 0.5f, 1.0f};
    morphMul.materialOffsets.push_back(offMul);

    // Morph 1: Add on all materials (-1): ambient + 0.1
    PmxMorph morphAdd;
    morphAdd.morphType = 8;
    MaterialMorphOffset offAdd;
    offAdd.materialIndex = -1; // All materials
    offAdd.opType = 1; // Add
    offAdd.diffuse = {0.0f, 0.0f, 0.0f, 0.0f};
    offAdd.specular = {0.0f, 0.0f, 0.0f};
    offAdd.specularPower = 0.0f;
    offAdd.ambient = {0.1f, 0.1f, 0.1f};
    offAdd.edgeColor = {0.0f, 0.0f, 0.0f, 0.0f};
    offAdd.edgeSize = 0.0f;
    offAdd.textureTint = {0.0f, 0.0f, 0.0f, 0.0f};
    offAdd.sphereTint = {0.0f, 0.0f, 0.0f, 0.0f};
    offAdd.toonTint = {0.0f, 0.0f, 0.0f, 0.0f};
    morphAdd.materialOffsets.push_back(offAdd);

    doc.morphs = {morphMul, morphAdd};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    scene.setMorphWeight(0, 1.0f); // Multiply by 0.5
    scene.setMorphWeight(1, 0.5f); // Add 0.5 * 0.1 = 0.05
    scene.evaluate();

    const auto& em0 = scene.evaluatedMaterial(0);
    const auto& em1 = scene.evaluatedMaterial(1);

    // Mat0 diffuse should be 1.0 * 0.5 = 0.5
    QVERIFY(std::abs(em0.diffuse.r - 0.5f) < 1e-5f);
    // Mat0 ambient should be 0.4 + 0.05 = 0.45
    QVERIFY(std::abs(em0.ambient.r - 0.45f) < 1e-5f);

    // Mat1 diffuse should be unchanged (0.6)
    QVERIFY(std::abs(em1.diffuse.r - 0.6f) < 1e-5f);
    // Mat1 ambient should be 0.2 + 0.05 = 0.25
    QVERIFY(std::abs(em1.ambient.r - 0.25f) < 1e-5f);
}

void TstPmxEvaluator::testUvMorphAccumulation() {
    PmxDocument doc;
    PmxVertex v0;
    doc.vertices = {v0};

    PmxMorph uvMorph;
    uvMorph.morphType = 3; // UV
    uvMorph.uvOffsets.push_back({0, {0.1f, 0.2f, 0.3f, 0.4f}});

    doc.morphs = {uvMorph};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    scene.setMorphWeight(0, 2.0f);
    scene.evaluate();

    glm::vec4 uv = scene.uvOffset(0);
    QVERIFY(std::abs(uv.x - 0.2f) < 1e-5f);
    QVERIFY(std::abs(uv.y - 0.4f) < 1e-5f);
    QVERIFY(std::abs(uv.z - 0.6f) < 1e-5f);
    QVERIFY(std::abs(uv.w - 0.8f) < 1e-5f);
}

void TstPmxEvaluator::testCyclicGroupMorphSafety() {
    PmxDocument doc;
    PmxVertex v0;
    doc.vertices = {v0};

    PmxMorph m0;
    m0.morphType = 0; // Group
    m0.groupOffsets.push_back({1, 1.0f}); // References 1

    PmxMorph m1;
    m1.morphType = 0; // Group
    m1.groupOffsets.push_back({0, 1.0f}); // References 0 (Cycle!)

    doc.morphs = {m0, m1};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    scene.setMorphWeight(0, 1.0f);
    // Must terminate cleanly without stack overflow
    scene.evaluate();
    QCOMPARE(scene.morphWeight(0), 1.0f);
}

void TstPmxEvaluator::testResetTransformsRestPose() {
    PmxDocument doc;
    PmxBone bone;
    bone.position = {1.0f, 2.0f, 3.0f};
    doc.bones = {bone};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    scene.setBoneTransform(0, glm::vec3(5.0f, 0.0f, 0.0f), glm::angleAxis(1.0f, glm::vec3(0, 1, 0)));
    scene.evaluate();

    // Verify non-rest state
    QVERIFY(scene.bone(0).userTranslation.x > 0.0f);

    // Reset transforms
    scene.resetTransforms();

    // Bone dynamic state must be zero/identity
    QVERIFY(glm::distance(scene.bone(0).userTranslation, glm::vec3(0.0f)) < 1e-6f);
    QVERIFY(scene.bone(0).userRotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // Skinning matrix must be identity matrix
    const glm::mat4& skin = scene.skinningMatrix(0);
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            float expected = (col == row) ? 1.0f : 0.0f;
            QVERIFY(std::abs(skin[col][row] - expected) < 1e-5f);
        }
    }
}

void TstPmxEvaluator::testCoordinateConversions() {
    // Vector conversion: LH (1, 2, 3) -> RH (1, 2, -3)
    glm::vec3 vLH(1.0f, 2.0f, 3.0f);
    glm::vec3 vRH = PmxRuntimeScene::toRightHanded(vLH);
    QCOMPARE(vRH.x, 1.0f);
    QCOMPARE(vRH.y, 2.0f);
    QCOMPARE(vRH.z, -3.0f);

    glm::vec3 vBack = PmxRuntimeScene::toLeftHanded(vRH);
    QCOMPARE(vBack, vLH);

    // Quaternion conversion
    glm::quat qLH(0.7071f, 0.1f, 0.2f, 0.3f); // (w, x, y, z)
    glm::quat qRH = PmxRuntimeScene::toRightHanded(qLH);
    QCOMPARE(qRH.w, qLH.w);
    QCOMPARE(qRH.x, -qLH.x);
    QCOMPARE(qRH.y, -qLH.y);
    QCOMPARE(qRH.z, qLH.z);

    glm::quat qRoundTrip = PmxRuntimeScene::toLeftHanded(qRH);
    QCOMPARE(qRoundTrip.w, qLH.w);
    QCOMPARE(qRoundTrip.x, qLH.x);
    QCOMPARE(qRoundTrip.y, qLH.y);
    QCOMPARE(qRoundTrip.z, qLH.z);
}

void TstPmxEvaluator::testCcdIk2LinkReachable() {
    PmxDocument doc;

    // Bone 0: Thigh (chain root)
    PmxBone thigh;
    thigh.nameJp = "左足";
    thigh.parentBoneIndex = -1;
    thigh.position = {0.0f, 2.0f, 0.0f};

    // Bone 1: Shin (knee)
    PmxBone shin;
    shin.nameJp = "左ひざ";
    shin.parentBoneIndex = 0;
    shin.position = {0.0f, 1.0f, 0.0f};

    // Bone 2: Ankle (effector)
    PmxBone ankle;
    ankle.nameJp = "左足首";
    ankle.parentBoneIndex = 1;
    ankle.position = {0.0f, 0.0f, 0.0f};

    // Bone 3: IK Controller Bone (IK bone)
    PmxBone legIk;
    legIk.nameJp = "左足ＩＫ";
    legIk.parentBoneIndex = -1;
    legIk.position = {0.0f, 0.0f, 0.0f};
    legIk.boneFlags = 0x0020; // IK Flag
    legIk.ikTargetBoneIndex = 2; // Ankle
    legIk.ikLoopCount = 40;
    legIk.ikLimitAngle = 0.5f;

    // Links: leaf (Shin) to root (Thigh)
    PmxIkLink linkShin;
    linkShin.boneIndex = 1;
    linkShin.hasAngleLimit = 0;

    PmxIkLink linkThigh;
    linkThigh.boneIndex = 0;
    linkThigh.hasAngleLimit = 0;

    legIk.ikLinks = {linkShin, linkThigh};

    doc.bones = {thigh, shin, ankle, legIk};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Initial pose: effector is at (0, 0, 0)
    QVERIFY(glm::distance(scene.worldPosition(2), glm::vec3(0.0f, 0.0f, 0.0f)) < 1e-4f);

    // Move IK target within reach: (0.0f, 0.5f, 0.5f)
    // Distance from Thigh (0, 2, 0) is sqrt(1.5^2 + 0.5^2) = sqrt(2.5) ~ 1.58 < 2.0 (reachable)
    scene.setBoneTransform(3, glm::vec3(0.0f, 0.5f, 0.5f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    glm::vec3 effectorPos = scene.worldPosition(2);
    glm::vec3 targetPos = scene.worldPosition(3);
    float dist = glm::distance(effectorPos, targetPos);
    QVERIFY2(dist < 1e-3f, qPrintable(QString("Effector dist %1 >= 1e-3").arg(dist)));
}

void TstPmxEvaluator::testCcdIkKneeAngleLimits() {
    PmxDocument doc;

    // Bone 0: Thigh
    PmxBone thigh;
    thigh.nameJp = "左足";
    thigh.parentBoneIndex = -1;
    thigh.position = {0.0f, 2.0f, 0.0f};

    // Bone 1: Shin (Knee joint)
    PmxBone shin;
    shin.nameJp = "左ひざ";
    shin.parentBoneIndex = 0;
    shin.position = {0.0f, 1.0f, 0.0f};

    // Bone 2: Ankle (effector)
    PmxBone ankle;
    ankle.nameJp = "左足首";
    ankle.parentBoneIndex = 1;
    ankle.position = {0.0f, 0.0f, 0.0f};

    // Bone 3: IK Controller
    PmxBone legIk;
    legIk.nameJp = "左足ＩＫ";
    legIk.parentBoneIndex = -1;
    legIk.position = {0.0f, 0.0f, 0.0f};
    legIk.boneFlags = 0x0020;
    legIk.ikTargetBoneIndex = 2;
    legIk.ikLoopCount = 60;
    legIk.ikLimitAngle = 0.5f;

    // Knee link has angle limit in Left-Handed PMX:
    // MMD Knee LH: limitMin = (-3.14159, 0, 0), limitMax = (-0.005, 0, 0)
    // Converted to RH: minX = -(-0.005) = +0.005, maxX = -(-3.14159) = +3.14159, Y=0, Z=0.
    PmxIkLink linkShin;
    linkShin.boneIndex = 1;
    linkShin.hasAngleLimit = 1;
    linkShin.limitMin = glm::vec3(-3.14159f, 0.0f, 0.0f);
    linkShin.limitMax = glm::vec3(-0.005f, 0.0f, 0.0f);

    PmxIkLink linkThigh;
    linkThigh.boneIndex = 0;
    linkThigh.hasAngleLimit = 0;

    legIk.ikLinks = {linkShin, linkThigh};

    doc.bones = {thigh, shin, ankle, legIk};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Case A: Move target where bending forward (+Z in RH) is required
    // Target at (0.0f, 0.6f, 0.8f) in RH
    scene.setBoneTransform(3, glm::vec3(0.0f, 0.6f, 0.8f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    glm::vec3 kneeEuler = MmdCcdIkSolver::quatToZxyEuler(scene.bone(1).ikRotation);
    // Knee must bend with positive X angle >= 0.005
    QVERIFY2(kneeEuler.x >= 0.005f, qPrintable(QString("Knee X %1 < 0.005").arg(kneeEuler.x)));
    QVERIFY2(std::abs(kneeEuler.y) < 1e-3f, "Knee Y angle should be clamped to 0");
    QVERIFY2(std::abs(kneeEuler.z) < 1e-3f, "Knee Z angle should be clamped to 0");

    // Case B: Move target in the opposite direction where bending backward (-X) would be attempted
    // Target at (0.0f, 0.6f, -0.8f)
    scene.setBoneTransform(3, glm::vec3(0.0f, 0.6f, -0.8f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    glm::vec3 kneeEulerOpposite = MmdCcdIkSolver::quatToZxyEuler(scene.bone(1).ikRotation);
    // Knee must NEVER bend backwards into negative angle: kneeEulerOpposite.x >= 0.0f
    QVERIFY2(kneeEulerOpposite.x >= 0.0f, qPrintable(QString("Knee bent backward into negative X: %1").arg(kneeEulerOpposite.x)));
    QVERIFY2(std::abs(kneeEulerOpposite.y) < 1e-3f, "Knee Y angle should remain 0");
    QVERIFY2(std::abs(kneeEulerOpposite.z) < 1e-3f, "Knee Z angle should remain 0");
}

void TstPmxEvaluator::testCcdIkIterationClamping() {
    PmxDocument doc;

    PmxBone thigh;
    thigh.nameJp = "Thigh";
    thigh.parentBoneIndex = -1;
    thigh.position = {0.0f, 2.0f, 0.0f};

    PmxBone ankle;
    ankle.nameJp = "Ankle";
    ankle.parentBoneIndex = 0;
    ankle.position = {0.0f, 0.0f, 0.0f};

    PmxBone ikBone;
    ikBone.nameJp = "IK";
    ikBone.parentBoneIndex = -1;
    ikBone.position = {0.0f, 0.0f, 0.0f};
    ikBone.boneFlags = 0x0020;
    ikBone.ikTargetBoneIndex = 1;
    // Set a very small limitAngle = 0.05 rad and loopCount = 2
    ikBone.ikLoopCount = 2;
    ikBone.ikLimitAngle = 0.05f;

    PmxIkLink linkThigh;
    linkThigh.boneIndex = 0;
    linkThigh.hasAngleLimit = 0;
    ikBone.ikLinks = {linkThigh};

    doc.bones = {thigh, ankle, ikBone};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Target is 90 degrees away: at (2.0f, 2.0f, 0.0f), requiring ~1.57 rad rotation
    scene.setBoneTransform(2, glm::vec3(2.0f, 2.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    // With loopCount=2 and limitAngle=0.05, max rotation cannot exceed 2 * 0.05 = 0.10 rad
    glm::quat rot = scene.bone(0).ikRotation;
    float angle = 2.0f * std::acos(std::clamp(rot.w, -1.0f, 1.0f));
    QVERIFY2(angle <= 0.105f, qPrintable(QString("Angle %1 exceeded max 2*limitAngle (0.10)").arg(angle)));
    QVERIFY2(angle >= 0.095f, qPrintable(QString("Angle %1 did not progress by 2*limitAngle").arg(angle)));

    // Now test with 4 iterations:
    scene.ikChains()[0].loopCount = 4;
    scene.evaluate();
    rot = scene.bone(0).ikRotation;
    angle = 2.0f * std::acos(std::clamp(rot.w, -1.0f, 1.0f));
    QVERIFY2(angle <= 0.205f, qPrintable(QString("Angle %1 exceeded max 4*limitAngle (0.20)").arg(angle)));
    QVERIFY2(angle >= 0.195f, qPrintable(QString("Angle %1 did not progress by 4*limitAngle").arg(angle)));
}

void TstPmxEvaluator::testCcdIkUnreachableTarget() {
    PmxDocument doc;

    PmxBone thigh;
    thigh.nameJp = "Thigh";
    thigh.parentBoneIndex = -1;
    thigh.position = {0.0f, 2.0f, 0.0f};

    PmxBone shin;
    shin.nameJp = "Shin";
    shin.parentBoneIndex = 0;
    shin.position = {0.0f, 1.0f, 0.0f};

    PmxBone ankle;
    ankle.nameJp = "Ankle";
    ankle.parentBoneIndex = 1;
    ankle.position = {0.0f, 0.0f, 0.0f};

    PmxBone ikBone;
    ikBone.nameJp = "IK";
    ikBone.parentBoneIndex = -1;
    ikBone.position = {0.0f, 0.0f, 0.0f};
    ikBone.boneFlags = 0x0020;
    ikBone.ikTargetBoneIndex = 2;
    ikBone.ikLoopCount = 40;
    ikBone.ikLimitAngle = 0.5f;

    PmxIkLink linkShin{1, 0, {}, {}};
    PmxIkLink linkThigh{0, 0, {}, {}};
    ikBone.ikLinks = {linkShin, linkThigh};

    doc.bones = {thigh, shin, ankle, ikBone};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Target far away along +X: (10.0, 2.0, 0.0) - chain total reach is 2.0
    scene.setBoneTransform(3, glm::vec3(10.0f, 2.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    // Check all bone world matrices and positions for NaN or Inf
    for (int i = 0; i < scene.boneCount(); ++i) {
        glm::vec3 pos = scene.worldPosition(i);
        QVERIFY(!std::isnan(pos.x) && !std::isnan(pos.y) && !std::isnan(pos.z));
        QVERIFY(!std::isinf(pos.x) && !std::isinf(pos.y) && !std::isinf(pos.z));
        const glm::mat4& m = scene.worldMatrix(i);
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                QVERIFY(!std::isnan(m[c][r]));
                QVERIFY(!std::isinf(m[c][r]));
            }
        }
    }

    // Effector should be extended along target direction (+X) from root (0, 2, 0)
    glm::vec3 effectorPos = scene.worldPosition(2);
    float distFromRoot = glm::distance(effectorPos, glm::vec3(0.0f, 2.0f, 0.0f));
    // Full extension of length 1.0 + 1.0 = 2.0
    QVERIFY2(std::abs(distFromRoot - 2.0f) < 0.05f,
             qPrintable(QString("Chain extension %1 != 2.0").arg(distFromRoot)));
    // Effector x position should be near 2.0
    QVERIFY2(std::abs(effectorPos.x - 2.0f) < 0.05f,
             qPrintable(QString("Effector X %1 != 2.0").arg(effectorPos.x)));
}

void TstPmxEvaluator::testCcdIkEnableToggle() {
    PmxDocument doc;

    PmxBone thigh;
    thigh.nameJp = "Thigh";
    thigh.parentBoneIndex = -1;
    thigh.position = {0.0f, 2.0f, 0.0f};

    PmxBone shin;
    shin.nameJp = "Shin";
    shin.parentBoneIndex = 0;
    shin.position = {0.0f, 1.0f, 0.0f};

    PmxBone ankle;
    ankle.nameJp = "Ankle";
    ankle.parentBoneIndex = 1;
    ankle.position = {0.0f, 0.0f, 0.0f};

    PmxBone ikBone;
    ikBone.nameJp = "IK";
    ikBone.parentBoneIndex = -1;
    ikBone.position = {0.0f, 0.0f, 0.0f};
    ikBone.boneFlags = 0x0020;
    ikBone.ikTargetBoneIndex = 2;
    ikBone.ikLoopCount = 40;
    ikBone.ikLimitAngle = 0.5f;

    PmxIkLink linkShin{1, 0, {}, {}};
    PmxIkLink linkThigh{0, 0, {}, {}};
    ikBone.ikLinks = {linkShin, linkThigh};

    doc.bones = {thigh, shin, ankle, ikBone};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Initial check: 1 IK chain loaded and enabled
    QCOMPARE(scene.ikChainCount(), 1);
    QVERIFY(scene.isIkEnabled(3));

    // Move target and evaluate: IK solves and bends links
    scene.setBoneTransform(3, glm::vec3(0.0f, 0.5f, 0.5f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    QVERIFY(glm::distance(scene.worldPosition(2), glm::vec3(0.0f, 0.5f, 0.5f)) < 1e-3f);
    // Link rotations are non-identity
    QVERIFY(scene.bone(0).ikRotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    QVERIFY(scene.bone(1).ikRotation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // Disable IK chain on the IK bone
    scene.setIkEnabled(3, false);
    QVERIFY(!scene.isIkEnabled(3));

    // Evaluate: links must revert to rest/user pose
    scene.evaluate();

    QVERIFY(scene.bone(0).ikRotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    QVERIFY(scene.bone(1).ikRotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    // Effector position is back at rest pose (0, 0, 0)
    QVERIFY(glm::distance(scene.worldPosition(2), glm::vec3(0.0f, 0.0f, 0.0f)) < 1e-4f);

    // Re-enable IK chain
    scene.setIkEnabled(3, true);
    QVERIFY(scene.isIkEnabled(3));
    scene.evaluate();

    // Effector reaches target again
    QVERIFY(glm::distance(scene.worldPosition(2), glm::vec3(0.0f, 0.5f, 0.5f)) < 1e-3f);
}

void TstPmxEvaluator::testEulerAngleDecomposition() {
    glm::vec3 testAngles[] = {
        {0.0f, 0.0f, 0.0f},
        {0.5f, 0.0f, 0.0f},
        {-0.3f, 0.0f, 0.0f},
        {0.0f, 0.4f, 0.0f},
        {0.0f, 0.0f, -0.6f},
        {0.2f, -0.3f, 0.4f},
        {-0.5f, 0.7f, -0.2f},
        {1.5707f, 0.3f, 0.0f},  // near gimbal lock +pi/2
        {-1.5707f, 0.5f, 0.0f}, // near gimbal lock -pi/2
    };

    for (const auto& a : testAngles) {
        glm::quat q = MmdCcdIkSolver::zxyEulerToQuat(a);
        glm::vec3 eulerRec = MmdCcdIkSolver::quatToZxyEuler(q);
        glm::quat qRec = MmdCcdIkSolver::zxyEulerToQuat(eulerRec);

        float dotVal = std::abs(glm::dot(q, qRec));
        QVERIFY2(std::abs(dotVal - 1.0f) < 1e-4f,
                 qPrintable(QString("Euler roundtrip failed for (%1, %2, %3)").arg(a.x).arg(a.y).arg(a.z)));
    }

    // Clamping test:
    glm::quat qOver = MmdCcdIkSolver::zxyEulerToQuat(glm::vec3(2.0f, -1.0f, 3.0f));
    glm::quat qClamped = MmdCcdIkSolver::clampEulerAngles(
        qOver, glm::vec3(0.0f, -0.5f, -1.0f), glm::vec3(1.0f, 0.5f, 1.0f));
    glm::vec3 eulerClamped = MmdCcdIkSolver::quatToZxyEuler(qClamped);
    QVERIFY(eulerClamped.x <= 1.001f && eulerClamped.x >= -0.001f);
    QVERIFY(eulerClamped.y <= 0.501f && eulerClamped.y >= -0.501f);
    QVERIFY(eulerClamped.z <= 1.001f && eulerClamped.z >= -1.001f);
}

QTEST_MAIN(TstPmxEvaluator)
#include "tst_pmx_evaluator.moc"
