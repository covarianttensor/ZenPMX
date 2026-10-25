#include "core/PmxDocument.h"
#include "core/PmxReader.h"
#include "core/PmxTypes.h"
#include "evaluator/PmxRuntimeScene.h"
#include "evaluator/MmdCcdIkSolver.h"
#include "evaluator/PmxPhysicsEngine.h"
#include "evaluator/VmdImporter.h"
#include "evaluator/VpdImporter.h"
#include "evaluator/PmxAnimationPlayer.h"

#include <QFile>
#include <QFileInfo>
#include <QSet>
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

    // Vertex Deformer Tests (Ticket 4.3):
    void testBdef1Skinning();
    void testBdef2Skinning();
    void testBdef4Skinning();
    void testSdefVolumePreservation();
    void testMorphPlusSkinningCombination();
    void testQdefFallback();
    void testZeroNormalFallback();
    void testSdefRestPoseIdentity();
    void testSdefRigidRotationInvariance();
    void testSdefDifferentialRotationWithBounds();

    // Bullet3 Physics & 6DoF Spring Tests (Ticket 4.4):
    void testBulletRigidBodyFreeFall();
    void testBulletKinematicBoneTracking();
    void testBulletSpringConstraintStability();
    void testBulletCollisionFiltering();
    void testPhysicsSceneReset();
    void testBulletPhysicsAndBoneMode();
    void testBulletCompoundEulerRotation();

    // VMD & VPD Motion / Pose Tests (Ticket 4.5):
    void testVmdBinaryImportAndKeyframes();
    void testVmdInterpolationEvaluation();
    void testVpdTextPoseImport();
    void testVmdIkSwitchingTimeline();
    void testAnimationPlaybackDeltaUpdate();
    void testVmdCubicBezierNonLinear();
    void testVmdRoundtrip();
    void testVpdRoundtrip();
    void testAnimationPlayerLooping();
    void testAnimationPlayerNanHandling();
    void testVmdMalformedBufferRejection();
    void testDiagnoseUserVmdMotion();
    void testDynamicRigidBodiesAlignToAnimatedBones();
    void testAnimationKneeFlexionAndIkLegPoses();
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

    // Knee link has angle limit in canonical Left-Handed PMX:
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

