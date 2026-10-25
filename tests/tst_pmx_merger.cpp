#include "core/PmxMerger.h"
#include "core/PmxReader.h"
#include "core/PmxWriter.h"
#include "models/PmxBoneTableModel.h"
#include "models/PmxDisplayFrameModel.h"
#include "models/PmxDocumentController.h"
#include "models/PmxJointTableModel.h"
#include "models/PmxMaterialTableModel.h"
#include "models/PmxMorphTableModel.h"
#include "models/PmxRigidBodyTableModel.h"
#include "models/PmxVertexTableModel.h"

#include <QTemporaryDir>
#include <QTest>
#include <cmath>

using namespace pmx;

class TstPmxMerger : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testBasicKitbashMerge();
    void testStandardBoneDeduplication();
    void testPhysicsRemapping();
    void testMorphRemapping();
    void testDisplayFrameRemapping();
    void testHeaderIndexSizeUpgrade();
    void testControllerMergeUndoRedo();
    void testBdef4AndQdefRemapping();
    void testInheritAndIkLinkRemapping();
    void testSoftBodyRemapping();
    void testUvAndImpulseMorphRemapping();
    void testBoneNameCollisionEdgeCase();
    void testBoundaryAndCornerCases();

private:
    QTemporaryDir m_tempDir;
};

void TstPmxMerger::initTestCase() {
    QVERIFY(m_tempDir.isValid());
}

void TstPmxMerger::cleanupTestCase() {
}

void TstPmxMerger::testBasicKitbashMerge() {
    PmxDocument baseDoc;
    baseDoc.header.vertexIndexSize = 1;
    baseDoc.header.textureIndexSize = 1;

    PmxVertex bv0, bv1, bv2;
    bv0.position = {0.0f, 0.0f, 0.0f};
    bv1.position = {1.0f, 0.0f, 0.0f};
    bv2.position = {0.0f, 1.0f, 0.0f};
    baseDoc.vertices = {bv0, bv1, bv2};
    baseDoc.faceIndices = {0, 1, 2};
    baseDoc.textures = {QStringLiteral("tex/shared.png"), QStringLiteral("tex/base_only.png")};

    PmxMaterial bm0;
    bm0.nameJp = QStringLiteral("BaseMat");
    bm0.textureIndex = 0;
    bm0.sphereTextureIndex = 1;
    bm0.faceVertexCount = 3;
    baseDoc.materials = {bm0};

    PmxBone bb0;
    bb0.nameJp = QStringLiteral("BaseBone");
    baseDoc.bones = {bb0};

    PmxDocument incomingDoc;
    PmxVertex iv0, iv1, iv2;
    iv0.position = {0.0f, 0.0f, 2.0f};
    iv1.position = {1.0f, 0.0f, 2.0f};
    iv2.position = {0.0f, 1.0f, 2.0f};
    incomingDoc.vertices = {iv0, iv1, iv2};
    incomingDoc.faceIndices = {0, 1, 2};
    incomingDoc.textures = {QStringLiteral("tex/shared.png"), QStringLiteral("tex/acc_only.png")};

    PmxMaterial im0, im1;
    im0.nameJp = QStringLiteral("AccMat0");
    im0.textureIndex = 0; // points to tex/shared.png
    im0.sphereTextureIndex = -1;
    im0.faceVertexCount = 3;

    im1.nameJp = QStringLiteral("AccMat1");
    im1.textureIndex = 1; // points to tex/acc_only.png
    im1.sphereTextureIndex = -1;
    im1.faceVertexCount = 0;
    incomingDoc.materials = {im0, im1};

    PmxBone ib0;
    ib0.nameJp = QStringLiteral("AccBone");
    incomingDoc.bones = {ib0};

    MergeOptions options;
    options.offsetPosition = {5.0f, 10.0f, 15.0f};

    QString errorMsg;
    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options, &errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));

    // Verify vertex counts & offset
    QCOMPARE(baseDoc.vertices.size(), 6u);
    QCOMPARE(baseDoc.vertices[0].position.x, 0.0f);
    QCOMPARE(baseDoc.vertices[0].position.y, 0.0f);
    QCOMPARE(baseDoc.vertices[0].position.z, 0.0f);
    QCOMPARE(baseDoc.vertices[3].position.x, 5.0f);
    QCOMPARE(baseDoc.vertices[3].position.y, 10.0f);
    QCOMPARE(baseDoc.vertices[3].position.z, 17.0f);

    // Verify face index offsets
    QCOMPARE(baseDoc.faceIndices.size(), 6u);
    QCOMPARE(baseDoc.faceIndices[0], 0);
    QCOMPARE(baseDoc.faceIndices[1], 1);
    QCOMPARE(baseDoc.faceIndices[2], 2);
    QCOMPARE(baseDoc.faceIndices[3], 3); // 0 + 3
    QCOMPARE(baseDoc.faceIndices[4], 4); // 1 + 3
    QCOMPARE(baseDoc.faceIndices[5], 5); // 2 + 3

    // Verify texture deduplication: shared.png must be reused, acc_only.png appended
    QCOMPARE(baseDoc.textures.size(), 3u);
    QCOMPARE(baseDoc.textures[0], QStringLiteral("tex/shared.png"));
    QCOMPARE(baseDoc.textures[1], QStringLiteral("tex/base_only.png"));
    QCOMPARE(baseDoc.textures[2], QStringLiteral("tex/acc_only.png"));

    // Verify material additions and remapped texture indices
    QCOMPARE(baseDoc.materials.size(), 3u);
    QCOMPARE(baseDoc.materials[0].nameJp, QStringLiteral("BaseMat"));
    QCOMPARE(baseDoc.materials[0].textureIndex, 0);
    QCOMPARE(baseDoc.materials[1].nameJp, QStringLiteral("AccMat0"));
    QCOMPARE(baseDoc.materials[1].textureIndex, 0); // remapped to base index 0
    QCOMPARE(baseDoc.materials[2].nameJp, QStringLiteral("AccMat1"));
    QCOMPARE(baseDoc.materials[2].textureIndex, 2); // remapped to base index 2
}

