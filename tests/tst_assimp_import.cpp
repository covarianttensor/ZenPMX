#include "core/AssimpImporter.h"
#include "core/PmxDocument.h"
#include "core/PmxReader.h"
#include "core/PmxWriter.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>
#include <cmath>

using namespace pmx;

class tst_assimp_import : public QObject {
    Q_OBJECT

private slots:
    void testSyntheticObjFromMemory();
    void testObjWithMtlFromFile();
    void testScaleOption();
    void testWithoutRootBone();
    void testFlipUVs();
    void testNormalsGeneration();
    void testTextureImport();
    void testMultiMeshImport();
    void testPmxRoundtrip();
    void testInvalidAndCorruptBuffers();
};

void tst_assimp_import::testSyntheticObjFromMemory() {
    // Synthetic OBJ with 1 quad (4 vertices, 4 UVs, 4 normals)
    const char objData[] =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 1.0 1.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vt 0.2 0.3\n"
        "vt 0.8 0.3\n"
        "vt 0.8 0.7\n"
        "vt 0.2 0.7\n"
        "vn 0.0 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "f 1/1/1 2/2/2 3/3/3 4/4/4\n";

    AssimpImportOptions options;
    options.scale = 1.0f;
    options.generateNormals = true;
    options.flipUVs = true;
    options.createRootBone = true;

    QString error;
    auto doc = AssimpImporter::importFromMemory(objData, sizeof(objData) - 1, "obj", options, &error);
    QVERIFY2(doc != nullptr, qPrintable(error));

    // 1. Quad face triangulated into 2 faces (6 indices)
    QCOMPARE(doc->faceIndices.size(), 6u);
    QCOMPARE(doc->vertices.size(), 4u);

    // 2. Normal vectors normalized (|n| ~= 1)
    for (size_t i = 0; i < doc->vertices.size(); ++i) {
        float len = glm::length(doc->vertices[i].normal);
        QVERIFY(std::abs(len - 1.0f) < 1e-4f);
    }

    // 3. UV coordinates properly extracted and flipped
    // With flipUVs = true, y becomes 1.0 - y: 0.3 -> 0.7
    QVERIFY(std::abs(doc->vertices[0].uv.x - 0.2f) < 1e-4f);
    QVERIFY(std::abs(doc->vertices[0].uv.y - 0.7f) < 1e-4f);

    // 4. Default / converted material properties
    QVERIFY(!doc->materials.empty());
    QCOMPARE(doc->materials[0].faceVertexCount, 6);
    QCOMPARE(doc->materials[0].drawFlags, 0x1F); // Double-sided, shadows, edge

    // 5. Root bone created and referenced by BDEF1 weights
    QCOMPARE(doc->bones.size(), 1u);
    QCOMPARE(doc->bones[0].nameJp, QString::fromUtf8("全ての親"));
    QCOMPARE(doc->bones[0].nameEn, QStringLiteral("Root"));
    QCOMPARE(doc->bones[0].parentBoneIndex, -1);
    QCOMPARE(doc->bones[0].deformTier, 0);
    QCOMPARE(doc->bones[0].boneFlags, 0x001E); // Rotatable, Translatable, Visible, Enabled

    for (size_t i = 0; i < doc->vertices.size(); ++i) {
        QCOMPARE(doc->vertices[i].weightType, WeightType::BDEF1);
        QCOMPARE(doc->vertices[i].boneIndices[0], 0);
        QVERIFY(std::abs(doc->vertices[i].boneWeights[0] - 1.0f) < 1e-5f);
    }

    // 6. Display frames initialized
    QCOMPARE(doc->displayFrames.size(), 2u);
    QCOMPARE(doc->displayFrames[0].nameJp, QStringLiteral("Root"));
    QCOMPARE(doc->displayFrames[0].nameEn, QStringLiteral("Root"));
    QCOMPARE(doc->displayFrames[0].specialFlag, 1);
    QCOMPARE(doc->displayFrames[0].elements.size(), 1u);
    QCOMPARE(doc->displayFrames[0].elements[0].targetType, 0); // Bone
    QCOMPARE(doc->displayFrames[0].elements[0].index, 0);

    QCOMPARE(doc->displayFrames[1].nameJp, QString::fromUtf8("表情"));
    QCOMPARE(doc->displayFrames[1].nameEn, QStringLiteral("Exp"));
    QCOMPARE(doc->displayFrames[1].specialFlag, 1);
}

