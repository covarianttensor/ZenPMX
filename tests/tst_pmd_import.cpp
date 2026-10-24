#include "core/PmdImporter.h"
#include "core/PmxDocument.h"
#include "core/PmxReader.h"
#include "core/PmxTypes.h"
#include "core/PmxWriter.h"

#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QStringEncoder>
#include <QtEndian>
#include <QtTest>
#include <cmath>
#include <cstring>

using namespace pmx;

namespace {

struct PmdBuilder {
    QByteArray buffer;

    void appendBytes(const void* data, size_t size) {
        buffer.append(reinterpret_cast<const char*>(data), static_cast<qsizetype>(size));
    }

    void appendU8(uint8_t val) {
        buffer.append(static_cast<char>(val));
    }

    void appendI8(int8_t val) {
        buffer.append(static_cast<char>(val));
    }

    void appendU16(uint16_t val) {
        quint16 le = qToLittleEndian(val);
        appendBytes(&le, sizeof(le));
    }

    void appendI16(int16_t val) {
        qint16 le = qToLittleEndian(val);
        appendBytes(&le, sizeof(le));
    }

    void appendU32(uint32_t val) {
        quint32 le = qToLittleEndian(val);
        appendBytes(&le, sizeof(le));
    }

    void appendI32(int32_t val) {
        qint32 le = qToLittleEndian(val);
        appendBytes(&le, sizeof(le));
    }

    void appendFloat(float val) {
        quint32 raw = 0;
        std::memcpy(&raw, &val, sizeof(float));
        raw = qToLittleEndian(raw);
        appendBytes(&raw, sizeof(raw));
    }

    void appendVec2(const glm::vec2& v) {
        appendFloat(v.x);
        appendFloat(v.y);
    }

    void appendVec3(const glm::vec3& v) {
        appendFloat(v.x);
        appendFloat(v.y);
        appendFloat(v.z);
    }

    void appendFixedSjis(const QString& str, size_t fixedLen) {
        QStringEncoder encoder("Shift-JIS");
        QByteArray encoded = encoder(str);
        QByteArray out(static_cast<qsizetype>(fixedLen), '\0');
        const qsizetype copyLen = std::min(encoded.size(), static_cast<qsizetype>(fixedLen));
        std::memcpy(out.data(), encoded.constData(), copyLen);
        appendBytes(out.constData(), fixedLen);
    }

    void appendHeader(const QString& name, const QString& comment, float version = 1.0f) {
        appendBytes("Pmd", 3);
        appendFloat(version);
        appendFixedSjis(name, 20);
        appendFixedSjis(comment, 256);
    }
};

PmdBuilder createMinimalValidPmdBuilder() {
    PmdBuilder b;
    b.appendHeader(QString::fromUtf8("テストモデル"), QString::fromUtf8("テストコメント"));

    // 3 Vertices
    b.appendU32(3);
    // Vertex 0
    b.appendVec3(glm::vec3(0.0f, 0.0f, 0.0f));
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec2(glm::vec2(0.0f, 0.0f));
    b.appendU16(0); // bone0
    b.appendU16(1); // bone1
    b.appendU8(50); // weight
    b.appendU8(0);  // edgeFlag (0 = draw edge)

    // Vertex 1
    b.appendVec3(glm::vec3(1.0f, 0.0f, 0.0f));
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec2(glm::vec2(1.0f, 0.0f));
    b.appendU16(0);
    b.appendU16(1);
    b.appendU8(100);
    b.appendU8(0);

    // Vertex 2
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec2(glm::vec2(0.0f, 1.0f));
    b.appendU16(1);
    b.appendU16(0);
    b.appendU8(0);
    b.appendU8(1);  // edgeFlag (1 = no edge)

    // 3 Face indices
    b.appendU32(3);
    b.appendU16(0);
    b.appendU16(1);
    b.appendU16(2);

    // 1 Material
    b.appendU32(1);
    b.appendVec3(glm::vec3(1.0f, 0.0f, 0.0f)); // diffuse
    b.appendFloat(1.0f);                        // alpha
    b.appendFloat(10.0f);                       // shininess
    b.appendVec3(glm::vec3(0.5f, 0.5f, 0.5f)); // specular
    b.appendVec3(glm::vec3(0.2f, 0.2f, 0.2f)); // ambient
    b.appendI8(0);                             // toonIndex
    b.appendU8(1);                             // edgeFlag
    b.appendU32(3);                            // faceVertexCount
    b.appendFixedSjis("test.png", 20);         // textureFileName

    // 2 Bones
    b.appendU16(2);
    // Bone 0: センター (Rotate/Translate)
    b.appendFixedSjis(QString::fromUtf8("センター"), 20);
    b.appendU16(0xFFFF); // parent
    b.appendU16(1);      // tail
    b.appendU8(1);       // type 1: Rotate/Translate
    b.appendU16(0xFFFF); // ikParent
    b.appendVec3(glm::vec3(0.0f, 0.0f, 0.0f));

    // Bone 1: 足 (Rotate)
    b.appendFixedSjis(QString::fromUtf8("足"), 20);
    b.appendU16(0);      // parent
    b.appendU16(0xFFFF); // tail
    b.appendU8(0);       // type 0: Rotate
    b.appendU16(0xFFFF); // ikParent
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));

    // 1 IK Chain
    b.appendU16(1);
    b.appendU16(0);     // ikBone
    b.appendU16(1);     // targetBone
    b.appendU8(1);      // linkCount
    b.appendU16(10);    // loopCount
    b.appendFloat(0.5f);// angleLimit
    b.appendU16(1);     // linkBone 0

    // 2 Morphs (Base + Eyebrow)
    b.appendU16(2);
    // Morph 0: base (Type 0)
    b.appendFixedSjis("base", 20);
    b.appendU32(2); // 2 base vertices
    b.appendU8(0);  // type 0: Base
    // Base vertex 0 -> global vertex 0
    b.appendU32(0);
    b.appendVec3(glm::vec3(0.0f, 0.0f, 0.0f));
    // Base vertex 1 -> global vertex 2
    b.appendU32(2);
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));

    // Morph 1: 真面目 (Type 1: Eyebrow)
    b.appendFixedSjis(QString::fromUtf8("真面目"), 20);
    b.appendU32(1); // 1 vertex offset
    b.appendU8(1);  // type 1: Eyebrow
    // Target base vertex 0 (which maps to global vertex 0)
    b.appendU32(0);
    b.appendVec3(glm::vec3(0.0f, 0.1f, 0.0f));

    // Display Frames
    // Face frames
    b.appendU8(1);
    b.appendU16(1); // references morph 1

    // Bone groups
    b.appendU8(1);
    b.appendFixedSjis(QString::fromUtf8("センター"), 50);

    // Bone display entries
    b.appendU32(1);
    b.appendU16(0); // bone 0
    b.appendU8(1);  // group 1

    return b;
}

} // namespace