void TstPmxMerger::testStandardBoneDeduplication() {
    PmxDocument baseDoc;
    PmxBone bRoot;
    bRoot.nameJp = QStringLiteral("全ての親");
    bRoot.nameEn = QStringLiteral("Root");
    bRoot.position = {0.0f, 0.0f, 0.0f};
    bRoot.parentBoneIndex = -1;

    PmxBone bCenter;
    bCenter.nameJp = QStringLiteral("センター");
    bCenter.nameEn = QStringLiteral("Center");
    bCenter.position = {0.0f, 10.0f, 0.0f};
    bCenter.parentBoneIndex = 0;

    baseDoc.bones = {bRoot, bCenter};

    PmxVertex bv;
    bv.weightType = WeightType::BDEF1;
    bv.boneIndices[0] = 1;
    baseDoc.vertices = {bv};

    PmxDocument incomingDoc;
    PmxBone iRoot;
    iRoot.nameJp = QStringLiteral("全ての親");
    iRoot.nameEn = QStringLiteral("Root");
    iRoot.position = {0.0f, 0.0f, 0.0f};
    iRoot.parentBoneIndex = -1;

    PmxBone iCenter;
    iCenter.nameJp = QStringLiteral("センター");
    iCenter.nameEn = QStringLiteral("Center");
    iCenter.position = {0.0f, 10.0f, 0.0f};
    iCenter.parentBoneIndex = 0;

    PmxBone iAccRoot;
    iAccRoot.nameJp = QStringLiteral("AccessoryRoot");
    iAccRoot.nameEn = QStringLiteral("AccRoot");
    iAccRoot.position = {0.0f, 12.0f, 0.0f};
    iAccRoot.parentBoneIndex = 1; // parented to センター!

    PmxBone iAccTip;
    iAccTip.nameJp = QStringLiteral("AccessoryTip");
    iAccTip.nameEn = QStringLiteral("AccTip");
    iAccTip.position = {0.0f, 14.0f, 0.0f};
    iAccTip.parentBoneIndex = 2; // parented to AccessoryRoot
    iAccTip.tailBoneIndex = 2;

    incomingDoc.bones = {iRoot, iCenter, iAccRoot, iAccTip};

    PmxVertex iv0, iv1, iv2;
    iv0.weightType = WeightType::BDEF1;
    iv0.boneIndices[0] = 2; // weighted to AccessoryRoot

    iv1.weightType = WeightType::BDEF2;
    iv1.boneIndices[0] = 1; // センター
    iv1.boneIndices[1] = 2; // AccessoryRoot
    iv1.boneWeights[0] = 0.7f;
    iv1.boneWeights[1] = 0.3f;

    iv2.weightType = WeightType::SDEF;
    iv2.boneIndices[0] = 0; // 全ての親
    iv2.boneIndices[1] = 2; // AccessoryRoot
    iv2.sdefC = {0.0f, 11.0f, 0.0f};

    incomingDoc.vertices = {iv0, iv1, iv2};

    MergeOptions options;
    options.mergeIdenticalBones = true;
    options.offsetPosition = {0.0f, 5.0f, 0.0f};

    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    // Verify 全ての親 and センター are NOT duplicated
    QCOMPARE(baseDoc.bones.size(), 4u);
    QCOMPARE(baseDoc.bones[0].nameJp, QStringLiteral("全ての親"));
    QCOMPARE(baseDoc.bones[0].position.y, 0.0f); // base bone not offset
    QCOMPARE(baseDoc.bones[1].nameJp, QStringLiteral("センター"));
    QCOMPARE(baseDoc.bones[1].position.y, 10.0f);

    // Verify AccessoryRoot is appended and parented to base センター (index 1)
    QCOMPARE(baseDoc.bones[2].nameJp, QStringLiteral("AccessoryRoot"));
    QCOMPARE(baseDoc.bones[2].position.y, 17.0f); // 12 + 5 offset
    QCOMPARE(baseDoc.bones[2].parentBoneIndex, 1);

    // Verify AccessoryTip is appended, parented to AccessoryRoot (index 2), tail remapped to 2
    QCOMPARE(baseDoc.bones[3].nameJp, QStringLiteral("AccessoryTip"));
    QCOMPARE(baseDoc.bones[3].position.y, 19.0f); // 14 + 5 offset
    QCOMPARE(baseDoc.bones[3].parentBoneIndex, 2);
    QCOMPARE(baseDoc.bones[3].tailBoneIndex, 2);

    // Verify vertex weights map to correct bones
    QCOMPARE(baseDoc.vertices.size(), 4u);
    // iv0: BDEF1 -> AccessoryRoot (index 2)
    QCOMPARE(baseDoc.vertices[1].boneIndices[0], 2);

    // iv1: BDEF2 -> センター (index 1) and AccessoryRoot (index 2)
    QCOMPARE(baseDoc.vertices[2].boneIndices[0], 1);
    QCOMPARE(baseDoc.vertices[2].boneIndices[1], 2);

    // iv2: SDEF -> 全ての親 (index 0) and AccessoryRoot (index 2)
    QCOMPARE(baseDoc.vertices[3].boneIndices[0], 0);
    QCOMPARE(baseDoc.vertices[3].boneIndices[1], 2);
    QCOMPARE(baseDoc.vertices[3].sdefC.y, 16.0f); // 11 + 5 offset
}