void tst_assimp_import::testObjWithMtlFromFile() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString mtlPath = tempDir.filePath("sample.mtl");
    QString objPath = tempDir.filePath("sample.obj");

    // Write sample.mtl
    {
        QFile mtlFile(mtlPath);
        QVERIFY(mtlFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&mtlFile);
        out << "newmtl GoldMaterial\n"
            << "Kd 0.8 0.6 0.2\n"
            << "d 0.85\n"
            << "Ks 0.9 0.8 0.7\n"
            << "Ns 32.0\n"
            << "Ka 0.2 0.15 0.05\n";
        mtlFile.close();
    }

    // Write sample.obj
    {
        QFile objFile(objPath);
        QVERIFY(objFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&objFile);
        out << "mtllib sample.mtl\n"
            << "v -1.0 -1.0 0.0\n"
            << "v  1.0 -1.0 0.0\n"
            << "v  1.0  1.0 0.0\n"
            << "v -1.0  1.0 0.0\n"
            << "vt 0.0 0.0\n"
            << "vt 1.0 0.0\n"
            << "vt 1.0 1.0\n"
            << "vt 0.0 1.0\n"
            << "usemtl GoldMaterial\n"
            << "f 1/1 2/2 3/3\n"
            << "f 1/1 3/3 4/4\n";
        objFile.close();
    }

    AssimpImportOptions options;
    QString error;
    auto doc = AssimpImporter::importFile(objPath, options, &error);
    QVERIFY2(doc != nullptr, qPrintable(error));

    QCOMPARE(doc->modelInfo.modelNameJp, QStringLiteral("sample"));
    QCOMPARE(doc->modelInfo.modelNameEn, QStringLiteral("sample"));
    QCOMPARE(doc->materials.size(), 1u);

    const PmxMaterial& mat = doc->materials[0];
    QCOMPARE(mat.nameJp, QStringLiteral("GoldMaterial"));
    QCOMPARE(mat.nameEn, QStringLiteral("GoldMaterial"));

    // Material diffuse RGBA matched
    QVERIFY(std::abs(mat.diffuse.r - 0.8f) < 1e-2f);
    QVERIFY(std::abs(mat.diffuse.g - 0.6f) < 1e-2f);
    QVERIFY(std::abs(mat.diffuse.b - 0.2f) < 1e-2f);
    QVERIFY(std::abs(mat.diffuse.a - 0.85f) < 1e-2f);

    // Specular RGB and Shininess matched
    QVERIFY(std::abs(mat.specular.r - 0.9f) < 1e-2f);
    QVERIFY(std::abs(mat.specular.g - 0.8f) < 1e-2f);
    QVERIFY(std::abs(mat.specular.b - 0.7f) < 1e-2f);
    QVERIFY(std::abs(mat.specularPower - 32.0f) < 1e-2f);

    // Ambient matched
    QVERIFY(std::abs(mat.ambient.r - 0.2f) < 1e-2f);
    QVERIFY(std::abs(mat.ambient.g - 0.15f) < 1e-2f);
    QVERIFY(std::abs(mat.ambient.b - 0.05f) < 1e-2f);

    QCOMPARE(mat.faceVertexCount, 6);
}