class tst_pmd_import : public QObject {
    Q_OBJECT

private slots:
    void testInvalidPmdHeader();
    void testMinimalPmdImport();
    void testPmdWeightConversion();
    void testPmdMaterialTextureSplitting();
    void testPmdEnglishExtension();
    void testPmdPhysicsExtension();
    void testPmdTruncationSafety();
    void testKneeAngleLimitAndBoneTypes();
    void testCustomToonTableWithNonStandardNames();
    void testMissingBaseMorphAndInvalidFaceCount();
    void testPmdToPmxFullRoundtrip();
    void testCorruptedSectionCounts();
};

void tst_pmd_import::testInvalidPmdHeader() {
    PmxDocument doc;
    QString err;

    // 1. Empty buffer
    QVERIFY(!PmdImporter::importFromMemory(QByteArray(), doc, &err));
    QVERIFY(!err.isEmpty());

    // 2. Buffer too small (< 283 bytes)
    QByteArray smallBuf(100, '\0');
    std::memcpy(smallBuf.data(), "Pmd", 3);
    QVERIFY(!PmdImporter::importFromMemory(smallBuf, doc, &err));
    QVERIFY(err.contains(QStringLiteral("too small")));

    // 3. Bad magic ("PMX " instead of "Pmd")
    PmdBuilder b1;
    b1.appendBytes("PMX", 3);
    b1.appendFloat(1.0f);
    b1.appendFixedSjis("Name", 20);
    b1.appendFixedSjis("Comment", 256);
    QVERIFY(!PmdImporter::importFromMemory(b1.buffer, doc, &err));
    QVERIFY(err.contains(QStringLiteral("magic")));

    // 4. Bad version (2.0f instead of 1.0f)
    PmdBuilder b2;
    b2.appendHeader("Model", "Comment", 2.0f);
    QVERIFY(!PmdImporter::importFromMemory(b2.buffer, doc, &err));
    QVERIFY(err.contains(QStringLiteral("version")));
}

void tst_pmd_import::testMinimalPmdImport() {
    PmdBuilder b = createMinimalValidPmdBuilder();
    PmxDocument doc;
    QString err;

    QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));

    // Verify model info
    QCOMPARE(doc.modelInfo.modelNameJp, QString::fromUtf8("テストモデル"));
    QCOMPARE(doc.modelInfo.commentJp, QString::fromUtf8("テストコメント"));

    // Verify vertices
    QCOMPARE(doc.vertices.size(), static_cast<size_t>(3));
    QCOMPARE(doc.vertices[0].position, glm::vec3(0.0f, 0.0f, 0.0f));
    QCOMPARE(doc.vertices[0].weightType, WeightType::BDEF2);
    QCOMPARE(doc.vertices[0].boneIndices[0], 0);
    QCOMPARE(doc.vertices[0].boneIndices[1], 1);
    QCOMPARE(doc.vertices[0].edgeScale, 1.0f);

    QCOMPARE(doc.vertices[2].edgeScale, 0.0f); // edgeFlag == 1

    // Verify faces
    QCOMPARE(doc.faceIndices.size(), static_cast<size_t>(3));
    QCOMPARE(doc.faceIndices[0], 0);
    QCOMPARE(doc.faceIndices[1], 1);
    QCOMPARE(doc.faceIndices[2], 2);

    // Verify materials
    QCOMPARE(doc.materials.size(), static_cast<size_t>(1));
    QCOMPARE(doc.materials[0].diffuse, glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    QCOMPARE(doc.materials[0].specularPower, 10.0f);
    QCOMPARE(doc.materials[0].faceVertexCount, 3);
    QCOMPARE(doc.materials[0].drawFlags, 0x1F); // edge on
    QCOMPARE(doc.textures.size(), static_cast<size_t>(1));
    QCOMPARE(doc.textures[0], QStringLiteral("test.png"));
    QCOMPARE(doc.materials[0].textureIndex, 0);

    // Verify bones
    QCOMPARE(doc.bones.size(), static_cast<size_t>(2));
    QCOMPARE(doc.bones[0].nameJp, QString::fromUtf8("センター"));
    QVERIFY((doc.bones[0].boneFlags & 0x0004) != 0); // Can translate
    QVERIFY((doc.bones[0].boneFlags & 0x0020) != 0); // IK
    QCOMPARE(doc.bones[0].ikTargetBoneIndex, 1);
    QCOMPARE(doc.bones[0].ikLinks.size(), static_cast<size_t>(1));
    QCOMPARE(doc.bones[0].ikLinks[0].boneIndex, 1);

    // Verify morphs: base morph is excluded, only "真面目" is present
    QCOMPARE(doc.morphs.size(), static_cast<size_t>(1));
    QCOMPARE(doc.morphs[0].nameJp, QString::fromUtf8("真面目"));
    QCOMPARE(doc.morphs[0].morphType, 1);      // Vertex morph
    QCOMPARE(doc.morphs[0].panelCategory, 1);  // Eyebrow
    QCOMPARE(doc.morphs[0].vertexOffsets.size(), static_cast<size_t>(1));
    // Base index 0 was remapped to global vertex index 0
    QCOMPARE(doc.morphs[0].vertexOffsets[0].vertexIndex, 0);
    QCOMPARE(doc.morphs[0].vertexOffsets[0].positionOffset, glm::vec3(0.0f, 0.1f, 0.0f));

    // Verify display frames
    QVERIFY(doc.displayFrames.size() >= 3);
    QCOMPARE(doc.displayFrames[0].nameJp, QString::fromUtf8("Root"));
    QCOMPARE(doc.displayFrames[1].nameJp, QString::fromUtf8("表情"));
    QCOMPARE(doc.displayFrames[2].nameJp, QString::fromUtf8("センター"));

    // Also test importFromFile
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    tempFile.write(b.buffer);
    tempFile.flush();

    PmxDocument fileDoc;
    QString fileErr;
    QVERIFY2(PmdImporter::importFromFile(tempFile.fileName(), fileDoc, &fileErr), qPrintable(fileErr));
    QCOMPARE(fileDoc.vertices.size(), doc.vertices.size());
    QCOMPARE(fileDoc.modelInfo.modelNameJp, doc.modelInfo.modelNameJp);
    QCOMPARE(fileDoc.sourceFilePath, tempFile.fileName());
}

