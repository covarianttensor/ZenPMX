#include "core/PmxDocument.h"
#include "core/PmxReader.h"
#include "core/PmxTypes.h"
#include "core/PmxWriter.h"

#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <cstring>
#include <limits>

using namespace pmx;

namespace {

void appendLeI32(QByteArray& buf, int32_t val) {
    qint32 le = qToLittleEndian<qint32>(val);
    char bytes[sizeof(qint32)];
    std::memcpy(bytes, &le, sizeof(qint32));
    buf.append(bytes, sizeof(qint32));
}

void writeLeI32At(QByteArray& buf, qsizetype offset, int32_t val) {
    qint32 le = qToLittleEndian<qint32>(val);
    std::memcpy(buf.data() + offset, &le, sizeof(qint32));
}

void writeU8At(QByteArray& buf, qsizetype offset, uint8_t val) {
    buf[offset] = static_cast<char>(val);
}

struct PmxOffsets {
    qsizetype vertexCountOffset = -1;
    qsizetype vertexWeightTypeOffset = -1;
    qsizetype faceVertexCountOffset = -1;
    qsizetype textureCountOffset = -1;
    qsizetype materialCountOffset = -1;
    qsizetype materialSphereModeOffset = -1;
    qsizetype materialFaceVertexCountOffset = -1;
    qsizetype boneCountOffset = -1;
    qsizetype ikLinkCountOffset = -1;
    qsizetype morphCountOffset = -1;
    qsizetype morphTypeOffset = -1;
    qsizetype morphOffsetCountOffset = -1;
    qsizetype displayFrameCountOffset = -1;
    qsizetype displayElementCountOffset = -1;
    qsizetype rigidBodyCountOffset = -1;
    qsizetype jointCountOffset = -1;
    qsizetype softBodyCountOffset = -1;
    qsizetype anchorCountOffset = -1;
    qsizetype pinCountOffset = -1;
};

PmxOffsets findPmxOffsets(const QByteArray& buf) {
    PmxOffsets off;
    size_t cur = 0;
    if (buf.size() < 17) {
        return off;
    }
    // magic (4) + version (4) + count (1)
    cur += 9;
    uint8_t addUv = static_cast<uint8_t>(buf[cur + 1]);
    uint8_t vIdxSize = static_cast<uint8_t>(buf[cur + 2]);
    uint8_t tIdxSize = static_cast<uint8_t>(buf[cur + 3]);
    uint8_t mIdxSize = static_cast<uint8_t>(buf[cur + 4]);
    uint8_t bIdxSize = static_cast<uint8_t>(buf[cur + 5]);
    uint8_t morphIdxSize = static_cast<uint8_t>(buf[cur + 6]);
    uint8_t rbIdxSize = static_cast<uint8_t>(buf[cur + 7]);
    cur += 8;

    auto skipText = [&](size_t& p) {
        if (p + 4 > static_cast<size_t>(buf.size())) return;
        qint32 raw = 0;
        std::memcpy(&raw, buf.constData() + p, 4);
        int32_t len = qFromLittleEndian(raw);
        p += 4;
        if (len > 0) {
            p += static_cast<size_t>(len);
        }
    };

    // skip 4 model info texts
    skipText(cur);
    skipText(cur);
    skipText(cur);
    skipText(cur);

    // vertexCount
    off.vertexCountOffset = static_cast<qsizetype>(cur);
    qint32 rawVCount = 0;
    std::memcpy(&rawVCount, buf.constData() + cur, 4);
    int32_t vCount = qFromLittleEndian(rawVCount);
    cur += 4;
    for (int32_t i = 0; i < vCount; ++i) {
        cur += 12 + 12 + 8 + static_cast<size_t>(addUv) * 16;
        if (i == 0) off.vertexWeightTypeOffset = static_cast<qsizetype>(cur);
        uint8_t wt = static_cast<uint8_t>(buf[cur]);
        cur += 1;
        if (wt == 0) cur += bIdxSize;
        else if (wt == 1) cur += 2 * bIdxSize + 4;
        else if (wt == 2 || wt == 4) cur += 4 * bIdxSize + 16;
        else if (wt == 3) cur += 2 * bIdxSize + 4 + 36;
        cur += 4; // edgeScale
    }

    // faceVertexCount
    off.faceVertexCountOffset = static_cast<qsizetype>(cur);
    qint32 rawFCount = 0;
    std::memcpy(&rawFCount, buf.constData() + cur, 4);
    int32_t fCount = qFromLittleEndian(rawFCount);
    cur += 4 + static_cast<size_t>(fCount) * vIdxSize;

    // textureCount
    off.textureCountOffset = static_cast<qsizetype>(cur);
    qint32 rawTCount = 0;
    std::memcpy(&rawTCount, buf.constData() + cur, 4);
    int32_t tCount = qFromLittleEndian(rawTCount);
    cur += 4;
    for (int32_t i = 0; i < tCount; ++i) {
        skipText(cur);
    }

    // materialCount
    off.materialCountOffset = static_cast<qsizetype>(cur);
    qint32 rawMCount = 0;
    std::memcpy(&rawMCount, buf.constData() + cur, 4);
    int32_t mCount = qFromLittleEndian(rawMCount);
    cur += 4;
    for (int32_t i = 0; i < mCount; ++i) {
        skipText(cur); // nameJp
        skipText(cur); // nameEn
        cur += 16 + 12 + 4 + 12 + 1 + 16 + 4 + 2 * tIdxSize;
        if (i == 0) off.materialSphereModeOffset = static_cast<qsizetype>(cur);
        cur += 1; // sphereMode
        uint8_t sharedToon = static_cast<uint8_t>(buf[cur]);
        cur += 1;
        if (sharedToon == 0) cur += tIdxSize;
        else cur += 1;
        skipText(cur); // memo
        if (i == 0) off.materialFaceVertexCountOffset = static_cast<qsizetype>(cur);
        cur += 4; // faceVertexCount
    }

    // boneCount
    off.boneCountOffset = static_cast<qsizetype>(cur);
    qint32 rawBCount = 0;
    std::memcpy(&rawBCount, buf.constData() + cur, 4);
    int32_t bCount = qFromLittleEndian(rawBCount);
    cur += 4;
    for (int32_t i = 0; i < bCount; ++i) {
        skipText(cur);
        skipText(cur);
        cur += 12 + bIdxSize + 4;
        quint16 rawFlags = 0;
        std::memcpy(&rawFlags, buf.constData() + cur, 2);
        uint16_t flags = qFromLittleEndian(rawFlags);
        cur += 2;
        if ((flags & 0x0001) == 0) cur += 12;
        else cur += bIdxSize;
        if ((flags & (0x0100 | 0x0200)) != 0) cur += bIdxSize + 4;
        if ((flags & 0x0400) != 0) cur += 12;
        if ((flags & 0x0800) != 0) cur += 24;
        if ((flags & 0x2000) != 0) cur += 4;
        if ((flags & 0x0020) != 0) {
            cur += bIdxSize + 4 + 4;
            if (off.ikLinkCountOffset == -1) off.ikLinkCountOffset = static_cast<qsizetype>(cur);
            qint32 rawIkCount = 0;
            std::memcpy(&rawIkCount, buf.constData() + cur, 4);
            int32_t ikCount = qFromLittleEndian(rawIkCount);
            cur += 4;
            for (int32_t l = 0; l < ikCount; ++l) {
                cur += bIdxSize;
                uint8_t hasLimit = static_cast<uint8_t>(buf[cur]);
                cur += 1;
                if (hasLimit != 0) cur += 24;
            }
        }
    }

    // morphCount
    off.morphCountOffset = static_cast<qsizetype>(cur);
    qint32 rawMorphCount = 0;
    std::memcpy(&rawMorphCount, buf.constData() + cur, 4);
    int32_t morphCount = qFromLittleEndian(rawMorphCount);
    cur += 4;
    for (int32_t i = 0; i < morphCount; ++i) {
        skipText(cur);
        skipText(cur);
        cur += 1; // panelCategory
        if (i == 0) off.morphTypeOffset = static_cast<qsizetype>(cur);
        uint8_t mType = static_cast<uint8_t>(buf[cur]);
        cur += 1;
        if (i == 0) off.morphOffsetCountOffset = static_cast<qsizetype>(cur);
        qint32 rawOffCount = 0;
        std::memcpy(&rawOffCount, buf.constData() + cur, 4);
        int32_t offCount = qFromLittleEndian(rawOffCount);
        cur += 4;
        for (int32_t j = 0; j < offCount; ++j) {
            switch (mType) {
            case 0:
            case 9:
                cur += morphIdxSize + 4;
                break;
            case 1:
                cur += vIdxSize + 12;
                break;
            case 2:
                cur += bIdxSize + 12 + 16;
                break;
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                cur += vIdxSize + 16;
                break;
            case 8:
                cur += mIdxSize + 1 + 28 * 4;
                break;
            case 10:
                cur += rbIdxSize + 1 + 24;
                break;
            default:
                break;
            }
        }
    }

    // displayFrames
    off.displayFrameCountOffset = static_cast<qsizetype>(cur);
    qint32 rawDfCount = 0;
    std::memcpy(&rawDfCount, buf.constData() + cur, 4);
    int32_t dfCount = qFromLittleEndian(rawDfCount);
    cur += 4;
    for (int32_t i = 0; i < dfCount; ++i) {
        skipText(cur);
        skipText(cur);
        cur += 1; // specialFlag
        if (i == 0) off.displayElementCountOffset = static_cast<qsizetype>(cur);
        qint32 rawElCount = 0;
        std::memcpy(&rawElCount, buf.constData() + cur, 4);
        int32_t elCount = qFromLittleEndian(rawElCount);
        cur += 4;
        for (int32_t j = 0; j < elCount; ++j) {
            uint8_t tType = static_cast<uint8_t>(buf[cur]);
            cur += 1;
            if (tType == 0) cur += bIdxSize;
            else cur += morphIdxSize;
        }
    }

    // rigidBodies
    off.rigidBodyCountOffset = static_cast<qsizetype>(cur);
    qint32 rawRbCount = 0;
    std::memcpy(&rawRbCount, buf.constData() + cur, 4);
    int32_t rbCount = qFromLittleEndian(rawRbCount);
    cur += 4;
    for (int32_t i = 0; i < rbCount; ++i) {
        skipText(cur);
        skipText(cur);
        cur += bIdxSize + 1 + 2 + 1 + 12 + 12 + 12 + 4 + 4 + 4 + 4 + 4 + 1;
    }

    // joints
    off.jointCountOffset = static_cast<qsizetype>(cur);
    qint32 rawJCount = 0;
    std::memcpy(&rawJCount, buf.constData() + cur, 4);
    int32_t jCount = qFromLittleEndian(rawJCount);
    cur += 4;
    for (int32_t i = 0; i < jCount; ++i) {
        skipText(cur);
        skipText(cur);
        cur += 1 + 2 * rbIdxSize + 96;
    }

    // softBodies (if version >= 2.1f)
    if (cur + 4 <= static_cast<size_t>(buf.size())) {
        off.softBodyCountOffset = static_cast<qsizetype>(cur);
        qint32 rawSbCount = 0;
        std::memcpy(&rawSbCount, buf.constData() + cur, 4);
        int32_t sbCount = qFromLittleEndian(rawSbCount);
        cur += 4;
        for (int32_t i = 0; i < sbCount; ++i) {
            skipText(cur);
            skipText(cur);
            cur += 1 + mIdxSize + 1 + 2 + 1 + 4 + 4 + 4 + 4 + 4 + 48 + 24 + 16 + 12;
            if (i == 0) off.anchorCountOffset = static_cast<qsizetype>(cur);
            qint32 rawAncCount = 0;
            std::memcpy(&rawAncCount, buf.constData() + cur, 4);
            int32_t ancCount = qFromLittleEndian(rawAncCount);
            cur += 4 + static_cast<size_t>(ancCount) * (rbIdxSize + vIdxSize + 1);

            if (i == 0) off.pinCountOffset = static_cast<qsizetype>(cur);
            qint32 rawPinCount = 0;
            std::memcpy(&rawPinCount, buf.constData() + cur, 4);
            int32_t pinCount = qFromLittleEndian(rawPinCount);
            cur += 4 + static_cast<size_t>(pinCount) * vIdxSize;
        }
    }

    return off;
}

PmxDocument createRichPmx20Model() {
    PmxDocument doc;
    doc.header.version = 2.0f;
    doc.header.textEncoding = 0; // UTF-16LE
    doc.header.additionalVec4Count = 2;
    doc.header.vertexIndexSize = 2;
    doc.header.textureIndexSize = 2;
    doc.header.materialIndexSize = 2;
    doc.header.boneIndexSize = 2;
    doc.header.morphIndexSize = 2;
    doc.header.rigidBodyIndexSize = 2;
    doc.header.preserveIndexSizesOnSave = true;

    doc.modelInfo.modelNameJp = QStringLiteral("PMX2.0_高精度検証モデル");
    doc.modelInfo.modelNameEn = QStringLiteral("PMX 2.0 Precision Verification Model");
    doc.modelInfo.commentJp = QStringLiteral("全フィールド網羅テストモデル\r\nBDEF/SDEF/IK/材質モーフ");
    doc.modelInfo.commentEn = QStringLiteral("Full field coverage test model\r\nBDEF/SDEF/IK/Material Morph");

    // 4 Vertices: BDEF1, BDEF2, BDEF4, SDEF
    {
        PmxVertex v0;
        v0.position = glm::vec3(-1.0f, 0.0f, 1.0f);
        v0.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        v0.uv = glm::vec2(0.1f, 0.2f);
        v0.additionalUVs[0] = glm::vec4(1.0f, 2.0f, 3.0f, 4.0f);
        v0.additionalUVs[1] = glm::vec4(5.0f, 6.0f, 7.0f, 8.0f);
        v0.weightType = WeightType::BDEF1;
        v0.boneIndices = {0, -1, -1, -1};
        v0.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
        v0.edgeScale = 1.0f;
        doc.vertices.push_back(v0);
    }
    {
        PmxVertex v1;
        v1.position = glm::vec3(1.0f, 0.0f, 1.0f);
        v1.normal = glm::vec3(0.267f, 0.534f, 0.801f);
        v1.uv = glm::vec2(0.8f, 0.2f);
        v1.additionalUVs[0] = glm::vec4(1.1f, 2.1f, 3.1f, 4.1f);
        v1.additionalUVs[1] = glm::vec4(5.1f, 6.1f, 7.1f, 8.1f);
        v1.weightType = WeightType::BDEF2;
        v1.boneIndices = {0, 1, -1, -1};
        v1.boneWeights = {0.65f, 0.35f, 0.0f, 0.0f};
        v1.edgeScale = 1.25f;
        doc.vertices.push_back(v1);
    }
    {
        PmxVertex v2;
        v2.position = glm::vec3(1.0f, 2.0f, -1.0f);
        v2.normal = glm::vec3(-0.577f, 0.577f, -0.577f);
        v2.uv = glm::vec2(0.8f, 0.9f);
        v2.additionalUVs[0] = glm::vec4(0.2f, 0.3f, 0.4f, 0.5f);
        v2.additionalUVs[1] = glm::vec4(0.6f, 0.7f, 0.8f, 0.9f);
        v2.weightType = WeightType::BDEF4;
        v2.boneIndices = {0, 1, 2, 3};
        v2.boneWeights = {0.4f, 0.3f, 0.2f, 0.1f};
        v2.edgeScale = 0.75f;
        doc.vertices.push_back(v2);
    }
    {
        PmxVertex v3;
        v3.position = glm::vec3(-1.0f, 2.0f, -1.0f);
        v3.normal = glm::vec3(0.0f, -1.0f, 0.0f);
        v3.uv = glm::vec2(0.1f, 0.9f);
        v3.additionalUVs[0] = glm::vec4(2.2f, 3.3f, 4.4f, 5.5f);
        v3.additionalUVs[1] = glm::vec4(6.6f, 7.7f, 8.8f, 9.9f);
        v3.weightType = WeightType::SDEF;
        v3.boneIndices = {1, 2, -1, -1};
        v3.boneWeights = {0.45f, 0.55f, 0.0f, 0.0f};
        v3.sdefC = glm::vec3(0.0f, 1.0f, 0.0f);
        v3.sdefR0 = glm::vec3(0.1f, 0.2f, 0.3f);
        v3.sdefR1 = glm::vec3(-0.1f, -0.2f, -0.3f);
        v3.edgeScale = 1.5f;
        doc.vertices.push_back(v3);
    }

    // 2 Triangles (6 face indices)
    doc.faceIndices = {0, 1, 2, 2, 3, 0};

    // Textures
    doc.textures = {
        QStringLiteral("textures/body_diffuse.png"),
        QStringLiteral("textures/cloth_diffuse.png"),
        QStringLiteral("textures/custom_toon.bmp"),
        QStringLiteral("textures/hair_sphere.sph")
    };

    // Materials:
    // Mat 0: sharedToonFlag == 0, toonIndex = 2 (texture table index)
    {
        PmxMaterial m0;
        m0.nameJp = QStringLiteral("ボディ材質");
        m0.nameEn = QStringLiteral("Body Material");
        m0.diffuse = glm::vec4(0.9f, 0.85f, 0.8f, 1.0f);
        m0.specular = glm::vec3(0.2f, 0.2f, 0.2f);
        m0.specularPower = 10.0f;
        m0.ambient = glm::vec3(0.5f, 0.5f, 0.5f);
        m0.drawFlags = 0x1F;
        m0.edgeColor = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
        m0.edgeSize = 1.0f;
        m0.textureIndex = 0;
        m0.sphereTextureIndex = 3;
        m0.sphereMode = 1; // Mul
        m0.sharedToonFlag = 0;
        m0.toonIndex = 2;
        m0.memo = QStringLiteral("Material 0 memo");
        m0.faceVertexCount = 3;
        doc.materials.push_back(m0);
    }
    // Mat 1: sharedToonFlag == 1, toonIndex = 5 (shared toon06)
    {
        PmxMaterial m1;
        m1.nameJp = QStringLiteral("服材質");
        m1.nameEn = QStringLiteral("Cloth Material");
        m1.diffuse = glm::vec4(0.2f, 0.4f, 0.9f, 0.95f);
        m1.specular = glm::vec3(0.5f, 0.5f, 0.5f);
        m1.specularPower = 20.0f;
        m1.ambient = glm::vec3(0.3f, 0.3f, 0.4f);
        m1.drawFlags = 0x1E;
        m1.edgeColor = glm::vec4(0.0f, 0.0f, 0.5f, 1.0f);
        m1.edgeSize = 1.2f;
        m1.textureIndex = 1;
        m1.sphereTextureIndex = -1; // null
        m1.sphereMode = 0; // None
        m1.sharedToonFlag = 1;
        m1.toonIndex = 5;
        m1.memo = QStringLiteral("Cloth memo");
        m1.faceVertexCount = 3;
        doc.materials.push_back(m1);
    }
    // Mat 2: null texture, null sphere, null toon (-1) with sharedToonFlag = 0
    {
        PmxMaterial m2;
        m2.nameJp = QStringLiteral("透明材質");
        m2.nameEn = QStringLiteral("Transparent Material");
        m2.diffuse = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
        m2.specular = glm::vec3(0.0f);
        m2.specularPower = 1.0f;
        m2.ambient = glm::vec3(0.0f);
        m2.drawFlags = 0x01;
        m2.edgeColor = glm::vec4(0.0f);
        m2.edgeSize = 0.0f;
        m2.textureIndex = -1;
        m2.sphereTextureIndex = -1;
        m2.sphereMode = 3; // SubTexture
        m2.sharedToonFlag = 0;
        m2.toonIndex = -1;
        m2.memo = QStringLiteral("Empty material");
        m2.faceVertexCount = 0;
        doc.materials.push_back(m2);
    }

    // Bones (8 bones covering all conditional boneFlags)
    {
        PmxBone b0;
        b0.nameJp = QStringLiteral("全ての親");
        b0.nameEn = QStringLiteral("Root");
        b0.position = glm::vec3(0.0f, 0.0f, 0.0f);
        b0.parentBoneIndex = -1;
        b0.deformTier = 0;
        b0.boneFlags = 0x0001; // TailIsBone = 1
        b0.tailBoneIndex = 1;
        doc.bones.push_back(b0);
    }
    {
        PmxBone b1;
        b1.nameJp = QStringLiteral("センター");
        b1.nameEn = QStringLiteral("Center");
        b1.position = glm::vec3(0.0f, 10.0f, 0.0f);
        b1.parentBoneIndex = 0;
        b1.deformTier = 0;
        b1.boneFlags = 0x0000; // TailIsBone = 0
        b1.tailOffset = glm::vec3(0.0f, 5.0f, 0.0f);
        doc.bones.push_back(b1);
    }
    {
        PmxBone b2;
        b2.nameJp = QStringLiteral("付与ボーン");
        b2.nameEn = QStringLiteral("GrantBone");
        b2.position = glm::vec3(2.0f, 10.0f, 0.0f);
        b2.parentBoneIndex = 1;
        b2.deformTier = 1;
        b2.boneFlags = 0x0100 | 0x0200; // InheritRotation | InheritTranslation
        b2.tailOffset = glm::vec3(1.0f, 0.0f, 0.0f);
        b2.inheritParentIndex = 1;
        b2.inheritWeight = 0.75f;
        doc.bones.push_back(b2);
    }
    {
        PmxBone b3;
        b3.nameJp = QStringLiteral("軸固定ボーン");
        b3.nameEn = QStringLiteral("FixedAxisBone");
        b3.position = glm::vec3(-2.0f, 10.0f, 0.0f);
        b3.parentBoneIndex = 1;
        b3.deformTier = 0;
        b3.boneFlags = 0x0400; // FixedAxis
        b3.tailOffset = glm::vec3(-1.0f, 0.0f, 0.0f);
        b3.fixedAxisVector = glm::vec3(0.0f, 1.0f, 0.0f);
        doc.bones.push_back(b3);
    }
    {
        PmxBone b4;
        b4.nameJp = QStringLiteral("ローカル軸ボーン");
        b4.nameEn = QStringLiteral("LocalAxisBone");
        b4.position = glm::vec3(0.0f, 15.0f, 0.0f);
        b4.parentBoneIndex = 1;
        b4.deformTier = 0;
        b4.boneFlags = 0x0800; // LocalAxis
        b4.tailOffset = glm::vec3(0.0f, 2.0f, 0.0f);
        b4.localAxisX = glm::vec3(1.0f, 0.0f, 0.0f);
        b4.localAxisZ = glm::vec3(0.0f, 0.0f, 1.0f);
        doc.bones.push_back(b4);
    }
    {
        PmxBone b5;
        b5.nameJp = QStringLiteral("外部親ボーン");
        b5.nameEn = QStringLiteral("ExternalParentBone");
        b5.position = glm::vec3(0.0f, 18.0f, 0.0f);
        b5.parentBoneIndex = 1;
        b5.deformTier = 0;
        b5.boneFlags = 0x2000; // ExternalParent
        b5.tailOffset = glm::vec3(0.0f, 1.0f, 0.0f);
        b5.externalKey = 4242;
        doc.bones.push_back(b5);
    }
    {
        PmxBone b6;
        b6.nameJp = QStringLiteral("足先");
        b6.nameEn = QStringLiteral("FootTip");
        b6.position = glm::vec3(0.0f, 1.0f, 2.0f);
        b6.parentBoneIndex = 1;
        b6.deformTier = 0;
        b6.boneFlags = 0x0000;
        b6.tailOffset = glm::vec3(0.0f, 0.0f, 1.0f);
        doc.bones.push_back(b6);
    }
    {
        PmxBone b7;
        b7.nameJp = QStringLiteral("左足ＩＫ");
        b7.nameEn = QStringLiteral("LegIK_L");
        b7.position = glm::vec3(1.0f, 1.0f, 2.0f);
        b7.parentBoneIndex = 0;
        b7.deformTier = 0;
        b7.boneFlags = 0x0020; // IK
        b7.tailOffset = glm::vec3(0.0f, 0.0f, 1.0f);
        b7.ikTargetBoneIndex = 6;
        b7.ikLoopCount = 40;
        b7.ikLimitAngle = 1.5707963f;
        PmxIkLink link0;
        link0.boneIndex = 1;
        link0.hasAngleLimit = 1;
        link0.limitMin = glm::vec3(-1.5f, 0.0f, 0.0f);
        link0.limitMax = glm::vec3(0.0f, 0.0f, 0.0f);
        PmxIkLink link1;
        link1.boneIndex = 0;
        link1.hasAngleLimit = 0;
        b7.ikLinks = {link0, link1};
        doc.bones.push_back(b7);
    }

    // Morphs: Group, Vertex, Bone, UV, AddUV1, Material (28 floats)
    {
        PmxMorph m0;
        m0.nameJp = QStringLiteral("グループモーフ");
        m0.nameEn = QStringLiteral("GroupMorph");
        m0.panelCategory = 4;
        m0.morphType = 0; // Group
        m0.groupOffsets = {
            GroupMorphOffset{1, 0.75f},
            GroupMorphOffset{2, 0.50f}
        };
        doc.morphs.push_back(m0);
    }
    {
        PmxMorph m1;
        m1.nameJp = QStringLiteral("まばたき");
        m1.nameEn = QStringLiteral("Blink");
        m1.panelCategory = 2; // Eye
        m1.morphType = 1; // Vertex
        m1.vertexOffsets = {
            VertexMorphOffset{0, glm::vec3(0.0f, -0.2f, 0.05f)},
            VertexMorphOffset{1, glm::vec3(0.0f, -0.2f, 0.05f)}
        };
        doc.morphs.push_back(m1);
    }
    {
        PmxMorph m2;
        m2.nameJp = QStringLiteral("ボーンモーフ");
        m2.nameEn = QStringLiteral("BoneMorph");
        m2.panelCategory = 4;
        m2.morphType = 2; // Bone
        m2.boneOffsets = {
            BoneMorphOffset{1, glm::vec3(0.0f, 1.5f, 0.0f), glm::quat(0.92388f, 0.0f, 0.38268f, 0.0f)},
            BoneMorphOffset{2, glm::vec3(0.5f, 0.0f, -0.5f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f)}
        };
        doc.morphs.push_back(m2);
    }
    {
        PmxMorph m3;
        m3.nameJp = QStringLiteral("UVモーフ");
        m3.nameEn = QStringLiteral("UvMorph");
        m3.panelCategory = 3;
        m3.morphType = 3; // UV
        m3.uvOffsets = {
            UvMorphOffset{0, glm::vec4(0.1f, 0.2f, 0.0f, 0.0f)},
            UvMorphOffset{1, glm::vec4(0.3f, 0.4f, 0.0f, 0.0f)}
        };
        doc.morphs.push_back(m3);
    }
    {
        PmxMorph m4;
        m4.nameJp = QStringLiteral("追加UV1モーフ");
        m4.nameEn = QStringLiteral("AddUv1Morph");
        m4.panelCategory = 4;
        m4.morphType = 4; // AddUV1
        m4.uvOffsets = {
            UvMorphOffset{2, glm::vec4(0.5f, 0.6f, 0.7f, 0.8f)}
        };
        doc.morphs.push_back(m4);
    }
    {
        PmxMorph m5;
        m5.nameJp = QStringLiteral("材質モーフ");
        m5.nameEn = QStringLiteral("MaterialMorph");
        m5.panelCategory = 4;
        m5.morphType = 8; // Material
        MaterialMorphOffset mo0;
        mo0.materialIndex = 0;
        mo0.opType = 0; // Multiply
        mo0.diffuse = glm::vec4(0.8f, 0.7f, 0.6f, 0.9f);
        mo0.specular = glm::vec3(0.5f, 0.4f, 0.3f);
        mo0.specularPower = 1.5f;
        mo0.ambient = glm::vec3(0.9f, 0.8f, 0.7f);
        mo0.edgeColor = glm::vec4(1.0f, 0.5f, 0.2f, 1.0f);
        mo0.edgeSize = 0.8f;
        mo0.textureTint = glm::vec4(1.0f, 0.9f, 0.8f, 1.0f);
        mo0.sphereTint = glm::vec4(0.7f, 0.7f, 0.7f, 1.0f);
        mo0.toonTint = glm::vec4(0.95f, 0.95f, 0.95f, 1.0f);

        MaterialMorphOffset mo1;
        mo1.materialIndex = -1; // All materials
        mo1.opType = 1; // Add
        mo1.diffuse = glm::vec4(0.1f, 0.2f, 0.3f, 0.0f);
        mo1.specular = glm::vec3(0.05f, 0.05f, 0.05f);
        mo1.specularPower = 2.0f;
        mo1.ambient = glm::vec3(0.1f, 0.1f, 0.1f);
        mo1.edgeColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.0f);
        mo1.edgeSize = 0.5f;
        mo1.textureTint = glm::vec4(0.0f, 0.1f, 0.2f, 0.0f);
        mo1.sphereTint = glm::vec4(0.1f, 0.0f, 0.1f, 0.0f);
        mo1.toonTint = glm::vec4(0.05f, 0.05f, 0.0f, 0.0f);

        m5.materialOffsets = {mo0, mo1};
        doc.morphs.push_back(m5);
    }

