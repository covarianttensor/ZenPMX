#include "core/PmdImporter.h"

#include <QFile>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QtEndian>
#include <cmath>
#include <cstring>

namespace pmx {

namespace {

void setError(QString* errorMessage, const QString& msg) {
    if (errorMessage != nullptr) {
        *errorMessage = msg;
    }
}

bool hasRemaining(std::span<const uint8_t> buffer, size_t offset, size_t count) {
    return offset <= buffer.size() && count <= (buffer.size() - offset);
}

bool checkCountBounds(int32_t count, size_t minWireBytes, size_t remainingBytes,
                      const char* sectionName, QString* errorMessage) {
    if (count < 0) {
        setError(errorMessage,
                 QStringLiteral("Negative count for %1: %2").arg(QString::fromUtf8(sectionName)).arg(count));
        return false;
    }
    if (minWireBytes > 0 && static_cast<size_t>(count) > (remainingBytes / minWireBytes)) {
        setError(errorMessage,
                 QStringLiteral("Count %1 for %2 exceeds remaining buffer bytes (%3, min record wire size %4)")
                     .arg(count)
                     .arg(QString::fromUtf8(sectionName))
                     .arg(remainingBytes)
                     .arg(minWireBytes));
        return false;
    }
    return true;
}

bool readU8(std::span<const uint8_t> buffer, size_t& offset, uint8_t& out, QString* errorMessage) {
    if (!hasRemaining(buffer, offset, sizeof(uint8_t))) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading uint8_t"));
        return false;
    }
    out = buffer[offset];
    offset += sizeof(uint8_t);
    return true;
}

bool readI8(std::span<const uint8_t> buffer, size_t& offset, int8_t& out, QString* errorMessage) {
    uint8_t raw = 0;
    if (!readU8(buffer, offset, raw, errorMessage)) {
        return false;
    }
    out = static_cast<int8_t>(raw);
    return true;
}

bool readU16(std::span<const uint8_t> buffer, size_t& offset, uint16_t& out, QString* errorMessage) {
    if (!hasRemaining(buffer, offset, sizeof(uint16_t))) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading uint16_t"));
        return false;
    }
    quint16 raw = 0;
    std::memcpy(&raw, buffer.data() + offset, sizeof(quint16));
    out = static_cast<uint16_t>(qFromLittleEndian(raw));
    offset += sizeof(uint16_t);
    return true;
}

bool readI16(std::span<const uint8_t> buffer, size_t& offset, int16_t& out, QString* errorMessage) {
    if (!hasRemaining(buffer, offset, sizeof(int16_t))) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading int16_t"));
        return false;
    }
    qint16 raw = 0;
    std::memcpy(&raw, buffer.data() + offset, sizeof(qint16));
    out = static_cast<int16_t>(qFromLittleEndian(raw));
    offset += sizeof(int16_t);
    return true;
}

bool readU32(std::span<const uint8_t> buffer, size_t& offset, uint32_t& out, QString* errorMessage) {
    if (!hasRemaining(buffer, offset, sizeof(uint32_t))) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading uint32_t"));
        return false;
    }
    quint32 raw = 0;
    std::memcpy(&raw, buffer.data() + offset, sizeof(quint32));
    out = static_cast<uint32_t>(qFromLittleEndian(raw));
    offset += sizeof(uint32_t);
    return true;
}

bool readFloat(std::span<const uint8_t> buffer, size_t& offset, float& out, QString* errorMessage) {
    if (!hasRemaining(buffer, offset, sizeof(uint32_t))) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading float"));
        return false;
    }
    quint32 rawBits = 0;
    std::memcpy(&rawBits, buffer.data() + offset, sizeof(quint32));
    rawBits = qFromLittleEndian(rawBits);
    std::memcpy(&out, &rawBits, sizeof(float));
    offset += sizeof(float);
    return true;
}

bool readVec2(std::span<const uint8_t> buffer, size_t& offset, glm::vec2& out, QString* errorMessage) {
    return readFloat(buffer, offset, out.x, errorMessage) &&
           readFloat(buffer, offset, out.y, errorMessage);
}

bool readVec3(std::span<const uint8_t> buffer, size_t& offset, glm::vec3& out, QString* errorMessage) {
    return readFloat(buffer, offset, out.x, errorMessage) &&
           readFloat(buffer, offset, out.y, errorMessage) &&
           readFloat(buffer, offset, out.z, errorMessage);
}

bool readBytes(std::span<const uint8_t> buffer, size_t& offset, void* dst, size_t size, QString* errorMessage) {
    if (!hasRemaining(buffer, offset, size)) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading %1 bytes").arg(size));
        return false;
    }
    std::memcpy(dst, buffer.data() + offset, size);
    offset += size;
    return true;
}

QString decodeShiftJis(const char* data, size_t maxLen) {
    size_t len = 0;
    while (len < maxLen && data[len] != '\0') {
        ++len;
    }
    QStringDecoder decoder("Shift-JIS");
    return decoder(QByteArrayView(data, static_cast<qsizetype>(len)));
}

} // namespace