void tst_pmd_import::testPmdWeightConversion() {
    PmdBuilder b;
    b.appendHeader("WeightTest", "Comment");

    // 3 Vertices with various weights
    b.appendU32(3);

    // Vertex 0: weight 75 (bone 2, 5)
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec2(glm::vec2(0.0f));
    b.appendU16(2);
    b.appendU16(5);
    b.appendU8(75);
    b.appendU8(0); // edgeFlag == 0 -> edgeScale 1.0

    // Vertex 1: weight 0 (bone 1, 3)
    b.appendVec3(glm::vec3(1.0f));
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec2(glm::vec2(0.0f));
    b.appendU16(1);
    b.appendU16(3);
    b.appendU8(0);
    b.appendU8(1); // edgeFlag == 1 -> edgeScale 0.0

    // Vertex 2: weight 100 (bone 0, 4)
    b.appendVec3(glm::vec3(2.0f));
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec2(glm::vec2(0.0f));
    b.appendU16(0);
    b.appendU16(4);
    b.appendU8(100);
    b.appendU8(0);

    // Faces: 3 indices
    b.appendU32(3);
    b.appendU16(0);
    b.appendU16(1);
    b.appendU16(2);

    // Materials: 0
    b.appendU32(0);
    // Bones: 0
    b.appendU16(0);
    // IK: 0
    b.appendU16(0);
    // Morphs: 0
    b.appendU16(0);
    // Display frames
    b.appendU8(0);  // face frames
    b.appendU8(0);  // bone groups
    b.appendU32(0); // bone disp entries

    PmxDocument doc;
    QString err;
    QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));

    QCOMPARE(doc.vertices.size(), static_cast<size_t>(3));

    // Vertex 0: weight 75 -> BDEF2 w0=0.75, w1=0.25
    const auto& v0 = doc.vertices[0];
    QCOMPARE(v0.weightType, WeightType::BDEF2);
    QCOMPARE(v0.boneIndices[0], 2);
    QCOMPARE(v0.boneIndices[1], 5);
    QVERIFY(std::abs(v0.boneWeights[0] - 0.75f) < 1e-4f);
    QVERIFY(std::abs(v0.boneWeights[1] - 0.25f) < 1e-4f);
    QCOMPARE(v0.edgeScale, 1.0f);

    // Vertex 1: weight 0 -> BDEF2 w0=0.0, w1=1.0
    const auto& v1 = doc.vertices[1];
    QCOMPARE(v1.boneIndices[0], 1);
    QCOMPARE(v1.boneIndices[1], 3);
    QVERIFY(std::abs(v1.boneWeights[0] - 0.0f) < 1e-4f);
    QVERIFY(std::abs(v1.boneWeights[1] - 1.0f) < 1e-4f);
    QCOMPARE(v1.edgeScale, 0.0f);

    // Vertex 2: weight 100 -> BDEF2 w0=1.0, w1=0.0
    const auto& v2 = doc.vertices[2];
    QCOMPARE(v2.boneIndices[0], 0);
    QCOMPARE(v2.boneIndices[1], 4);
    QVERIFY(std::abs(v2.boneWeights[0] - 1.0f) < 1e-4f);
    QVERIFY(std::abs(v2.boneWeights[1] - 0.0f) < 1e-4f);
}

void tst_pmd_import::testPmdMaterialTextureSplitting() {
    PmdBuilder b;
    b.appendHeader("TexTest", "Comment");

    // 0 Vertices, 0 Faces
    b.appendU32(0);
    b.appendU32(0);

    // 4 Materials
    b.appendU32(4);

    auto appendMat = [&](const QString& texStr) {
        b.appendVec3(glm::vec3(1.0f));
        b.appendFloat(1.0f);
        b.appendFloat(5.0f);
        b.appendVec3(glm::vec3(0.0f));
        b.appendVec3(glm::vec3(0.5f));
        b.appendI8(-1);
        b.appendU8(1);
        b.appendU32(0);
        b.appendFixedSjis(texStr, 20);
    };

    // Note: PMD material texture buffer is strictly 20 bytes Shift-JIS
    appendMat(QStringLiteral("tex.png*sphere.sph"));
    appendMat(QStringLiteral("dif.png*glass.spa"));
    appendMat(QStringLiteral("*pure_sphere.sph"));
    appendMat(QStringLiteral("metal.spa"));

    // Bones, IK, Morphs, Display frames: 0
    b.appendU16(0);
    b.appendU16(0);
    b.appendU16(0);
    b.appendU8(0);
    b.appendU8(0);
    b.appendU32(0);

    PmxDocument doc;
    QString err;
    QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));

    QCOMPARE(doc.materials.size(), static_cast<size_t>(4));

    // Material 0: tex.png * sphere.sph -> Multiply
    const auto& m0 = doc.materials[0];
    QVERIFY(m0.textureIndex != -1);
    QVERIFY(m0.sphereTextureIndex != -1);
    QCOMPARE(doc.textures[m0.textureIndex], QStringLiteral("tex.png"));
    QCOMPARE(doc.textures[m0.sphereTextureIndex], QStringLiteral("sphere.sph"));
    QCOMPARE(m0.sphereMode, 1); // 1 = Multiply

    // Material 1: dif.png * glass.spa -> Add
    const auto& m1 = doc.materials[1];
    QVERIFY(m1.textureIndex != -1);
    QVERIFY(m1.sphereTextureIndex != -1);
    QCOMPARE(doc.textures[m1.textureIndex], QStringLiteral("dif.png"));
    QCOMPARE(doc.textures[m1.sphereTextureIndex], QStringLiteral("glass.spa"));
    QCOMPARE(m1.sphereMode, 2); // 2 = Add

    // Material 2: *pure_sphere.sph -> Sphere only, Multiply
    const auto& m2 = doc.materials[2];
    QCOMPARE(m2.textureIndex, -1);
    QVERIFY(m2.sphereTextureIndex != -1);
    QCOMPARE(doc.textures[m2.sphereTextureIndex], QStringLiteral("pure_sphere.sph"));
    QCOMPARE(m2.sphereMode, 1);

    // Material 3: metal.spa -> Sphere only, Add
    const auto& m3 = doc.materials[3];
    QCOMPARE(m3.textureIndex, -1);
    QVERIFY(m3.sphereTextureIndex != -1);
    QCOMPARE(doc.textures[m3.sphereTextureIndex], QStringLiteral("metal.spa"));
    QCOMPARE(m3.sphereMode, 2);
}