void tst_assimp_import::testScaleOption() {
    const char objData[] =
        "v 2.0 4.0 6.0\n"
        "v 4.0 4.0 6.0\n"
        "v 4.0 8.0 6.0\n"
        "f 1 2 3\n";

    AssimpImportOptions options;
    options.scale = 0.5f;

    QString error;
    auto doc = AssimpImporter::importFromMemory(objData, sizeof(objData) - 1, "obj", options, &error);
    QVERIFY2(doc != nullptr, qPrintable(error));

    QCOMPARE(doc->vertices.size(), 3u);
    // After scale 0.5, (2, 4, 6) becomes (1, 2, -3) due to left-handed z-flip
    QVERIFY(std::abs(doc->vertices[0].position.x - 1.0f) < 1e-4f);
    QVERIFY(std::abs(doc->vertices[0].position.y - 2.0f) < 1e-4f);
    QVERIFY(std::abs(doc->vertices[0].position.z - (-3.0f)) < 1e-4f);
}

void tst_assimp_import::testWithoutRootBone() {
    const char objData[] =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 3\n";

    AssimpImportOptions options;
    options.createRootBone = false;

    QString error;
    auto doc = AssimpImporter::importFromMemory(objData, sizeof(objData) - 1, "obj", options, &error);
    QVERIFY2(doc != nullptr, qPrintable(error));

    QCOMPARE(doc->bones.size(), 0u);
    QCOMPARE(doc->displayFrames.size(), 2u);
    QCOMPARE(doc->displayFrames[0].elements.size(), 0u); // No bones in root frame
    QCOMPARE(doc->vertices[0].boneIndices[0], -1);
}

void tst_assimp_import::testFlipUVs() {
    const char objData[] =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "vt 0.35 0.75\n"
        "vt 0.50 0.75\n"
        "vt 0.35 0.90\n"
        "f 1/1 2/2 3/3\n";

    // With flipUVs = false
    {
        AssimpImportOptions options;
        options.flipUVs = false;
        auto doc = AssimpImporter::importFromMemory(objData, sizeof(objData) - 1, "obj", options);
        QVERIFY(doc != nullptr);
        QVERIFY(std::abs(doc->vertices[0].uv.y - 0.75f) < 1e-4f);
    }

    // With flipUVs = true
    {
        AssimpImportOptions options;
        options.flipUVs = true;
        auto doc = AssimpImporter::importFromMemory(objData, sizeof(objData) - 1, "obj", options);
        QVERIFY(doc != nullptr);
        // 1.0 - 0.75 = 0.25
        QVERIFY(std::abs(doc->vertices[0].uv.y - 0.25f) < 1e-4f);
    }
}

void tst_assimp_import::testNormalsGeneration() {
    // OBJ without normal definitions
    const char objData[] =
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 3\n";

    AssimpImportOptions options;
    options.generateNormals = true;

    auto doc = AssimpImporter::importFromMemory(objData, sizeof(objData) - 1, "obj", options);
    QVERIFY(doc != nullptr);
    QCOMPARE(doc->vertices.size(), 3u);

    for (size_t i = 0; i < doc->vertices.size(); ++i) {
        float len = glm::length(doc->vertices[i].normal);
        QVERIFY(std::abs(len - 1.0f) < 1e-4f);
    }
}

void tst_assimp_import::testTextureImport() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString mtlPath = tempDir.filePath("materials.mtl");
    QString objPath = tempDir.filePath("model.obj");

    // Write materials.mtl with Windows backslashes and duplicate texture references
    {
        QFile mtlFile(mtlPath);
        QVERIFY(mtlFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&mtlFile);
        out << "newmtl MatA\n"
            << "Kd 1.0 1.0 1.0\n"
            << "map_Kd tex\\diffuse.png\n"
            << "\n"
            << "newmtl MatB\n"
            << "Kd 0.5 0.5 0.5\n"
            << "map_Kd tex/diffuse.png\n"
            << "\n"
            << "newmtl MatC\n"
            << "Kd 0.2 0.2 0.2\n";
        mtlFile.close();
    }

    // Write model.obj with 3 faces using MatA, MatB, MatC
    {
        QFile objFile(objPath);
        QVERIFY(objFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&objFile);
        out << "mtllib materials.mtl\n"
            << "v 0 0 0\n"
            << "v 1 0 0\n"
            << "v 0 1 0\n"
            << "vt 0 0\n"
            << "vt 1 0\n"
            << "vt 0 1\n"
            << "usemtl MatA\n"
            << "f 1/1 2/2 3/3\n"
            << "usemtl MatB\n"
            << "f 1/1 2/2 3/3\n"
            << "usemtl MatC\n"
            << "f 1/1 2/2 3/3\n";
        objFile.close();
    }

    AssimpImportOptions options;
    QString error;
    auto doc = AssimpImporter::importFile(objPath, options, &error);
    QVERIFY2(doc != nullptr, qPrintable(error));

    // Texture table: 1 unique texture deduplicated and normalized to forward slashes
    QCOMPARE(doc->textures.size(), 1u);
    QCOMPARE(doc->textures[0], QStringLiteral("tex/diffuse.png"));

    // 3 materials
    QCOMPARE(doc->materials.size(), 3u);
    QCOMPARE(doc->materials[0].nameJp, QStringLiteral("MatA"));
    QCOMPARE(doc->materials[0].textureIndex, 0);

    QCOMPARE(doc->materials[1].nameJp, QStringLiteral("MatB"));
    QCOMPARE(doc->materials[1].textureIndex, 0);

    QCOMPARE(doc->materials[2].nameJp, QStringLiteral("MatC"));
    QCOMPARE(doc->materials[2].textureIndex, -1);
}