void TstPmxMerger::testPhysicsRemapping() {
    PmxDocument baseDoc;
    PmxBone bb0, bb1;
    bb0.nameJp = QStringLiteral("Base0");
    bb1.nameJp = QStringLiteral("センター");
    baseDoc.bones = {bb0, bb1};

    PmxRigidBody brb0, brb1;
    brb0.nameJp = QStringLiteral("BaseRB0");
    brb0.boneIndex = 0;
    brb0.position = {0.0f, 0.0f, 0.0f};

    brb1.nameJp = QStringLiteral("BaseRB1");
    brb1.boneIndex = 1;
    brb1.position = {0.0f, 5.0f, 0.0f};
    baseDoc.rigidBodies = {brb0, brb1};

    PmxJoint bj0;
    bj0.nameJp = QStringLiteral("BaseJoint0");
    bj0.rigidBodyIndexA = 0;
    bj0.rigidBodyIndexB = 1;
    bj0.position = {0.0f, 2.5f, 0.0f};
    baseDoc.joints = {bj0};

    PmxDocument incomingDoc;
    PmxBone ib0, ib1;
    ib0.nameJp = QStringLiteral("センター"); // matches baseDoc bone 1
    ib1.nameJp = QStringLiteral("Ribbon");   // new bone
    incomingDoc.bones = {ib0, ib1};

    PmxRigidBody irb0, irb1;
    irb0.nameJp = QStringLiteral("IncomingRB0");
    irb0.boneIndex = 0; // matches センター
    irb0.position = {0.0f, 10.0f, 0.0f};

    irb1.nameJp = QStringLiteral("IncomingRB1");
    irb1.boneIndex = 1; // Ribbon
    irb1.position = {0.0f, 12.0f, 0.0f};
    incomingDoc.rigidBodies = {irb0, irb1};

    PmxJoint ij0;
    ij0.nameJp = QStringLiteral("IncomingJoint0");
    ij0.rigidBodyIndexA = 0;
    ij0.rigidBodyIndexB = 1;
    ij0.position = {0.0f, 11.0f, 0.0f};
    incomingDoc.joints = {ij0};

    MergeOptions options;
    options.mergeIdenticalBones = true;
    options.importPhysics = true;
    options.offsetPosition = {1.0f, 2.0f, 3.0f};

    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    // Verify rigid bodies
    QCOMPARE(baseDoc.rigidBodies.size(), 4u);
    // Base rigid bodies untouched
    QCOMPARE(baseDoc.rigidBodies[0].nameJp, QStringLiteral("BaseRB0"));
    QCOMPARE(baseDoc.rigidBodies[0].boneIndex, 0);
    QCOMPARE(baseDoc.rigidBodies[1].nameJp, QStringLiteral("BaseRB1"));
    QCOMPARE(baseDoc.rigidBodies[1].boneIndex, 1);

    // IncomingRB0: boneIndex remapped to base センター (index 1), position offset
    QCOMPARE(baseDoc.rigidBodies[2].nameJp, QStringLiteral("IncomingRB0"));
    QCOMPARE(baseDoc.rigidBodies[2].boneIndex, 1);
    QCOMPARE(baseDoc.rigidBodies[2].position.x, 1.0f);
    QCOMPARE(baseDoc.rigidBodies[2].position.y, 12.0f);
    QCOMPARE(baseDoc.rigidBodies[2].position.z, 3.0f);

    // IncomingRB1: boneIndex remapped to Ribbon (index 2), position offset
    QCOMPARE(baseDoc.rigidBodies[3].nameJp, QStringLiteral("IncomingRB1"));
    QCOMPARE(baseDoc.rigidBodies[3].boneIndex, 2);
    QCOMPARE(baseDoc.rigidBodies[3].position.x, 1.0f);
    QCOMPARE(baseDoc.rigidBodies[3].position.y, 14.0f);
    QCOMPARE(baseDoc.rigidBodies[3].position.z, 3.0f);

    // Verify joints: incoming joint connected to rigid bodies A and B offset by baseRigidBodyCount (2)
    QCOMPARE(baseDoc.joints.size(), 2u);
    QCOMPARE(baseDoc.joints[0].nameJp, QStringLiteral("BaseJoint0"));
    QCOMPARE(baseDoc.joints[1].nameJp, QStringLiteral("IncomingJoint0"));
    QCOMPARE(baseDoc.joints[1].rigidBodyIndexA, 2); // 0 + 2
    QCOMPARE(baseDoc.joints[1].rigidBodyIndexB, 3); // 1 + 2
    QCOMPARE(baseDoc.joints[1].position.x, 1.0f);
    QCOMPARE(baseDoc.joints[1].position.y, 13.0f);
    QCOMPARE(baseDoc.joints[1].position.z, 3.0f);
}