void TstPmxEvaluator::testBdef1Skinning() {
    PmxDocument doc;
    PmxBone bone0;
    bone0.nameJp = "Bone0";
    bone0.parentBoneIndex = -1;
    bone0.position = {0.0f, 0.0f, 0.0f};

    PmxVertex v0;
    v0.position = {2.0f, 3.0f, 4.0f}; // LH (2, 3, 4) -> RH (2, 3, -4)
    v0.normal = {0.0f, 1.0f, 0.0f};
    v0.weightType = WeightType::BDEF1;
    v0.boneIndices = {0, -1, -1, -1};
    v0.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};

    doc.bones = {bone0};
    doc.vertices = {v0};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Initial rest pose: deformed position matches rest position
    QVERIFY(glm::distance(scene.deformedPosition(0), glm::vec3(2.0f, 3.0f, -4.0f)) < 1e-5f);
    QVERIFY(glm::distance(scene.deformedNormal(0), glm::vec3(0.0f, 1.0f, 0.0f)) < 1e-5f);

    // Translate bone by (1, 2, 3) in RH
    scene.setBoneTransform(0, glm::vec3(1.0f, 2.0f, 3.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    glm::vec3 expectedPos = glm::vec3(2.0f + 1.0f, 3.0f + 2.0f, -4.0f + 3.0f);
    QVERIFY(glm::distance(scene.deformedPosition(0), expectedPos) < 1e-5f);
    // Normal does not change under pure translation
    QVERIFY(glm::distance(scene.deformedNormal(0), glm::vec3(0.0f, 1.0f, 0.0f)) < 1e-5f);
}

void TstPmxEvaluator::testBdef2Skinning() {
    PmxDocument doc;
    PmxBone boneA;
    boneA.nameJp = "BoneA";
    boneA.parentBoneIndex = -1;
    boneA.position = {0.0f, 0.0f, 0.0f};

    PmxBone boneB;
    boneB.nameJp = "BoneB";
    boneB.parentBoneIndex = -1;
    boneB.position = {0.0f, 0.0f, 0.0f};

    PmxVertex v0;
    v0.position = {0.0f, 0.0f, 0.0f};
    v0.normal = {0.0f, 1.0f, 0.0f};
    v0.weightType = WeightType::BDEF2;
    v0.boneIndices = {0, 1, -1, -1};
    v0.boneWeights = {0.5f, 0.5f, 0.0f, 0.0f};

    doc.bones = {boneA, boneB};
    doc.vertices = {v0};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Bone A translates (0, 2, 0), Bone B translates (0, 0, 0)
    scene.setBoneTransform(0, glm::vec3(0.0f, 2.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(1, glm::vec3(0.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    // 50/50 blend moves vertex to (0, 1, 0)
    QVERIFY(glm::distance(scene.deformedPosition(0), glm::vec3(0.0f, 1.0f, 0.0f)) < 1e-5f);
}

void TstPmxEvaluator::testBdef4Skinning() {
    PmxDocument doc;
    for (int i = 0; i < 4; ++i) {
        PmxBone b;
        b.nameJp = QString("Bone%1").arg(i);
        b.parentBoneIndex = -1;
        b.position = {0.0f, 0.0f, 0.0f};
        doc.bones.push_back(b);
    }

    PmxVertex v0;
    v0.position = {0.0f, 0.0f, 0.0f};
    v0.normal = {0.0f, 1.0f, 0.0f};
    v0.weightType = WeightType::BDEF4;
    v0.boneIndices = {0, 1, 2, 3};
    v0.boneWeights = {0.4f, 0.3f, 0.2f, 0.1f};
    doc.vertices = {v0};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    glm::vec3 t0{1.0f, 0.0f, 0.0f};
    glm::vec3 t1{0.0f, 2.0f, 0.0f};
    glm::vec3 t2{0.0f, 0.0f, 3.0f};
    glm::vec3 t3{4.0f, 4.0f, 4.0f};

    scene.setBoneTransform(0, t0, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(1, t1, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(2, t2, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(3, t3, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    glm::vec3 expected = 0.4f * t0 + 0.3f * t1 + 0.2f * t2 + 0.1f * t3;
    QVERIFY(glm::distance(scene.deformedPosition(0), expected) < 1e-5f);
}

void TstPmxEvaluator::testSdefVolumePreservation() {
    // Construct 2-bone joint: Bone 0 (parent/upper arm) at (0, 0, 0), Bone 1 (child/forearm) at (0, 1, 0)
    PmxDocument doc;
    PmxBone bone0;
    bone0.nameJp = "UpperArm";
    bone0.parentBoneIndex = -1;
    bone0.position = {0.0f, 0.0f, 0.0f};

    PmxBone bone1;
    bone1.nameJp = "Forearm";
    bone1.parentBoneIndex = 0;
    bone1.position = {0.0f, 1.0f, 0.0f};

    doc.bones = {bone0, bone1};

    // Vertex on outer joint surface: rest position (1, 1, 0), normal (1, 0, 0)
    // Rotation center C is at joint (0, 1, 0)
    // Distance from C is 1.0
    // Vertex 0: SDEF
    PmxVertex vSdef;
    vSdef.position = {1.0f, 1.0f, 0.0f};
    vSdef.normal = {1.0f, 0.0f, 0.0f};
    vSdef.weightType = WeightType::SDEF;
    vSdef.boneIndices = {0, 1, -1, -1};
    vSdef.boneWeights = {0.5f, 0.5f, 0.0f, 0.0f};
    vSdef.sdefC = {0.0f, 1.0f, 0.0f};
    vSdef.sdefR0 = {0.0f, 0.0f, 0.0f};
    vSdef.sdefR1 = {0.0f, 0.0f, 0.0f};

    // Vertex 1: BDEF2 with same weights for comparison
    PmxVertex vBdef2;
    vBdef2.position = {1.0f, 1.0f, 0.0f};
    vBdef2.normal = {1.0f, 0.0f, 0.0f};
    vBdef2.weightType = WeightType::BDEF2;
    vBdef2.boneIndices = {0, 1, -1, -1};
    vBdef2.boneWeights = {0.5f, 0.5f, 0.0f, 0.0f};

    doc.vertices = {vSdef, vBdef2};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Rotate forearm (bone 1) by 90 degrees around Z axis
    glm::quat rotZ90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    scene.setBoneTransform(1, glm::vec3(0.0f), rotZ90);
    scene.evaluate();

    glm::vec3 center = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 posSdef = scene.deformedPosition(0);
    glm::vec3 posBdef2 = scene.deformedPosition(1);

    float distSdef = glm::distance(posSdef, center);
    float distBdef2 = glm::distance(posBdef2, center);

    // Initial radial distance from joint center C is 1.0
    // SDEF preserves spherical volume: radial distance remains 1.0
    QVERIFY2(std::abs(distSdef - 1.0f) < 1e-4f,
             qPrintable(QString("SDEF distance should be 1.0, got %1").arg(distSdef)));

    // BDEF2 collapses: linear midpoint of (1, 0, 0) and (0, 1, 0) has distance sqrt(0.5) ~ 0.7071
    QVERIFY2(distBdef2 < 0.75f,
             qPrintable(QString("BDEF2 should exhibit volume collapse (< 0.75), got %1").arg(distBdef2)));
    QVERIFY(distSdef > distBdef2 + 0.25f);
}

void TstPmxEvaluator::testMorphPlusSkinningCombination() {
    PmxDocument doc;
    PmxBone bone0;
    bone0.nameJp = "Bone0";
    bone0.parentBoneIndex = -1;
    bone0.position = {0.0f, 0.0f, 0.0f};
    doc.bones = {bone0};

    // Vertex at (1, 0, 0)
    PmxVertex v0;
    v0.position = {1.0f, 0.0f, 0.0f};
    v0.normal = {1.0f, 0.0f, 0.0f};
    v0.weightType = WeightType::BDEF1;
    v0.boneIndices = {0, -1, -1, -1};
    v0.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
    doc.vertices = {v0};

    // Vertex morph offset + (0, 1, 0)
    PmxMorph morph;
    morph.nameJp = "MorphY";
    morph.morphType = 1; // Vertex morph
    VertexMorphOffset vOff;
    vOff.vertexIndex = 0;
    vOff.positionOffset = {0.0f, 1.0f, 0.0f}; // LH (0, 1, 0) -> RH (0, 1, 0)
    morph.vertexOffsets = {vOff};
    doc.morphs = {morph};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Apply morph weight = 1.0 and 90 deg rotation around Z
    scene.setMorphWeight(0, 1.0f);
    glm::quat rotZ90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    scene.setBoneTransform(0, glm::vec3(0.0f), rotZ90);
    scene.evaluate();

    // Eff position before rotation: (1, 0, 0) + (0, 1, 0) = (1, 1, 0)
    // Rotated 90 deg around Z: (-1, 1, 0)
    glm::vec3 expectedPos{-1.0f, 1.0f, 0.0f};
    QVERIFY(glm::distance(scene.deformedPosition(0), expectedPos) < 1e-4f);
}

void TstPmxEvaluator::testQdefFallback() {
    PmxDocument doc;
    for (int i = 0; i < 4; ++i) {
        PmxBone b;
        b.nameJp = QString("Bone%1").arg(i);
        b.parentBoneIndex = -1;
        b.position = {0.0f, 0.0f, 0.0f};
        doc.bones.push_back(b);
    }

    PmxVertex v0;
    v0.position = {0.0f, 0.0f, 0.0f};
    v0.normal = {0.0f, 1.0f, 0.0f};
    v0.weightType = WeightType::QDEF;
    v0.boneIndices = {0, 1, 2, 3};
    v0.boneWeights = {0.25f, 0.25f, 0.25f, 0.25f};
    doc.vertices = {v0};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    scene.setBoneTransform(0, glm::vec3(4.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(1, glm::vec3(0.0f, 4.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(2, glm::vec3(0.0f, 0.0f, 4.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(3, glm::vec3(4.0f, 4.0f, 4.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.evaluate();

    // 0.25 * ( (4,0,0) + (0,4,0) + (0,0,4) + (4,4,4) ) = 0.25 * (8, 8, 8) = (2, 2, 2)
    QVERIFY(glm::distance(scene.deformedPosition(0), glm::vec3(2.0f, 2.0f, 2.0f)) < 1e-5f);
}

void TstPmxEvaluator::testZeroNormalFallback() {
    PmxDocument doc;
    PmxBone b;
    b.nameJp = "Bone0";
    b.parentBoneIndex = -1;
    b.position = {0.0f, 0.0f, 0.0f};
    doc.bones = {b};

    PmxVertex v0;
    v0.position = {0.0f, 0.0f, 0.0f};
    v0.normal = {0.0f, 0.0f, 0.0f}; // Zero normal
    v0.weightType = WeightType::BDEF1;
    v0.boneIndices = {0, -1, -1, -1};
    v0.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
    doc.vertices = {v0};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Fallback normal should be (0, 1, 0)
    QCOMPARE(scene.deformedNormal(0), glm::vec3(0.0f, 1.0f, 0.0f));
}

void TstPmxEvaluator::testSdefRestPoseIdentity() {
    PmxDocument doc;
    PmxBone b0;
    b0.position = {0.0f, 0.0f, 0.0f};
    PmxBone b1;
    b1.position = {0.0f, 2.0f, 0.0f};
    doc.bones = {b0, b1};

    PmxVertex v;
    v.position = {1.5f, 2.5f, -3.5f};
    v.normal = {0.0f, 1.0f, 0.0f};
    v.weightType = WeightType::SDEF;
    v.boneIndices = {0, 1, -1, -1};
    v.boneWeights = {0.6f, 0.4f, 0.0f, 0.0f};
    v.sdefC = {0.0f, 1.0f, 0.0f};
    v.sdefR0 = {-0.4f, 0.0f, 0.0f};
    v.sdefR1 = {0.6f, 0.0f, 0.0f};
    doc.vertices = {v};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Rest pose matches rest position exactly
    glm::vec3 expectedRestPos = PmxRuntimeScene::toRightHanded(v.position);
    glm::vec3 expectedRestNormal = PmxRuntimeScene::toRightHanded(v.normal);
    QVERIFY(glm::distance(scene.deformedPosition(0), expectedRestPos) < 1e-5f);
    QVERIFY(glm::distance(scene.deformedNormal(0), expectedRestNormal) < 1e-5f);
}

void TstPmxEvaluator::testSdefRigidRotationInvariance() {
    PmxDocument doc;
    PmxBone b0;
    b0.nameJp = "Bone0";
    b0.parentBoneIndex = -1;
    b0.position = {0.0f, 0.0f, 0.0f};

    PmxBone b1;
    b1.nameJp = "Bone1";
    b1.parentBoneIndex = -1;
    b1.position = {0.0f, 0.0f, 0.0f};

    doc.bones = {b0, b1};

    PmxVertex v;
    v.position = {1.0f, 0.0f, 0.0f};
    v.normal = {0.0f, 1.0f, 0.0f};
    v.weightType = WeightType::SDEF;
    v.boneIndices = {0, 1, -1, -1};
    v.boneWeights = {0.5f, 0.5f, 0.0f, 0.0f};
    v.sdefC = {0.5f, 0.5f, 0.0f};
    v.sdefR0 = {0.2f, 0.0f, 0.0f};
    v.sdefR1 = {-0.2f, 0.0f, 0.0f};
    doc.vertices = {v};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    glm::quat rotZ90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    scene.setBoneTransform(0, glm::vec3(0.0f), rotZ90);
    scene.setBoneTransform(1, glm::vec3(0.0f), rotZ90);
    scene.evaluate();

    glm::vec3 expectedPos{0.0f, 1.0f, 0.0f};
    glm::vec3 actualPos = scene.deformedPosition(0);
    QVERIFY2(glm::distance(actualPos, expectedPos) < 1e-4f,
             qPrintable(QString("Rigid rotation invariance failed: got (%1, %2, %3), expected (0, 1, 0)")
                            .arg(actualPos.x).arg(actualPos.y).arg(actualPos.z)));

    glm::vec3 expectedNormal{-1.0f, 0.0f, 0.0f};
    glm::vec3 actualNormal = scene.deformedNormal(0);
    QVERIFY2(glm::distance(actualNormal, expectedNormal) < 1e-4f,
             qPrintable(QString("Rigid rotation normal failed: got (%1, %2, %3)")
                            .arg(actualNormal.x).arg(actualNormal.y).arg(actualNormal.z)));
}

void TstPmxEvaluator::testSdefDifferentialRotationWithBounds() {
    PmxDocument doc;
    PmxBone b0;
    b0.nameJp = "Bone0";
    b0.parentBoneIndex = -1;
    b0.position = {0.0f, 0.0f, 0.0f};

    PmxBone b1;
    b1.nameJp = "Bone1";
    b1.parentBoneIndex = -1;
    b1.position = {0.0f, 0.0f, 0.0f};

    doc.bones = {b0, b1};

    PmxVertex v;
    v.position = {1.0f, 0.0f, 0.0f};
    v.normal = {0.0f, 1.0f, 0.0f};
    v.weightType = WeightType::SDEF;
    v.boneIndices = {0, 1, -1, -1};
    v.boneWeights = {0.5f, 0.5f, 0.0f, 0.0f};
    v.sdefC = {0.0f, 0.0f, 0.0f};
    v.sdefR0 = {0.2f, 0.1f, 0.0f};
    v.sdefR1 = {-0.2f, -0.1f, 0.0f};
    doc.vertices = {v};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    glm::quat rotZ90 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    scene.setBoneTransform(0, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
    scene.setBoneTransform(1, glm::vec3(0.0f), rotZ90);
    scene.evaluate();

    float s45 = std::sin(glm::radians(45.0f));
    glm::vec3 expectedPos{s45 + 0.15f, s45 - 0.05f, 0.0f};
    glm::vec3 actualPos = scene.deformedPosition(0);

    QVERIFY2(glm::distance(actualPos, expectedPos) < 1e-4f,
             qPrintable(QString("Differential rotation with bounds failed: got (%1, %2, %3), expected (%4, %5, %6)")
                            .arg(actualPos.x).arg(actualPos.y).arg(actualPos.z)
                            .arg(expectedPos.x).arg(expectedPos.y).arg(expectedPos.z)));
}

void TstPmxEvaluator::testBulletRigidBodyFreeFall() {
    PmxDocument doc;
    PmxRigidBody rb;
    rb.nameJp = "FreeFallBody";
    rb.boneIndex = -1;
    rb.shape = 0; // Sphere
    rb.size = {1.0f, 0.0f, 0.0f};
    rb.position = {0.0f, 50.0f, 0.0f}; // LH (0, 50, 0) -> RH (0, 50, 0)
    rb.rotation = {0.0f, 0.0f, 0.0f};
    rb.mass = 1.0f;
    rb.linearDamping = 0.0f;
    rb.angularDamping = 0.0f;
    rb.physicsMode = 1; // Dynamic Physics
    doc.rigidBodies = {rb};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    QVERIFY(scene.physicsEngine() != nullptr);
    QCOMPARE(scene.physicsEngine()->rigidBodyCount(), 1);

    glm::vec3 pos0 = scene.physicsEngine()->rigidBodyPosition(0);
    QVERIFY(std::abs(pos0.y - 50.0f) < 1e-4f);

    // Step simulation across multiple steps
    for (int step = 0; step < 10; ++step) {
        scene.evaluate(1.0f / 60.0f);
    }

    glm::vec3 posT = scene.physicsEngine()->rigidBodyPosition(0);
    glm::vec3 velT = scene.physicsEngine()->rigidBodyLinearVelocity(0);

    // Assert falling downward along -Y under gravity
    QVERIFY2(posT.y < pos0.y, qPrintable(QString("Rigid body did not fall: posT.y=%1 >= pos0.y=%2").arg(posT.y).arg(pos0.y)));
    QVERIFY2(velT.y < -1.0f, qPrintable(QString("Rigid body linear velocity y=%1 is not negative").arg(velT.y)));
    QVERIFY(!std::isnan(posT.x) && !std::isnan(posT.y) && !std::isnan(posT.z));
    QVERIFY(!std::isnan(velT.x) && !std::isnan(velT.y) && !std::isnan(velT.z));
}

void TstPmxEvaluator::testBulletKinematicBoneTracking() {
    PmxDocument doc;

    PmxBone bone;
    bone.nameJp = "KinematicBone";
    bone.parentBoneIndex = -1;
    bone.position = {0.0f, 10.0f, 0.0f}; // LH (0, 10, 0) -> RH (0, 10, 0)

    PmxRigidBody rb;
    rb.nameJp = "KinematicBody";
    rb.boneIndex = 0;
    rb.shape = 1; // Box
    rb.size = {1.0f, 1.0f, 1.0f};
    rb.position = {0.0f, 12.0f, 0.0f}; // LH (0, 12, 0) -> RH (0, 12, 0), offset is +2 in Y
    rb.rotation = {0.0f, 0.0f, 0.0f};
    rb.physicsMode = 0; // FollowBone / Kinematic

    doc.bones = {bone};
    doc.rigidBodies = {rb};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Initial position at rest pose
    glm::vec3 initPos = scene.physicsEngine()->rigidBodyPosition(0);
    QVERIFY(std::abs(initPos.y - 12.0f) < 1e-4f);

    // Translate and rotate parent bone
    glm::vec3 userTrans(3.0f, 4.0f, 5.0f);
    glm::quat userRot = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // 90 deg around Y
    scene.setBoneTransform(0, userTrans, userRot);
    scene.evaluate(1.0f / 60.0f);

    // Compute expected world matrix for the rigid body:
    // W_body = W_bone * offset
    const auto* meta = scene.physicsEngine()->runtimeRigidBody(0);
    QVERIFY(meta != nullptr);
    glm::mat4 expectedWorld = scene.worldMatrix(0) * meta->boneToBodyOffset;
    glm::mat4 actualWorld = scene.physicsEngine()->rigidBodyTransform(0);

    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            QVERIFY2(std::abs(actualWorld[c][r] - expectedWorld[c][r]) < 1e-4f,
                     qPrintable(QString("Matrix mismatch at [%1][%2]: actual=%3, expected=%4")
                                    .arg(c).arg(r).arg(actualWorld[c][r]).arg(expectedWorld[c][r])));
        }
    }
}

void TstPmxEvaluator::testBulletSpringConstraintStability() {
    PmxDocument doc;

    // Body A: Anchored kinematic body at (0, 10, 0)
    PmxRigidBody rbA;
    rbA.nameJp = "Anchor";
    rbA.boneIndex = -1;
    rbA.shape = 1; // Box
    rbA.size = {1.0f, 1.0f, 1.0f};
    rbA.position = {0.0f, 10.0f, 0.0f};
    rbA.physicsMode = 0; // Kinematic

    // Body B: Dynamic body at (0, 8, 0)
    PmxRigidBody rbB;
    rbB.nameJp = "Bob";
    rbB.boneIndex = -1;
    rbB.shape = 0; // Sphere
    rbB.size = {1.0f, 1.0f, 1.0f};
    rbB.position = {0.0f, 8.0f, 0.0f};
    rbB.mass = 2.0f;
    rbB.linearDamping = 0.1f;
    rbB.angularDamping = 0.1f;
    rbB.physicsMode = 1; // Dynamic

    // Joint linking A (0) and B (1) at (0, 9, 0)
    PmxJoint joint;
    joint.nameJp = "SpringJoint";
    joint.rigidBodyIndexA = 0;
    joint.rigidBodyIndexB = 1;
    joint.position = {0.0f, 9.0f, 0.0f};
    joint.linearLowerLimit = {-1.0f, -5.0f, -1.0f};
    joint.linearUpperLimit = {1.0f, 5.0f, 1.0f};
    joint.angularLowerLimit = {-0.5f, -0.5f, -0.5f};
    joint.angularUpperLimit = {0.5f, 0.5f, 0.5f};
    joint.springLinearStiffness = {50.0f, 200.0f, 50.0f};
    joint.springAngularStiffness = {10.0f, 10.0f, 10.0f};

    doc.rigidBodies = {rbA, rbB};
    doc.joints = {joint};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    QCOMPARE(scene.physicsEngine()->constraintCount(), 1);

    // Simulate 60 steps
    for (int step = 0; step < 60; ++step) {
        scene.evaluate(1.0f / 60.0f);

        glm::vec3 pos = scene.physicsEngine()->rigidBodyPosition(1);
        glm::vec3 vel = scene.physicsEngine()->rigidBodyLinearVelocity(1);

        QVERIFY(!std::isnan(pos.x) && !std::isnan(pos.y) && !std::isnan(pos.z));
        QVERIFY(!std::isinf(pos.x) && !std::isinf(pos.y) && !std::isinf(pos.z));
        QVERIFY(!std::isnan(vel.x) && !std::isnan(vel.y) && !std::isnan(vel.z));
        QVERIFY(!std::isinf(vel.x) && !std::isinf(vel.y) && !std::isinf(vel.z));

        // Stable bounds: Dynamic body must remain within reasonable bounds around anchor
        QVERIFY2(pos.y > 0.0f && pos.y < 15.0f,
                 qPrintable(QString("Body Y pos out of bounds (exploded): %1").arg(pos.y)));
    }
}

void TstPmxEvaluator::testBulletCollisionFiltering() {
    PmxDocument doc;

    // Body 0: Kinematic platform at (0, 0, 0), group 0
    PmxRigidBody platform;
    platform.nameJp = "Platform";
    platform.boneIndex = -1;
    platform.shape = 1; // Box
    platform.size = {10.0f, 1.0f, 10.0f};
    platform.position = {0.0f, 0.0f, 0.0f};
    platform.groupIndex = 0;
    // Mask to ignore collisions with group 1 (bit 1 set)
    platform.nonCollisionMask = static_cast<uint16_t>(1 << 1);
    platform.physicsMode = 0; // Kinematic

    // Body 1: Dynamic falling ball at (0, 3, 0), group 1
    PmxRigidBody ball;
    ball.nameJp = "FallingBall";
    ball.boneIndex = -1;
    ball.shape = 0; // Sphere
    ball.size = {1.0f, 1.0f, 1.0f};
    ball.position = {0.0f, 3.0f, 0.0f};
    ball.groupIndex = 1;
    // Mask to ignore collisions with group 0 (bit 0 set)
    ball.nonCollisionMask = static_cast<uint16_t>(1 << 0);
    ball.mass = 1.0f;
    ball.physicsMode = 1; // Dynamic

    doc.rigidBodies = {platform, ball};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Step simulation 60 frames. With gravity -98, ball falls fast.
    // If collision filtering works, ball penetrates through platform (y=0) and reaches negative Y.
    for (int step = 0; step < 60; ++step) {
        scene.evaluate(1.0f / 60.0f);
    }

    glm::vec3 ballPos = scene.physicsEngine()->rigidBodyPosition(1);
    QVERIFY2(ballPos.y < -1.0f,
             qPrintable(QString("Ball did not pass through platform (collided instead): ballPos.y = %1").arg(ballPos.y)));
}

void TstPmxEvaluator::testPhysicsSceneReset() {
    PmxDocument doc;

    PmxBone bone;
    bone.nameJp = "DynamicBone";
    bone.parentBoneIndex = -1;
    bone.position = {0.0f, 25.0f, 0.0f}; // LH (0, 25, 0) -> RH (0, 25, 0)

    PmxRigidBody rb;
    rb.nameJp = "DynamicBody";
    rb.boneIndex = 0;
    rb.shape = 0; // Sphere
    rb.size = {1.0f, 1.0f, 1.0f};
    rb.position = {0.0f, 25.0f, 0.0f};
    rb.mass = 1.0f;
    rb.physicsMode = 1; // Dynamic

    doc.bones = {bone};
    doc.rigidBodies = {rb};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    glm::vec3 restPos = scene.worldPosition(0);
    QVERIFY(std::abs(restPos.y - 25.0f) < 1e-4f);

    // Step simulation so the body falls
    for (int step = 0; step < 30; ++step) {
        scene.evaluate(1.0f / 60.0f);
    }

    glm::vec3 fallenPos = scene.physicsEngine()->rigidBodyPosition(0);
    glm::vec3 fallenVel = scene.physicsEngine()->rigidBodyLinearVelocity(0);
    QVERIFY(fallenPos.y < 20.0f);
    QVERIFY(fallenVel.y < -1.0f);

    // Bone also followed dynamic body
    glm::vec3 boneFallenPos = scene.worldPosition(0);
    QVERIFY(boneFallenPos.y < 20.0f);

    // Reset physics
    scene.resetPhysics();

    glm::vec3 resetPos = scene.physicsEngine()->rigidBodyPosition(0);
    glm::vec3 resetVel = scene.physicsEngine()->rigidBodyLinearVelocity(0);
    glm::vec3 resetAngVel = scene.physicsEngine()->rigidBodyAngularVelocity(0);
    glm::vec3 resetBonePos = scene.worldPosition(0);

    // Dynamic body restored to original rest coordinates
    QVERIFY2(glm::distance(resetPos, glm::vec3(0.0f, 25.0f, 0.0f)) < 1e-4f,
             qPrintable(QString("Reset position (%1, %2, %3) != rest position").arg(resetPos.x).arg(resetPos.y).arg(resetPos.z)));
    // Velocities zeroed
    QVERIFY(glm::distance(resetVel, glm::vec3(0.0f)) < 1e-6f);
    QVERIFY(glm::distance(resetAngVel, glm::vec3(0.0f)) < 1e-6f);
    // Dynamic bone restored to rest position
    QVERIFY2(glm::distance(resetBonePos, glm::vec3(0.0f, 25.0f, 0.0f)) < 1e-4f,
             qPrintable(QString("Reset bone position (%1, %2, %3) != rest position").arg(resetBonePos.x).arg(resetBonePos.y).arg(resetBonePos.z)));
}

void TstPmxEvaluator::testBulletPhysicsAndBoneMode() {
    PmxDocument doc;

    // Parent bone at (0, 0, 0)
    PmxBone parent;
    parent.nameJp = "Parent";
    parent.parentBoneIndex = -1;
    parent.position = {0.0f, 0.0f, 0.0f};

    // Child bone at (0, 10, 0)
    PmxBone child;
    child.nameJp = "Child";
    child.parentBoneIndex = 0;
    child.position = {0.0f, 10.0f, 0.0f};

    // Dynamic body linked to child bone (index 1), with physicsMode = 2 (PhysicsAndBone)
    PmxRigidBody rb;
    rb.nameJp = "PhysicsAndBoneBody";
    rb.boneIndex = 1;
    rb.shape = 0; // Sphere
    rb.size = {1.0f, 1.0f, 1.0f};
    rb.position = {0.0f, 10.0f, 0.0f};
    rb.mass = 1.0f;
    rb.physicsMode = 2; // PhysicsAndBone

    doc.bones = {parent, child};
    doc.rigidBodies = {rb};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    // Initial check: child bone position is (0, 10, 0)
    QVERIFY(glm::distance(scene.worldPosition(1), glm::vec3(0.0f, 10.0f, 0.0f)) < 1e-4f);

    // Animate parent bone: translate by (5, 3, 2)
    glm::vec3 parentTrans(5.0f, 3.0f, 2.0f);
    scene.setBoneTransform(0, parentTrans, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));

    // Apply angular velocity to the rigid body so it rotates under simulation
    auto* btBody = scene.physicsEngine()->rigidBody(0);
    QVERIFY(btBody != nullptr);
    btBody->setAngularVelocity(btVector3(2.0f, 3.0f, 4.0f));

    // Simulate for 10 steps
    for (int step = 0; step < 10; ++step) {
        scene.evaluate(1.0f / 60.0f);
    }

    // 1. Verify rigid body has actually rotated
    glm::vec3 angVel = scene.physicsEngine()->rigidBodyAngularVelocity(0);
    QVERIFY(glm::length(angVel) > 0.1f);
    glm::mat4 bodyTransform = scene.physicsEngine()->rigidBodyTransform(0);

    // 2. Child bone position MUST remain locked to parent translation:
    // Expected position = restPosition(0, 10, 0) + parentTrans(5, 3, 2) = (5, 13, 2)
    glm::vec3 expectedChildPos = glm::vec3(0.0f, 10.0f, 0.0f) + parentTrans;
    glm::vec3 actualChildPos = scene.worldPosition(1);
    QVERIFY2(glm::distance(actualChildPos, expectedChildPos) < 1e-4f,
             qPrintable(QString("PhysicsAndBone translation drift: got (%1, %2, %3), expected (%4, %5, %6)")
                            .arg(actualChildPos.x).arg(actualChildPos.y).arg(actualChildPos.z)
                            .arg(expectedChildPos.x).arg(expectedChildPos.y).arg(expectedChildPos.z)));

    // 3. Child bone rotation MUST match the simulated rigid body's rotation:
    // W_bone_expected_rot = bodyWorld_rot * bodyToBoneOffset_rot
    const auto* meta = scene.physicsEngine()->runtimeRigidBody(0);
    QVERIFY(meta != nullptr);
    glm::mat4 expectedFull = bodyTransform * meta->bodyToBoneOffset;
    const glm::mat4& actualBoneWorld = scene.worldMatrix(1);

    // Check upper 3x3 rotation basis matches physics orientation
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            QVERIFY2(std::abs(actualBoneWorld[c][r] - expectedFull[c][r]) < 1e-3f,
                     qPrintable(QString("PhysicsAndBone rotation mismatch at [%1][%2]: actual=%3, expected=%4")
                                    .arg(c).arg(r).arg(actualBoneWorld[c][r]).arg(expectedFull[c][r])));
        }
    }
}

void TstPmxEvaluator::testBulletCompoundEulerRotation() {
    // Non-zero 3-axis Euler rotation in LH radians
    glm::vec3 rotLH(0.35f, -0.45f, 0.65f);
    glm::vec3 posLH(1.5f, 2.5f, 3.5f);

    // Expected RH values
    glm::vec3 posRH(posLH.x, posLH.y, -posLH.z);
    glm::vec3 rotRH(-rotLH.x, -rotLH.y, rotLH.z);

    // Canonical extrinsic ZXY / intrinsic YXZ rotation: R = Ry * Rx * Rz
    glm::quat qx = glm::angleAxis(rotRH.x, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qy = glm::angleAxis(rotRH.y, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qz = glm::angleAxis(rotRH.z, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::quat qExpected = glm::normalize(qy * qx * qz);
    glm::mat3 rotExpected = glm::mat3_cast(qExpected);

    // Create btTransform via PmxPhysicsEngine::createBtTransform
    btTransform t = PmxPhysicsEngine::createBtTransform(posLH, rotLH);

    // Check origin
    const btVector3& origin = t.getOrigin();
    QVERIFY(std::abs(static_cast<float>(origin.x()) - posRH.x) < 1e-4f);
    QVERIFY(std::abs(static_cast<float>(origin.y()) - posRH.y) < 1e-4f);
    QVERIFY(std::abs(static_cast<float>(origin.z()) - posRH.z) < 1e-4f);

    // Check 3x3 rotation basis matches canonical ZXY matrix within 10^-4 tolerance
    const btMatrix3x3& basis = t.getBasis();
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            float actualVal = static_cast<float>(basis[r][c]);
            float expectedVal = rotExpected[c][r]; // GLM is [col][row]
            QVERIFY2(std::abs(actualVal - expectedVal) < 1e-4f,
                     qPrintable(QString("Compound Euler basis mismatch at row %1 col %2: actual=%3, expected=%4")
                                    .arg(r).arg(c).arg(actualVal).arg(expectedVal)));
        }
    }

    // Also verify via full document and runtime scene loading
    PmxDocument doc;
    PmxRigidBody rb;
    rb.nameJp = "EulerBody";
    rb.boneIndex = -1;
    rb.shape = 1; // Box
    rb.size = {2.0f, 2.0f, 2.0f};
    rb.position = posLH;
    rb.rotation = rotLH;
    rb.physicsMode = 0; // Kinematic
    doc.rigidBodies = {rb};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    glm::mat4 sceneWorld = scene.physicsEngine()->rigidBodyTransform(0);
    for (int c = 0; c < 3; ++c) {
        for (int r = 0; r < 3; ++r) {
            float actualVal = sceneWorld[c][r];
            float expectedVal = rotExpected[c][r];
            QVERIFY2(std::abs(actualVal - expectedVal) < 1e-4f,
                     qPrintable(QString("Scene body transform basis mismatch at [%1][%2]: actual=%3, expected=%4")
                                    .arg(c).arg(r).arg(actualVal).arg(expectedVal)));
        }
    }
}

void TstPmxEvaluator::testVmdBinaryImportAndKeyframes() {
    // Construct a raw binary VMD 2.0 stream manually to test exact parser layout
    QByteArray rawData;

    // Header: 30 bytes "Vocaloid Motion Data 0002\0..."
    QByteArray header = "Vocaloid Motion Data 0002";
    header.append(QByteArray(30 - header.size(), '\0'));
    rawData.append(header);

    // Model name: 20 bytes "TestModel\0..."
    QByteArray modelName = "TestModel";
    modelName.append(QByteArray(20 - modelName.size(), '\0'));
    rawData.append(modelName);

    // Bone keyframe count: 3
    quint32 boneCount = qToLittleEndian(static_cast<quint32>(3));
    rawData.append(reinterpret_cast<const char*>(&boneCount), sizeof(quint32));

    auto appendBoneKf = [&](const QString& name, quint32 frame, glm::vec3 posLH, glm::vec4 rotLH) {
        QByteArray nameBytes = name.toUtf8();
        nameBytes.append(QByteArray(15 - nameBytes.size(), '\0'));
        rawData.append(nameBytes);

        quint32 leFrame = qToLittleEndian(frame);
        rawData.append(reinterpret_cast<const char*>(&leFrame), sizeof(quint32));

        quint32 px = qToLittleEndian(*reinterpret_cast<quint32*>(&posLH.x));
        quint32 py = qToLittleEndian(*reinterpret_cast<quint32*>(&posLH.y));
        quint32 pz = qToLittleEndian(*reinterpret_cast<quint32*>(&posLH.z));
        rawData.append(reinterpret_cast<const char*>(&px), sizeof(quint32));
        rawData.append(reinterpret_cast<const char*>(&py), sizeof(quint32));
        rawData.append(reinterpret_cast<const char*>(&pz), sizeof(quint32));

        quint32 rx = qToLittleEndian(*reinterpret_cast<quint32*>(&rotLH.x));
        quint32 ry = qToLittleEndian(*reinterpret_cast<quint32*>(&rotLH.y));
        quint32 rz = qToLittleEndian(*reinterpret_cast<quint32*>(&rotLH.z));
        quint32 rw = qToLittleEndian(*reinterpret_cast<quint32*>(&rotLH.w));
        rawData.append(reinterpret_cast<const char*>(&rx), sizeof(quint32));
        rawData.append(reinterpret_cast<const char*>(&ry), sizeof(quint32));
        rawData.append(reinterpret_cast<const char*>(&rz), sizeof(quint32));
        rawData.append(reinterpret_cast<const char*>(&rw), sizeof(quint32));

        // 64-byte interpolation curves
        uint8_t interp[64]{};
        for (int i = 0; i < 64; ++i) {
            interp[i] = (i % 2 == 0) ? 20 : 107;
        }
        rawData.append(reinterpret_cast<const char*>(interp), 64);
    };

    appendBoneKf("BoneA", 0, {1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    appendBoneKf("BoneA", 20, {4.0f, 5.0f, 6.0f}, {0.0f, 0.0f, 0.0f, 1.0f});
    appendBoneKf("BoneB", 10, {0.0f, 1.0f, 0.0f}, {0.0f, 0.707107f, 0.0f, 0.707107f});

    // Morph keyframe count: 2
    quint32 morphCount = qToLittleEndian(static_cast<quint32>(2));
    rawData.append(reinterpret_cast<const char*>(&morphCount), sizeof(quint32));

    auto appendMorphKf = [&](const QString& name, quint32 frame, float weight) {
        QByteArray nameBytes = name.toUtf8();
        nameBytes.append(QByteArray(15 - nameBytes.size(), '\0'));
        rawData.append(nameBytes);

        quint32 leFrame = qToLittleEndian(frame);
        rawData.append(reinterpret_cast<const char*>(&leFrame), sizeof(quint32));

        quint32 rawW = qToLittleEndian(*reinterpret_cast<quint32*>(&weight));
        rawData.append(reinterpret_cast<const char*>(&rawW), sizeof(quint32));
    };

    appendMorphKf("MorphA", 0, 0.25f);
    appendMorphKf("MorphB", 35, 0.85f);

    // Ingest via VmdImporter
    QString err;
    auto optMotion = VmdImporter::loadFromByteArray(rawData, &err);
    QVERIFY2(optMotion.has_value(), qPrintable(err));

    const auto& motion = *optMotion;
    QVERIFY(motion.version.startsWith("Vocaloid Motion Data"));
    QCOMPARE(motion.modelName, QStringLiteral("TestModel"));
    QCOMPARE(motion.boneKeyframes.size(), size_t(3));
    QCOMPARE(motion.morphKeyframes.size(), size_t(2));
    QCOMPARE(motion.maxFrame, quint32(35));

    // Verify bone keyframe coordinate conversion (LH to RH)
    // BoneA frame 0: LH (1, 2, 3) -> RH (1, 2, -3)
    QCOMPARE(motion.boneKeyframes[0].boneName, QStringLiteral("BoneA"));
    QCOMPARE(motion.boneKeyframes[0].frame, quint32(0));
    QVERIFY(std::abs(motion.boneKeyframes[0].translation.x - 1.0f) < 1e-4f);
    QVERIFY(std::abs(motion.boneKeyframes[0].translation.y - 2.0f) < 1e-4f);
    QVERIFY(std::abs(motion.boneKeyframes[0].translation.z - (-3.0f)) < 1e-4f);

    // BoneA frame 20: LH (4, 5, 6) -> RH (4, 5, -6)
    QCOMPARE(motion.boneKeyframes[1].boneName, QStringLiteral("BoneA"));
    QCOMPARE(motion.boneKeyframes[1].frame, quint32(20));
    QVERIFY(std::abs(motion.boneKeyframes[1].translation.x - 4.0f) < 1e-4f);
    QVERIFY(std::abs(motion.boneKeyframes[1].translation.y - 5.0f) < 1e-4f);
    QVERIFY(std::abs(motion.boneKeyframes[1].translation.z - (-6.0f)) < 1e-4f);

    // BoneB frame 10: LH rot (0, 0.707107, 0, 0.707107) -> RH rot (w, -x, -y, z) = (0.707107, 0, -0.707107, 0)
    QCOMPARE(motion.boneKeyframes[2].boneName, QStringLiteral("BoneB"));
    QCOMPARE(motion.boneKeyframes[2].frame, quint32(10));
    QVERIFY(std::abs(motion.boneKeyframes[2].rotation.w - 0.707107f) < 1e-4f);
    QVERIFY(std::abs(motion.boneKeyframes[2].rotation.y - (-0.707107f)) < 1e-4f);

    // Verify morphs
    QCOMPARE(motion.morphKeyframes[0].morphName, QStringLiteral("MorphA"));
    QCOMPARE(motion.morphKeyframes[0].frame, quint32(0));
    QVERIFY(std::abs(motion.morphKeyframes[0].weight - 0.25f) < 1e-4f);

    QCOMPARE(motion.morphKeyframes[1].morphName, QStringLiteral("MorphB"));
    QCOMPARE(motion.morphKeyframes[1].frame, quint32(35));
    QVERIFY(std::abs(motion.morphKeyframes[1].weight - 0.85f) < 1e-4f);
}

void TstPmxEvaluator::testVmdInterpolationEvaluation() {
    // Keyframe 0: t = (0, 0, 0), q = identity
    // Keyframe 30: t = (0, 10, 0), q = rotY(90 deg)
    VmdBoneKeyframe kf0;
    kf0.boneName = "TestBone";
    kf0.frame = 0;
    kf0.translation = glm::vec3(0.0f, 0.0f, 0.0f);
    kf0.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    VmdBoneKeyframe kf1;
    kf1.boneName = "TestBone";
    kf1.frame = 30;
    kf1.translation = glm::vec3(0.0f, 10.0f, 0.0f);
    kf1.rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    VmdMotion motion;
    motion.boneKeyframes = {kf0, kf1};
    motion.maxFrame = 30;

    PmxAnimationPlayer player;
    player.loadMotion(motion);
    player.setFrame(15.0f);

    glm::vec3 t = player.evaluateBoneTranslation("TestBone", 15.0f);
    glm::quat q = player.evaluateBoneRotation("TestBone", 15.0f);

    // Assert translation Y in [4.9, 5.1]
    QVERIFY2(t.y >= 4.9f && t.y <= 5.1f,
             qPrintable(QString("Translation Y was %1, expected in [4.9, 5.1]").arg(t.y)));
    QVERIFY(std::abs(t.x) < 1e-4f);
    QVERIFY(std::abs(t.z) < 1e-4f);

    // Assert rotation angle ~ 45 degrees
    float angleRad = 2.0f * std::acos(std::clamp(q.w, -1.0f, 1.0f));
    float angleDeg = glm::degrees(angleRad);
    QVERIFY2(std::abs(angleDeg - 45.0f) < 0.5f,
             qPrintable(QString("Rotation angle was %1 deg, expected ~45 deg").arg(angleDeg)));

    // Also apply to scene and verify runtime bone transforms
    PmxDocument doc;
    PmxBone bone;
    bone.nameJp = "TestBone";
    bone.position = {0.0f, 0.0f, 0.0f};
    doc.bones = {bone};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);
    player.applyToScene(scene);

    QVERIFY(scene.bone(0).userTranslation.y >= 4.9f && scene.bone(0).userTranslation.y <= 5.1f);
    float sceneAngleDeg = glm::degrees(2.0f * std::acos(std::clamp(scene.bone(0).userRotation.w, -1.0f, 1.0f)));
    QVERIFY(std::abs(sceneAngleDeg - 45.0f) < 0.5f);
}

void TstPmxEvaluator::testVpdTextPoseImport() {
    QByteArray vpdData =
        "Vocaloid Pose Data file\r\n\r\n"
        "YYB_Miku.osm; // 親ファイル名\r\n"
        "2; // 総ポーズボーン数\r\n\r\n"
        "Bone0{Center\r\n"
        "  1.000000,2.000000,3.000000;\r\n"
        "  0.000000,0.000000,0.000000,1.000000;\r\n"
        "}\r\n\r\n"
        "Bone1{Arm\r\n"
        "  0.000000,5.000000,0.000000;\r\n"
        "  0.000000,0.707107,0.000000,0.707107;\r\n"
        "}\r\n\r\n"
        "Morph0{Smile\r\n"
        "  0.850000;\r\n"
        "}\r\n";

    QString err;
    auto optPose = VpdImporter::loadFromByteArray(vpdData, &err);
    QVERIFY2(optPose.has_value(), qPrintable(err));

    const auto& pose = *optPose;
    QCOMPARE(pose.modelName, QStringLiteral("YYB_Miku.osm"));
    QCOMPARE(pose.bones.size(), size_t(2));
    QCOMPARE(pose.morphs.size(), size_t(1));

    // Create scene and apply pose
    PmxDocument doc;
    PmxBone b0;
    b0.nameJp = "Center";
    PmxBone b1;
    b1.nameJp = "Arm";
    doc.bones = {b0, b1};

    PmxMorph m0;
    m0.nameJp = "Smile";
    m0.morphType = 1;
    doc.morphs = {m0};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    VpdImporter::applyToScene(pose, scene);

    // Verify Center bone: LH (1, 2, 3) -> RH (1, 2, -3)
    QVERIFY(std::abs(scene.bone(0).userTranslation.x - 1.0f) < 1e-4f);
    QVERIFY(std::abs(scene.bone(0).userTranslation.y - 2.0f) < 1e-4f);
    QVERIFY(std::abs(scene.bone(0).userTranslation.z - (-3.0f)) < 1e-4f);
    QVERIFY(std::abs(scene.bone(0).userRotation.w - 1.0f) < 1e-4f);

    // Verify Arm bone: translation (0, 5, 0), rot (0.707107, 0, -0.707107, 0)
    QVERIFY(std::abs(scene.bone(1).userTranslation.y - 5.0f) < 1e-4f);
    QVERIFY(std::abs(scene.bone(1).userRotation.w - 0.707107f) < 1e-4f);
    QVERIFY(std::abs(scene.bone(1).userRotation.y - (-0.707107f)) < 1e-4f);

    // Verify morph weight
    QVERIFY(std::abs(scene.morphWeight(0) - 0.85f) < 1e-4f);
}

void TstPmxEvaluator::testVmdIkSwitchingTimeline() {
    PmxDocument doc;
    PmxBone boneIk;
    boneIk.nameJp = "LegIK";
    boneIk.boneFlags = 0x0020; // Has IK
    boneIk.ikTargetBoneIndex = 1;

    PmxBone boneTarget;
    boneTarget.nameJp = "LegTarget";

    doc.bones = {boneIk, boneTarget};

    PmxRuntimeScene scene;
    scene.loadDocument(doc);
    QVERIFY(scene.isIkEnabled(0));

    // Construct VMD with IK enable switch keyframes at frame 0 (enabled) and frame 10 (disabled)
    VmdMotion motion;
    VmdIkKeyframe ikKf0;
    ikKf0.frame = 0;
    ikKf0.ikToggles.push_back({"LegIK", true});

    VmdIkKeyframe ikKf10;
    ikKf10.frame = 10;
    ikKf10.ikToggles.push_back({"LegIK", false});

    motion.ikKeyframes = {ikKf0, ikKf10};
    motion.maxFrame = 20;

    PmxAnimationPlayer player;
    player.loadMotion(motion);

    // Evaluate player at frame 5 (IK enabled)
    player.setFrame(5.0f);
    player.applyToScene(scene);
    QVERIFY(scene.isIkEnabled(0));

    // Evaluate player at frame 15 (IK disabled)
    player.setFrame(15.0f);
    player.applyToScene(scene);
    QVERIFY(!scene.isIkEnabled(0));

    // Evaluate back at frame 2
    player.setFrame(2.0f);
    player.applyToScene(scene);
    QVERIFY(scene.isIkEnabled(0));
}

void TstPmxEvaluator::testAnimationPlaybackDeltaUpdate() {
    VmdMotion motion;
    motion.maxFrame = 60;

    PmxAnimationPlayer player;
    player.loadMotion(motion);
    player.setFrame(0.0f);
    QCOMPARE(player.currentFrame(), 0.0f);

    player.play();
    QVERIFY(player.isPlaying());

    // Advance by 1/30s -> exactly 1.0 frame
    player.update(1.0f / 30.0f);
    QVERIFY2(std::abs(player.currentFrame() - 1.0f) < 1e-4f,
             qPrintable(QString("Current frame was %1, expected 1.0").arg(player.currentFrame())));

    // Advance by another 0.5s -> 15.0 frames
    player.update(0.5f);
    QVERIFY2(std::abs(player.currentFrame() - 16.0f) < 1e-4f,
             qPrintable(QString("Current frame was %1, expected 16.0").arg(player.currentFrame())));

    // Pause: frame should not advance on update
    player.pause();
    QVERIFY(!player.isPlaying());
    player.update(0.5f);
    QVERIFY2(std::abs(player.currentFrame() - 16.0f) < 1e-4f,
             qPrintable(QString("Frame advanced while paused: %1").arg(player.currentFrame())));

    // Stop: frame should reset to 0 and stop playing
    player.stop();
    QVERIFY(!player.isPlaying());
    QCOMPARE(player.currentFrame(), 0.0f);
}

void TstPmxEvaluator::testVmdCubicBezierNonLinear() {
    // Default linear: {20, 20, 107, 107}
    uint8_t linearInterp[4] = {20, 20, 107, 107};
    QCOMPARE(PmxAnimationPlayer::evaluateBezier(linearInterp, 0.0f), 0.0f);
    QCOMPARE(PmxAnimationPlayer::evaluateBezier(linearInterp, 1.0f), 1.0f);
    QVERIFY(std::abs(PmxAnimationPlayer::evaluateBezier(linearInterp, 0.5f) - 0.5f) < 1e-4f);
    QVERIFY(std::abs(PmxAnimationPlayer::evaluateBezier(linearInterp, 0.25f) - 0.25f) < 1e-4f);

    // Steep ease-in / ease-out (S-curve): x1=127, y1=0, x2=0, y2=127
    uint8_t sCurve[4] = {127, 0, 0, 127};
    float midY = PmxAnimationPlayer::evaluateBezier(sCurve, 0.5f);
    QVERIFY2(std::abs(midY - 0.5f) < 1e-3f,
             qPrintable(QString("S-curve midpoint was %1, expected ~0.5").arg(midY)));

    // Near start (t=0.2), ease-in should be significantly slower than linear (< 0.2)
    float earlyY = PmxAnimationPlayer::evaluateBezier(sCurve, 0.2f);
    QVERIFY2(earlyY < 0.15f, qPrintable(QString("Ease-in early Y was %1, expected < 0.15").arg(earlyY)));

    // Clamping boundary tests
    QCOMPARE(PmxAnimationPlayer::evaluateBezier(linearInterp, -0.5f), 0.0f);
    QCOMPARE(PmxAnimationPlayer::evaluateBezier(linearInterp, 1.5f), 1.0f);
}

void TstPmxEvaluator::testVmdRoundtrip() {
    VmdMotion srcMotion;
    srcMotion.version = "Vocaloid Motion Data 0002";
    srcMotion.modelName = "MikuRoundtrip";

    VmdBoneKeyframe bk0;
    bk0.boneName = "Root";
    bk0.frame = 0;
    bk0.translation = {1.0f, 2.0f, 3.0f};
    bk0.rotation = glm::angleAxis(glm::radians(30.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    VmdBoneKeyframe bk1;
    bk1.boneName = "Root";
    bk1.frame = 30;
    bk1.translation = {5.0f, 6.0f, 7.0f};
    bk1.rotation = glm::angleAxis(glm::radians(60.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    srcMotion.boneKeyframes = {bk0, bk1};

    VmdMorphKeyframe mk0;
    mk0.morphName = "Blink";
    mk0.frame = 0;
    mk0.weight = 0.1f;

    VmdMorphKeyframe mk1;
    mk1.morphName = "Blink";
    mk1.frame = 15;
    mk1.weight = 0.9f;

    srcMotion.morphKeyframes = {mk0, mk1};

    VmdIkKeyframe ikKf;
    ikKf.frame = 10;
    ikKf.showModel = true;
    ikKf.ikToggles.push_back({"LeftLegIK", true});
    ikKf.ikToggles.push_back({"RightLegIK", false});
    srcMotion.ikKeyframes = {ikKf};
    srcMotion.maxFrame = 30;

    QByteArray bytes = VmdImporter::writeToByteArray(srcMotion);
    QString err;
    auto dstMotionOpt = VmdImporter::loadFromByteArray(bytes, &err);
    QVERIFY2(dstMotionOpt.has_value(), qPrintable(err));

    const auto& dst = *dstMotionOpt;
    QCOMPARE(dst.modelName, srcMotion.modelName);
    QCOMPARE(dst.maxFrame, srcMotion.maxFrame);
    QCOMPARE(dst.boneKeyframes.size(), srcMotion.boneKeyframes.size());
    QCOMPARE(dst.morphKeyframes.size(), srcMotion.morphKeyframes.size());
    QCOMPARE(dst.ikKeyframes.size(), srcMotion.ikKeyframes.size());

    // Verify bone values roundtrip exactly
    for (size_t i = 0; i < srcMotion.boneKeyframes.size(); ++i) {
        const auto& s = srcMotion.boneKeyframes[i];
        const auto& d = dst.boneKeyframes[i];
        QCOMPARE(d.boneName, s.boneName);
        QCOMPARE(d.frame, s.frame);
        QVERIFY(glm::distance(d.translation, s.translation) < 1e-4f);
        QVERIFY(std::abs(glm::dot(d.rotation, s.rotation)) > 0.999f);
    }

    // Verify morphs roundtrip
    for (size_t i = 0; i < srcMotion.morphKeyframes.size(); ++i) {
        const auto& s = srcMotion.morphKeyframes[i];
        const auto& d = dst.morphKeyframes[i];
        QCOMPARE(d.morphName, s.morphName);
        QCOMPARE(d.frame, s.frame);
        QVERIFY(std::abs(d.weight - s.weight) < 1e-4f);
    }

    // Verify IK toggles roundtrip
    QCOMPARE(dst.ikKeyframes[0].frame, ikKf.frame);
    QCOMPARE(dst.ikKeyframes[0].ikToggles.size(), size_t(2));
    QCOMPARE(dst.ikKeyframes[0].ikToggles[0].ikBoneName, QStringLiteral("LeftLegIK"));
    QCOMPARE(dst.ikKeyframes[0].ikToggles[0].enable, true);
    QCOMPARE(dst.ikKeyframes[0].ikToggles[1].ikBoneName, QStringLiteral("RightLegIK"));
    QCOMPARE(dst.ikKeyframes[0].ikToggles[1].enable, false);
}

void TstPmxEvaluator::testVpdRoundtrip() {
    VpdPose src;
    src.modelName = "MikuPose";

    VpdBonePose bp0;
    bp0.boneName = "Root";
    bp0.translation = {1.5f, 2.5f, -3.5f};
    bp0.rotation = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    VpdBonePose bp1;
    bp1.boneName = "Head";
    bp1.translation = {0.0f, 10.0f, 0.0f};
    bp1.rotation = glm::angleAxis(glm::radians(-20.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    src.bones = {bp0, bp1};

    VpdMorphPose mp0;
    mp0.morphName = "Smile";
    mp0.weight = 0.77f;
    src.morphs = {mp0};

    QByteArray bytes = VpdImporter::writeToByteArray(src);
    QString err;
    auto dstOpt = VpdImporter::loadFromByteArray(bytes, &err);
    QVERIFY2(dstOpt.has_value(), qPrintable(err));

    const auto& dst = *dstOpt;
    QCOMPARE(dst.modelName, src.modelName);
    QCOMPARE(dst.bones.size(), src.bones.size());
    QCOMPARE(dst.morphs.size(), src.morphs.size());

    for (size_t i = 0; i < src.bones.size(); ++i) {
        QCOMPARE(dst.bones[i].boneName, src.bones[i].boneName);
        QVERIFY(glm::distance(dst.bones[i].translation, src.bones[i].translation) < 1e-4f);
        QVERIFY(std::abs(glm::dot(dst.bones[i].rotation, src.bones[i].rotation)) > 0.999f);
    }

    QCOMPARE(dst.morphs[0].morphName, src.morphs[0].morphName);
    QVERIFY(std::abs(dst.morphs[0].weight - src.morphs[0].weight) < 1e-4f);
}

void TstPmxEvaluator::testAnimationPlayerLooping() {
    VmdMotion motion;
    motion.maxFrame = 10;

    PmxAnimationPlayer player;
    player.loadMotion(motion);
    player.setLoop(true);
    QVERIFY(player.isLoop());

    player.play();
    player.setFrame(9.5f);
    // 1 frame advance -> 10.5 -> loops to 0.5
    player.update(1.0f / 30.0f);
    QVERIFY(player.isPlaying());
    QVERIFY2(std::abs(player.currentFrame() - 0.5f) < 1e-4f,
             qPrintable(QString("Current frame was %1, expected 0.5").arg(player.currentFrame())));

    // Non-looping: reaches maxFrame and stops
    player.setLoop(false);
    player.setFrame(9.5f);
    player.update(1.0f / 30.0f);
    QVERIFY(!player.isPlaying());
    QCOMPARE(player.currentFrame(), 10.0f);
}

void TstPmxEvaluator::testAnimationPlayerNanHandling() {
    float nanVal = std::numeric_limits<float>::quiet_NaN();
    float infVal = std::numeric_limits<float>::infinity();

    // 1. evaluateBezier with NaN, Inf, and out-of-range inputs
    uint8_t interp[4] = {20, 20, 107, 107};
    QCOMPARE(PmxAnimationPlayer::evaluateBezier(interp, nanVal), 0.0f);
    QCOMPARE(PmxAnimationPlayer::evaluateBezier(interp, -1.0f), 0.0f);
    QCOMPARE(PmxAnimationPlayer::evaluateBezier(interp, infVal), 1.0f);

    // 2. Player loaded with multiple keyframes
    VmdMotion motion;
    VmdBoneKeyframe bk0;
    bk0.boneName = "Arm";
    bk0.frame = 10;
    bk0.translation = {1.0f, 2.0f, 3.0f};
    bk0.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    VmdBoneKeyframe bk1;
    bk1.boneName = "Arm";
    bk1.frame = 30;
    bk1.translation = {4.0f, 5.0f, 6.0f};
    bk1.rotation = glm::quat(0.707107f, 0.0f, 0.707107f, 0.0f);

    motion.boneKeyframes = {bk0, bk1};

    VmdMorphKeyframe mk0;
    mk0.morphName = "Blink";
    mk0.frame = 10;
    mk0.weight = 0.2f;

    VmdMorphKeyframe mk1;
    mk1.morphName = "Blink";
    mk1.frame = 30;
    mk1.weight = 0.8f;

    motion.morphKeyframes = {mk0, mk1};

    VmdIkKeyframe ik0;
    ik0.frame = 10;
    ik0.ikToggles.push_back({"LegIK", true});

    VmdIkKeyframe ik1;
    ik1.frame = 30;
    ik1.ikToggles.push_back({"LegIK", false});

    motion.ikKeyframes = {ik0, ik1};
    motion.maxFrame = 40;

    PmxAnimationPlayer player;
    player.loadMotion(motion);

    // Test setFrame with NaN and negative: should reset to 0.0f without crash
    player.setFrame(nanVal);
    QCOMPARE(player.currentFrame(), 0.0f);
    player.setFrame(-5.0f);
    QCOMPARE(player.currentFrame(), 0.0f);

    // Test evaluateBoneTranslation with NaN: should safely return track.front().translation
    glm::vec3 tNan = player.evaluateBoneTranslation("Arm", nanVal);
    QCOMPARE(tNan, bk0.translation);

    // Test evaluateBoneRotation with NaN: should safely return track.front().rotation
    glm::quat qNan = player.evaluateBoneRotation("Arm", nanVal);
    QCOMPARE(qNan, bk0.rotation);

    // Test evaluateMorphWeight with NaN: should safely return track.front().weight
    float wNan = player.evaluateMorphWeight("Blink", nanVal);
    QCOMPARE(wNan, mk0.weight);

    // Test evaluateIkState with NaN: should safely return first keyframe state
    bool ikNan = player.evaluateIkState("LegIK", nanVal);
    QCOMPARE(ikNan, true);

    // Test update with NaN: should not crash and should not advance
    player.play();
    player.setFrame(10.0f);
    player.update(nanVal);
    QCOMPARE(player.currentFrame(), 10.0f);

    // Test applyToScene with NaN currentFrame: should not crash
    PmxDocument doc;
    PmxBone b;
    b.nameJp = "Arm";
    doc.bones = {b};
    PmxRuntimeScene scene;
    scene.loadDocument(doc);

    player.setFrame(nanVal);
    player.applyToScene(scene);
    QCOMPARE(scene.bone(0).userTranslation, bk0.translation);
}

void TstPmxEvaluator::testVmdMalformedBufferRejection() {
    QString err;

    // 1. Buffer smaller than minimum header size (< 50 bytes)
    QByteArray tiny(30, '\0');
    QVERIFY(!VmdImporter::loadFromByteArray(tiny, &err).has_value());

    // 2. Buffer with invalid signature
    QByteArray badSig(60, '\0');
    std::memcpy(badSig.data(), "Invalid Motion Data Header!!", 28);
    QVERIFY(!VmdImporter::loadFromByteArray(badSig, &err).has_value());

    // 3. Buffer claiming huge boneCount (0x7FFFFFFF) beyond file size -> must reject, no bad_alloc
    QByteArray hugeBones;
    QByteArray header = "Vocaloid Motion Data 0002";
    header.append(QByteArray(30 - header.size(), '\0'));
    hugeBones.append(header);
    hugeBones.append(QByteArray(20, '\0')); // model name
    quint32 hugeCount = qToLittleEndian(static_cast<quint32>(0x7FFFFFFF));
    hugeBones.append(reinterpret_cast<const char*>(&hugeCount), sizeof(quint32));
    QVERIFY(!VmdImporter::loadFromByteArray(hugeBones, &err).has_value());

    // 4. Truncated bone keyframe record (count=1, but only 20 bytes payload instead of 111)
    QByteArray truncBone = header;
    truncBone.append(QByteArray(20, '\0'));
    quint32 oneBone = qToLittleEndian(static_cast<quint32>(1));
    truncBone.append(reinterpret_cast<const char*>(&oneBone), sizeof(quint32));
    truncBone.append(QByteArray(20, 'A')); // Only 20 bytes instead of 111
    QVERIFY(!VmdImporter::loadFromByteArray(truncBone, &err).has_value());

    // 5. Valid bone and morph, but huge ikCount (0x7FFFFFFF) -> must reject, no bad_alloc
    QByteArray hugeIk = header;
    hugeIk.append(QByteArray(20, '\0')); // model
    quint32 zero = 0;
    hugeIk.append(reinterpret_cast<const char*>(&zero), sizeof(quint32)); // bone count = 0
    hugeIk.append(reinterpret_cast<const char*>(&zero), sizeof(quint32)); // morph count = 0
    hugeIk.append(reinterpret_cast<const char*>(&zero), sizeof(quint32)); // camera count = 0
    hugeIk.append(reinterpret_cast<const char*>(&zero), sizeof(quint32)); // light count = 0
    hugeIk.append(reinterpret_cast<const char*>(&zero), sizeof(quint32)); // shadow count = 0
    hugeIk.append(reinterpret_cast<const char*>(&hugeCount), sizeof(quint32)); // ikCount = 0x7FFFFFFF
    QVERIFY(!VmdImporter::loadFromByteArray(hugeIk, &err).has_value());

    // 6. Huge camera count beyond buffer
    QByteArray hugeCam = header;
    hugeCam.append(QByteArray(20, '\0'));
    hugeCam.append(reinterpret_cast<const char*>(&zero), sizeof(quint32)); // bone count = 0
    hugeCam.append(reinterpret_cast<const char*>(&zero), sizeof(quint32)); // morph count = 0
    hugeCam.append(reinterpret_cast<const char*>(&hugeCount), sizeof(quint32)); // camera count = 0x7FFFFFFF
    QVERIFY(!VmdImporter::loadFromByteArray(hugeCam, &err).has_value());
}

void TstPmxEvaluator::testDiagnoseUserVmdMotion() {
    const QString vmdPath = QStringLiteral("/home/oscarc/Documents/3D Objects/PHODI PHODI by MizukiC/Motion 1.vmd");
    const QString pmxPath = QStringLiteral("/home/oscarc/Documents/3D Objects/Barbara/Barbara.pmx");

    qDebug().noquote() << "\n========================================================";
    qDebug().noquote() << "=== DIAGNOSING USER VMD MOTION & PMX INTEGRATION ===";
    qDebug().noquote() << "========================================================";

    // 1. Check if VMD file exists and can be opened
    QFileInfo vmdInfo(vmdPath);
    qDebug().noquote() << "[Step 1] Checking VMD File:" << vmdPath;
    qDebug().noquote() << "  File exists:" << vmdInfo.exists();
    qDebug().noquote() << "  File size (bytes):" << vmdInfo.size();

    QVERIFY2(vmdInfo.exists(), "VMD file does not exist on disk!");

    QFile vmdFile(vmdPath);
    bool openOk = vmdFile.open(QIODevice::ReadOnly);
    qDebug().noquote() << "  File open(ReadOnly):" << openOk;
    QVERIFY2(openOk, "Cannot open VMD file!");

    QByteArray rawData = vmdFile.readAll();
    vmdFile.close();
    qDebug().noquote() << "  Bytes read from file:" << rawData.size();

    // 2. Read header signature (first 30 bytes), version string, and model name
    QByteArray headerBytes = rawData.left(30);
    qDebug().noquote() << "[Step 2] Header Signature & Metadata:";
    qDebug().noquote() << "  Header raw hex:" << headerBytes.toHex();
    QString headerStr = QString::fromLatin1(headerBytes.constData(), qMin<int>(30, headerBytes.indexOf('\0') >= 0 ? headerBytes.indexOf('\0') : 30));
    qDebug().noquote() << "  Header string:" << headerStr;

    QByteArray modelBytes = rawData.mid(30, 20);
    qDebug().noquote() << "  Model Name raw hex:" << modelBytes.toHex();
    QString modelStr = QString::fromLatin1(modelBytes.constData(), qMin<int>(20, modelBytes.indexOf('\0') >= 0 ? modelBytes.indexOf('\0') : 20));
    qDebug().noquote() << "  Model Name (Latin1 preview):" << modelStr;

    // 3. Call VmdImporter::loadFromFile
    qDebug().noquote() << "[Step 3] Calling pmx::VmdImporter::loadFromFile...";
    QString vmdError;
    auto motionOpt = VmdImporter::loadFromFile(vmdPath, &vmdError);

    if (!motionOpt.has_value()) {
        qDebug().noquote() << "  [ERROR] VmdImporter::loadFromFile returned std::nullopt!";
        qDebug().noquote() << "  Error string:" << vmdError;
        QFAIL(qPrintable(QStringLiteral("VmdImporter failed to parse VMD: %1").arg(vmdError)));
        return;
    }

    qDebug().noquote() << "  [SUCCESS] VmdImporter::loadFromFile succeeded!";
    const auto& motion = *motionOpt;
    qDebug().noquote() << "  Parsed version:" << motion.version;
    qDebug().noquote() << "  Parsed modelName:" << motion.modelName;
    qDebug().noquote() << "  Bone keyframes count:" << motion.boneKeyframes.size();
    qDebug().noquote() << "  Morph keyframes count:" << motion.morphKeyframes.size();
    qDebug().noquote() << "  IK keyframes count:" << motion.ikKeyframes.size();
    qDebug().noquote() << "  Max frame:" << motion.maxFrame;

    // Unique bone names present in the motion file
    QSet<QString> uniqueBonesSet;
    for (const auto& kf : motion.boneKeyframes) {
        uniqueBonesSet.insert(kf.boneName);
    }
    QStringList sortedVmdBones = uniqueBonesSet.values();
    sortedVmdBones.sort();
    qDebug().noquote() << "  Unique bone names count in motion:" << sortedVmdBones.size();
    qDebug().noquote() << "  Unique bone names in motion:";
    for (int i = 0; i < sortedVmdBones.size(); i += 8) {
        QStringList chunk = sortedVmdBones.mid(i, 8);
        qDebug().noquote() << "   " << chunk.join(QStringLiteral(", "));
    }

    // 4. Load Barbara.pmx and compare bone names
    qDebug().noquote() << "\n[Step 4] Checking Barbara.pmx:";
    QFileInfo pmxInfo(pmxPath);
    qDebug().noquote() << "  PMX Path:" << pmxPath;
    qDebug().noquote() << "  PMX exists:" << pmxInfo.exists();
    qDebug().noquote() << "  PMX size (bytes):" << pmxInfo.size();
    QVERIFY2(pmxInfo.exists(), "PMX file does not exist on disk!");

    PmxDocument pmxDoc;
    QString pmxError;
    bool pmxReadOk = PmxReader::readFromFile(pmxPath, pmxDoc, &pmxError);
    qDebug().noquote() << "  PmxReader::readFromFile ok:" << pmxReadOk << "err:" << pmxError;
    QVERIFY2(pmxReadOk, "Failed to load Barbara.pmx!");

    qDebug().noquote() << "  Model Name JP:" << pmxDoc.modelInfo.modelNameJp;
    qDebug().noquote() << "  Model Name EN:" << pmxDoc.modelInfo.modelNameEn;
    qDebug().noquote() << "  Total bones in Barbara.pmx:" << pmxDoc.bones.size();
    for (int i = 0; i < qMin<int>(25, static_cast<int>(pmxDoc.bones.size())); ++i) {
        qDebug().noquote() << "    Model bone" << i << "JP:" << pmxDoc.bones[i].nameJp << "EN:" << pmxDoc.bones[i].nameEn;
    }

    QSet<QString> modelBonesJp;
    QSet<QString> modelBonesEn;
    for (const auto& b : pmxDoc.bones) {
        if (!b.nameJp.isEmpty()) modelBonesJp.insert(b.nameJp);
        if (!b.nameEn.isEmpty()) modelBonesEn.insert(b.nameEn);
    }

    QStringList matchedBones;
    QStringList unmatchedBones;
    for (const QString& bName : sortedVmdBones) {
        if (modelBonesJp.contains(bName) || modelBonesEn.contains(bName)) {
            matchedBones.append(bName);
        } else {
            unmatchedBones.append(bName);
        }
    }

    qDebug().noquote() << "\n[Bone Matching Results]:";
    qDebug().noquote() << "  Matched bones count:" << matchedBones.size() << "of" << sortedVmdBones.size()
                       << QStringLiteral("(%1%)").arg(100.0 * matchedBones.size() / sortedVmdBones.size(), 0, 'f', 1);
    qDebug().noquote() << "  Matched bones list:" << matchedBones;
    qDebug().noquote() << "  Unmatched bones count:" << unmatchedBones.size();
    qDebug().noquote() << "  Sample unmatched bones (first 25):" << unmatchedBones.mid(0, 25);

    QVERIFY2(matchedBones.size() > 50, "At least standard humanoid skeleton bones must match!");

    // 5. Evaluate animation at frame 0 and check scene deformation
    qDebug().noquote() << "\n[Step 5] Evaluating Animation on PmxRuntimeScene:";
    PmxRuntimeScene scene;
    scene.loadDocument(pmxDoc);

    // Store rest positions of matched bones
    QHash<int, glm::vec3> restPositions;
    for (const QString& bName : matchedBones) {
        int idx = scene.findBoneByName(bName);
        if (idx >= 0) {
            restPositions[idx] = glm::vec3(scene.bone(idx).worldMatrix[3]); // world translation
        }
    }

    PmxAnimationPlayer player;
    player.loadMotion(motion);
    qDebug().noquote() << "  Player loaded motion. MaxFrame:" << player.maxFrame()
                       << "CurrentFrame:" << player.currentFrame();

    // Check keyframes at frame 0 specifically
    int frame0BoneKfCount = 0;
    int movedBonesAt0 = 0;
    for (const auto& kf : motion.boneKeyframes) {
        if (kf.frame == 0) {
            frame0BoneKfCount++;
            if (glm::length(kf.translation) > 1e-4f ||
                std::abs(kf.rotation.w - 1.0f) > 1e-4f ||
                glm::length(glm::vec3(kf.rotation.x, kf.rotation.y, kf.rotation.z)) > 1e-4f) {
                movedBonesAt0++;
            }
        }
    }
    qDebug().noquote() << "  VMD bone keyframes at frame 0:" << frame0BoneKfCount
                       << "(non-identity transforms:" << movedBonesAt0 << ")";

    // Apply frame 0 to scene
    player.setFrame(0.0f);
    player.applyToScene(scene);
    scene.evaluate(0.0f);

    int alteredBonesAtFrame0 = 0;
    for (auto it = restPositions.begin(); it != restPositions.end(); ++it) {
        int idx = it.key();
        glm::vec3 curPos = glm::vec3(scene.bone(idx).worldMatrix[3]);
        if (glm::distance(curPos, it.value()) > 1e-3f) {
            alteredBonesAtFrame0++;
            if (alteredBonesAtFrame0 <= 5) {
                qDebug().noquote() << "    Bone" << scene.bone(idx).nameJp
                                   << "moved from rest:" << it.value().x << it.value().y << it.value().z
                                   << "to frame 0:" << curPos.x << curPos.y << curPos.z;
            }
        }
    }
    qDebug().noquote() << "  Bones with world pos change at frame 0:" << alteredBonesAtFrame0 << "of" << matchedBones.size();

    // Now test frame 30 (1 second in)
    player.setFrame(30.0f);
    player.applyToScene(scene);
    scene.evaluate(0.0f);

    int alteredBonesAtFrame30 = 0;
    for (auto it = restPositions.begin(); it != restPositions.end(); ++it) {
        int idx = it.key();
        glm::vec3 curPos = glm::vec3(scene.bone(idx).worldMatrix[3]);
        if (glm::distance(curPos, it.value()) > 1e-3f) {
            alteredBonesAtFrame30++;
        }
    }
    qDebug().noquote() << "  Bones with world pos change at frame 30:" << alteredBonesAtFrame30 << "of" << matchedBones.size();

    // Check player update with delta
    player.play();
    qDebug().noquote() << "  Player playing state after play():" << player.isPlaying();
    player.update(1.0f / 30.0f);
    qDebug().noquote() << "  Player current frame after 1 update (dt=1/30):" << player.currentFrame();

    qDebug().noquote() << "========================================================\n";
}

void TstPmxEvaluator::testDynamicRigidBodiesAlignToAnimatedBones() {
    const QString vmdPath = QStringLiteral("/home/oscarc/Documents/3D Objects/PHODI PHODI by MizukiC/Motion 1.vmd");
    const QString pmxPath = QStringLiteral("/home/oscarc/Documents/3D Objects/Barbara/Barbara.pmx");

    if (!QFile::exists(vmdPath) || !QFile::exists(pmxPath)) {
        QSKIP("Barbara.pmx or Motion 1.vmd not found on system");
    }

    PmxDocument pmxDoc;
    QString pmxErr;
    QVERIFY2(PmxReader::readFromFile(pmxPath, pmxDoc, &pmxErr), qPrintable(pmxErr));

    QString vmdErr;
    auto motionOpt = VmdImporter::loadFromFile(vmdPath, &vmdErr);
    QVERIFY2(motionOpt.has_value(), qPrintable(vmdErr));
    const auto& motion = *motionOpt;

    PmxRuntimeScene scene;
    scene.loadDocument(pmxDoc);
    scene.setPhysicsEnabled(true);

    // Track dynamic rigid bodies (physicsMode 1 or 2)
    struct DynamicBodyInfo {
        int bodyIndex;
        int boneIndex;
        int parentBoneIndex;
        uint8_t physicsMode;
        glm::vec3 restBonePos;
    };
    std::vector<DynamicBodyInfo> dynamicBodies;

    for (size_t i = 0; i < pmxDoc.rigidBodies.size(); ++i) {
        const auto& rb = pmxDoc.rigidBodies[i];
        if ((rb.physicsMode == 1 || rb.physicsMode == 2) &&
            rb.boneIndex >= 0 && rb.boneIndex < static_cast<int>(pmxDoc.bones.size())) {
            DynamicBodyInfo info;
            info.bodyIndex = static_cast<int>(i);
            info.boneIndex = rb.boneIndex;
            info.parentBoneIndex = pmxDoc.bones[static_cast<size_t>(rb.boneIndex)].parentBoneIndex;
            info.physicsMode = rb.physicsMode;
            info.restBonePos = scene.worldPosition(rb.boneIndex);
            dynamicBodies.push_back(info);
        }
    }
    QVERIFY(!dynamicBodies.empty());

    // Apply frame 0 of motion
    PmxAnimationPlayer player;
    player.loadMotion(motion);
    player.setFrame(0.0f);
    player.applyToScene(scene);

    // Evaluate static pose at frame 0 (deltaTime = 0.0f)
    scene.evaluate(0.0f);

    // 1. Verify dynamic bones did not stay at rest/origin when parents moved,
    // and rigid bodies are aligned to boneWorld * boneToBodyOffset.
    int parentMovedCount = 0;
    int alignedBodyCount = 0;

    for (const auto& item : dynamicBodies) {
        glm::vec3 curBonePos = scene.worldPosition(item.boneIndex);
        QVERIFY(!std::isnan(curBonePos.x) && !std::isnan(curBonePos.y) && !std::isnan(curBonePos.z));
        QVERIFY(!std::isinf(curBonePos.x) && !std::isinf(curBonePos.y) && !std::isinf(curBonePos.z));

        if (item.parentBoneIndex >= 0 && item.parentBoneIndex < static_cast<int>(pmxDoc.bones.size())) {
            glm::vec3 parentCurPos = scene.worldPosition(item.parentBoneIndex);
            glm::vec3 parentRestPos = PmxRuntimeScene::toRightHanded(pmxDoc.bones[static_cast<size_t>(item.parentBoneIndex)].position);
            if (glm::distance(parentCurPos, parentRestPos) > 0.5f) {
                // Parent bone moved by > 0.5m. Dynamic bone must follow and not be stuck at rest position.
                float distFromRest = glm::distance(curBonePos, item.restBonePos);
                QVERIFY2(distFromRest > 0.1f,
                         qPrintable(QString("Bone %1 (index %2) remained stuck at rest (%3, %4, %5) despite parent moving!")
                                        .arg(pmxDoc.bones[static_cast<size_t>(item.boneIndex)].nameJp)
                                        .arg(item.boneIndex)
                                        .arg(curBonePos.x).arg(curBonePos.y).arg(curBonePos.z)));
                parentMovedCount++;
            }
        }

        // Rigid body world transform in Bullet must match scene.worldMatrix(meta.boneIndex) * meta.boneToBodyOffset
        glm::vec3 curBodyPos = scene.physicsEngine()->rigidBodyPosition(item.bodyIndex);
        QVERIFY(!std::isnan(curBodyPos.x) && !std::isnan(curBodyPos.y) && !std::isnan(curBodyPos.z));

        const auto* meta = scene.physicsEngine()->runtimeRigidBody(item.bodyIndex);
        QVERIFY(meta != nullptr);
        glm::mat4 expectedBodyWorld = scene.worldMatrix(item.boneIndex) * meta->boneToBodyOffset;
        glm::vec3 expectedBodyPos = glm::vec3(expectedBodyWorld[3]);
        QVERIFY2(glm::distance(curBodyPos, expectedBodyPos) < 1e-3f,
                 qPrintable(QString("Dynamic rigid body %1 position (%2, %3, %4) does not match bone transform (%5, %6, %7)")
                                .arg(item.bodyIndex)
                                .arg(curBodyPos.x).arg(curBodyPos.y).arg(curBodyPos.z)
                                .arg(expectedBodyPos.x).arg(expectedBodyPos.y).arg(expectedBodyPos.z)));
        alignedBodyCount++;
    }

    QVERIFY(parentMovedCount > 0);
    QCOMPARE(alignedBodyCount, static_cast<int>(dynamicBodies.size()));

    // 2. Step simulation with delta (e.g. deltaTime = 1.0f / 30.0f) and verify stable stepping
    for (int step = 1; step <= 30; ++step) {
        player.setFrame(static_cast<float>(step));
        player.applyToScene(scene);
        scene.evaluate(1.0f / 30.0f);

        for (const auto& item : dynamicBodies) {
            glm::vec3 bonePos = scene.worldPosition(item.boneIndex);
            QVERIFY(!std::isnan(bonePos.x) && !std::isnan(bonePos.y) && !std::isnan(bonePos.z));
            QVERIFY(!std::isinf(bonePos.x) && !std::isinf(bonePos.y) && !std::isinf(bonePos.z));
            // Sanity check: no physics explosion (e.g. bones flying off to infinity)
            QVERIFY2(glm::length(bonePos) < 50.0f,
                     qPrintable(QString("Explosion detected at step %1 for bone %2: pos=(%3, %4, %5)")
                                    .arg(step)
                                    .arg(pmxDoc.bones[static_cast<size_t>(item.boneIndex)].nameJp)
                                    .arg(bonePos.x).arg(bonePos.y).arg(bonePos.z)));

            glm::vec3 bodyPos = scene.physicsEngine()->rigidBodyPosition(item.bodyIndex);
            QVERIFY(!std::isnan(bodyPos.x) && !std::isnan(bodyPos.y) && !std::isnan(bodyPos.z));
            QVERIFY(!std::isinf(bodyPos.x) && !std::isinf(bodyPos.y) && !std::isinf(bodyPos.z));
            QVERIFY2(glm::length(bodyPos) < 50.0f,
                     qPrintable(QString("Explosion detected at step %1 for rigid body %2").arg(step).arg(item.bodyIndex)));
        }
    }
}

void TstPmxEvaluator::testAnimationKneeFlexionAndIkLegPoses() {
    // 1. Synthetic 2-bone leg chain test for 1-DOF hinge joint knee limits
    {
        PmxDocument doc;
        PmxBone thigh;
        thigh.nameJp = "左足";
        thigh.parentBoneIndex = -1;
        thigh.position = {0.0f, 2.0f, 0.0f};

        PmxBone shin;
        shin.nameJp = "左ひざ";
        shin.parentBoneIndex = 0;
        shin.position = {0.0f, 1.0f, 0.0f};

        PmxBone ankle;
        ankle.nameJp = "左足首";
        ankle.parentBoneIndex = 1;
        ankle.position = {0.0f, 0.0f, 0.0f};

        PmxBone legIk;
        legIk.nameJp = "左足ＩＫ";
        legIk.parentBoneIndex = -1;
        legIk.position = {0.0f, 0.0f, 0.0f};
        legIk.boneFlags = 0x0020;
        legIk.ikTargetBoneIndex = 2;
        legIk.ikLoopCount = 60;
        legIk.ikLimitAngle = 0.5f;

        // 1-DOF X-only hinge knee link with canonical Left-Handed PMX limits
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

        PmxRuntimeScene synthScene;
        synthScene.loadDocument(doc);

        // Move target forward (+Z in RH) and slightly up
        synthScene.setBoneTransform(3, glm::vec3(0.0f, 0.5f, 0.5f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        synthScene.evaluate();

        glm::vec3 kneeEuler = MmdCcdIkSolver::quatToZxyEuler(synthScene.bone(1).ikRotation);
        QVERIFY2(kneeEuler.x >= 0.005f,
                 qPrintable(QString("Synthetic knee flexion X %1 < 0.005 rad").arg(kneeEuler.x)));
        QVERIFY2(std::abs(kneeEuler.y) < 1e-4f,
                 qPrintable(QString("Synthetic knee non-zero Y %1").arg(kneeEuler.y)));
        QVERIFY2(std::abs(kneeEuler.z) < 1e-4f,
                 qPrintable(QString("Synthetic knee non-zero Z %1").arg(kneeEuler.z)));

        glm::vec3 effectorPos = synthScene.worldPosition(2);
        glm::vec3 targetPos = synthScene.worldPosition(3);
        QVERIFY2(glm::distance(effectorPos, targetPos) < 1e-3f, "Synthetic ankle did not reach IK target");
    }

    // 2. Live Barbara.pmx + Motion 1.vmd test
    const QString pmxPath = QStringLiteral("/home/oscarc/Documents/3D Objects/Barbara/Barbara.pmx");
    const QString vmdPath = QStringLiteral("/home/oscarc/Documents/3D Objects/PHODI PHODI by MizukiC/Motion 1.vmd");

    if (!QFile::exists(pmxPath) || !QFile::exists(vmdPath)) {
        qDebug() << "Skipping live animation check: Barbara.pmx or Motion 1.vmd not found.";
        return;
    }

    PmxDocument pmxDoc;
    QString pmxErr;
    QVERIFY2(PmxReader::readFromFile(pmxPath, pmxDoc, &pmxErr), qPrintable(pmxErr));

    QString vmdErr;
    auto motionOpt = VmdImporter::loadFromFile(vmdPath, &vmdErr);
    QVERIFY2(motionOpt.has_value(), qPrintable(vmdErr));
    const auto& motion = *motionOpt;

    PmxRuntimeScene scene;
    scene.loadDocument(pmxDoc);

    auto findBone = [&](const QString& n1, const QString& n2) -> int {
        int idx = scene.findBoneByName(n1);
        if (idx < 0) idx = scene.findBoneByName(n2);
        return idx;
    };

    int kneeL = findBone(QString::fromUtf8("左ひざ"), QStringLiteral("ひざ.L"));
    int kneeR = findBone(QString::fromUtf8("右ひざ"), QStringLiteral("ひざ.R"));
    int ankleL = findBone(QString::fromUtf8("左足首"), QStringLiteral("足首.L"));
    int ankleR = findBone(QString::fromUtf8("右足首"), QStringLiteral("足首.R"));
    int ikL = findBone(QString::fromUtf8("左足ＩＫ"), QStringLiteral("足ＩＫ.L"));
    int ikR = findBone(QString::fromUtf8("右足ＩＫ"), QStringLiteral("足ＩＫ.R"));
    int thighL = findBone(QString::fromUtf8("左足"), QStringLiteral("足.L"));
    int thighR = findBone(QString::fromUtf8("右足"), QStringLiteral("足.R"));

    QVERIFY2(kneeL >= 0, "Left knee bone not found in Barbara");
    QVERIFY2(kneeR >= 0, "Right knee bone not found in Barbara");
    QVERIFY2(ankleL >= 0, "Left ankle bone not found in Barbara");
    QVERIFY2(ankleR >= 0, "Right ankle bone not found in Barbara");
    QVERIFY2(ikL >= 0, "Left foot IK bone not found in Barbara");
    QVERIFY2(ikR >= 0, "Right foot IK bone not found in Barbara");
    QVERIFY2(thighL >= 0, "Left thigh bone not found in Barbara");
    QVERIFY2(thighR >= 0, "Right thigh bone not found in Barbara");

    for (const auto& l : pmxDoc.bones[ikL].ikLinks) {
        qDebug() << "Left Foot IK Link bone:" << pmxDoc.bones[l.boneIndex].nameJp
                 << "hasLimit:" << l.hasAngleLimit
                 << "limitMin:" << l.limitMin.x << l.limitMin.y << l.limitMin.z
                 << "limitMax:" << l.limitMax.x << l.limitMax.y << l.limitMax.z;
    }

    PmxAnimationPlayer player;
    player.loadMotion(motion);

    const std::vector<float> testFrames = {39.0f, 53.0f, 119.0f, 224.0f, 379.0f};

    for (float frame : testFrames) {
        player.setFrame(frame);
        player.applyToScene(scene);
        scene.evaluate(0.0f);

        glm::vec3 eulerKneeL = MmdCcdIkSolver::quatToZxyEuler(scene.bone(kneeL).ikRotation);
        glm::vec3 eulerKneeR = MmdCcdIkSolver::quatToZxyEuler(scene.bone(kneeR).ikRotation);
        glm::vec3 posAnkleL = scene.worldPosition(ankleL);
        glm::vec3 posIkL = scene.worldPosition(ikL);
        float ankleIkDistL = glm::distance(posAnkleL, posIkL);

        glm::vec3 eulerThighL = MmdCcdIkSolver::quatToZxyEuler(scene.bone(thighL).userRotation * scene.bone(thighL).ikRotation);
        glm::vec3 thighDirL = glm::normalize(scene.worldPosition(kneeL) - scene.worldPosition(thighL));

        qDebug().noquote() << QStringLiteral("[Frame %1] KneeL_X: %2 rad (%3 deg), KneeR_X: %4 rad (%5 deg), AnkleL-IkL dist: %6, ThighL dir: (%7, %8, %9)")
                                  .arg(frame)
                                  .arg(eulerKneeL.x, 0, 'f', 4)
                                  .arg(glm::degrees(eulerKneeL.x), 0, 'f', 1)
                                  .arg(eulerKneeR.x, 0, 'f', 4)
                                  .arg(glm::degrees(eulerKneeR.x), 0, 'f', 1)
                                  .arg(ankleIkDistL, 0, 'f', 4)
                                  .arg(thighDirL.x, 0, 'f', 3)
                                  .arg(thighDirL.y, 0, 'f', 3)
                                  .arg(thighDirL.z, 0, 'f', 3);

        if (std::abs(frame - 39.0f) < 1e-3f) {
            // At frame 39: positive flexion > 0.35 rad (approx +20 deg to +50 deg)
            QVERIFY2(eulerKneeL.x > 0.35f,
                     qPrintable(QString("Frame 39 Left Knee flexion %1 rad (%2 deg) <= 0.35 rad")
                                    .arg(eulerKneeL.x).arg(glm::degrees(eulerKneeL.x))));
            QVERIFY2(ankleIkDistL < 0.2f,
                     qPrintable(QString("Frame 39 AnkleL to IkL distance %1 >= 0.2").arg(ankleIkDistL)));
            // Thigh bone must point mostly downwards (Y < -0.5), not flipped 90 degrees outward into the air
            QVERIFY2(thighDirL.y < -0.5f,
                     qPrintable(QString("Frame 39 ThighL flipped outward: dir.y = %1").arg(thighDirL.y)));
        } else if (std::abs(frame - 53.0f) < 1e-3f) {
            QVERIFY2(eulerKneeL.x > 0.2f, "Frame 53 Left Knee should flex forward");
            QVERIFY2(eulerKneeR.x > 0.35f, "Frame 53 Right Knee should flex forward");
            QVERIFY2(ankleIkDistL < 0.2f, "Frame 53 AnkleL should track IkL");
            QVERIFY2(thighDirL.y < -0.5f, "Frame 53 ThighL should point downward");
        } else if (std::abs(frame - 119.0f) < 1e-3f) {
            QVERIFY2(eulerKneeL.x >= 0.0f, "Frame 119 Left Knee should not hyperextend");
            QVERIFY2(thighDirL.y < -0.5f, "Frame 119 ThighL should point downward");
        } else if (std::abs(frame - 224.0f) < 1e-3f) {
            QVERIFY2(eulerKneeL.x > 1.0f, "Frame 224 Left Knee should flex high");
            QVERIFY2(ankleIkDistL < 0.3f, "Frame 224 AnkleL should track IkL");
        } else if (std::abs(frame - 379.0f) < 1e-3f) {
            QVERIFY2(eulerKneeL.x > 0.35f, "Frame 379 Left Knee should flex");
            QVERIFY2(eulerKneeR.x > 0.35f, "Frame 379 Right Knee should flex");
        }

        // General invariants across all test frames (39, 53, 119, 224, 379):
        // Knee flexion must never be negative (no hyperextension / backwards bending)
        QVERIFY2(eulerKneeL.x >= 0.0f,
                 qPrintable(QString("Frame %1 Left knee hyperextended: %2").arg(frame).arg(eulerKneeL.x)));
        QVERIFY2(eulerKneeR.x >= 0.0f,
                 qPrintable(QString("Frame %1 Right knee hyperextended: %2").arg(frame).arg(eulerKneeR.x)));

        // Knee 1-DOF hinge invariant: Y and Z angles must remain 0
        QVERIFY2(std::abs(eulerKneeL.y) < 1e-3f && std::abs(eulerKneeL.z) < 1e-3f,
                 qPrintable(QString("Frame %1 Left knee non-zero Y/Z: (%2, %3)")
                                .arg(frame).arg(eulerKneeL.y).arg(eulerKneeL.z)));
        QVERIFY2(std::abs(eulerKneeR.y) < 1e-3f && std::abs(eulerKneeR.z) < 1e-3f,
                 qPrintable(QString("Frame %1 Right knee non-zero Y/Z: (%2, %3)")
                                .arg(frame).arg(eulerKneeR.y).arg(eulerKneeR.z)));

        glm::vec3 posThighL = scene.worldPosition(thighL);
        glm::vec3 posKneeL = scene.worldPosition(kneeL);
        qDebug().noquote() << QStringLiteral("       ThighL pos: (%1, %2, %3), KneeL pos: (%4, %5, %6), AnkleL pos: (%7, %8, %9), IkL pos: (%10, %11, %12)")
                                  .arg(posThighL.x, 0, 'f', 2).arg(posThighL.y, 0, 'f', 2).arg(posThighL.z, 0, 'f', 2)
                                  .arg(posKneeL.x, 0, 'f', 2).arg(posKneeL.y, 0, 'f', 2).arg(posKneeL.z, 0, 'f', 2)
                                  .arg(posAnkleL.x, 0, 'f', 2).arg(posAnkleL.y, 0, 'f', 2).arg(posAnkleL.z, 0, 'f', 2)
                                  .arg(posIkL.x, 0, 'f', 2).arg(posIkL.y, 0, 'f', 2).arg(posIkL.z, 0, 'f', 2);
    }
}

QTEST_MAIN(TstPmxEvaluator)
#include "tst_pmx_evaluator.moc"