void tst_assimp_import::testMultiMeshImport() {
    const char multiMeshData[] =
        "o MeshA\n"
        "v 0.0 0.0 0.0\n"
        "v 1.0 0.0 0.0\n"
        "v 0.0 1.0 0.0\n"
        "f 1 2 3\n"
        "\n"
        "o MeshB\n"
        "v 5.0 5.0 5.0\n"
        "v 6.0 5.0 5.0\n"
        "v 5.0 6.0 5.0\n"
        "f 4 5 6\n";

    AssimpImportOptions options;
    QString error;
    auto doc = AssimpImporter::importFromMemory(multiMeshData, sizeof(multiMeshData) - 1, "obj", options, &error);
    QVERIFY2(doc != nullptr, qPrintable(error));

    // Total vertices: 6 (3 from MeshA, 3 from MeshB)
    QCOMPARE(doc->vertices.size(), 6u);

    // Total face vertex indices: 6 (2 triangles)
    QCOMPARE(doc->faceIndices.size(), 6u);

    // MeshA triangle indices should reference 0, 1, 2 (or equivalent winding)
    for (int i = 0; i < 3; ++i) {
        QVERIFY(doc->faceIndices[i] >= 0 && doc->faceIndices[i] < 3);
    }

    // MeshB triangle indices should reference 3, 4, 5 (properly offset by baseVertex)
    for (int i = 3; i < 6; ++i) {
        QVERIFY(doc->faceIndices[i] >= 3 && doc->faceIndices[i] < 6);
    }
}