void TstPmxMerger::testMorphRemapping() {
    PmxDocument baseDoc;
    baseDoc.vertices.resize(4);
    PmxBone bb0, bb1;
    bb0.nameJp = QStringLiteral("Base0");
    bb1.nameJp = QStringLiteral("Base1");
    baseDoc.bones = {bb0, bb1};
    PmxMaterial bm0;
    bm0.nameJp = QStringLiteral("BaseMat");
    baseDoc.materials = {bm0};

    PmxMorph baseMorph;
    baseMorph.nameJp = QStringLiteral("BaseSmile");
    baseMorph.vertexOffsets.push_back({0, {0.0f, 1.0f, 0.0f}});
    baseDoc.morphs = {baseMorph};

    PmxDocument incomingDoc;
    incomingDoc.vertices.resize(3);
    PmxBone ib0;
    ib0.nameJp = QStringLiteral("AccBone");
    incomingDoc.bones = {ib0};
    PmxMaterial im0;
    im0.nameJp = QStringLiteral("AccMat");
    incomingDoc.materials = {im0};

    PmxMorph imMorph0;
    imMorph0.nameJp = QStringLiteral("AccVertexMorph");
    imMorph0.vertexOffsets.push_back({1, {0.0f, 0.5f, 0.0f}});

    PmxMorph imMorph1;
    imMorph1.nameJp = QStringLiteral("AccBoneMorph");
    imMorph1.boneOffsets.push_back({0, {0.0f, 1.0f, 0.0f}, glm::quat(1.0f, 0.0f, 0.0f, 0.0f)});

    PmxMorph imMorph2;
    imMorph2.nameJp = QStringLiteral("AccMatMorph");
    MaterialMorphOffset allMats;
    allMats.materialIndex = -1; // all materials
    MaterialMorphOffset singleMat;
    singleMat.materialIndex = 0; // single incoming mat
    imMorph2.materialOffsets = {allMats, singleMat};

    PmxMorph imMorph3;
    imMorph3.nameJp = QStringLiteral("AccGroupMorph");
    imMorph3.groupOffsets.push_back({0, 1.0f}); // points to incoming morph 0

    incomingDoc.morphs = {imMorph0, imMorph1, imMorph2, imMorph3};

    MergeOptions options;
    options.importMorphs = true;

    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    QCOMPARE(baseDoc.morphs.size(), 5u);

    // Base morph untouched
    QCOMPARE(baseDoc.morphs[0].nameJp, QStringLiteral("BaseSmile"));
    QCOMPARE(baseDoc.morphs[0].vertexOffsets[0].vertexIndex, 0);

    // AccVertexMorph: vertexIndex offset by baseVertexCount (4) -> 1 + 4 = 5
    QCOMPARE(baseDoc.morphs[1].nameJp, QStringLiteral("AccVertexMorph"));
    QCOMPARE(baseDoc.morphs[1].vertexOffsets[0].vertexIndex, 5);

    // AccBoneMorph: boneIndex remapped to new bone (index 2)
    QCOMPARE(baseDoc.morphs[2].nameJp, QStringLiteral("AccBoneMorph"));
    QCOMPARE(baseDoc.morphs[2].boneOffsets[0].boneIndex, 2);

    // AccMatMorph: materialIndex -1 remains -1, materialIndex 0 becomes baseMaterialCount (1) + 0 = 1
    QCOMPARE(baseDoc.morphs[3].nameJp, QStringLiteral("AccMatMorph"));
    QCOMPARE(baseDoc.morphs[3].materialOffsets[0].materialIndex, -1);
    QCOMPARE(baseDoc.morphs[3].materialOffsets[1].materialIndex, 1);

    // AccGroupMorph: morphIndex 0 becomes baseMorphCount (1) + 0 = 1
    QCOMPARE(baseDoc.morphs[4].nameJp, QStringLiteral("AccGroupMorph"));
    QCOMPARE(baseDoc.morphs[4].groupOffsets[0].morphIndex, 1);
}

void TstPmxMerger::testDisplayFrameRemapping() {
    PmxDocument baseDoc;
    baseDoc.bones.resize(2);
    baseDoc.bones[0].nameJp = QStringLiteral("RootBone");
    baseDoc.bones[1].nameJp = QStringLiteral("センター");
    baseDoc.morphs.resize(1);

    PmxDisplayFrame rootFrame;
    rootFrame.nameJp = QStringLiteral("Root");
    rootFrame.elements.push_back({0, 0}); // Bone 0
    baseDoc.displayFrames = {rootFrame};

    PmxDocument incomingDoc;
    incomingDoc.bones.resize(2);
    incomingDoc.bones[0].nameJp = QStringLiteral("RootBone");      // matches base bone 0
    incomingDoc.bones[1].nameJp = QStringLiteral("AccessoryBone"); // new bone
    incomingDoc.morphs.resize(1);

    PmxDisplayFrame incRootFrame;
    incRootFrame.nameJp = QStringLiteral("Root");
    incRootFrame.elements.push_back({0, 0}); // Bone 0 (already in frame, should deduplicate)
    incRootFrame.elements.push_back({0, 1}); // Bone 1 (new)

    PmxDisplayFrame incAccFrame;
    incAccFrame.nameJp = QStringLiteral("AccessoryFrame");
    incAccFrame.elements.push_back({1, 0}); // Morph 0

    incomingDoc.displayFrames = {incRootFrame, incAccFrame};

    MergeOptions options;
    options.importDisplayFrames = true;

    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    // Root frame merged into existing frame, AccessoryFrame appended
    QCOMPARE(baseDoc.displayFrames.size(), 2u);
    QCOMPARE(baseDoc.displayFrames[0].nameJp, QStringLiteral("Root"));
    // Initial element was {0, 0}. Re-mapped incoming has {0, 2} (new bone). Bone 0 wasn't remapped to 0.
    // Total elements in Root frame: 2 (no duplicates)
    QCOMPARE(baseDoc.displayFrames[0].elements.size(), 2u);

    QCOMPARE(baseDoc.displayFrames[1].nameJp, QStringLiteral("AccessoryFrame"));
    QCOMPARE(baseDoc.displayFrames[1].elements.size(), 1u);
    QCOMPARE(baseDoc.displayFrames[1].elements[0].targetType, 1);
    QCOMPARE(baseDoc.displayFrames[1].elements[0].index, 1); // morphIndex 0 + baseMorphCount 1 = 1
}

void TstPmxMerger::testHeaderIndexSizeUpgrade() {
    PmxDocument baseDoc;
    baseDoc.header.vertexIndexSize = 1;
    baseDoc.vertices.resize(200);

    PmxDocument incomingDoc;
    incomingDoc.vertices.resize(100);

    MergeOptions options;
    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    // Total vertices = 300 (> 255), so vertexIndexSize must be upgraded to 2
    QCOMPARE(baseDoc.vertices.size(), 300u);
    QCOMPARE(baseDoc.header.vertexIndexSize, 2);
}