void tst_pmd_import::testPmdEnglishExtension() {
    PmdBuilder b = createMinimalValidPmdBuilder();

    // English extension
    b.appendU8(1); // englishFlag = 1
    b.appendFixedSjis("English Model", 20);
    b.appendFixedSjis("English Comment", 256);
    b.appendFixedSjis("Center Bone", 20); // Bone 0
    b.appendFixedSjis("Foot Bone", 20);   // Bone 1
    b.appendFixedSjis("Serious Face", 20);// Morph 0 (non-base)
    b.appendFixedSjis("Center Group", 50);// Bone Group 0

    PmxDocument doc;
    QString err;
    QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));

    QCOMPARE(doc.modelInfo.modelNameEn, QStringLiteral("English Model"));
    QCOMPARE(doc.modelInfo.commentEn, QStringLiteral("English Comment"));
    QCOMPARE(doc.bones[0].nameEn, QStringLiteral("Center Bone"));
    QCOMPARE(doc.bones[1].nameEn, QStringLiteral("Foot Bone"));
    QCOMPARE(doc.morphs[0].nameEn, QStringLiteral("Serious Face"));
    QCOMPARE(doc.displayFrames[2].nameEn, QStringLiteral("Center Group"));
}

void tst_pmd_import::testPmdPhysicsExtension() {
    PmdBuilder b = createMinimalValidPmdBuilder();

    // English extension disabled
    b.appendU8(0);

    // Toon Texture List (10 * 100 bytes)
    for (int t = 1; t <= 10; ++t) {
        b.appendFixedSjis(QStringLiteral("toon%1.bmp").arg(t, 2, 10, QLatin1Char('0')), 100);
    }

    // Rigid Bodies: 2
    b.appendU32(2);

    // Body 0: Sphere
    b.appendFixedSjis("RigidA", 20);
    b.appendI16(0);                              // bone 0
    b.appendU8(1);                               // group 1
    b.appendU16(0xFFFE);                         // mask
    b.appendU8(0);                               // shape 0: Sphere
    b.appendVec3(glm::vec3(1.0f, 1.0f, 1.0f));   // size
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));   // pos
    b.appendVec3(glm::vec3(0.0f, 0.0f, 0.0f));   // rot
    b.appendFloat(2.5f);                         // mass
    b.appendFloat(0.1f);                         // linear damping
    b.appendFloat(0.2f);                         // angular damping
    b.appendFloat(0.3f);                         // restitution
    b.appendFloat(0.4f);                         // friction
    b.appendU8(0);                               // mode 0: FollowBone

    // Body 1: Box
    b.appendFixedSjis("RigidB", 20);
    b.appendI16(1);                              // bone 1
    b.appendU8(2);                               // group 2
    b.appendU16(0xFFFD);                         // mask
    b.appendU8(1);                               // shape 1: Box
    b.appendVec3(glm::vec3(2.0f, 2.0f, 2.0f));   // size
    b.appendVec3(glm::vec3(0.0f, 2.0f, 0.0f));   // pos
    b.appendVec3(glm::vec3(0.1f, 0.2f, 0.3f));   // rot
    b.appendFloat(1.5f);                         // mass
    b.appendFloat(0.2f);                         // linear damping
    b.appendFloat(0.3f);                         // angular damping
    b.appendFloat(0.4f);                         // restitution
    b.appendFloat(0.5f);                         // friction
    b.appendU8(1);                               // mode 1: Physics

    // Joints: 1
    b.appendU32(1);
    b.appendFixedSjis("Joint0", 20);
    b.appendU32(0);                               // bodyA
    b.appendU32(1);                               // bodyB
    b.appendVec3(glm::vec3(0.0f, 1.5f, 0.0f));    // pos
    b.appendVec3(glm::vec3(0.0f, 0.0f, 0.0f));    // rot
    b.appendVec3(glm::vec3(-1.0f, -1.0f, -1.0f)); // posMin
    b.appendVec3(glm::vec3(1.0f, 1.0f, 1.0f));    // posMax
    b.appendVec3(glm::vec3(-0.5f, -0.5f, -0.5f)); // rotMin
    b.appendVec3(glm::vec3(0.5f, 0.5f, 0.5f));    // rotMax
    b.appendVec3(glm::vec3(10.0f, 10.0f, 10.0f)); // springPos
    b.appendVec3(glm::vec3(20.0f, 20.0f, 20.0f)); // springRot

    PmxDocument doc;
    QString err;
    QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));

    // Verify Rigid Bodies
    QCOMPARE(doc.rigidBodies.size(), static_cast<size_t>(2));
    QCOMPARE(doc.rigidBodies[0].nameJp, QStringLiteral("RigidA"));
    QCOMPARE(doc.rigidBodies[0].boneIndex, 0);
    QCOMPARE(doc.rigidBodies[0].groupIndex, 1);
    QCOMPARE(doc.rigidBodies[0].shape, 0);
    QCOMPARE(doc.rigidBodies[0].mass, 2.5f);
    QCOMPARE(doc.rigidBodies[0].physicsMode, 0);

    QCOMPARE(doc.rigidBodies[1].nameJp, QStringLiteral("RigidB"));
    QCOMPARE(doc.rigidBodies[1].boneIndex, 1);
    QCOMPARE(doc.rigidBodies[1].groupIndex, 2);
    QCOMPARE(doc.rigidBodies[1].shape, 1);
    QCOMPARE(doc.rigidBodies[1].mass, 1.5f);
    QCOMPARE(doc.rigidBodies[1].physicsMode, 1);

    // Verify Joints
    QCOMPARE(doc.joints.size(), static_cast<size_t>(1));
    QCOMPARE(doc.joints[0].nameJp, QStringLiteral("Joint0"));
    QCOMPARE(doc.joints[0].rigidBodyIndexA, 0);
    QCOMPARE(doc.joints[0].rigidBodyIndexB, 1);
    QCOMPARE(doc.joints[0].position, glm::vec3(0.0f, 1.5f, 0.0f));
    QCOMPARE(doc.joints[0].springLinearStiffness, glm::vec3(10.0f, 10.0f, 10.0f));
    QCOMPARE(doc.joints[0].springAngularStiffness, glm::vec3(20.0f, 20.0f, 20.0f));
}