bool PmdImporter::importFromFile(const QString& filePath, PmxDocument& outDoc, QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Failed to open file: %1").arg(filePath));
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    bool ok = importFromMemory(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size())),
        outDoc, errorMessage);
    if (ok) {
        outDoc.sourceFilePath = filePath;
    }
    return ok;
}

bool PmdImporter::importFromMemory(const QByteArray& data, PmxDocument& outDoc, QString* errorMessage) {
    return importFromMemory(
        std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size())),
        outDoc, errorMessage);
}

bool PmdImporter::importFromMemory(std::span<const uint8_t> data, PmxDocument& outDoc, QString* errorMessage) {
    outDoc = PmxDocument{};

    // PMD header wire size: Magic (3) + Version (4) + Name (20) + Comment (256) = 283 bytes
    constexpr size_t kMinHeaderSize = 3 + 4 + 20 + 256;
    if (data.size() < kMinHeaderSize) {
        setError(errorMessage,
                 QStringLiteral("PMD buffer too small for header: %1 bytes (minimum %2)")
                     .arg(data.size())
                     .arg(kMinHeaderSize));
        return false;
    }

    // 1. Magic check: "Pmd"
    if (data[0] != 'P' || data[1] != 'm' || data[2] != 'd') {
        setError(errorMessage, QStringLiteral("Invalid PMD magic header (expected 'Pmd')"));
        return false;
    }

    size_t offset = 3;

    // 2. Version
    float version = 0.0f;
    if (!readFloat(data, offset, version, errorMessage)) {
        return false;
    }
    if (std::abs(version - 1.0f) > 0.05f) {
        setError(errorMessage,
                 QStringLiteral("Unsupported PMD version: %1 (expected 1.0)").arg(static_cast<double>(version)));
        return false;
    }

    // 3. Model Name & Comment (Shift-JIS)
    char modelNameBuf[20];
    if (!readBytes(data, offset, modelNameBuf, sizeof(modelNameBuf), errorMessage)) {
        return false;
    }
    outDoc.modelInfo.modelNameJp = decodeShiftJis(modelNameBuf, sizeof(modelNameBuf));

    char commentBuf[256];
    if (!readBytes(data, offset, commentBuf, sizeof(commentBuf), errorMessage)) {
        return false;
    }
    outDoc.modelInfo.commentJp = decodeShiftJis(commentBuf, sizeof(commentBuf));

    // 4. Vertices
    uint32_t vertexCount = 0;
    if (!readU32(data, offset, vertexCount, errorMessage)) {
        return false;
    }
    constexpr size_t kVertexWireSize = 38; // 12 pos + 12 norm + 8 uv + 4 bones + 1 weight + 1 edge
    if (!checkCountBounds(static_cast<int32_t>(vertexCount), kVertexWireSize, data.size() - offset, "Vertices", errorMessage)) {
        return false;
    }

    outDoc.vertices.reserve(vertexCount);
    for (uint32_t i = 0; i < vertexCount; ++i) {
        glm::vec3 pos{0.0f};
        glm::vec3 norm{0.0f};
        glm::vec2 uv{0.0f};
        uint16_t bone0 = 0;
        uint16_t bone1 = 0;
        uint8_t weight = 0;
        uint8_t edgeFlag = 0;

        if (!readVec3(data, offset, pos, errorMessage) ||
            !readVec3(data, offset, norm, errorMessage) ||
            !readVec2(data, offset, uv, errorMessage) ||
            !readU16(data, offset, bone0, errorMessage) ||
            !readU16(data, offset, bone1, errorMessage) ||
            !readU8(data, offset, weight, errorMessage) ||
            !readU8(data, offset, edgeFlag, errorMessage)) {
            return false;
        }

        PmxVertex v;
        v.position = pos;
        v.normal = norm;
        v.uv = uv;
        v.weightType = WeightType::BDEF2;
        v.boneIndices = {static_cast<int32_t>(bone0), static_cast<int32_t>(bone1), -1, -1};
        const float w0 = static_cast<float>(weight) / 100.0f;
        v.boneWeights = {w0, 1.0f - w0, 0.0f, 0.0f};
        v.edgeScale = (edgeFlag == 0 ? 1.0f : 0.0f);
        outDoc.vertices.push_back(v);
    }

    // 5. Faces (Indices)
    uint32_t faceIndexCount = 0;
    if (!readU32(data, offset, faceIndexCount, errorMessage)) {
        return false;
    }
    if ((faceIndexCount % 3) != 0) {
        setError(errorMessage,
                 QStringLiteral("PMD face index count is not a multiple of 3: %1").arg(faceIndexCount));
        return false;
    }
    constexpr size_t kFaceIndexWireSize = 2;
    if (!checkCountBounds(static_cast<int32_t>(faceIndexCount), kFaceIndexWireSize, data.size() - offset, "Faces", errorMessage)) {
        return false;
    }

    outDoc.faceIndices.reserve(faceIndexCount);
    for (uint32_t i = 0; i < faceIndexCount; ++i) {
        uint16_t idx = 0;
        if (!readU16(data, offset, idx, errorMessage)) {
            return false;
        }
        outDoc.faceIndices.push_back(static_cast<int32_t>(idx));
    }

    // 6. Materials
    uint32_t materialCount = 0;
    if (!readU32(data, offset, materialCount, errorMessage)) {
        return false;
    }
    constexpr size_t kMaterialWireSize = 70; // 12 + 4 + 4 + 12 + 12 + 1 + 1 + 4 + 20
    if (!checkCountBounds(static_cast<int32_t>(materialCount), kMaterialWireSize, data.size() - offset, "Materials", errorMessage)) {
        return false;
    }

    auto addTexture = [&](const QString& texPath) -> int32_t {
        if (texPath.isEmpty()) {
            return -1;
        }
        for (size_t t = 0; t < outDoc.textures.size(); ++t) {
            if (outDoc.textures[t] == texPath) {
                return static_cast<int32_t>(t);
            }
        }
        outDoc.textures.push_back(texPath);
        return static_cast<int32_t>(outDoc.textures.size() - 1);
    };

    std::vector<int8_t> rawToonIndices(materialCount);
    outDoc.materials.reserve(materialCount);

    for (uint32_t i = 0; i < materialCount; ++i) {
        glm::vec3 diffuse{0.0f};
        float alpha = 1.0f;
        float shininess = 0.0f;
        glm::vec3 specular{0.0f};
        glm::vec3 ambient{0.0f};
        int8_t toonIndex = -1;
        uint8_t edgeFlag = 0;
        uint32_t faceVertexCount = 0;
        char texBuf[20];

        if (!readVec3(data, offset, diffuse, errorMessage) ||
            !readFloat(data, offset, alpha, errorMessage) ||
            !readFloat(data, offset, shininess, errorMessage) ||
            !readVec3(data, offset, specular, errorMessage) ||
            !readVec3(data, offset, ambient, errorMessage) ||
            !readI8(data, offset, toonIndex, errorMessage) ||
            !readU8(data, offset, edgeFlag, errorMessage) ||
            !readU32(data, offset, faceVertexCount, errorMessage) ||
            !readBytes(data, offset, texBuf, sizeof(texBuf), errorMessage)) {
            return false;
        }

        rawToonIndices[i] = toonIndex;

        PmxMaterial mat;
        mat.nameJp = QStringLiteral("Material_%1").arg(i + 1);
        mat.nameEn = mat.nameJp;
        mat.diffuse = glm::vec4(diffuse, alpha);
        mat.specularPower = shininess;
        mat.specular = specular;
        mat.ambient = ambient;
        mat.faceVertexCount = static_cast<int32_t>(faceVertexCount);
        mat.edgeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        mat.edgeSize = 1.0f;
        mat.drawFlags = (edgeFlag != 0) ? 0x1F : 0x0F;

        // Texture and sphere map parsing
        QString rawTexStr = decodeShiftJis(texBuf, sizeof(texBuf)).trimmed();
        if (!rawTexStr.isEmpty()) {
            int starIdx = rawTexStr.indexOf(QLatin1Char('*'));
            if (starIdx != -1) {
                QString diffTex = rawTexStr.left(starIdx).trimmed();
                QString sphTex = rawTexStr.mid(starIdx + 1).trimmed();
                if (!diffTex.isEmpty()) {
                    mat.textureIndex = addTexture(diffTex);
                }
                if (!sphTex.isEmpty()) {
                    mat.sphereTextureIndex = addTexture(sphTex);
                    if (sphTex.endsWith(QStringLiteral(".spa"), Qt::CaseInsensitive)) {
                        mat.sphereMode = 2; // Add
                    } else if (sphTex.endsWith(QStringLiteral(".sph"), Qt::CaseInsensitive)) {
                        mat.sphereMode = 1; // Multiply
                    } else {
                        mat.sphereMode = 1;
                    }
                }
            } else {
                if (rawTexStr.endsWith(QStringLiteral(".spa"), Qt::CaseInsensitive)) {
                    mat.textureIndex = -1;
                    mat.sphereTextureIndex = addTexture(rawTexStr);
                    mat.sphereMode = 2;
                } else if (rawTexStr.endsWith(QStringLiteral(".sph"), Qt::CaseInsensitive)) {
                    mat.textureIndex = -1;
                    mat.sphereTextureIndex = addTexture(rawTexStr);
                    mat.sphereMode = 1;
                } else {
                    mat.textureIndex = addTexture(rawTexStr);
                    mat.sphereTextureIndex = -1;
                    mat.sphereMode = 0;
                }
            }
        }

        if (toonIndex >= 0 && toonIndex <= 9) {
            mat.sharedToonFlag = 1;
            mat.toonIndex = toonIndex;
        } else {
            mat.sharedToonFlag = 0;
            mat.toonIndex = -1;
        }

        outDoc.materials.push_back(mat);
    }

    // 7. Bones
    uint16_t boneCount = 0;
    if (!readU16(data, offset, boneCount, errorMessage)) {
        return false;
    }
    constexpr size_t kBoneWireSize = 39; // 20 + 2 + 2 + 1 + 2 + 12
    if (!checkCountBounds(static_cast<int32_t>(boneCount), kBoneWireSize, data.size() - offset, "Bones", errorMessage)) {
        return false;
    }

    outDoc.bones.reserve(boneCount);
    for (uint16_t i = 0; i < boneCount; ++i) {
        char nameBuf[20];
        uint16_t parentBoneIndex = 0xFFFF;
        uint16_t tailBoneIndex = 0xFFFF;
        uint8_t boneType = 0;
        uint16_t ikParentBoneIndex = 0xFFFF;
        glm::vec3 headPos{0.0f};

        if (!readBytes(data, offset, nameBuf, sizeof(nameBuf), errorMessage) ||
            !readU16(data, offset, parentBoneIndex, errorMessage) ||
            !readU16(data, offset, tailBoneIndex, errorMessage) ||
            !readU8(data, offset, boneType, errorMessage) ||
            !readU16(data, offset, ikParentBoneIndex, errorMessage) ||
            !readVec3(data, offset, headPos, errorMessage)) {
            return false;
        }

        PmxBone bone;
        bone.nameJp = decodeShiftJis(nameBuf, sizeof(nameBuf));
        bone.position = headPos;
        bone.parentBoneIndex = (parentBoneIndex != 0xFFFF && parentBoneIndex < boneCount)
                                   ? static_cast<int32_t>(parentBoneIndex)
                                   : -1;
        bone.tailBoneIndex = (tailBoneIndex != 0xFFFF && tailBoneIndex < boneCount)
                                 ? static_cast<int32_t>(tailBoneIndex)
                                 : -1;

        uint16_t flags = 0x0002 | 0x0010; // Can rotate, Operable
        if (boneType != 7) {
            flags |= 0x0008; // Visible
        }
        if (bone.tailBoneIndex != -1) {
            flags |= 0x0001; // Tail connection is bone
        }
        if (boneType == 1) { // Rotate & Translate
            flags |= 0x0004;
        } else if (boneType == 2) { // IK
            flags |= 0x0020;
        } else if (boneType == 5 || boneType == 8 || boneType == 9) { // Under rotate, Twist, Follow rotate
            flags |= 0x0100; // Inherit rotation
            bone.inheritParentIndex = (ikParentBoneIndex != 0xFFFF && ikParentBoneIndex < boneCount)
                                          ? static_cast<int32_t>(ikParentBoneIndex)
                                          : -1;
            bone.inheritWeight = 1.0f;
        }
        if (boneType == 8) { // Twist
            flags |= 0x0400; // Fixed axis
        }
        bone.boneFlags = flags;
        outDoc.bones.push_back(bone);
    }

    // 8. IK Chains
    uint16_t ikCount = 0;
    if (!readU16(data, offset, ikCount, errorMessage)) {
        return false;
    }
    constexpr size_t kMinIkWireSize = 11; // 2 ikBone + 2 targetBone + 1 linkCount + 2 loopCount + 4 angleLimit
    if (!checkCountBounds(static_cast<int32_t>(ikCount), kMinIkWireSize, data.size() - offset, "IK", errorMessage)) {
        return false;
    }

    for (uint16_t i = 0; i < ikCount; ++i) {
        uint16_t ikBoneIndex = 0;
        uint16_t targetBoneIndex = 0;
        uint8_t linkCount = 0;
        uint16_t loopCount = 0;
        float angleLimit = 0.0f;

        if (!readU16(data, offset, ikBoneIndex, errorMessage) ||
            !readU16(data, offset, targetBoneIndex, errorMessage) ||
            !readU8(data, offset, linkCount, errorMessage) ||
            !readU16(data, offset, loopCount, errorMessage) ||
            !readFloat(data, offset, angleLimit, errorMessage)) {
            return false;
        }

        if (!hasRemaining(data, offset, static_cast<size_t>(linkCount) * sizeof(uint16_t))) {
            setError(errorMessage, QStringLiteral("Unexpected EOF while reading IK link bones"));
            return false;
        }

        std::vector<uint16_t> linkBoneIndices(linkCount);
        for (uint8_t j = 0; j < linkCount; ++j) {
            if (!readU16(data, offset, linkBoneIndices[j], errorMessage)) {
                return false;
            }
        }

        if (ikBoneIndex < outDoc.bones.size()) {
            auto& ikBone = outDoc.bones[ikBoneIndex];
            ikBone.boneFlags |= 0x0020; // Ensure IK flag is set
            ikBone.ikTargetBoneIndex = (targetBoneIndex < outDoc.bones.size())
                                           ? static_cast<int32_t>(targetBoneIndex)
                                           : -1;
            ikBone.ikLoopCount = static_cast<int32_t>(loopCount);
            ikBone.ikLimitAngle = angleLimit;
            ikBone.ikLinks.resize(linkCount);

            for (uint8_t j = 0; j < linkCount; ++j) {
                const uint16_t lIdx = linkBoneIndices[j];
                ikBone.ikLinks[j].boneIndex = (lIdx < outDoc.bones.size()) ? static_cast<int32_t>(lIdx) : -1;

                if (ikBone.ikLinks[j].boneIndex >= 0) {
                    const QString& linkName = outDoc.bones[ikBone.ikLinks[j].boneIndex].nameJp;
                    if (linkName.contains(QString::fromUtf8("ひざ")) || linkName.contains(QString::fromUtf8("膝"))) {
                        ikBone.ikLinks[j].hasAngleLimit = 1;
                        ikBone.ikLinks[j].limitMin = glm::vec3(glm::radians(0.5f), 0.0f, 0.0f);
                        ikBone.ikLinks[j].limitMax = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
                    }
                }
            }
        }
    }

    // 9. Morphs (Skin)
    uint16_t morphCount = 0;
    if (!readU16(data, offset, morphCount, errorMessage)) {
        return false;
    }
    constexpr size_t kMinMorphWireSize = 25; // 20 name + 4 vertexCount + 1 type
    if (!checkCountBounds(static_cast<int32_t>(morphCount), kMinMorphWireSize, data.size() - offset, "Morphs", errorMessage)) {
        return false;
    }

    struct RawPmdMorph {
        QString name;
        uint8_t type = 0;
        std::vector<uint32_t> indices;
        std::vector<glm::vec3> offsets;
    };

    std::vector<RawPmdMorph> rawMorphs;
    rawMorphs.reserve(morphCount);

    for (uint16_t i = 0; i < morphCount; ++i) {
        char nameBuf[20];
        uint32_t morphVertexCount = 0;
        uint8_t morphType = 0;

        if (!readBytes(data, offset, nameBuf, sizeof(nameBuf), errorMessage) ||
            !readU32(data, offset, morphVertexCount, errorMessage) ||
            !readU8(data, offset, morphType, errorMessage)) {
            return false;
        }

        constexpr size_t kMorphVertexWireSize = 16; // 4 index + 12 offset
        if (!checkCountBounds(static_cast<int32_t>(morphVertexCount), kMorphVertexWireSize, data.size() - offset,
                              "MorphVertices", errorMessage)) {
            return false;
        }

        RawPmdMorph rm;
        rm.name = decodeShiftJis(nameBuf, sizeof(nameBuf));
        rm.type = morphType;
        rm.indices.reserve(morphVertexCount);
        rm.offsets.reserve(morphVertexCount);

        for (uint32_t k = 0; k < morphVertexCount; ++k) {
            uint32_t vIdx = 0;
            glm::vec3 posOffset{0.0f};
            if (!readU32(data, offset, vIdx, errorMessage) ||
                !readVec3(data, offset, posOffset, errorMessage)) {
                return false;
            }
            rm.indices.push_back(vIdx);
            rm.offsets.push_back(posOffset);
        }
        rawMorphs.push_back(std::move(rm));
    }

    // Remap PMD morphs: Type 0 is Base morph containing global vertex indices
    const RawPmdMorph* baseMorph = nullptr;
    for (const auto& rm : rawMorphs) {
        if (rm.type == 0) {
            baseMorph = &rm;
            break;
        }
    }

    std::vector<int32_t> pmdToPmxMorphIndex(rawMorphs.size(), -1);
    for (size_t i = 0; i < rawMorphs.size(); ++i) {
        const auto& rm = rawMorphs[i];
        if (rm.type == 0) {
            // Base morph is not added to outDoc.morphs
            pmdToPmxMorphIndex[i] = -1;
            continue;
        }

        PmxMorph pmxMorph;
        pmxMorph.nameJp = rm.name;
        pmxMorph.morphType = 1; // Vertex morph
        pmxMorph.panelCategory = (rm.type >= 1 && rm.type <= 4) ? rm.type : 4;
        pmxMorph.vertexOffsets.reserve(rm.indices.size());

        for (size_t k = 0; k < rm.indices.size(); ++k) {
            const uint32_t rawIdx = rm.indices[k];
            int32_t globalIdx = -1;
            if (baseMorph != nullptr) {
                if (rawIdx < baseMorph->indices.size()) {
                    globalIdx = static_cast<int32_t>(baseMorph->indices[rawIdx]);
                }
            } else {
                globalIdx = static_cast<int32_t>(rawIdx);
            }
            pmxMorph.vertexOffsets.push_back({globalIdx, rm.offsets[k]});
        }

        outDoc.morphs.push_back(std::move(pmxMorph));
        pmdToPmxMorphIndex[i] = static_cast<int32_t>(outDoc.morphs.size() - 1);
    }

    // 10. Display Frames
    // 10a. Face display frame
    uint8_t faceFrameCount = 0;
    if (!readU8(data, offset, faceFrameCount, errorMessage)) {
        return false;
    }
    if (!hasRemaining(data, offset, static_cast<size_t>(faceFrameCount) * sizeof(uint16_t))) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading face display frame list"));
        return false;
    }
    std::vector<uint16_t> faceMorphIndices(faceFrameCount);
    for (uint8_t i = 0; i < faceFrameCount; ++i) {
        if (!readU16(data, offset, faceMorphIndices[i], errorMessage)) {
            return false;
        }
    }

    // 10b. Bone group list
    uint8_t boneGroupCount = 0;
    if (!readU8(data, offset, boneGroupCount, errorMessage)) {
        return false;
    }
    constexpr size_t kBoneGroupNameWireSize = 50;
    if (!hasRemaining(data, offset, static_cast<size_t>(boneGroupCount) * kBoneGroupNameWireSize)) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading bone display group list"));
        return false;
    }
    std::vector<QString> boneGroupNames(boneGroupCount);
    for (uint8_t i = 0; i < boneGroupCount; ++i) {
        char groupNameBuf[50];
        if (!readBytes(data, offset, groupNameBuf, sizeof(groupNameBuf), errorMessage)) {
            return false;
        }
        boneGroupNames[i] = decodeShiftJis(groupNameBuf, sizeof(groupNameBuf));
    }

    // 10c. Bone display list
    uint32_t boneFrameCount = 0;
    if (!readU32(data, offset, boneFrameCount, errorMessage)) {
        return false;
    }
    constexpr size_t kBoneDispWireSize = 3; // 2 boneIdx + 1 groupIdx
    if (!checkCountBounds(static_cast<int32_t>(boneFrameCount), kBoneDispWireSize, data.size() - offset, "BoneDisplay", errorMessage)) {
        return false;
    }

    struct BoneDispItem {
        uint16_t boneIdx = 0;
        uint8_t groupIdx = 0;
    };
    std::vector<BoneDispItem> boneDispItems(boneFrameCount);
    for (uint32_t i = 0; i < boneFrameCount; ++i) {
        if (!readU16(data, offset, boneDispItems[i].boneIdx, errorMessage) ||
            !readU8(data, offset, boneDispItems[i].groupIdx, errorMessage)) {
            return false;
        }
    }

    // Build PmxDisplayFrames
    // 1. Root frame
    PmxDisplayFrame rootFrame;
    rootFrame.nameJp = QString::fromUtf8("Root");
    rootFrame.nameEn = QStringLiteral("Root");
    rootFrame.specialFlag = 1;
    if (!outDoc.bones.empty()) {
        rootFrame.elements.push_back({0, 0});
    }
    outDoc.displayFrames.push_back(rootFrame);

    // 2. Expressions frame
    PmxDisplayFrame expFrame;
    expFrame.nameJp = QString::fromUtf8("表情");
    expFrame.nameEn = QStringLiteral("Expressions");
    expFrame.specialFlag = 1;
    for (uint16_t pmdMorphIdx : faceMorphIndices) {
        if (pmdMorphIdx < pmdToPmxMorphIndex.size()) {
            int32_t pmxIdx = pmdToPmxMorphIndex[pmdMorphIdx];
            if (pmxIdx >= 0 && pmxIdx < static_cast<int32_t>(outDoc.morphs.size())) {
                expFrame.elements.push_back({1, pmxIdx});
            }
        }
    }
    if (faceMorphIndices.empty() && !outDoc.morphs.empty()) {
        for (size_t m = 0; m < outDoc.morphs.size(); ++m) {
            expFrame.elements.push_back({1, static_cast<int32_t>(m)});
        }
    }
    outDoc.displayFrames.push_back(expFrame);

    // 3. User bone group frames
    std::vector<PmxDisplayFrame> groupFrames(boneGroupCount);
    for (uint8_t g = 0; g < boneGroupCount; ++g) {
        groupFrames[g].nameJp = boneGroupNames[g];
        groupFrames[g].nameEn = boneGroupNames[g];
        groupFrames[g].specialFlag = 0;
    }

    for (const auto& item : boneDispItems) {
        if (item.boneIdx >= outDoc.bones.size()) {
            continue;
        }
        if (item.groupIdx > 0 && static_cast<size_t>(item.groupIdx - 1) < groupFrames.size()) {
            groupFrames[item.groupIdx - 1].elements.push_back({0, static_cast<int32_t>(item.boneIdx)});
        } else if (item.groupIdx == 0 && !groupFrames.empty()) {
            groupFrames[0].elements.push_back({0, static_cast<int32_t>(item.boneIdx)});
        }
    }

    for (auto& gf : groupFrames) {
        outDoc.displayFrames.push_back(std::move(gf));
    }

    // 11. English Extension (optional)
    if (offset < data.size()) {
        uint8_t englishFlag = 0;
        if (!readU8(data, offset, englishFlag, errorMessage)) {
            return false;
        }

        if (englishFlag == 1) {
            const size_t nonBaseSkinCount = rawMorphs.empty() ? 0 : (rawMorphs.size() - 1);
            const size_t engTotalSize = 20 + 256 + static_cast<size_t>(boneCount) * 20 +
                                        nonBaseSkinCount * 20 + static_cast<size_t>(boneGroupCount) * 50;

            if (!hasRemaining(data, offset, engTotalSize)) {
                setError(errorMessage, QStringLiteral("Unexpected EOF while reading PMD English extension"));
                return false;
            }

            char engModelName[20];
            char engComment[256];
            if (!readBytes(data, offset, engModelName, sizeof(engModelName), errorMessage) ||
                !readBytes(data, offset, engComment, sizeof(engComment), errorMessage)) {
                return false;
            }
            outDoc.modelInfo.modelNameEn = decodeShiftJis(engModelName, sizeof(engModelName));
            outDoc.modelInfo.commentEn = decodeShiftJis(engComment, sizeof(engComment));

            for (uint16_t b = 0; b < boneCount; ++b) {
                char engBoneName[20];
                if (!readBytes(data, offset, engBoneName, sizeof(engBoneName), errorMessage)) {
                    return false;
                }
                if (b < outDoc.bones.size()) {
                    outDoc.bones[b].nameEn = decodeShiftJis(engBoneName, sizeof(engBoneName));
                }
            }

            for (size_t s = 0; s < nonBaseSkinCount; ++s) {
                char engSkinName[20];
                if (!readBytes(data, offset, engSkinName, sizeof(engSkinName), errorMessage)) {
                    return false;
                }
                if (s < outDoc.morphs.size()) {
                    outDoc.morphs[s].nameEn = decodeShiftJis(engSkinName, sizeof(engSkinName));
                }
            }

            for (uint8_t g = 0; g < boneGroupCount; ++g) {
                char engGroupName[50];
                if (!readBytes(data, offset, engGroupName, sizeof(engGroupName), errorMessage)) {
                    return false;
                }
                // Group frames start at index 2 (after Root and Expressions)
                const size_t frameIdx = 2 + g;
                if (frameIdx < outDoc.displayFrames.size()) {
                    outDoc.displayFrames[frameIdx].nameEn = decodeShiftJis(engGroupName, sizeof(engGroupName));
                }
            }
        }
    }

    // 12. Custom Toon Texture List (optional, 10 * 100 bytes)
    if (offset < data.size()) {
        constexpr size_t kToonTableWireSize = 1000;
        if (data.size() - offset >= kToonTableWireSize) {
            std::array<QString, 10> customToonNames;
            for (int t = 0; t < 10; ++t) {
                char toonBuf[100];
                if (!readBytes(data, offset, toonBuf, sizeof(toonBuf), errorMessage)) {
                    return false;
                }
                customToonNames[t] = decodeShiftJis(toonBuf, sizeof(toonBuf)).trimmed();
            }

            static const QRegularExpression stdToonRe(QStringLiteral("^toon(0[1-9]|10)\\.bmp$"),
                                                      QRegularExpression::CaseInsensitiveOption);

            for (size_t m = 0; m < outDoc.materials.size(); ++m) {
                const int8_t rawToon = rawToonIndices[m];
                if (rawToon >= 0 && rawToon <= 9) {
                    const QString& toonPath = customToonNames[rawToon];
                    if (stdToonRe.match(toonPath).hasMatch() || toonPath.isEmpty()) {
                        outDoc.materials[m].sharedToonFlag = 1;
                        outDoc.materials[m].toonIndex = rawToon;
                    } else {
                        outDoc.materials[m].sharedToonFlag = 0;
                        outDoc.materials[m].toonIndex = addTexture(toonPath);
                    }
                }
            }
        } else if (data.size() - offset > 0) {
            // Buffer has trailing bytes that are insufficient for a full toon table
            setError(errorMessage,
                     QStringLiteral("Truncated PMD file: remaining bytes (%1) insufficient for toon texture table")
                         .arg(data.size() - offset));
            return false;
        }
    }

    // 13. Bullet Physics Extension (optional)
    if (offset < data.size()) {
        uint32_t rigidBodyCount = 0;
        if (!readU32(data, offset, rigidBodyCount, errorMessage)) {
            return false;
        }
        constexpr size_t kRigidBodyWireSize = 83; // 20+2+1+2+1+12+12+12+4+4+4+4+4+1
        if (!checkCountBounds(static_cast<int32_t>(rigidBodyCount), kRigidBodyWireSize, data.size() - offset,
                              "RigidBodies", errorMessage)) {
            return false;
        }

        outDoc.rigidBodies.reserve(rigidBodyCount);
        for (uint32_t i = 0; i < rigidBodyCount; ++i) {
            char nameBuf[20];
            int16_t boneIdx = -1;
            uint8_t group = 0;
            uint16_t mask = 0xFFFF;
            uint8_t shape = 0;
            glm::vec3 size{1.0f};
            glm::vec3 pos{0.0f};
            glm::vec3 rot{0.0f};
            float mass = 1.0f;
            float linearDamping = 0.0f;
            float angularDamping = 0.0f;
            float restitution = 0.0f;
            float friction = 0.0f;
            uint8_t mode = 0;

            if (!readBytes(data, offset, nameBuf, sizeof(nameBuf), errorMessage) ||
                !readI16(data, offset, boneIdx, errorMessage) ||
                !readU8(data, offset, group, errorMessage) ||
                !readU16(data, offset, mask, errorMessage) ||
                !readU8(data, offset, shape, errorMessage) ||
                !readVec3(data, offset, size, errorMessage) ||
                !readVec3(data, offset, pos, errorMessage) ||
                !readVec3(data, offset, rot, errorMessage) ||
                !readFloat(data, offset, mass, errorMessage) ||
                !readFloat(data, offset, linearDamping, errorMessage) ||
                !readFloat(data, offset, angularDamping, errorMessage) ||
                !readFloat(data, offset, restitution, errorMessage) ||
                !readFloat(data, offset, friction, errorMessage) ||
                !readU8(data, offset, mode, errorMessage)) {
                return false;
            }

            PmxRigidBody rb;
            rb.nameJp = decodeShiftJis(nameBuf, sizeof(nameBuf));
            rb.nameEn = rb.nameJp;
            rb.boneIndex = (boneIdx >= 0 && boneIdx < static_cast<int32_t>(outDoc.bones.size()))
                               ? static_cast<int32_t>(boneIdx)
                               : -1;
            rb.groupIndex = group;
            rb.nonCollisionMask = mask;
            rb.shape = shape;
            rb.size = size;
            rb.position = pos;
            rb.rotation = rot;
            rb.mass = mass;
            rb.linearDamping = linearDamping;
            rb.angularDamping = angularDamping;
            rb.restitution = restitution;
            rb.friction = friction;
            rb.physicsMode = mode;
            outDoc.rigidBodies.push_back(rb);
        }

        // 13b. Joints
        if (offset < data.size()) {
            uint32_t jointCount = 0;
            if (!readU32(data, offset, jointCount, errorMessage)) {
                return false;
            }
            constexpr size_t kJointWireSize = 124; // 20+4+4+12*8
            if (!checkCountBounds(static_cast<int32_t>(jointCount), kJointWireSize, data.size() - offset,
                                  "Joints", errorMessage)) {
                return false;
            }

            outDoc.joints.reserve(jointCount);
            for (uint32_t i = 0; i < jointCount; ++i) {
                char nameBuf[20];
                uint32_t bodyA = 0;
                uint32_t bodyB = 0;
                glm::vec3 pos{0.0f};
                glm::vec3 rot{0.0f};
                glm::vec3 posMin{0.0f};
                glm::vec3 posMax{0.0f};
                glm::vec3 rotMin{0.0f};
                glm::vec3 rotMax{0.0f};
                glm::vec3 springPos{0.0f};
                glm::vec3 springRot{0.0f};

                if (!readBytes(data, offset, nameBuf, sizeof(nameBuf), errorMessage) ||
                    !readU32(data, offset, bodyA, errorMessage) ||
                    !readU32(data, offset, bodyB, errorMessage) ||
                    !readVec3(data, offset, pos, errorMessage) ||
                    !readVec3(data, offset, rot, errorMessage) ||
                    !readVec3(data, offset, posMin, errorMessage) ||
                    !readVec3(data, offset, posMax, errorMessage) ||
                    !readVec3(data, offset, rotMin, errorMessage) ||
                    !readVec3(data, offset, rotMax, errorMessage) ||
                    !readVec3(data, offset, springPos, errorMessage) ||
                    !readVec3(data, offset, springRot, errorMessage)) {
                    return false;
                }

                PmxJoint joint;
                joint.nameJp = decodeShiftJis(nameBuf, sizeof(nameBuf));
                joint.nameEn = joint.nameJp;
                joint.jointType = 0;
                joint.rigidBodyIndexA = (bodyA < outDoc.rigidBodies.size()) ? static_cast<int32_t>(bodyA) : -1;
                joint.rigidBodyIndexB = (bodyB < outDoc.rigidBodies.size()) ? static_cast<int32_t>(bodyB) : -1;
                joint.position = pos;
                joint.rotation = rot;
                joint.linearLowerLimit = posMin;
                joint.linearUpperLimit = posMax;
                joint.angularLowerLimit = rotMin;
                joint.angularUpperLimit = rotMax;
                joint.springLinearStiffness = springPos;
                joint.springAngularStiffness = springRot;
                outDoc.joints.push_back(joint);
            }
        }
    }

    // 14. Populate PmxDocument Header
    outDoc.header.version = 2.0f;
    outDoc.header.textEncoding = 0; // UTF-16LE
    outDoc.header.additionalVec4Count = 0;
    outDoc.header.preserveIndexSizesOnSave = false;

    if (outDoc.vertices.size() <= 255) {
        outDoc.header.vertexIndexSize = 1;
    } else if (outDoc.vertices.size() <= 65535) {
        outDoc.header.vertexIndexSize = 2;
    } else {
        outDoc.header.vertexIndexSize = 4;
    }

    outDoc.header.textureIndexSize = (outDoc.textures.size() <= 127) ? 1 : (outDoc.textures.size() <= 32767 ? 2 : 4);
    outDoc.header.materialIndexSize = (outDoc.materials.size() <= 127) ? 1 : (outDoc.materials.size() <= 32767 ? 2 : 4);
    outDoc.header.boneIndexSize = (outDoc.bones.size() <= 127) ? 1 : (outDoc.bones.size() <= 32767 ? 2 : 4);
    outDoc.header.morphIndexSize = (outDoc.morphs.size() <= 127) ? 1 : (outDoc.morphs.size() <= 32767 ? 2 : 4);
    outDoc.header.rigidBodyIndexSize = (outDoc.rigidBodies.size() <= 127) ? 1 : (outDoc.rigidBodies.size() <= 32767 ? 2 : 4);

    return true;
}

} // namespace pmx