void TstPmxMerger::testControllerMergeUndoRedo() {
    PmxDocumentController controller;

    // Create a base document and save to temporary file
    PmxDocument baseDoc;
    baseDoc.header.vertexIndexSize = 1;
    PmxVertex bv0, bv1;
    bv0.position = {0.0f, 0.0f, 0.0f};
    bv1.position = {1.0f, 0.0f, 0.0f};
    baseDoc.vertices = {bv0, bv1};
    baseDoc.faceIndices = {0, 1, 0};
    PmxMaterial bm;
    bm.nameJp = QStringLiteral("BaseMat");
    bm.faceVertexCount = 3;
    baseDoc.materials = {bm};
    PmxBone bb;
    bb.nameJp = QStringLiteral("BaseBone");
    baseDoc.bones = {bb};
    PmxMorph bMorph;
    bMorph.nameJp = QStringLiteral("BaseMorph");
    baseDoc.morphs = {bMorph};
    PmxDisplayFrame bFrame;
    bFrame.nameJp = QStringLiteral("BaseFrame");
    baseDoc.displayFrames = {bFrame};
    PmxRigidBody bRb;
    bRb.nameJp = QStringLiteral("BaseRB");
    baseDoc.rigidBodies = {bRb};
    PmxJoint bJoint;
    bJoint.nameJp = QStringLiteral("BaseJoint");
    baseDoc.joints = {bJoint};

    QString baseFilePath = m_tempDir.filePath(QStringLiteral("base_test.pmx"));
    QVERIFY(PmxWriter::writeToFile(baseFilePath, baseDoc));
    QVERIFY(controller.loadFromFile(baseFilePath));

    QCOMPARE(controller.isModified(), false);
    QCOMPARE(controller.document().vertices.size(), 2u);
    QCOMPARE(controller.document().materials.size(), 1u);
    QCOMPARE(controller.document().bones.size(), 1u);
    QCOMPARE(controller.document().morphs.size(), 1u);
    QCOMPARE(controller.document().displayFrames.size(), 1u);
    QCOMPARE(controller.document().rigidBodies.size(), 1u);
    QCOMPARE(controller.document().joints.size(), 1u);

    QCOMPARE(controller.vertexModel()->rowCount(), 2);
    QCOMPARE(controller.materialModel()->rowCount(), 1);
    QCOMPARE(controller.boneModel()->rowCount(), 1);
    QCOMPARE(controller.morphModel()->rowCount(), 1);
    QCOMPARE(controller.displayFrameModel()->rowCount(), 1);
    QCOMPARE(controller.rigidBodyModel()->rowCount(), 1);
    QCOMPARE(controller.jointModel()->rowCount(), 1);

    // Incoming model
    PmxDocument incomingDoc;
    PmxVertex iv0, iv1, iv2;
    iv0.position = {0.0f, 5.0f, 0.0f};
    iv1.position = {1.0f, 5.0f, 0.0f};
    iv2.position = {0.0f, 6.0f, 0.0f};
    incomingDoc.vertices = {iv0, iv1, iv2};
    incomingDoc.faceIndices = {0, 1, 2};
    PmxMaterial im;
    im.nameJp = QStringLiteral("AccMat");
    im.faceVertexCount = 3;
    incomingDoc.materials = {im};
    PmxBone ib;
    ib.nameJp = QStringLiteral("AccBone");
    incomingDoc.bones = {ib};
    PmxMorph iMorph;
    iMorph.nameJp = QStringLiteral("AccMorph");
    incomingDoc.morphs = {iMorph};
    PmxDisplayFrame iFrame;
    iFrame.nameJp = QStringLiteral("AccFrame");
    incomingDoc.displayFrames = {iFrame};
    PmxRigidBody iRb;
    iRb.nameJp = QStringLiteral("AccRB");
    incomingDoc.rigidBodies = {iRb};
    PmxJoint iJoint;
    iJoint.nameJp = QStringLiteral("AccJoint");
    incomingDoc.joints = {iJoint};

    MergeOptions options;
    QString errorMsg;
    bool ok = controller.mergeModel(incomingDoc, options, &errorMsg);
    QVERIFY2(ok, qPrintable(errorMsg));

    // After merge: all 7 models must show merged counts
    QCOMPARE(controller.isModified(), true);
    QCOMPARE(controller.document().vertices.size(), 5u);
    QCOMPARE(controller.document().materials.size(), 2u);
    QCOMPARE(controller.document().bones.size(), 2u);
    QCOMPARE(controller.document().morphs.size(), 2u);
    QCOMPARE(controller.document().displayFrames.size(), 2u);
    QCOMPARE(controller.document().rigidBodies.size(), 2u);
    QCOMPARE(controller.document().joints.size(), 2u);

    QCOMPARE(controller.vertexModel()->rowCount(), 5);
    QCOMPARE(controller.materialModel()->rowCount(), 2);
    QCOMPARE(controller.boneModel()->rowCount(), 2);
    QCOMPARE(controller.morphModel()->rowCount(), 2);
    QCOMPARE(controller.displayFrameModel()->rowCount(), 2);
    QCOMPARE(controller.rigidBodyModel()->rowCount(), 2);
    QCOMPARE(controller.jointModel()->rowCount(), 2);

    // Undo: should revert to exact initial state across all 7 models
    controller.undo();
    QCOMPARE(controller.isModified(), false);
    QCOMPARE(controller.document().vertices.size(), 2u);
    QCOMPARE(controller.document().materials.size(), 1u);
    QCOMPARE(controller.document().bones.size(), 1u);
    QCOMPARE(controller.document().morphs.size(), 1u);
    QCOMPARE(controller.document().displayFrames.size(), 1u);
    QCOMPARE(controller.document().rigidBodies.size(), 1u);
    QCOMPARE(controller.document().joints.size(), 1u);

    QCOMPARE(controller.vertexModel()->rowCount(), 2);
    QCOMPARE(controller.materialModel()->rowCount(), 1);
    QCOMPARE(controller.boneModel()->rowCount(), 1);
    QCOMPARE(controller.morphModel()->rowCount(), 1);
    QCOMPARE(controller.displayFrameModel()->rowCount(), 1);
    QCOMPARE(controller.rigidBodyModel()->rowCount(), 1);
    QCOMPARE(controller.jointModel()->rowCount(), 1);

    // Redo: should re-apply the merge cleanly across all 7 models
    controller.redo();
    QCOMPARE(controller.isModified(), true);
    QCOMPARE(controller.document().vertices.size(), 5u);
    QCOMPARE(controller.document().materials.size(), 2u);
    QCOMPARE(controller.document().bones.size(), 2u);
    QCOMPARE(controller.document().morphs.size(), 2u);
    QCOMPARE(controller.document().displayFrames.size(), 2u);
    QCOMPARE(controller.document().rigidBodies.size(), 2u);
    QCOMPARE(controller.document().joints.size(), 2u);

    QCOMPARE(controller.vertexModel()->rowCount(), 5);
    QCOMPARE(controller.materialModel()->rowCount(), 2);
    QCOMPARE(controller.boneModel()->rowCount(), 2);
    QCOMPARE(controller.morphModel()->rowCount(), 2);
    QCOMPARE(controller.displayFrameModel()->rowCount(), 2);
    QCOMPARE(controller.rigidBodyModel()->rowCount(), 2);
    QCOMPARE(controller.jointModel()->rowCount(), 2);
}