void tst_pmd_import::testPmdTruncationSafety() {
    PmdBuilder b = createMinimalValidPmdBuilder();
    const qsizetype baseModelSize = b.buffer.size();

    // English extension
    b.appendU8(1);
    b.appendFixedSjis("ModelEn", 20);
    b.appendFixedSjis("CommentEn", 256);
    b.appendFixedSjis("Bone0En", 20);
    b.appendFixedSjis("Bone1En", 20);
    b.appendFixedSjis("Morph0En", 20);
    b.appendFixedSjis("Group0En", 50);
    const qsizetype englishEnd = b.buffer.size();

    // Toons
    for (int t = 1; t <= 10; ++t) {
        b.appendFixedSjis(QStringLiteral("toon%1.bmp").arg(t, 2, 10, QLatin1Char('0')), 100);
    }
    const qsizetype toonEnd = b.buffer.size();

    // Rigid bodies
    b.appendU32(1);
    b.appendFixedSjis("Body0", 20);
    b.appendI16(0);
    b.appendU8(0);
    b.appendU16(0xFFFF);
    b.appendU8(0);
    b.appendVec3(glm::vec3(1.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendFloat(1.0f);
    b.appendFloat(0.1f);
    b.appendFloat(0.1f);
    b.appendFloat(0.0f);
    b.appendFloat(0.5f);
    b.appendU8(0);
    const qsizetype rigidEnd = b.buffer.size();

    // Joints
    b.appendU32(1);
    b.appendFixedSjis("Joint0", 20);
    b.appendU32(0);
    b.appendU32(0);
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.0f));

    const QByteArray fullData = b.buffer;
    QVERIFY(fullData.size() > 500);

    // 1. Mandatory base section truncation (0 to baseModelSize - 1): must fail
    for (qsizetype len = 0; len < baseModelSize; len += 5) {
        QByteArray truncated = fullData.left(len);
        PmxDocument doc;
        QString err;
        const bool ok = PmdImporter::importFromMemory(truncated, doc, &err);
        QVERIFY(!ok);
        QVERIFY(!err.isEmpty());
    }

    // 2. Truncation within English extension (baseModelSize + 1 to englishEnd - 1): must fail
    for (qsizetype len = baseModelSize + 1; len < englishEnd; len += 11) {
        QByteArray truncated = fullData.left(len);
        PmxDocument doc;
        QString err;
        const bool ok = PmdImporter::importFromMemory(truncated, doc, &err);
        QVERIFY(!ok);
        QVERIFY(!err.isEmpty());
    }

    // 3. Truncation within Toon table (englishEnd + 1 to toonEnd - 1): must fail
    for (qsizetype len = englishEnd + 1; len < toonEnd; len += 23) {
        QByteArray truncated = fullData.left(len);
        PmxDocument doc;
        QString err;
        const bool ok = PmdImporter::importFromMemory(truncated, doc, &err);
        QVERIFY(!ok);
        QVERIFY(!err.isEmpty());
    }

    // 4. Truncation within RigidBody table (toonEnd + 1 to rigidEnd - 1): must fail
    for (qsizetype len = toonEnd + 1; len < rigidEnd; len += 9) {
        QByteArray truncated = fullData.left(len);
        PmxDocument doc;
        QString err;
        const bool ok = PmdImporter::importFromMemory(truncated, doc, &err);
        QVERIFY(!ok);
        QVERIFY(!err.isEmpty());
    }

    // 5. Truncation within Joints table (rigidEnd + 1 to fullData.size() - 1): must fail
    for (qsizetype len = rigidEnd + 1; len < fullData.size(); len += 13) {
        QByteArray truncated = fullData.left(len);
        PmxDocument doc;
        QString err;
        const bool ok = PmdImporter::importFromMemory(truncated, doc, &err);
        QVERIFY(!ok);
        QVERIFY(!err.isEmpty());
    }

    // 6. Memory safety fuzz across ALL byte lengths (single-byte granularity): zero crashes/ASan errors
    for (qsizetype len = 0; len <= fullData.size(); ++len) {
        QByteArray truncated = fullData.left(len);
        PmxDocument doc;
        QString err;
        const bool ok = PmdImporter::importFromMemory(truncated, doc, &err);
        if (!ok) {
            QVERIFY(!err.isEmpty());
        }
    }

    // 7. Corrupted counts should be rejected safely by checkCountBounds
    {
        // Corrupted vertex count
        QByteArray corrupted = fullData;
        const qsizetype vertexCountOffset = 3 + 4 + 20 + 256;
        quint32 hugeCount = qToLittleEndian<quint32>(0x7FFFFFFF);
        std::memcpy(corrupted.data() + vertexCountOffset, &hugeCount, sizeof(hugeCount));

        PmxDocument doc;
        QString err;
        QVERIFY(!PmdImporter::importFromMemory(corrupted, doc, &err));
        QVERIFY(err.contains(QStringLiteral("exceeds remaining")) || err.contains(QStringLiteral("Negative")));
    }
}