void tst_assimp_import::testPmxRoundtrip() {
    const char objData[] =
        "v -2.0 -1.0 0.0\n"
        "v  2.0 -1.0 0.0\n"
        "v  2.0  1.0 0.0\n"
        "v -2.0  1.0 0.0\n"
        "vt 0.0 0.0\n"
        "vt 1.0 0.0\n"
        "vt 1.0 1.0\n"
        "vt 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "vn 0.0 0.0 1.0\n"
        "f 1/1/1 2/2/2 3/3/3 4/4/4\n";

    AssimpImportOptions options;
    QString error;
    auto doc = AssimpImporter::importFromMemory(objData, sizeof(objData) - 1, "obj", options, &error);
    QVERIFY2(doc != nullptr, qPrintable(error));

    // Serialize to bytes with PmxWriter
    QByteArray writtenBytes;
    bool writeOk = PmxWriter::writeToBuffer(*doc, writtenBytes, &error);
    QVERIFY2(writeOk, qPrintable(error));
    QVERIFY(!writtenBytes.isEmpty());

    // Deserialize with PmxReader
    PmxDocument roundtripDoc;
    bool readOk = PmxReader::readFromBuffer(writtenBytes, roundtripDoc, &error);
    QVERIFY2(readOk, qPrintable(error));

    // Assert semantic and bitwise parity
    QCOMPARE(roundtripDoc.header.version, 2.0f);
    QCOMPARE(roundtripDoc.vertices.size(), doc->vertices.size());
    QCOMPARE(roundtripDoc.faceIndices.size(), doc->faceIndices.size());
    QCOMPARE(roundtripDoc.faceIndices, doc->faceIndices);
    QCOMPARE(roundtripDoc.materials.size(), doc->materials.size());
    QCOMPARE(roundtripDoc.bones.size(), doc->bones.size());
    QCOMPARE(roundtripDoc.displayFrames.size(), doc->displayFrames.size());

    for (size_t i = 0; i < doc->vertices.size(); ++i) {
        QCOMPARE(roundtripDoc.vertices[i].position, doc->vertices[i].position);
        QCOMPARE(roundtripDoc.vertices[i].normal, doc->vertices[i].normal);
        QCOMPARE(roundtripDoc.vertices[i].uv, doc->vertices[i].uv);
        QCOMPARE(roundtripDoc.vertices[i].weightType, doc->vertices[i].weightType);
        QCOMPARE(roundtripDoc.vertices[i].boneIndices, doc->vertices[i].boneIndices);
        QCOMPARE(roundtripDoc.vertices[i].boneWeights, doc->vertices[i].boneWeights);
    }

    for (size_t i = 0; i < doc->materials.size(); ++i) {
        QCOMPARE(roundtripDoc.materials[i].nameJp, doc->materials[i].nameJp);
        QCOMPARE(roundtripDoc.materials[i].diffuse, doc->materials[i].diffuse);
        QCOMPARE(roundtripDoc.materials[i].specular, doc->materials[i].specular);
        QCOMPARE(roundtripDoc.materials[i].specularPower, doc->materials[i].specularPower);
        QCOMPARE(roundtripDoc.materials[i].ambient, doc->materials[i].ambient);
        QCOMPARE(roundtripDoc.materials[i].drawFlags, doc->materials[i].drawFlags);
        QCOMPARE(roundtripDoc.materials[i].faceVertexCount, doc->materials[i].faceVertexCount);
    }

    if (!doc->bones.empty()) {
        QCOMPARE(roundtripDoc.bones[0].nameJp, doc->bones[0].nameJp);
        QCOMPARE(roundtripDoc.bones[0].nameEn, doc->bones[0].nameEn);
        QCOMPARE(roundtripDoc.bones[0].position, doc->bones[0].position);
        QCOMPARE(roundtripDoc.bones[0].boneFlags, doc->bones[0].boneFlags);
    }
}

void tst_assimp_import::testInvalidAndCorruptBuffers() {
    QString error;

    // 1. Null data
    auto doc1 = AssimpImporter::importFromMemory(nullptr, 100, "obj", {}, &error);
    QVERIFY(doc1 == nullptr);
    QVERIFY(!error.isEmpty());

    // 2. Zero length
    const char dummy[] = "v 0 0 0\n";
    auto doc2 = AssimpImporter::importFromMemory(dummy, 0, "obj", {}, &error);
    QVERIFY(doc2 == nullptr);
    QVERIFY(!error.isEmpty());

    // 3. Corrupt binary garbage
    const uint8_t garbage[] = {0xFF, 0x00, 0xFE, 0xED, 0xBA, 0xBE, 0x12, 0x34, 0x56, 0x78};
    auto doc3 = AssimpImporter::importFromMemory(garbage, sizeof(garbage), "obj", {}, &error);
    QVERIFY(doc3 == nullptr);
    QVERIFY(!error.isEmpty());

    // 4. Empty file path
    auto doc4 = AssimpImporter::importFile("", {}, &error);
    QVERIFY(doc4 == nullptr);
    QVERIFY(!error.isEmpty());

    // 5. Non-existent file path
    auto doc5 = AssimpImporter::importFile("/non/existent/path/model.obj", {}, &error);
    QVERIFY(doc5 == nullptr);
    QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(tst_assimp_import)
#include "tst_assimp_import.moc"