void TstPmxMerger::testBdef4AndQdefRemapping() {
    PmxDocument baseDoc;
    PmxBone b0, b1;
    b0.nameJp = QStringLiteral("Base0");
    b1.nameJp = QStringLiteral("Base1");
    baseDoc.bones = {b0, b1};

    PmxDocument incomingDoc;
    PmxBone ib0, ib1, ib2;
    ib0.nameJp = QStringLiteral("Base1"); // matches base bone 1
    ib1.nameJp = QStringLiteral("Acc0");
    ib2.nameJp = QStringLiteral("Acc1");
    incomingDoc.bones = {ib0, ib1, ib2};

    PmxVertex vBdef4;
    vBdef4.weightType = WeightType::BDEF4;
    vBdef4.boneIndices = {0, 1, 2, -1};
    vBdef4.boneWeights = {0.4f, 0.3f, 0.3f, 0.0f};

    PmxVertex vQdef;
    vQdef.weightType = WeightType::QDEF;
    vQdef.boneIndices = {1, 2, 0, -1};
    vQdef.boneWeights = {0.25f, 0.25f, 0.5f, 0.0f};

    incomingDoc.vertices = {vBdef4, vQdef};

    MergeOptions options;
    options.mergeIdenticalBones = true;
    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    QCOMPARE(baseDoc.bones.size(), 4u);
    QCOMPARE(baseDoc.vertices.size(), 2u);

    // Vertex 0 (BDEF4):
    // ib0 was Base1 -> remapped to base bone 1
    // ib1 was Acc0  -> remapped to appended bone 2
    // ib2 was Acc1  -> remapped to appended bone 3
    // boneIndices[3] was -1 -> preserved as -1
    QCOMPARE(baseDoc.vertices[0].weightType, WeightType::BDEF4);
    QCOMPARE(baseDoc.vertices[0].boneIndices[0], 1);
    QCOMPARE(baseDoc.vertices[0].boneIndices[1], 2);
    QCOMPARE(baseDoc.vertices[0].boneIndices[2], 3);
    QCOMPARE(baseDoc.vertices[0].boneIndices[3], -1);
    QCOMPARE(baseDoc.vertices[0].boneWeights[0], 0.4f);

    // Vertex 1 (QDEF):
    // ib1 -> 2, ib2 -> 3, ib0 -> 1, -1 -> -1
    QCOMPARE(baseDoc.vertices[1].weightType, WeightType::QDEF);
    QCOMPARE(baseDoc.vertices[1].boneIndices[0], 2);
    QCOMPARE(baseDoc.vertices[1].boneIndices[1], 3);
    QCOMPARE(baseDoc.vertices[1].boneIndices[2], 1);
    QCOMPARE(baseDoc.vertices[1].boneIndices[3], -1);

    // Header version upgraded to 2.1f for QDEF
    QVERIFY(baseDoc.header.version >= 2.1f);
}

void TstPmxMerger::testInheritAndIkLinkRemapping() {
    PmxDocument baseDoc;
    PmxBone bParent, bChild;
    bParent.nameJp = QStringLiteral("ParentBone");
    bChild.nameJp = QStringLiteral("ChildBone");
    baseDoc.bones = {bParent, bChild};

    PmxDocument incomingDoc;
    PmxBone ibChild;
    ibChild.nameJp = QStringLiteral("ChildBone"); // matches base bone 1

    PmxBone ibHelper;
    ibHelper.nameJp = QStringLiteral("AccHelper");
    ibHelper.boneFlags |= 0x0100; // InheritRotation
    ibHelper.inheritParentIndex = 0; // points to incoming ChildBone
    ibHelper.inheritWeight = 0.5f;

    PmxBone ibTarget;
    ibTarget.nameJp = QStringLiteral("AccTarget");

    PmxBone ibIk;
    ibIk.nameJp = QStringLiteral("AccIk");
    ibIk.boneFlags |= 0x0020; // IK
    ibIk.ikTargetBoneIndex = 2; // points to AccTarget
    PmxIkLink link0, link1;
    link0.boneIndex = 0; // points to ChildBone (matches base 1)
    link1.boneIndex = 1; // points to AccHelper (appended as bone 2)
    ibIk.ikLinks = {link0, link1};

    PmxBone ibUnconnected;
    ibUnconnected.nameJp = QStringLiteral("AccUnconnected");
    ibUnconnected.parentBoneIndex = -1;
    ibUnconnected.inheritParentIndex = -1;
    ibUnconnected.ikTargetBoneIndex = -1;

    incomingDoc.bones = {ibChild, ibHelper, ibTarget, ibIk, ibUnconnected};

    MergeOptions options;
    options.mergeIdenticalBones = true;
    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    // Total bones = 2 (base) + 4 (incoming ibHelper, ibTarget, ibIk, ibUnconnected) = 6
    QCOMPARE(baseDoc.bones.size(), 6u);

    // AccHelper is at index 2. inheritParentIndex was 0 (ChildBone), remapped to base bone 1
    QCOMPARE(baseDoc.bones[2].nameJp, QStringLiteral("AccHelper"));
    QCOMPARE(baseDoc.bones[2].inheritParentIndex, 1);

    // AccTarget is at index 3
    QCOMPARE(baseDoc.bones[3].nameJp, QStringLiteral("AccTarget"));

    // AccIk is at index 4. ikTargetBoneIndex was 2 (AccTarget), remapped to 3
    QCOMPARE(baseDoc.bones[4].nameJp, QStringLiteral("AccIk"));
    QCOMPARE(baseDoc.bones[4].ikTargetBoneIndex, 3);
    QCOMPARE(baseDoc.bones[4].ikLinks.size(), 2u);
    QCOMPARE(baseDoc.bones[4].ikLinks[0].boneIndex, 1); // ChildBone -> base bone 1
    QCOMPARE(baseDoc.bones[4].ikLinks[1].boneIndex, 2); // AccHelper -> base bone 2

    // AccUnconnected is at index 5. Null indices preserved
    QCOMPARE(baseDoc.bones[5].nameJp, QStringLiteral("AccUnconnected"));
    QCOMPARE(baseDoc.bones[5].parentBoneIndex, -1);
    QCOMPARE(baseDoc.bones[5].inheritParentIndex, -1);
    QCOMPARE(baseDoc.bones[5].ikTargetBoneIndex, -1);
}