void tst_pmd_import::testKneeAngleLimitAndBoneTypes() {
    PmdBuilder b;
    b.appendHeader("KneeAndBoneTypes", "Comment");

    // 3 Vertices
    b.appendU32(3);
    for (int i = 0; i < 3; ++i) {
        b.appendVec3(glm::vec3(0.0f));
        b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
        b.appendVec2(glm::vec2(0.0f));
        b.appendU16(0);
        b.appendU16(1);
        b.appendU8(50);
        b.appendU8(0);
    }

    // 3 Faces
    b.appendU32(3);
    b.appendU16(0);
    b.appendU16(1);
    b.appendU16(2);

    // 1 Material
    b.appendU32(1);
    b.appendVec3(glm::vec3(1.0f));
    b.appendFloat(1.0f);
    b.appendFloat(5.0f);
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.2f));
    b.appendI8(-1);
    b.appendU8(0);
    b.appendU32(3);
    b.appendFixedSjis("tex.png", 20);

    // 5 Bones
    b.appendU16(5);
    // Bone 0: センター (Type 1: Rotate/Translate)
    b.appendFixedSjis(QString::fromUtf8("センター"), 20);
    b.appendU16(0xFFFF);
    b.appendU16(0xFFFF);
    b.appendU8(1);
    b.appendU16(0xFFFF);
    b.appendVec3(glm::vec3(0.0f));

    // Bone 1: 右ひざ (Type 0: Rotate)
    b.appendFixedSjis(QString::fromUtf8("右ひざ"), 20);
    b.appendU16(0);
    b.appendU16(0xFFFF);
    b.appendU8(0);
    b.appendU16(0xFFFF);
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));

    // Bone 2: 腕捩 (Type 8: Twist)
    b.appendFixedSjis(QString::fromUtf8("腕捩"), 20);
    b.appendU16(0);
    b.appendU16(0xFFFF);
    b.appendU8(8); // Type 8: Twist
    b.appendU16(0); // ikParent points to Bone 0
    b.appendVec3(glm::vec3(1.0f, 0.0f, 0.0f));

    // Bone 3: 非表示ボーン (Type 7: Hidden)
    b.appendFixedSjis(QString::fromUtf8("非表示"), 20);
    b.appendU16(0);
    b.appendU16(0xFFFF);
    b.appendU8(7); // Type 7: Hidden
    b.appendU16(0xFFFF);
    b.appendVec3(glm::vec3(0.0f, 2.0f, 0.0f));

    // Bone 4: 右足ＩＫ (Type 2: IK)
    b.appendFixedSjis(QString::fromUtf8("右足ＩＫ"), 20);
    b.appendU16(0xFFFF);
    b.appendU16(0xFFFF);
    b.appendU8(2); // Type 2: IK
    b.appendU16(0xFFFF);
    b.appendVec3(glm::vec3(0.0f, 0.0f, 1.0f));

    // 1 IK Chain for Bone 4
    b.appendU16(1);
    b.appendU16(4);      // ikBone
    b.appendU16(0);      // targetBone
    b.appendU8(1);       // linkCount
    b.appendU16(20);     // loopCount
    b.appendFloat(0.8f); // angleLimit
    b.appendU16(1);      // linkBone 0 -> Bone 1 ("右ひざ")

    // Morphs: 0
    b.appendU16(0);
    // Display frames: 0
    b.appendU8(0);
    b.appendU8(0);
    b.appendU32(0);

    PmxDocument doc;
    QString err;
    QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));

    QCOMPARE(doc.bones.size(), static_cast<size_t>(5));

    // Verify Bone 1 link in Bone 4 IK has knee angle limits
    const auto& ikBone = doc.bones[4];
    QCOMPARE(ikBone.ikLinks.size(), static_cast<size_t>(1));
    QCOMPARE(ikBone.ikLinks[0].boneIndex, 1);
    QCOMPARE(ikBone.ikLinks[0].hasAngleLimit, static_cast<uint8_t>(1));
    QVERIFY(std::abs(ikBone.ikLinks[0].limitMin.x - glm::radians(0.5f)) < 1e-4f);
    QVERIFY(std::abs(ikBone.ikLinks[0].limitMax.x - glm::radians(180.0f)) < 1e-4f);

    // Verify Bone 2 (Twist): has InheritRotation (0x0100) and FixedAxis (0x0400)
    const auto& twistBone = doc.bones[2];
    QVERIFY((twistBone.boneFlags & 0x0100) != 0);
    QVERIFY((twistBone.boneFlags & 0x0400) != 0);
    QCOMPARE(twistBone.inheritParentIndex, 0);
    QCOMPARE(twistBone.inheritWeight, 1.0f);

    // Verify Bone 3 (Hidden): does NOT have Visible flag (0x0008)
    const auto& hiddenBone = doc.bones[3];
    QVERIFY((hiddenBone.boneFlags & 0x0008) == 0);
}

void tst_pmd_import::testCustomToonTableWithNonStandardNames() {
    PmdBuilder b;
    b.appendHeader("CustomToonTest", "Comment");

    // 3 Vertices, 3 Faces
    b.appendU32(3);
    for (int i = 0; i < 3; ++i) {
        b.appendVec3(glm::vec3(0.0f));
        b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
        b.appendVec2(glm::vec2(0.0f));
        b.appendU16(0);
        b.appendU16(0);
        b.appendU8(100);
        b.appendU8(0);
    }
    b.appendU32(3);
    b.appendU16(0);
    b.appendU16(1);
    b.appendU16(2);

    // 2 Materials: Mat0 uses toon 0, Mat1 uses toon 1
    b.appendU32(2);
    // Mat 0
    b.appendVec3(glm::vec3(1.0f));
    b.appendFloat(1.0f);
    b.appendFloat(5.0f);
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.2f));
    b.appendI8(0); // toonIndex 0
    b.appendU8(0);
    b.appendU32(3);
    b.appendFixedSjis("mat0.png", 20);

    // Mat 1
    b.appendVec3(glm::vec3(0.5f));
    b.appendFloat(1.0f);
    b.appendFloat(5.0f);
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(0.2f));
    b.appendI8(1); // toonIndex 1
    b.appendU8(0);
    b.appendU32(0);
    b.appendFixedSjis("mat1.png", 20);

    // Bones, IK, Morphs, Display frames: 0
    b.appendU16(0);
    b.appendU16(0);
    b.appendU16(0);
    b.appendU8(0);
    b.appendU8(0);
    b.appendU32(0);

    // English extension disabled
    b.appendU8(0);

    // Custom toon table (10 * 100 bytes)
    // Toon 0: standard "toon01.bmp" -> sharedToonFlag = 1, toonIndex = 0
    b.appendFixedSjis("toon01.bmp", 100);
    // Toon 1: non-standard custom toon "anime_cel_toon.png" -> sharedToonFlag = 0, toonIndex = texture index
    b.appendFixedSjis("anime_cel_toon.png", 100);
    for (int t = 2; t < 10; ++t) {
        b.appendFixedSjis(QStringLiteral("toon%1.bmp").arg(t + 1, 2, 10, QLatin1Char('0')), 100);
    }

    PmxDocument doc;
    QString err;
    QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));

    QCOMPARE(doc.materials.size(), static_cast<size_t>(2));

    // Mat 0: matches standard toon01.bmp -> sharedToonFlag == 1, toonIndex == 0
    QCOMPARE(doc.materials[0].sharedToonFlag, static_cast<uint8_t>(1));
    QCOMPARE(doc.materials[0].toonIndex, 0);

    // Mat 1: custom toon anime_cel_toon.png -> sharedToonFlag == 0, toonIndex points into doc.textures
    QCOMPARE(doc.materials[1].sharedToonFlag, static_cast<uint8_t>(0));
    QVERIFY(doc.materials[1].toonIndex >= 0);
    QCOMPARE(doc.textures[doc.materials[1].toonIndex], QStringLiteral("anime_cel_toon.png"));
}