    // Display Frames
    {
        PmxDisplayFrame f0;
        f0.nameJp = QStringLiteral("Root");
        f0.nameEn = QStringLiteral("Root");
        f0.specialFlag = 1;
        f0.elements = {PmxDisplayElement{0, 0}}; // Bone 0
        doc.displayFrames.push_back(f0);
    }
    {
        PmxDisplayFrame f1;
        f1.nameJp = QStringLiteral("表情");
        f1.nameEn = QStringLiteral("Expression");
        f1.specialFlag = 1;
        f1.elements = {
            PmxDisplayElement{1, 0}, // Morph 0
            PmxDisplayElement{1, 1}  // Morph 1
        };
        doc.displayFrames.push_back(f1);
    }
    {
        PmxDisplayFrame f2;
        f2.nameJp = QStringLiteral("センター枠");
        f2.nameEn = QStringLiteral("CenterFrame");
        f2.specialFlag = 0;
        f2.elements = {
            PmxDisplayElement{0, 1}, // Bone 1
            PmxDisplayElement{0, 2}  // Bone 2
        };
        doc.displayFrames.push_back(f2);
    }

    // Rigid Bodies: Sphere, Box, Capsule
    {
        PmxRigidBody rb0;
        rb0.nameJp = QStringLiteral("剛体0");
        rb0.nameEn = QStringLiteral("RigidBody0");
        rb0.boneIndex = 0;
        rb0.groupIndex = 0;
        rb0.nonCollisionMask = 0xFFFE;
        rb0.shape = 0; // Sphere
        rb0.size = glm::vec3(1.0f, 0.0f, 0.0f);
        rb0.position = glm::vec3(0.0f, 1.0f, 0.0f);
        rb0.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        rb0.mass = 1.0f;
        rb0.linearDamping = 0.1f;
        rb0.angularDamping = 0.1f;
        rb0.restitution = 0.0f;
        rb0.friction = 0.5f;
        rb0.physicsMode = 0; // FollowBone
        doc.rigidBodies.push_back(rb0);
    }
    {
        PmxRigidBody rb1;
        rb1.nameJp = QStringLiteral("剛体1");
        rb1.nameEn = QStringLiteral("RigidBody1");
        rb1.boneIndex = 1;
        rb1.groupIndex = 1;
        rb1.nonCollisionMask = 0xFFFD;
        rb1.shape = 1; // Box
        rb1.size = glm::vec3(2.0f, 3.0f, 4.0f);
        rb1.position = glm::vec3(0.0f, 10.0f, 0.0f);
        rb1.rotation = glm::vec3(0.1f, 0.2f, 0.3f);
        rb1.mass = 5.0f;
        rb1.linearDamping = 0.2f;
        rb1.angularDamping = 0.3f;
        rb1.restitution = 0.4f;
        rb1.friction = 0.8f;
        rb1.physicsMode = 1; // Physics
        doc.rigidBodies.push_back(rb1);
    }
    {
        PmxRigidBody rb2;
        rb2.nameJp = QStringLiteral("剛体2");
        rb2.nameEn = QStringLiteral("RigidBody2");
        rb2.boneIndex = 2;
        rb2.groupIndex = 2;
        rb2.nonCollisionMask = 0xFFFB;
        rb2.shape = 2; // Capsule
        rb2.size = glm::vec3(0.5f, 2.0f, 0.0f);
        rb2.position = glm::vec3(2.0f, 10.0f, 0.0f);
        rb2.rotation = glm::vec3(-0.1f, -0.2f, 0.0f);
        rb2.mass = 2.0f;
        rb2.linearDamping = 0.15f;
        rb2.angularDamping = 0.25f;
        rb2.restitution = 0.1f;
        rb2.friction = 0.6f;
        rb2.physicsMode = 2; // PhysicsAndBone
        doc.rigidBodies.push_back(rb2);
    }