void TstPmxMerger::testSoftBodyRemapping() {
    PmxDocument baseDoc;
    baseDoc.header.version = 2.0f;
    baseDoc.vertices.resize(2);
    baseDoc.materials.resize(1);
    baseDoc.rigidBodies.resize(2);

    PmxDocument incomingDoc;
    incomingDoc.header.version = 2.1f;
    incomingDoc.vertices.resize(3);
    incomingDoc.materials.resize(1);
    incomingDoc.rigidBodies.resize(2);

    PmxSoftBody sb;
    sb.nameJp = QStringLiteral("AccSoftBody");
    sb.materialIndex = 0;
    PmxSoftBodyAnchor a0{0, 1, 0}; // rb 0, vertex 1
    PmxSoftBodyAnchor a1{1, 2, 0}; // rb 1, vertex 2
    sb.anchors = {a0, a1};
    sb.pinVertexIndices = {0, 2};
    incomingDoc.softBodies = {sb};

    MergeOptions options;
    options.importPhysics = true;
    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    QCOMPARE(baseDoc.softBodies.size(), 1u);
    const auto& mergedSb = baseDoc.softBodies[0];
    QCOMPARE(mergedSb.nameJp, QStringLiteral("AccSoftBody"));
    // materialIndex 0 + baseMaterialCount (1) = 1
    QCOMPARE(mergedSb.materialIndex, 1);
    // Anchor 0: rb 0 + baseRigidBodyCount (2) = 2, v 1 + baseVertexCount (2) = 3
    QCOMPARE(mergedSb.anchors[0].rigidBodyIndex, 2);
    QCOMPARE(mergedSb.anchors[0].vertexIndex, 3);
    // Anchor 1: rb 1 + 2 = 3, v 2 + 2 = 4
    QCOMPARE(mergedSb.anchors[1].rigidBodyIndex, 3);
    QCOMPARE(mergedSb.anchors[1].vertexIndex, 4);
    // Pin vertices: 0 + 2 = 2, 2 + 2 = 4
    QCOMPARE(mergedSb.pinVertexIndices.size(), 2u);
    QCOMPARE(mergedSb.pinVertexIndices[0], 2);
    QCOMPARE(mergedSb.pinVertexIndices[1], 4);
    // Header version upgraded to 2.1f
    QVERIFY(baseDoc.header.version >= 2.1f);
}

void TstPmxMerger::testUvAndImpulseMorphRemapping() {
    PmxDocument baseDoc;
    baseDoc.vertices.resize(2);
    baseDoc.rigidBodies.resize(1);

    PmxDocument incomingDoc;
    incomingDoc.vertices.resize(3);
    incomingDoc.rigidBodies.resize(2);

    PmxMorph uvMorph;
    uvMorph.nameJp = QStringLiteral("AccUV");
    uvMorph.morphType = 3; // UV
    uvMorph.uvOffsets.push_back({1, glm::vec4(0.1f, 0.2f, 0.0f, 0.0f)});

    PmxMorph impulseMorph;
    impulseMorph.nameJp = QStringLiteral("AccImpulse");
    impulseMorph.morphType = 10; // Impulse
    impulseMorph.impulseOffsets.push_back({1, 0, glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)});

    incomingDoc.morphs = {uvMorph, impulseMorph};

    MergeOptions options;
    options.importPhysics = true;
    options.importMorphs = true;

    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    QCOMPARE(baseDoc.morphs.size(), 2u);
    // UV morph: vertexIndex 1 + baseVertexCount (2) = 3
    QCOMPARE(baseDoc.morphs[0].uvOffsets[0].vertexIndex, 3);
    // Impulse morph: rigidBodyIndex 1 + baseRigidBodyCount (1) = 2
    QCOMPARE(baseDoc.morphs[1].impulseOffsets[0].rigidBodyIndex, 2);
    // PMX 2.1 version upgrade
    QVERIFY(baseDoc.header.version >= 2.1f);

    // Now test with importPhysics = false: impulse rigidBodyIndex should be -1
    PmxDocument baseDocNoPhys;
    baseDocNoPhys.vertices.resize(2);
    baseDocNoPhys.rigidBodies.resize(1);

    MergeOptions optionsNoPhys;
    optionsNoPhys.importPhysics = false;
    optionsNoPhys.importMorphs = true;

    ok = PmxMerger::merge(baseDocNoPhys, incomingDoc, optionsNoPhys);
    QVERIFY(ok);
    QCOMPARE(baseDocNoPhys.morphs.size(), 2u);
    QCOMPARE(baseDocNoPhys.morphs[1].impulseOffsets[0].rigidBodyIndex, -1);
}