void tst_pmd_import::testMissingBaseMorphAndInvalidFaceCount() {
    // 1. Invalid face count (not multiple of 3)
    {
        PmdBuilder b;
        b.appendHeader("BadFaceCount", "Comment");
        b.appendU32(0); // 0 vertices
        b.appendU32(4); // 4 face indices (not multiple of 3!)
        b.appendU16(0);
        b.appendU16(1);
        b.appendU16(2);
        b.appendU16(3);

        PmxDocument doc;
        QString err;
        QVERIFY(!PmdImporter::importFromMemory(b.buffer, doc, &err));
        QVERIFY(err.contains(QStringLiteral("multiple of 3")));
    }

    // 2. Missing base morph: model contains morph type 1 without base morph (type 0)
    {
        PmdBuilder b;
        b.appendHeader("NoBaseMorph", "Comment");
        b.appendU32(3); // 3 vertices
        for (int i = 0; i < 3; ++i) {
            b.appendVec3(glm::vec3(0.0f));
            b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
            b.appendVec2(glm::vec2(0.0f));
            b.appendU16(0);
            b.appendU16(0);
            b.appendU8(100);
            b.appendU8(0);
        }
        b.appendU32(3);
        b.appendU16(0);
        b.appendU16(1);
        b.appendU16(2);
        b.appendU32(0); // 0 materials
        b.appendU16(0); // 0 bones
        b.appendU16(0); // 0 IK

        // 1 Morph of Type 1 (Eyebrow), no Base morph
        b.appendU16(1);
        b.appendFixedSjis("EyebrowOnly", 20);
        b.appendU32(1);
        b.appendU8(1); // type 1
        b.appendU32(2); // directly references global vertex 2
        b.appendVec3(glm::vec3(0.0f, 0.5f, 0.0f));

        b.appendU8(0);
        b.appendU8(0);
        b.appendU32(0);

        PmxDocument doc;
        QString err;
        QVERIFY2(PmdImporter::importFromMemory(b.buffer, doc, &err), qPrintable(err));
        QCOMPARE(doc.morphs.size(), static_cast<size_t>(1));
        QCOMPARE(doc.morphs[0].nameJp, QStringLiteral("EyebrowOnly"));
        QCOMPARE(doc.morphs[0].vertexOffsets.size(), static_cast<size_t>(1));
        // Fallback directly maps raw index 2 as global index 2
        QCOMPARE(doc.morphs[0].vertexOffsets[0].vertexIndex, 2);
        QCOMPARE(doc.morphs[0].vertexOffsets[0].positionOffset, glm::vec3(0.0f, 0.5f, 0.0f));
    }
}