    // Joints: Spring6DoF
    {
        PmxJoint j0;
        j0.nameJp = QStringLiteral("ジョイント0");
        j0.nameEn = QStringLiteral("Joint0");
        j0.jointType = 0; // Spring6DoF
        j0.rigidBodyIndexA = 0;
        j0.rigidBodyIndexB = 1;
        j0.position = glm::vec3(0.0f, 5.0f, 0.0f);
        j0.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        j0.linearLowerLimit = glm::vec3(-1.0f, -1.0f, -1.0f);
        j0.linearUpperLimit = glm::vec3(1.0f, 1.0f, 1.0f);
        j0.angularLowerLimit = glm::vec3(-0.5f, -0.5f, -0.5f);
        j0.angularUpperLimit = glm::vec3(0.5f, 0.5f, 0.5f);
        j0.springLinearStiffness = glm::vec3(100.0f, 100.0f, 100.0f);
        j0.springAngularStiffness = glm::vec3(50.0f, 50.0f, 50.0f);
        doc.joints.push_back(j0);
    }

    return doc;
}

PmxDocument createRichPmx21Model() {
    PmxDocument doc = createRichPmx20Model();
    doc.header.version = 2.1f;
    doc.header.textEncoding = 1; // UTF-8

    // Add QDEF vertex
    {
        PmxVertex vQdef;
        vQdef.position = glm::vec3(0.0f, 3.0f, 0.0f);
        vQdef.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        vQdef.uv = glm::vec2(0.5f, 0.5f);
        vQdef.additionalUVs[0] = glm::vec4(0.1f, 0.2f, 0.3f, 0.4f);
        vQdef.additionalUVs[1] = glm::vec4(0.5f, 0.6f, 0.7f, 0.8f);
        vQdef.weightType = WeightType::QDEF;
        vQdef.boneIndices = {0, 1, 2, 3};
        vQdef.boneWeights = {0.25f, 0.25f, 0.25f, 0.25f};
        vQdef.edgeScale = 1.1f;
        doc.vertices.push_back(vQdef);
    }

    // Add Flip (9) and Impulse (10) morphs
    {
        PmxMorph mFlip;
        mFlip.nameJp = QStringLiteral("フリップモーフ");
        mFlip.nameEn = QStringLiteral("FlipMorph");
        mFlip.panelCategory = 4;
        mFlip.morphType = 9; // Flip
        mFlip.groupOffsets = {
            GroupMorphOffset{0, 0.8f}
        };
        doc.morphs.push_back(mFlip);
    }
    {
        PmxMorph mImpulse;
        mImpulse.nameJp = QStringLiteral("インパルスモーフ");
        mImpulse.nameEn = QStringLiteral("ImpulseMorph");
        mImpulse.panelCategory = 4;
        mImpulse.morphType = 10; // Impulse
        mImpulse.impulseOffsets = {
            ImpulseMorphOffset{0, 1, glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(0.1f, 0.2f, 0.3f)}
        };
        doc.morphs.push_back(mImpulse);
    }

    // Add non-zero Joint types
    {
        PmxJoint j1;
        j1.nameJp = QStringLiteral("6DoFジョイント");
        j1.nameEn = QStringLiteral("6DoFJoint");
        j1.jointType = 1; // 6DoF
        j1.rigidBodyIndexA = 1;
        j1.rigidBodyIndexB = 2;
        j1.position = glm::vec3(1.0f, 10.0f, 0.0f);
        doc.joints.push_back(j1);
    }
    {
        PmxJoint j2;
        j2.nameJp = QStringLiteral("P2Pジョイント");
        j2.nameEn = QStringLiteral("P2PJoint");
        j2.jointType = 2; // P2P
        j2.rigidBodyIndexA = 0;
        j2.rigidBodyIndexB = 2;
        j2.position = glm::vec3(0.0f, 2.0f, 0.0f);
        doc.joints.push_back(j2);
    }

    // Add SoftBody with anchors and pinVertexIndices
    {
        PmxSoftBody sb;
        sb.nameJp = QStringLiteral("布ソフトボディ");
        sb.nameEn = QStringLiteral("ClothSoftBody");
        sb.shape = 0;
        sb.materialIndex = 1;
        sb.groupIndex = 3;
        sb.nonCollisionMask = 0xFFF7;
        sb.flags = 0x05;
        sb.blinkDistance = 10;
        sb.clusterCount = 8;
        sb.totalMass = 3.5f;
        sb.collisionMargin = 0.04f;
        sb.aeroModel = 2;
        sb.configFloats = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f};
        sb.clusterFloats = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
        sb.iterationInts = {16, 8, 4, 2};
        sb.materialInts = {100, 200, 300};
        sb.anchors = {
            PmxSoftBodyAnchor{0, 0, 1},
            PmxSoftBodyAnchor{1, 1, 0}
        };
        sb.pinVertexIndices = {0, 1, 2};
        doc.softBodies.push_back(sb);
    }

    return doc;
}

} // namespace

class TestPmxRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void testPmx20HeaderAndInfoRoundTripUtf16Le() {
        PmxDocument original;
        original.header.version = 2.0f;
        original.header.textEncoding = 0; // UTF-16LE
        original.header.additionalVec4Count = 2;
        original.header.vertexIndexSize = 1;
        original.header.textureIndexSize = 2;
        original.header.materialIndexSize = 1;
        original.header.boneIndexSize = 4;
        original.header.morphIndexSize = 2;
        original.header.rigidBodyIndexSize = 4;
        original.header.preserveIndexSizesOnSave = true;

        original.modelInfo.modelNameJp = QStringLiteral("初音ミク_テストモデル");
        original.modelInfo.modelNameEn = QStringLiteral("Hatsune Miku Test Model");
        original.modelInfo.commentJp = QStringLiteral("1行目のコメント\r\n2行目の日本語コメント（UTF-16LE検証）");
        original.modelInfo.commentEn = QStringLiteral("Line 1 comment\r\nLine 2 English comment (UTF-16LE verification)");

        QByteArray inputBytes;
        QString writeErr;
        QVERIFY2(PmxWriter::writeToBuffer(original, inputBytes, &writeErr), qPrintable(writeErr));
        QVERIFY(!inputBytes.isEmpty());

        PmxDocument parsed;
        QString readErr;
        QVERIFY2(PmxReader::readFromBuffer(inputBytes, parsed, &readErr), qPrintable(readErr));

        QCOMPARE(parsed.header.version, 2.0f);
        QCOMPARE(parsed.header.textEncoding, uint8_t(0));
        QCOMPARE(parsed.header.additionalVec4Count, uint8_t(2));
        QCOMPARE(parsed.header.vertexIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.textureIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.materialIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.boneIndexSize, uint8_t(4));
        QCOMPARE(parsed.header.morphIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.rigidBodyIndexSize, uint8_t(4));
        QCOMPARE(parsed.header.preserveIndexSizesOnSave, true);

        QCOMPARE(parsed.modelInfo.modelNameJp, original.modelInfo.modelNameJp);
        QCOMPARE(parsed.modelInfo.modelNameEn, original.modelInfo.modelNameEn);
        QCOMPARE(parsed.modelInfo.commentJp, original.modelInfo.commentJp);
        QCOMPARE(parsed.modelInfo.commentEn, original.modelInfo.commentEn);