void TstPmxMerger::testBoneNameCollisionEdgeCase() {
    PmxDocument baseDoc;
    PmxBone bLeftArm;
    bLeftArm.nameJp = QStringLiteral("左腕");
    bLeftArm.nameEn = QStringLiteral("Arm");
    bLeftArm.position = {-2.0f, 10.0f, 0.0f};

    PmxBone bRightArm;
    bRightArm.nameJp = QStringLiteral("右腕");
    bRightArm.nameEn = QStringLiteral("Arm");
    bRightArm.position = {2.0f, 10.0f, 0.0f};

    PmxBone bWesternRoot;
    bWesternRoot.nameJp = QString();
    bWesternRoot.nameEn = QStringLiteral("WesternRoot");

    baseDoc.bones = {bLeftArm, bRightArm, bWesternRoot};

    PmxDocument incomingDoc;
    PmxBone iRightArm;
    iRightArm.nameJp = QStringLiteral("右腕");
    iRightArm.nameEn = QStringLiteral("Arm"); // same English name as both left and right!
    iRightArm.position = {2.0f, 10.0f, 0.0f};

    PmxBone iWesternRoot;
    iWesternRoot.nameJp = QString();
    iWesternRoot.nameEn = QStringLiteral("WesternRoot");

    PmxVertex iv;
    iv.weightType = WeightType::BDEF1;
    iv.boneIndices[0] = 0; // points to incoming 右腕

    incomingDoc.bones = {iRightArm, iWesternRoot};
    incomingDoc.vertices = {iv};

    MergeOptions options;
    options.mergeIdenticalBones = true;

    bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
    QVERIFY(ok);

    // Both bones should deduplicate cleanly:
    // "右腕" MUST match base bone 1 ("右腕"), NOT base bone 0 ("左腕")!
    // "WesternRoot" MUST match base bone 2 ("WesternRoot") by English name fallback!
    QCOMPARE(baseDoc.bones.size(), 3u);
    QCOMPARE(baseDoc.vertices.size(), 1u);
    QCOMPARE(baseDoc.vertices[0].boneIndices[0], 1); // MUST be index 1 (右腕)!
}

void TstPmxMerger::testBoundaryAndCornerCases() {
    // 1. Empty into empty
    {
        PmxDocument baseDoc;
        PmxDocument incomingDoc;
        MergeOptions options;
        QString errorMsg;
        bool ok = PmxMerger::merge(baseDoc, incomingDoc, options, &errorMsg);
        QVERIFY2(ok, qPrintable(errorMsg));
        QCOMPARE(baseDoc.vertices.size(), 0u);
        QCOMPARE(baseDoc.materials.size(), 0u);
        QCOMPARE(baseDoc.bones.size(), 0u);
    }

    // 2. Empty incoming into populated base
    {
        PmxDocument baseDoc;
        baseDoc.vertices.resize(5);
        baseDoc.bones.resize(2);
        PmxDocument incomingDoc;
        MergeOptions options;
        bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
        QVERIFY(ok);
        QCOMPARE(baseDoc.vertices.size(), 5u);
        QCOMPARE(baseDoc.bones.size(), 2u);
    }

    // 3. Populated incoming into empty base
    {
        PmxDocument baseDoc;
        PmxDocument incomingDoc;
        incomingDoc.vertices.resize(4);
        incomingDoc.bones.resize(3);
        MergeOptions options;
        bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
        QVERIFY(ok);
        QCOMPARE(baseDoc.vertices.size(), 4u);
        QCOMPARE(baseDoc.bones.size(), 3u);
    }

    // 4. Out-of-bounds / malformed indices
    {
        PmxDocument baseDoc;
        baseDoc.vertices.resize(1);
        baseDoc.bones.resize(1);

        PmxDocument incomingDoc;
        PmxVertex iv;
        iv.weightType = WeightType::BDEF1;
        iv.boneIndices[0] = 9999; // out-of-bounds bone index
        incomingDoc.vertices = {iv};

        PmxMaterial im;
        im.textureIndex = 9999;       // out-of-bounds texture
        im.sphereTextureIndex = 9999; // out-of-bounds sphere
        im.sharedToonFlag = 0;
        im.toonIndex = 9999;          // out-of-bounds toon
        incomingDoc.materials = {im};

        PmxJoint ij;
        ij.rigidBodyIndexA = 9999; // out-of-bounds rigid body
        ij.rigidBodyIndexB = -1;
        incomingDoc.joints = {ij};

        PmxDisplayFrame idf;
        idf.nameJp = QStringLiteral("MalformedFrame");
        idf.elements.push_back({0, 9999}); // invalid bone
        idf.elements.push_back({1, 9999}); // invalid morph
        incomingDoc.displayFrames = {idf};

        PmxSoftBody isb;
        isb.materialIndex = 9999;
        isb.anchors.push_back({9999, 9999, 0});
        isb.pinVertexIndices = {9999};
        incomingDoc.softBodies = {isb};

        MergeOptions options;
        options.importPhysics = true;
        options.importMorphs = true;
        options.importDisplayFrames = true;

        bool ok = PmxMerger::merge(baseDoc, incomingDoc, options);
        QVERIFY(ok);

        // Verify out-of-bounds bone index clamped to -1
        QCOMPARE(baseDoc.vertices[1].boneIndices[0], -1);

        // Verify out-of-bounds material indices clamped to -1
        QCOMPARE(baseDoc.materials[0].textureIndex, -1);
        QCOMPARE(baseDoc.materials[0].sphereTextureIndex, -1);
        QCOMPARE(baseDoc.materials[0].toonIndex, -1);

        // Verify joint rigid body clamped to -1
        QCOMPARE(baseDoc.joints[0].rigidBodyIndexA, -1);
        QCOMPARE(baseDoc.joints[0].rigidBodyIndexB, -1);

        // Verify invalid elements dropped from display frame
        QCOMPARE(baseDoc.displayFrames[0].elements.size(), 0u);

        // Verify soft body out-of-bounds indices clamped to -1
        QCOMPARE(baseDoc.softBodies[0].materialIndex, -1);
        QCOMPARE(baseDoc.softBodies[0].anchors[0].rigidBodyIndex, -1);
        QCOMPARE(baseDoc.softBodies[0].anchors[0].vertexIndex, -1);
        QCOMPARE(baseDoc.softBodies[0].pinVertexIndices[0], -1);
    }
}

QTEST_MAIN(TstPmxMerger)
#include "tst_pmx_merger.moc"