void tst_pmd_import::testPmdToPmxFullRoundtrip() {
    PmdBuilder b = createMinimalValidPmdBuilder();

    // Add English extension
    b.appendU8(1);
    b.appendFixedSjis("ModelEn", 20);
    b.appendFixedSjis("CommentEn", 256);
    b.appendFixedSjis("CenterEn", 20);
    b.appendFixedSjis("FootEn", 20);
    b.appendFixedSjis("SeriousEn", 20);
    b.appendFixedSjis("CenterGroupEn", 50);

    // Add Toons
    for (int t = 1; t <= 10; ++t) {
        b.appendFixedSjis(QStringLiteral("toon%1.bmp").arg(t, 2, 10, QLatin1Char('0')), 100);
    }

    // Add Rigid Bodies
    b.appendU32(2);
    // Body 0
    b.appendFixedSjis("BodyA", 20);
    b.appendI16(0);
    b.appendU8(1);
    b.appendU16(0xFFFE);
    b.appendU8(0); // Sphere
    b.appendVec3(glm::vec3(1.0f));
    b.appendVec3(glm::vec3(0.0f, 1.0f, 0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendFloat(2.0f);
    b.appendFloat(0.1f);
    b.appendFloat(0.1f);
    b.appendFloat(0.5f);
    b.appendFloat(0.5f);
    b.appendU8(0); // FollowBone

    // Body 1
    b.appendFixedSjis("BodyB", 20);
    b.appendI16(1);
    b.appendU8(2);
    b.appendU16(0xFFFD);
    b.appendU8(1); // Box
    b.appendVec3(glm::vec3(2.0f));
    b.appendVec3(glm::vec3(0.0f, 2.0f, 0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendFloat(1.0f);
    b.appendFloat(0.2f);
    b.appendFloat(0.2f);
    b.appendFloat(0.0f);
    b.appendFloat(0.8f);
    b.appendU8(1); // Physics

    // Add Joint
    b.appendU32(1);
    b.appendFixedSjis("Joint0", 20);
    b.appendU32(0);
    b.appendU32(1);
    b.appendVec3(glm::vec3(0.0f, 1.5f, 0.0f));
    b.appendVec3(glm::vec3(0.0f));
    b.appendVec3(glm::vec3(-1.0f));
    b.appendVec3(glm::vec3(1.0f));
    b.appendVec3(glm::vec3(-0.5f));
    b.appendVec3(glm::vec3(0.5f));
    b.appendVec3(glm::vec3(10.0f));
    b.appendVec3(glm::vec3(20.0f));

    // 1. Import PMD
    PmxDocument pmdDoc;
    QString importErr;
    QVERIFY2(PmdImporter::importFromMemory(b.buffer, pmdDoc, &importErr), qPrintable(importErr));

    // 2. Serialize to PMX via PmxWriter
    QByteArray pmxBytes;
    QString writerErr;
    QVERIFY2(PmxWriter::writeToBuffer(pmdDoc, pmxBytes, &writerErr), qPrintable(writerErr));
    QVERIFY(!pmxBytes.isEmpty());

    // 3. Deserialize via PmxReader
    PmxDocument roundtripDoc;
    QString readerErr;
    QVERIFY2(PmxReader::readFromBuffer(pmxBytes, roundtripDoc, &readerErr), qPrintable(readerErr));

    // 4. Verify all tables roundtrip cleanly
    QCOMPARE(roundtripDoc.header.version, 2.0f);
    QCOMPARE(roundtripDoc.modelInfo.modelNameJp, pmdDoc.modelInfo.modelNameJp);
    QCOMPARE(roundtripDoc.modelInfo.modelNameEn, pmdDoc.modelInfo.modelNameEn);
    QCOMPARE(roundtripDoc.modelInfo.commentJp, pmdDoc.modelInfo.commentJp);
    QCOMPARE(roundtripDoc.modelInfo.commentEn, pmdDoc.modelInfo.commentEn);

    QCOMPARE(roundtripDoc.vertices.size(), pmdDoc.vertices.size());
    for (size_t i = 0; i < pmdDoc.vertices.size(); ++i) {
        QCOMPARE(roundtripDoc.vertices[i].position, pmdDoc.vertices[i].position);
        QCOMPARE(roundtripDoc.vertices[i].weightType, pmdDoc.vertices[i].weightType);
        QCOMPARE(roundtripDoc.vertices[i].boneIndices, pmdDoc.vertices[i].boneIndices);
        QCOMPARE(roundtripDoc.vertices[i].boneWeights, pmdDoc.vertices[i].boneWeights);
        QCOMPARE(roundtripDoc.vertices[i].edgeScale, pmdDoc.vertices[i].edgeScale);
    }

    QCOMPARE(roundtripDoc.faceIndices, pmdDoc.faceIndices);

    QCOMPARE(roundtripDoc.materials.size(), pmdDoc.materials.size());
    for (size_t i = 0; i < pmdDoc.materials.size(); ++i) {
        QCOMPARE(roundtripDoc.materials[i].diffuse, pmdDoc.materials[i].diffuse);
        QCOMPARE(roundtripDoc.materials[i].specular, pmdDoc.materials[i].specular);
        QCOMPARE(roundtripDoc.materials[i].specularPower, pmdDoc.materials[i].specularPower);
        QCOMPARE(roundtripDoc.materials[i].ambient, pmdDoc.materials[i].ambient);
        QCOMPARE(roundtripDoc.materials[i].faceVertexCount, pmdDoc.materials[i].faceVertexCount);
        QCOMPARE(roundtripDoc.materials[i].drawFlags, pmdDoc.materials[i].drawFlags);
    }

    QCOMPARE(roundtripDoc.bones.size(), pmdDoc.bones.size());
    for (size_t i = 0; i < pmdDoc.bones.size(); ++i) {
        QCOMPARE(roundtripDoc.bones[i].nameJp, pmdDoc.bones[i].nameJp);
        QCOMPARE(roundtripDoc.bones[i].nameEn, pmdDoc.bones[i].nameEn);
        QCOMPARE(roundtripDoc.bones[i].position, pmdDoc.bones[i].position);
        QCOMPARE(roundtripDoc.bones[i].parentBoneIndex, pmdDoc.bones[i].parentBoneIndex);
        QCOMPARE(roundtripDoc.bones[i].boneFlags, pmdDoc.bones[i].boneFlags);
    }

    QCOMPARE(roundtripDoc.morphs.size(), pmdDoc.morphs.size());
    for (size_t i = 0; i < pmdDoc.morphs.size(); ++i) {
        QCOMPARE(roundtripDoc.morphs[i].nameJp, pmdDoc.morphs[i].nameJp);
        QCOMPARE(roundtripDoc.morphs[i].nameEn, pmdDoc.morphs[i].nameEn);
        QCOMPARE(roundtripDoc.morphs[i].vertexOffsets.size(), pmdDoc.morphs[i].vertexOffsets.size());
    }

    QCOMPARE(roundtripDoc.rigidBodies.size(), pmdDoc.rigidBodies.size());
    for (size_t i = 0; i < pmdDoc.rigidBodies.size(); ++i) {
        QCOMPARE(roundtripDoc.rigidBodies[i].nameJp, pmdDoc.rigidBodies[i].nameJp);
        QCOMPARE(roundtripDoc.rigidBodies[i].boneIndex, pmdDoc.rigidBodies[i].boneIndex);
        QCOMPARE(roundtripDoc.rigidBodies[i].mass, pmdDoc.rigidBodies[i].mass);
        QCOMPARE(roundtripDoc.rigidBodies[i].shape, pmdDoc.rigidBodies[i].shape);
        QCOMPARE(roundtripDoc.rigidBodies[i].physicsMode, pmdDoc.rigidBodies[i].physicsMode);
    }

    QCOMPARE(roundtripDoc.joints.size(), pmdDoc.joints.size());
    for (size_t i = 0; i < pmdDoc.joints.size(); ++i) {
        QCOMPARE(roundtripDoc.joints[i].nameJp, pmdDoc.joints[i].nameJp);
        QCOMPARE(roundtripDoc.joints[i].rigidBodyIndexA, pmdDoc.joints[i].rigidBodyIndexA);
        QCOMPARE(roundtripDoc.joints[i].rigidBodyIndexB, pmdDoc.joints[i].rigidBodyIndexB);
        QCOMPARE(roundtripDoc.joints[i].position, pmdDoc.joints[i].position);
        QCOMPARE(roundtripDoc.joints[i].springLinearStiffness, pmdDoc.joints[i].springLinearStiffness);
    }
}

void tst_pmd_import::testCorruptedSectionCounts() {
    PmdBuilder b = createMinimalValidPmdBuilder();
    const QByteArray baseData = b.buffer;

    // Test corrupted negative count on face indices
    {
        PmdBuilder bad;
        bad.appendHeader("BadFace", "Comment");
        bad.appendU32(0); // 0 vertices
        bad.appendI32(-1); // negative face count (as uint32_t, 0xFFFFFFFF % 3 == 0)

        PmxDocument doc;
        QString err;
        QVERIFY(!PmdImporter::importFromMemory(bad.buffer, doc, &err));
        QVERIFY(err.contains(QStringLiteral("Negative")) || err.contains(QStringLiteral("exceeds")));
    }

    // Test corrupted negative count on materials
    {
        PmdBuilder bad;
        bad.appendHeader("BadMat", "Comment");
        bad.appendU32(0); // 0 vertices
        bad.appendU32(0); // 0 faces
        bad.appendI32(-1); // negative material count

        PmxDocument doc;
        QString err;
        QVERIFY(!PmdImporter::importFromMemory(bad.buffer, doc, &err));
        QVERIFY(err.contains(QStringLiteral("Negative")) || err.contains(QStringLiteral("exceeds")));
    }

    // Test corrupted huge material count exceeding buffer
    {
        PmdBuilder bad;
        bad.appendHeader("HugeMat", "Comment");
        bad.appendU32(0); // 0 vertices
        bad.appendU32(0); // 0 faces
        bad.appendU32(100000); // 100k materials with 0 bytes remaining

        PmxDocument doc;
        QString err;
        QVERIFY(!PmdImporter::importFromMemory(bad.buffer, doc, &err));
        QVERIFY(err.contains(QStringLiteral("exceeds remaining")));
    }

    // Test corrupted huge bone count exceeding buffer
    {
        PmdBuilder bad;
        bad.appendHeader("HugeBone", "Comment");
        bad.appendU32(0);
        bad.appendU32(0);
        bad.appendU32(0);
        bad.appendU16(50000); // 50k bones with 0 bytes remaining

        PmxDocument doc;
        QString err;
        QVERIFY(!PmdImporter::importFromMemory(bad.buffer, doc, &err));
        QVERIFY(err.contains(QStringLiteral("exceeds remaining")));
    }

    // Test corrupted huge morph count exceeding buffer
    {
        PmdBuilder bad;
        bad.appendHeader("HugeMorph", "Comment");
        bad.appendU32(0);
        bad.appendU32(0);
        bad.appendU32(0);
        bad.appendU16(0); // 0 bones
        bad.appendU16(0); // 0 IK
        bad.appendU16(60000); // 60k morphs with 0 bytes remaining

        PmxDocument doc;
        QString err;
        QVERIFY(!PmdImporter::importFromMemory(bad.buffer, doc, &err));
        QVERIFY(err.contains(QStringLiteral("exceeds remaining")));
    }
}

QTEST_MAIN(tst_pmd_import)
#include "tst_pmd_import.moc"