        QByteArray outputBytes;
        QVERIFY2(PmxWriter::writeToBuffer(parsed, outputBytes, &writeErr), qPrintable(writeErr));
        QCOMPARE(outputBytes, inputBytes);
    }

    void testPmx21HeaderAndInfoRoundTripUtf8() {
        PmxDocument original;
        original.header.version = 2.1f;
        original.header.textEncoding = 1; // UTF-8
        original.header.additionalVec4Count = 4;
        original.header.vertexIndexSize = 4;
        original.header.textureIndexSize = 1;
        original.header.materialIndexSize = 2;
        original.header.boneIndexSize = 2;
        original.header.morphIndexSize = 1;
        original.header.rigidBodyIndexSize = 2;
        original.header.preserveIndexSizesOnSave = true;

        original.modelInfo.modelNameJp = QStringLiteral("PMX2.1_標準テストモデル");
        original.modelInfo.modelNameEn = QStringLiteral("PMX 2.1 Standard Test Model");
        original.modelInfo.commentJp = QStringLiteral("UTF-8 エンコーディング\nSoftBodyセクション含む");
        original.modelInfo.commentEn = QStringLiteral("UTF-8 Encoding\nIncludes trailing SoftBody count");

        QByteArray inputBytes;
        QString err;
        QVERIFY2(PmxWriter::writeToBuffer(original, inputBytes, &err), qPrintable(err));

        PmxDocument parsed;
        QVERIFY2(PmxReader::readFromBuffer(inputBytes, parsed, &err), qPrintable(err));

        QCOMPARE(parsed.header.version, 2.1f);
        QCOMPARE(parsed.header.textEncoding, uint8_t(1));
        QCOMPARE(parsed.header.additionalVec4Count, uint8_t(4));
        QCOMPARE(parsed.header.vertexIndexSize, uint8_t(4));
        QCOMPARE(parsed.header.textureIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.materialIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.boneIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.morphIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.rigidBodyIndexSize, uint8_t(2));
        QCOMPARE(parsed.softBodies.size(), size_t(0));

        QCOMPARE(parsed.modelInfo.modelNameJp, original.modelInfo.modelNameJp);
        QCOMPARE(parsed.modelInfo.modelNameEn, original.modelInfo.modelNameEn);
        QCOMPARE(parsed.modelInfo.commentJp, original.modelInfo.commentJp);
        QCOMPARE(parsed.modelInfo.commentEn, original.modelInfo.commentEn);

        QByteArray outputBytes;
        QVERIFY2(PmxWriter::writeToBuffer(parsed, outputBytes, &err), qPrintable(err));
        QCOMPARE(outputBytes, inputBytes);

        // Also verify file I/O round-trip
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString tempFilePath = tempDir.filePath(QStringLiteral("roundtrip_21.pmx"));
        QVERIFY2(PmxWriter::writeToFile(tempFilePath, parsed, &err), qPrintable(err));

        PmxDocument fromFile;
        QVERIFY2(PmxReader::readFromFile(tempFilePath, fromFile, &err), qPrintable(err));
        QCOMPARE(fromFile.sourceFilePath, tempFilePath);

        QByteArray fileRoundTripBytes;
        QVERIFY2(PmxWriter::writeToBuffer(fromFile, fileRoundTripBytes, &err), qPrintable(err));
        QCOMPARE(fileRoundTripBytes, inputBytes);
    }

    void testVertexVsNonVertexIndexSignednessLaw() {
        QString err;

        // 1. Non-Vertex Index: -1 must encode to 0xFF (1B), 0xFFFF (2B), 0xFFFFFFFF (4B) and decode back to -1
        {
            QByteArray buf1;
            QVERIFY2(PmxWriter::writeNonVertexIndex(buf1, -1, 1, &err), qPrintable(err));
            QCOMPARE(buf1.size(), qsizetype(1));
            QCOMPARE(static_cast<uint8_t>(buf1[0]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readNonVertexIndex(buf1, offset, 1, decoded, &err), qPrintable(err));
            QCOMPARE(offset, size_t(1));
            QCOMPARE(decoded, -1);
        }
        {
            QByteArray buf2;
            QVERIFY2(PmxWriter::writeNonVertexIndex(buf2, -1, 2, &err), qPrintable(err));
            QCOMPARE(buf2.size(), qsizetype(2));
            QCOMPARE(static_cast<uint8_t>(buf2[0]), uint8_t(0xFF));
            QCOMPARE(static_cast<uint8_t>(buf2[1]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readNonVertexIndex(buf2, offset, 2, decoded, &err), qPrintable(err));
            QCOMPARE(offset, size_t(2));
            QCOMPARE(decoded, -1);
        }
        {
            QByteArray buf4;
            QVERIFY2(PmxWriter::writeNonVertexIndex(buf4, -1, 4, &err), qPrintable(err));
            QCOMPARE(buf4.size(), qsizetype(4));
            for (int i = 0; i < 4; ++i) {
                QCOMPARE(static_cast<uint8_t>(buf4[i]), uint8_t(0xFF));
            }

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readNonVertexIndex(buf4, offset, 4, decoded, &err), qPrintable(err));
            QCOMPARE(offset, size_t(4));
            QCOMPARE(decoded, -1);
        }

        // 2. Vertex Index: 0xFF (size 1) MUST decode to +255, and 0xFFFF (size 2) MUST decode to +65535
        {
            QByteArray vbuf1;
            QVERIFY2(PmxWriter::writeVertexIndex(vbuf1, 255, 1, &err), qPrintable(err));
            QCOMPARE(vbuf1.size(), qsizetype(1));
            QCOMPARE(static_cast<uint8_t>(vbuf1[0]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readVertexIndex(vbuf1, offset, 1, decoded, &err), qPrintable(err));
            QCOMPARE(decoded, 255);
        }
        {
            QByteArray vbuf2;
            QVERIFY2(PmxWriter::writeVertexIndex(vbuf2, 65535, 2, &err), qPrintable(err));
            QCOMPARE(vbuf2.size(), qsizetype(2));
            QCOMPARE(static_cast<uint8_t>(vbuf2[0]), uint8_t(0xFF));
            QCOMPARE(static_cast<uint8_t>(vbuf2[1]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readVertexIndex(vbuf2, offset, 2, decoded, &err), qPrintable(err));
            QCOMPARE(decoded, 65535);
        }
        {
            QByteArray vbuf4;
            QVERIFY2(PmxWriter::writeVertexIndex(vbuf4, 123456789, 4, &err), qPrintable(err));
            QCOMPARE(vbuf4.size(), qsizetype(4));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readVertexIndex(vbuf4, offset, 4, decoded, &err), qPrintable(err));
            QCOMPARE(decoded, 123456789);
        }

        // 3. Boundary & out-of-range checks for Vertex vs Non-Vertex indices
        {
            QByteArray dummy;
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, -1, 1));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, 256, 1));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, -1, 2));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, 65536, 2));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, -1, 4));

            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, -129, 1));
            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, 128, 1));
            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, -32769, 2));
            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, 32768, 2));
        }
    }

    void testAdversarialMalformedStreams() {
        PmxDocument validDoc;
        validDoc.header.version = 2.0f;
        validDoc.header.textEncoding = 0;
        validDoc.header.preserveIndexSizesOnSave = true;
        validDoc.modelInfo.modelNameJp = QStringLiteral("テスト");

        QByteArray validBytes;
        QVERIFY(PmxWriter::writeToBuffer(validDoc, validBytes));

        PmxDocument outDoc;

        // 1. Empty buffer
        QVERIFY(!PmxReader::readFromBuffer(QByteArray(), outDoc));

        // 2. Truncated magic & truncated header at every byte position up to validBytes.size() - 1
        for (qsizetype len = 1; len < validBytes.size(); ++len) {
            QByteArray truncated = validBytes.left(len);
            QVERIFY2(!PmxReader::readFromBuffer(truncated, outDoc),
                     qPrintable(QStringLiteral("Expected failure on truncated buffer of length %1").arg(len)));
        }

        // 3. Wrong magic ("PMD ")
        {
            QByteArray badMagic = validBytes;
            badMagic[0] = 'P';
            badMagic[1] = 'M';
            badMagic[2] = 'D';
            badMagic[3] = ' ';
            QVERIFY(!PmxReader::readFromBuffer(badMagic, outDoc));
        }

        // 4. Unsupported versions (1.0f, 3.0f, NaN)
        for (float badVer : {1.0f, 3.0f, std::numeric_limits<float>::quiet_NaN()}) {
            QByteArray badVersionBytes = validBytes;
            quint32 bits = 0;
            std::memcpy(&bits, &badVer, sizeof(float));
            bits = qToLittleEndian(bits);
            std::memcpy(badVersionBytes.data() + 4, &bits, sizeof(uint32_t));
            QVERIFY(!PmxReader::readFromBuffer(badVersionBytes, outDoc));
        }

        // 5. globalsCount != 8
        for (uint8_t badGlobalsCount : {uint8_t(0), uint8_t(7), uint8_t(9), uint8_t(255)}) {
            QByteArray badGlobals = validBytes;
            badGlobals[8] = static_cast<char>(badGlobalsCount);
            QVERIFY(!PmxReader::readFromBuffer(badGlobals, outDoc));
        }

        // 6. Invalid textEncoding (2) and invalid additionalVec4Count (5)
        {
            QByteArray badEncoding = validBytes;
            badEncoding[9] = 2;
            QVERIFY(!PmxReader::readFromBuffer(badEncoding, outDoc));

            QByteArray badAddUv = validBytes;
            badAddUv[10] = 5;
            QVERIFY(!PmxReader::readFromBuffer(badAddUv, outDoc));
        }

        // 7. Invalid index byte sizes (0, 3, 8) across all 6 index size slots (globals[2..7] at offsets 11..16)
        for (int slot = 11; slot <= 16; ++slot) {
            for (uint8_t badIdxSize : {uint8_t(0), uint8_t(3), uint8_t(8)}) {
                QByteArray mutated = validBytes;
                mutated[slot] = static_cast<char>(badIdxSize);
                QVERIFY(!PmxReader::readFromBuffer(mutated, outDoc));
            }
        }

        // 8. Negative string length (at offset 17, first PmxText byteLength)
        {
            QByteArray negStr = validBytes.left(17);
            appendLeI32(negStr, -4);
            QVERIFY(!PmxReader::readFromBuffer(negStr, outDoc));
        }

        // 9. Odd UTF-16LE byteLength (textEncoding == 0, byteLength == 3)
        {
            QByteArray oddUtf16 = validBytes.left(17);
            appendLeI32(oddUtf16, 3);
            oddUtf16.append("abc", 3);
            QVERIFY(!PmxReader::readFromBuffer(oddUtf16, outDoc));
        }

        // 10. String byteLength exceeding buffer end
        {
            QByteArray hugeStr = validBytes.left(17);
            appendLeI32(hugeStr, 4096);
            hugeStr.append("abcd", 4);
            QVERIFY(!PmxReader::readFromBuffer(hugeStr, outDoc));
        }
    }

    void testComprehensivePmx20FullModelRoundTrip() {
        const PmxDocument original = createRichPmx20Model();
        QByteArray originalBytes;
        QString err;
        QVERIFY2(PmxWriter::writeToBuffer(original, originalBytes, &err), qPrintable(err));
        QVERIFY(!originalBytes.isEmpty());

        PmxDocument parsed;
        QVERIFY2(PmxReader::readFromBuffer(originalBytes, parsed, &err), qPrintable(err));

        // Field equality checks
        QCOMPARE(parsed.header.version, original.header.version);
        QCOMPARE(parsed.header.textEncoding, original.header.textEncoding);
        QCOMPARE(parsed.header.additionalVec4Count, original.header.additionalVec4Count);
        QCOMPARE(parsed.header.vertexIndexSize, original.header.vertexIndexSize);
        QCOMPARE(parsed.header.textureIndexSize, original.header.textureIndexSize);
        QCOMPARE(parsed.header.materialIndexSize, original.header.materialIndexSize);
        QCOMPARE(parsed.header.boneIndexSize, original.header.boneIndexSize);
        QCOMPARE(parsed.header.morphIndexSize, original.header.morphIndexSize);
        QCOMPARE(parsed.header.rigidBodyIndexSize, original.header.rigidBodyIndexSize);

        QCOMPARE(parsed.modelInfo.modelNameJp, original.modelInfo.modelNameJp);
        QCOMPARE(parsed.modelInfo.modelNameEn, original.modelInfo.modelNameEn);
        QCOMPARE(parsed.modelInfo.commentJp, original.modelInfo.commentJp);
        QCOMPARE(parsed.modelInfo.commentEn, original.modelInfo.commentEn);

        // Vertices
        QCOMPARE(parsed.vertices.size(), original.vertices.size());
        for (size_t i = 0; i < original.vertices.size(); ++i) {
            const auto& vo = original.vertices[i];
            const auto& vp = parsed.vertices[i];
            QCOMPARE(vp.position, vo.position);
            QCOMPARE(vp.normal, vo.normal);
            QCOMPARE(vp.uv, vo.uv);
            for (uint8_t u = 0; u < original.header.additionalVec4Count; ++u) {
                QCOMPARE(vp.additionalUVs[u], vo.additionalUVs[u]);
            }
            QCOMPARE(vp.weightType, vo.weightType);
            QCOMPARE(vp.boneIndices, vo.boneIndices);
            for (int b = 0; b < 4; ++b) {
                QCOMPARE(vp.boneWeights[b], vo.boneWeights[b]);
            }
            if (vo.weightType == WeightType::SDEF) {
                QCOMPARE(vp.sdefC, vo.sdefC);
                QCOMPARE(vp.sdefR0, vo.sdefR0);
                QCOMPARE(vp.sdefR1, vo.sdefR1);
            }
            QCOMPARE(vp.edgeScale, vo.edgeScale);
        }

        // Face Indices
        QCOMPARE(parsed.faceIndices, original.faceIndices);

        // Textures
        QCOMPARE(parsed.textures, original.textures);

        // Materials
        QCOMPARE(parsed.materials.size(), original.materials.size());
        for (size_t i = 0; i < original.materials.size(); ++i) {
            const auto& mo = original.materials[i];
            const auto& mp = parsed.materials[i];
            QCOMPARE(mp.nameJp, mo.nameJp);
            QCOMPARE(mp.nameEn, mo.nameEn);
            QCOMPARE(mp.diffuse, mo.diffuse);
            QCOMPARE(mp.specular, mo.specular);
            QCOMPARE(mp.specularPower, mo.specularPower);
            QCOMPARE(mp.ambient, mo.ambient);
            QCOMPARE(mp.drawFlags, mo.drawFlags);
            QCOMPARE(mp.edgeColor, mo.edgeColor);
            QCOMPARE(mp.edgeSize, mo.edgeSize);
            QCOMPARE(mp.textureIndex, mo.textureIndex);
            QCOMPARE(mp.sphereTextureIndex, mo.sphereTextureIndex);
            QCOMPARE(mp.sphereMode, mo.sphereMode);
            QCOMPARE(mp.sharedToonFlag, mo.sharedToonFlag);
            QCOMPARE(mp.toonIndex, mo.toonIndex);
            QCOMPARE(mp.memo, mo.memo);
            QCOMPARE(mp.faceVertexCount, mo.faceVertexCount);
        }

        // Bones
        QCOMPARE(parsed.bones.size(), original.bones.size());
        for (size_t i = 0; i < original.bones.size(); ++i) {
            const auto& bo = original.bones[i];
            const auto& bp = parsed.bones[i];
            QCOMPARE(bp.nameJp, bo.nameJp);
            QCOMPARE(bp.nameEn, bo.nameEn);
            QCOMPARE(bp.position, bo.position);
            QCOMPARE(bp.parentBoneIndex, bo.parentBoneIndex);
            QCOMPARE(bp.deformTier, bo.deformTier);
            QCOMPARE(bp.boneFlags, bo.boneFlags);
            if ((bo.boneFlags & 0x0001) == 0) {
                QCOMPARE(bp.tailOffset, bo.tailOffset);
            } else {
                QCOMPARE(bp.tailBoneIndex, bo.tailBoneIndex);
            }
            if ((bo.boneFlags & (0x0100 | 0x0200)) != 0) {
                QCOMPARE(bp.inheritParentIndex, bo.inheritParentIndex);
                QCOMPARE(bp.inheritWeight, bo.inheritWeight);
            }
            if ((bo.boneFlags & 0x0400) != 0) {
                QCOMPARE(bp.fixedAxisVector, bo.fixedAxisVector);
            }
            if ((bo.boneFlags & 0x0800) != 0) {
                QCOMPARE(bp.localAxisX, bo.localAxisX);
                QCOMPARE(bp.localAxisZ, bo.localAxisZ);
            }
            if ((bo.boneFlags & 0x2000) != 0) {
                QCOMPARE(bp.externalKey, bo.externalKey);
            }
            if ((bo.boneFlags & 0x0020) != 0) {
                QCOMPARE(bp.ikTargetBoneIndex, bo.ikTargetBoneIndex);
                QCOMPARE(bp.ikLoopCount, bo.ikLoopCount);
                QCOMPARE(bp.ikLimitAngle, bo.ikLimitAngle);
                QCOMPARE(bp.ikLinks.size(), bo.ikLinks.size());
                for (size_t l = 0; l < bo.ikLinks.size(); ++l) {
                    QCOMPARE(bp.ikLinks[l].boneIndex, bo.ikLinks[l].boneIndex);
                    QCOMPARE(bp.ikLinks[l].hasAngleLimit, bo.ikLinks[l].hasAngleLimit);
                    if (bo.ikLinks[l].hasAngleLimit != 0) {
                        QCOMPARE(bp.ikLinks[l].limitMin, bo.ikLinks[l].limitMin);
                        QCOMPARE(bp.ikLinks[l].limitMax, bo.ikLinks[l].limitMax);
                    }
                }
            }
        }

        // Morphs
        QCOMPARE(parsed.morphs.size(), original.morphs.size());
        for (size_t i = 0; i < original.morphs.size(); ++i) {
            const auto& mo = original.morphs[i];
            const auto& mp = parsed.morphs[i];
            QCOMPARE(mp.nameJp, mo.nameJp);
            QCOMPARE(mp.nameEn, mo.nameEn);
            QCOMPARE(mp.panelCategory, mo.panelCategory);
            QCOMPARE(mp.morphType, mo.morphType);
            switch (mo.morphType) {
            case 0:
            case 9:
                QCOMPARE(mp.groupOffsets.size(), mo.groupOffsets.size());
                for (size_t j = 0; j < mo.groupOffsets.size(); ++j) {
                    QCOMPARE(mp.groupOffsets[j].morphIndex, mo.groupOffsets[j].morphIndex);
                    QCOMPARE(mp.groupOffsets[j].weight, mo.groupOffsets[j].weight);
                }
                break;
            case 1:
                QCOMPARE(mp.vertexOffsets.size(), mo.vertexOffsets.size());
                for (size_t j = 0; j < mo.vertexOffsets.size(); ++j) {
                    QCOMPARE(mp.vertexOffsets[j].vertexIndex, mo.vertexOffsets[j].vertexIndex);
                    QCOMPARE(mp.vertexOffsets[j].positionOffset, mo.vertexOffsets[j].positionOffset);
                }
                break;
            case 2:
                QCOMPARE(mp.boneOffsets.size(), mo.boneOffsets.size());
                for (size_t j = 0; j < mo.boneOffsets.size(); ++j) {
                    QCOMPARE(mp.boneOffsets[j].boneIndex, mo.boneOffsets[j].boneIndex);
                    QCOMPARE(mp.boneOffsets[j].translation, mo.boneOffsets[j].translation);
                    QCOMPARE(mp.boneOffsets[j].rotation.x, mo.boneOffsets[j].rotation.x);
                    QCOMPARE(mp.boneOffsets[j].rotation.y, mo.boneOffsets[j].rotation.y);
                    QCOMPARE(mp.boneOffsets[j].rotation.z, mo.boneOffsets[j].rotation.z);
                    QCOMPARE(mp.boneOffsets[j].rotation.w, mo.boneOffsets[j].rotation.w);
                }
                break;
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                QCOMPARE(mp.uvOffsets.size(), mo.uvOffsets.size());
                for (size_t j = 0; j < mo.uvOffsets.size(); ++j) {
                    QCOMPARE(mp.uvOffsets[j].vertexIndex, mo.uvOffsets[j].vertexIndex);
                    QCOMPARE(mp.uvOffsets[j].uvOffset, mo.uvOffsets[j].uvOffset);
                }
                break;
            case 8:
                QCOMPARE(mp.materialOffsets.size(), mo.materialOffsets.size());
                for (size_t j = 0; j < mo.materialOffsets.size(); ++j) {
                    const auto& moff_o = mo.materialOffsets[j];
                    const auto& moff_p = mp.materialOffsets[j];
                    QCOMPARE(moff_p.materialIndex, moff_o.materialIndex);
                    QCOMPARE(moff_p.opType, moff_o.opType);
                    QCOMPARE(moff_p.diffuse, moff_o.diffuse);
                    QCOMPARE(moff_p.specular, moff_o.specular);
                    QCOMPARE(moff_p.specularPower, moff_o.specularPower);
                    QCOMPARE(moff_p.ambient, moff_o.ambient);
                    QCOMPARE(moff_p.edgeColor, moff_o.edgeColor);
                    QCOMPARE(moff_p.edgeSize, moff_o.edgeSize);
                    QCOMPARE(moff_p.textureTint, moff_o.textureTint);
                    QCOMPARE(moff_p.sphereTint, moff_o.sphereTint);
                    QCOMPARE(moff_p.toonTint, moff_o.toonTint);
                }
                break;
            default:
                break;
            }
        }

        // Display Frames
        QCOMPARE(parsed.displayFrames.size(), original.displayFrames.size());
        for (size_t i = 0; i < original.displayFrames.size(); ++i) {
            const auto& fo = original.displayFrames[i];
            const auto& fp = parsed.displayFrames[i];
            QCOMPARE(fp.nameJp, fo.nameJp);
            QCOMPARE(fp.nameEn, fo.nameEn);
            QCOMPARE(fp.specialFlag, fo.specialFlag);
            QCOMPARE(fp.elements.size(), fo.elements.size());
            for (size_t j = 0; j < fo.elements.size(); ++j) {
                QCOMPARE(fp.elements[j].targetType, fo.elements[j].targetType);
                QCOMPARE(fp.elements[j].index, fo.elements[j].index);
            }
        }

        // Rigid Bodies
        QCOMPARE(parsed.rigidBodies.size(), original.rigidBodies.size());
        for (size_t i = 0; i < original.rigidBodies.size(); ++i) {
            const auto& rbo = original.rigidBodies[i];
            const auto& rbp = parsed.rigidBodies[i];
            QCOMPARE(rbp.nameJp, rbo.nameJp);
            QCOMPARE(rbp.nameEn, rbo.nameEn);
            QCOMPARE(rbp.boneIndex, rbo.boneIndex);
            QCOMPARE(rbp.groupIndex, rbo.groupIndex);
            QCOMPARE(rbp.nonCollisionMask, rbo.nonCollisionMask);
            QCOMPARE(rbp.shape, rbo.shape);
            QCOMPARE(rbp.size, rbo.size);
            QCOMPARE(rbp.position, rbo.position);
            QCOMPARE(rbp.rotation, rbo.rotation);
            QCOMPARE(rbp.mass, rbo.mass);
            QCOMPARE(rbp.linearDamping, rbo.linearDamping);
            QCOMPARE(rbp.angularDamping, rbo.angularDamping);
            QCOMPARE(rbp.restitution, rbo.restitution);
            QCOMPARE(rbp.friction, rbo.friction);
            QCOMPARE(rbp.physicsMode, rbo.physicsMode);
        }

        // Joints
        QCOMPARE(parsed.joints.size(), original.joints.size());
        for (size_t i = 0; i < original.joints.size(); ++i) {
            const auto& jo = original.joints[i];
            const auto& jp = parsed.joints[i];
            QCOMPARE(jp.nameJp, jo.nameJp);
            QCOMPARE(jp.nameEn, jo.nameEn);
            QCOMPARE(jp.jointType, jo.jointType);
            QCOMPARE(jp.rigidBodyIndexA, jo.rigidBodyIndexA);
            QCOMPARE(jp.rigidBodyIndexB, jo.rigidBodyIndexB);
            QCOMPARE(jp.position, jo.position);
            QCOMPARE(jp.rotation, jo.rotation);
            QCOMPARE(jp.linearLowerLimit, jo.linearLowerLimit);
            QCOMPARE(jp.linearUpperLimit, jo.linearUpperLimit);
            QCOMPARE(jp.angularLowerLimit, jo.angularLowerLimit);
            QCOMPARE(jp.angularUpperLimit, jo.angularUpperLimit);
            QCOMPARE(jp.springLinearStiffness, jo.springLinearStiffness);
            QCOMPARE(jp.springAngularStiffness, jo.springAngularStiffness);
        }

        // Exact byte-for-byte identity check
        QByteArray reSerializedBytes;
        QVERIFY2(PmxWriter::writeToBuffer(parsed, reSerializedBytes, &err), qPrintable(err));
        QCOMPARE(reSerializedBytes.size(), originalBytes.size());
        QCOMPARE(reSerializedBytes, originalBytes);

        // File round-trip check
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath(QStringLiteral("rich20.pmx"));
        QVERIFY2(PmxWriter::writeToFile(filePath, parsed, &err), qPrintable(err));

        PmxDocument fileDoc;
        QVERIFY2(PmxReader::readFromFile(filePath, fileDoc, &err), qPrintable(err));
        QByteArray fileBytes;
        QVERIFY2(PmxWriter::writeToBuffer(fileDoc, fileBytes, &err), qPrintable(err));
        QCOMPARE(fileBytes, originalBytes);
    }

    void testComprehensivePmx21FullModelRoundTrip() {
        const PmxDocument original = createRichPmx21Model();
        QByteArray originalBytes;
        QString err;
        QVERIFY2(PmxWriter::writeToBuffer(original, originalBytes, &err), qPrintable(err));
        QVERIFY(!originalBytes.isEmpty());

        PmxDocument parsed;
        QVERIFY2(PmxReader::readFromBuffer(originalBytes, parsed, &err), qPrintable(err));

        QCOMPARE(parsed.header.version, 2.1f);
        QCOMPARE(parsed.header.textEncoding, uint8_t(1)); // UTF-8

        // Verify QDEF vertex
        QVERIFY(parsed.vertices.size() >= 5);
        const auto& qdefV = parsed.vertices.back();
        QCOMPARE(qdefV.weightType, WeightType::QDEF);
        QCOMPARE(qdefV.boneIndices[0], 0);
        QCOMPARE(qdefV.boneIndices[1], 1);
        QCOMPARE(qdefV.boneIndices[2], 2);
        QCOMPARE(qdefV.boneIndices[3], 3);
        QCOMPARE(qdefV.boneWeights[0], 0.25f);
        QCOMPARE(qdefV.boneWeights[1], 0.25f);
        QCOMPARE(qdefV.boneWeights[2], 0.25f);
        QCOMPARE(qdefV.boneWeights[3], 0.25f);

        // Verify Flip and Impulse morphs
        bool hasFlip = false;
        bool hasImpulse = false;
        for (const auto& m : parsed.morphs) {
            if (m.morphType == 9) {
                hasFlip = true;
                QCOMPARE(m.groupOffsets.size(), size_t(1));
                QCOMPARE(m.groupOffsets[0].morphIndex, 0);
                QCOMPARE(m.groupOffsets[0].weight, 0.8f);
            } else if (m.morphType == 10) {
                hasImpulse = true;
                QCOMPARE(m.impulseOffsets.size(), size_t(1));
                QCOMPARE(m.impulseOffsets[0].rigidBodyIndex, 0);
                QCOMPARE(m.impulseOffsets[0].isLocal, uint8_t(1));
                QCOMPARE(m.impulseOffsets[0].velocity, glm::vec3(1.0f, 2.0f, 3.0f));
                QCOMPARE(m.impulseOffsets[0].torque, glm::vec3(0.1f, 0.2f, 0.3f));
            }
        }
        QVERIFY(hasFlip);
        QVERIFY(hasImpulse);

        // Verify non-zero Joint types
        QVERIFY(parsed.joints.size() >= 3);
        QCOMPARE(parsed.joints[1].jointType, uint8_t(1));
        QCOMPARE(parsed.joints[2].jointType, uint8_t(2));

        // Verify SoftBody
        QCOMPARE(parsed.softBodies.size(), size_t(1));
        const auto& sbp = parsed.softBodies[0];
        const auto& sbo = original.softBodies[0];
        QCOMPARE(sbp.nameJp, sbo.nameJp);
        QCOMPARE(sbp.nameEn, sbo.nameEn);
        QCOMPARE(sbp.shape, sbo.shape);
        QCOMPARE(sbp.materialIndex, sbo.materialIndex);
        QCOMPARE(sbp.groupIndex, sbo.groupIndex);
        QCOMPARE(sbp.nonCollisionMask, sbo.nonCollisionMask);
        QCOMPARE(sbp.flags, sbo.flags);
        QCOMPARE(sbp.blinkDistance, sbo.blinkDistance);
        QCOMPARE(sbp.clusterCount, sbo.clusterCount);
        QCOMPARE(sbp.totalMass, sbo.totalMass);
        QCOMPARE(sbp.collisionMargin, sbo.collisionMargin);
        QCOMPARE(sbp.aeroModel, sbo.aeroModel);
        QCOMPARE(sbp.configFloats, sbo.configFloats);
        QCOMPARE(sbp.clusterFloats, sbo.clusterFloats);
        QCOMPARE(sbp.iterationInts, sbo.iterationInts);
        QCOMPARE(sbp.materialInts, sbo.materialInts);
        QCOMPARE(sbp.anchors.size(), sbo.anchors.size());
        for (size_t a = 0; a < sbo.anchors.size(); ++a) {
            QCOMPARE(sbp.anchors[a].rigidBodyIndex, sbo.anchors[a].rigidBodyIndex);
            QCOMPARE(sbp.anchors[a].vertexIndex, sbo.anchors[a].vertexIndex);
            QCOMPARE(sbp.anchors[a].nearMode, sbo.anchors[a].nearMode);
        }
        QCOMPARE(sbp.pinVertexIndices, sbo.pinVertexIndices);

        // Exact byte-for-byte identity check
        QByteArray reSerializedBytes;
        QVERIFY2(PmxWriter::writeToBuffer(parsed, reSerializedBytes, &err), qPrintable(err));
        QCOMPARE(reSerializedBytes.size(), originalBytes.size());
        QCOMPARE(reSerializedBytes, originalBytes);

        // File round-trip check
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath(QStringLiteral("rich21.pmx"));
        QVERIFY2(PmxWriter::writeToFile(filePath, parsed, &err), qPrintable(err));

        PmxDocument fileDoc;
        QVERIFY2(PmxReader::readFromFile(filePath, fileDoc, &err), qPrintable(err));
        QByteArray fileBytes;
        QVERIFY2(PmxWriter::writeToBuffer(fileDoc, fileBytes, &err), qPrintable(err));
        QCOMPARE(fileBytes, originalBytes);
    }

    void testSurgicalSingleFieldEditIsolation() {
        const PmxDocument original = createRichPmx20Model();
        QByteArray bytesA;
        QString err;
        QVERIFY2(PmxWriter::writeToBuffer(original, bytesA, &err), qPrintable(err));

        PmxDocument docEdit;
        QVERIFY2(PmxReader::readFromBuffer(bytesA, docEdit, &err), qPrintable(err));

        // Material 1 uses sharedToonFlag == 1 (1 byte int8_t toon index, originally 5).
        // Change it to 8.
        QCOMPARE(docEdit.materials[1].sharedToonFlag, uint8_t(1));
        QCOMPARE(docEdit.materials[1].toonIndex, 5);
        docEdit.materials[1].toonIndex = 8;

        QByteArray bytesB;
        QVERIFY2(PmxWriter::writeToBuffer(docEdit, bytesB, &err), qPrintable(err));

        QCOMPARE(bytesB.size(), bytesA.size());
        QVector<qsizetype> diffOffsets;
        for (qsizetype i = 0; i < bytesA.size(); ++i) {
            if (bytesA[i] != bytesB[i]) {
                diffOffsets.append(i);
            }
        }

        // Exactly one byte in the entire file differs!
        QCOMPARE(diffOffsets.size(), 1);
        const qsizetype diffOffset = diffOffsets[0];
        QCOMPARE(static_cast<uint8_t>(bytesA[diffOffset]), uint8_t(5));
        QCOMPARE(static_cast<uint8_t>(bytesB[diffOffset]), uint8_t(8));

        // Now test material 0 which uses sharedToonFlag == 0 (2-byte texture index).
        // Change toonIndex from 2 to 3.
        PmxDocument docEdit2;
        QVERIFY2(PmxReader::readFromBuffer(bytesA, docEdit2, &err), qPrintable(err));
        QCOMPARE(docEdit2.materials[0].sharedToonFlag, uint8_t(0));
        QCOMPARE(docEdit2.materials[0].toonIndex, 2);
        docEdit2.materials[0].toonIndex = 3;

        QByteArray bytesC;
        QVERIFY2(PmxWriter::writeToBuffer(docEdit2, bytesC, &err), qPrintable(err));
        QCOMPARE(bytesC.size(), bytesA.size());
        QVector<qsizetype> diffOffsets2;
        for (qsizetype i = 0; i < bytesA.size(); ++i) {
            if (bytesA[i] != bytesC[i]) {
                diffOffsets2.append(i);
            }
        }
        // Exactly 1 byte differs (low byte of 2-byte LE int16: 2 -> 3)
        QCOMPARE(diffOffsets2.size(), 1);

        // Also test change that alters both bytes of int16_t (e.g. 2 -> 0x0103)
        docEdit2.materials[0].toonIndex = 0x0103;
        QByteArray bytesC2;
        QVERIFY2(PmxWriter::writeToBuffer(docEdit2, bytesC2, &err), qPrintable(err));
        QVector<qsizetype> diffOffsetsC2;
        for (qsizetype i = 0; i < bytesA.size(); ++i) {
            if (bytesA[i] != bytesC2[i]) {
                diffOffsetsC2.append(i);
            }
        }
        // Both bytes of the 2-byte texture index differ and are strictly contiguous
        QCOMPARE(diffOffsetsC2.size(), 2);
        QCOMPARE(diffOffsetsC2[1], diffOffsetsC2[0] + 1);
    }

    void testAdversarialSectionCountAndTruncationFuzzing() {
        const PmxDocument doc20 = createRichPmx20Model();
        QByteArray bytes20;
        QString err;
        QVERIFY(PmxWriter::writeToBuffer(doc20, bytes20, &err));

        const PmxDocument doc21 = createRichPmx21Model();
        QByteArray bytes21;
        QVERIFY(PmxWriter::writeToBuffer(doc21, bytes21, &err));

        PmxOffsets off20 = findPmxOffsets(bytes20);
        PmxOffsets off21 = findPmxOffsets(bytes21);

        QVERIFY(off20.vertexCountOffset > 0);
        QVERIFY(off20.faceVertexCountOffset > 0);
        QVERIFY(off20.textureCountOffset > 0);
        QVERIFY(off20.materialCountOffset > 0);
        QVERIFY(off20.boneCountOffset > 0);
        QVERIFY(off20.ikLinkCountOffset > 0);
        QVERIFY(off20.morphCountOffset > 0);
        QVERIFY(off20.morphOffsetCountOffset > 0);
        QVERIFY(off20.displayFrameCountOffset > 0);
        QVERIFY(off20.displayElementCountOffset > 0);
        QVERIFY(off20.rigidBodyCountOffset > 0);
        QVERIFY(off20.jointCountOffset > 0);

        QVERIFY(off21.softBodyCountOffset > 0);
        QVERIFY(off21.anchorCountOffset > 0);
        QVERIFY(off21.pinCountOffset > 0);

        const int32_t kIntMax = std::numeric_limits<int32_t>::max();
        const std::vector<qsizetype> sectionOffsets20 = {
            off20.vertexCountOffset,
            off20.faceVertexCountOffset,
            off20.textureCountOffset,
            off20.materialCountOffset,
            off20.boneCountOffset,
            off20.ikLinkCountOffset,
            off20.morphCountOffset,
            off20.morphOffsetCountOffset,
            off20.displayFrameCountOffset,
            off20.displayElementCountOffset,
            off20.rigidBodyCountOffset,
            off20.jointCountOffset
        };

        const std::vector<qsizetype> sectionOffsets21 = {
            off21.softBodyCountOffset,
            off21.anchorCountOffset,
            off21.pinCountOffset
        };

        PmxDocument dummy;

        // 1. Test INT32_MAX for all 12 PMX 2.0 sections and nested arrays
        for (qsizetype offset : sectionOffsets20) {
            QByteArray corrupted = bytes20;
            writeLeI32At(corrupted, offset, kIntMax);
            QVERIFY2(!PmxReader::readFromBuffer(corrupted, dummy),
                     qPrintable(QStringLiteral("Expected INT32_MAX count rejection at offset %1").arg(offset)));

            // Also test negative count -1
            writeLeI32At(corrupted, offset, -1);
            QVERIFY2(!PmxReader::readFromBuffer(corrupted, dummy),
                     qPrintable(QStringLiteral("Expected negative count rejection at offset %1").arg(offset)));
        }

        // 2. Test INT32_MAX and negative for PMX 2.1 SoftBody sections
        for (qsizetype offset : sectionOffsets21) {
            QByteArray corrupted = bytes21;
            writeLeI32At(corrupted, offset, kIntMax);
            QVERIFY2(!PmxReader::readFromBuffer(corrupted, dummy),
                     qPrintable(QStringLiteral("Expected INT32_MAX count rejection at offset %1").arg(offset)));

            writeLeI32At(corrupted, offset, -1);
            QVERIFY2(!PmxReader::readFromBuffer(corrupted, dummy),
                     qPrintable(QStringLiteral("Expected negative count rejection at offset %1").arg(offset)));
        }

        // 3. Test non-multiple-of-3 faceVertexCount
        for (int32_t badFaceCount : {1, 2, 4, 5, 7, 10}) {
            QByteArray corrupted = bytes20;
            writeLeI32At(corrupted, off20.faceVertexCountOffset, badFaceCount);
            QVERIFY(!PmxReader::readFromBuffer(corrupted, dummy));
        }

        // 4. Test non-multiple-of-3 material faceVertexCount
        for (int32_t badMatFaceCount : {1, 2, 4, 5, 7}) {
            QByteArray corrupted = bytes20;
            writeLeI32At(corrupted, off20.materialFaceVertexCountOffset, badMatFaceCount);
            QVERIFY(!PmxReader::readFromBuffer(corrupted, dummy));
        }

        // 5. Test unknown WeightType (5, 255)
        for (uint8_t badWt : {uint8_t(5), uint8_t(255)}) {
            QByteArray corrupted = bytes20;
            writeU8At(corrupted, off20.vertexWeightTypeOffset, badWt);
            QVERIFY(!PmxReader::readFromBuffer(corrupted, dummy));
        }

        // 6. Test unknown morphType (11, 255)
        for (uint8_t badMt : {uint8_t(11), uint8_t(255)}) {
            QByteArray corrupted = bytes20;
            writeU8At(corrupted, off20.morphTypeOffset, badMt);
            QVERIFY(!PmxReader::readFromBuffer(corrupted, dummy));
        }

        // 7. Test invalid sphereMode (4, 255)
        for (uint8_t badSm : {uint8_t(4), uint8_t(255)}) {
            QByteArray corrupted = bytes20;
            writeU8At(corrupted, off20.materialSphereModeOffset, badSm);
            QVERIFY(!PmxReader::readFromBuffer(corrupted, dummy));
        }

        // 8. Mid-record truncation across every section under ASan/UBSan
        for (qsizetype len = 0; len < bytes20.size(); ++len) {
            QByteArray truncated = bytes20.left(len);
            QVERIFY2(!PmxReader::readFromBuffer(truncated, dummy),
                     qPrintable(QStringLiteral("Expected truncation failure for length %1 in PMX 2.0").arg(len)));
        }
        for (qsizetype len = 0; len < bytes21.size(); ++len) {
            QByteArray truncated = bytes21.left(len);
            QVERIFY2(!PmxReader::readFromBuffer(truncated, dummy),
                     qPrintable(QStringLiteral("Expected truncation failure for length %1 in PMX 2.1").arg(len)));
        }
    }
};

QTEST_MAIN(TestPmxRoundtrip)
#include "tst_pmx_roundtrip.moc"
