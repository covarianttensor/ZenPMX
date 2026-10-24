#include "core/PmxReader.h"

#include <QFile>
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

bool readI32(std::span<const uint8_t> buffer, size_t& offset, int32_t& out, QString* errorMessage) {
    if (!hasRemaining(buffer, offset, sizeof(int32_t))) {
        setError(errorMessage, QStringLiteral("Unexpected EOF while reading int32_t"));
        return false;
    }
    qint32 raw = 0;
    std::memcpy(&raw, buffer.data() + offset, sizeof(qint32));
    out = static_cast<int32_t>(qFromLittleEndian(raw));
    offset += sizeof(int32_t);
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

bool readVec4(std::span<const uint8_t> buffer, size_t& offset, glm::vec4& out, QString* errorMessage) {
    return readFloat(buffer, offset, out.x, errorMessage) &&
           readFloat(buffer, offset, out.y, errorMessage) &&
           readFloat(buffer, offset, out.z, errorMessage) &&
           readFloat(buffer, offset, out.w, errorMessage);
}

bool isValidIndexSize(uint8_t size) {
    return size == 1 || size == 2 || size == 4;
}

} // namespace

bool PmxReader::readVertexIndex(std::span<const uint8_t> buffer,
                                size_t& offset,
                                uint8_t indexSize,
                                int32_t& outIndex,
                                QString* errorMessage) {
    switch (indexSize) {
    case 1: {
        uint8_t val = 0;
        if (!readU8(buffer, offset, val, errorMessage)) {
            return false;
        }
        outIndex = static_cast<int32_t>(val);
        return true;
    }
    case 2: {
        uint16_t val = 0;
        if (!readU16(buffer, offset, val, errorMessage)) {
            return false;
        }
        outIndex = static_cast<int32_t>(val);
        return true;
    }
    case 4: {
        int32_t val = 0;
        if (!readI32(buffer, offset, val, errorMessage)) {
            return false;
        }
        if (val < 0) {
            setError(errorMessage, QStringLiteral("Invalid negative 4-byte vertex index"));
            return false;
        }
        outIndex = val;
        return true;
    }
    default:
        setError(errorMessage, QStringLiteral("Invalid vertexIndexSize: %1").arg(indexSize));
        return false;
    }
}

bool PmxReader::readVertexIndex(const QByteArray& buffer,
                                size_t& offset,
                                uint8_t indexSize,
                                int32_t& outIndex,
                                QString* errorMessage) {
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(buffer.constData()),
                                  static_cast<size_t>(buffer.size()));
    return readVertexIndex(span, offset, indexSize, outIndex, errorMessage);
}

bool PmxReader::readNonVertexIndex(std::span<const uint8_t> buffer,
                                   size_t& offset,
                                   uint8_t indexSize,
                                   int32_t& outIndex,
                                   QString* errorMessage) {
    switch (indexSize) {
    case 1: {
        int8_t val = 0;
        if (!readI8(buffer, offset, val, errorMessage)) {
            return false;
        }
        outIndex = static_cast<int32_t>(val);
        return true;
    }
    case 2: {
        int16_t val = 0;
        if (!readI16(buffer, offset, val, errorMessage)) {
            return false;
        }
        outIndex = static_cast<int32_t>(val);
        return true;
    }
    case 4: {
        int32_t val = 0;
        if (!readI32(buffer, offset, val, errorMessage)) {
            return false;
        }
        outIndex = val;
        return true;
    }
    default:
        setError(errorMessage, QStringLiteral("Invalid non-vertex indexSize: %1").arg(indexSize));
        return false;
    }
}

bool PmxReader::readNonVertexIndex(const QByteArray& buffer,
                                   size_t& offset,
                                   uint8_t indexSize,
                                   int32_t& outIndex,
                                   QString* errorMessage) {
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(buffer.constData()),
                                  static_cast<size_t>(buffer.size()));
    return readNonVertexIndex(span, offset, indexSize, outIndex, errorMessage);
}

bool PmxReader::readText(std::span<const uint8_t> buffer,
                         size_t& offset,
                         uint8_t textEncoding,
                         QString& outText,
                         QString* errorMessage) {
    if (textEncoding != 0 && textEncoding != 1) {
        setError(errorMessage, QStringLiteral("Invalid textEncoding: %1").arg(textEncoding));
        return false;
    }

    int32_t byteLength = 0;
    if (!readI32(buffer, offset, byteLength, errorMessage)) {
        return false;
    }
    if (byteLength < 0) {
        setError(errorMessage, QStringLiteral("Negative PmxText byteLength: %1").arg(byteLength));
        return false;
    }
    if (textEncoding == 0 && (byteLength % 2) != 0) {
        setError(errorMessage, QStringLiteral("Odd UTF-16LE PmxText byteLength: %1").arg(byteLength));
        return false;
    }

    const size_t length = static_cast<size_t>(byteLength);
    if (!hasRemaining(buffer, offset, length)) {
        setError(errorMessage, QStringLiteral("PmxText byteLength exceeds remaining buffer size"));
        return false;
    }

    if (length == 0) {
        outText.clear();
        return true;
    }

    if (textEncoding == 0) {
        const size_t charCount = length / 2;
        std::vector<char16_t> utf16Buf(charCount);
        for (size_t i = 0; i < charCount; ++i) {
            quint16 rawUnit = 0;
            std::memcpy(&rawUnit, buffer.data() + offset + i * 2, sizeof(quint16));
            utf16Buf[i] = static_cast<char16_t>(qFromLittleEndian(rawUnit));
        }
        outText = QString::fromUtf16(utf16Buf.data(), static_cast<qsizetype>(charCount));
    } else {
        outText = QString::fromUtf8(reinterpret_cast<const char*>(buffer.data() + offset),
                                    static_cast<qsizetype>(length));
    }

    offset += length;
    return true;
}

bool PmxReader::readText(const QByteArray& buffer,
                         size_t& offset,
                         uint8_t textEncoding,
                         QString& outText,
                         QString* errorMessage) {
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(buffer.constData()),
                                  static_cast<size_t>(buffer.size()));
    return readText(span, offset, textEncoding, outText, errorMessage);
}

bool PmxReader::readFromBuffer(std::span<const uint8_t> buffer,
                               PmxDocument& outDocument,
                               QString* errorMessage) {
    PmxDocument doc;
    size_t offset = 0;

    // 1. Magic ("PMX " = 0x50, 0x4D, 0x58, 0x20)
    if (!hasRemaining(buffer, offset, 4)) {
        setError(errorMessage, QStringLiteral("Buffer too short for PMX magic header"));
        return false;
    }
    if (buffer[0] != 0x50 || buffer[1] != 0x4D || buffer[2] != 0x58 || buffer[3] != 0x20) {
        setError(errorMessage, QStringLiteral("Invalid PMX magic bytes (expected 'PMX ')"));
        return false;
    }
    offset += 4;

    // 2. Version (2.0f or 2.1f)
    float version = 0.0f;
    if (!readFloat(buffer, offset, version, errorMessage)) {
        return false;
    }
    if (std::isnan(version) || std::isinf(version) ||
        (std::abs(version - 2.0f) > 1e-6f && std::abs(version - 2.1f) > 1e-6f)) {
        setError(errorMessage, QStringLiteral("Unsupported PMX version: %1").arg(version));
        return false;
    }
    doc.header.version = (std::abs(version - 2.1f) <= 1e-6f) ? 2.1f : 2.0f;

    // 3. Globals count (must be 8)
    uint8_t globalsCount = 0;
    if (!readU8(buffer, offset, globalsCount, errorMessage)) {
        return false;
    }
    if (globalsCount != 8) {
        setError(errorMessage, QStringLiteral("Invalid PMX globalsCount: %1 (expected 8)").arg(globalsCount));
        return false;
    }

    if (!hasRemaining(buffer, offset, 8)) {
        setError(errorMessage, QStringLiteral("Truncated PMX globals array"));
        return false;
    }

    doc.header.textEncoding = buffer[offset + 0];
    doc.header.additionalVec4Count = buffer[offset + 1];
    doc.header.vertexIndexSize = buffer[offset + 2];
    doc.header.textureIndexSize = buffer[offset + 3];
    doc.header.materialIndexSize = buffer[offset + 4];
    doc.header.boneIndexSize = buffer[offset + 5];
    doc.header.morphIndexSize = buffer[offset + 6];
    doc.header.rigidBodyIndexSize = buffer[offset + 7];
    doc.header.preserveIndexSizesOnSave = true;
    offset += 8;

    if (doc.header.textEncoding != 0 && doc.header.textEncoding != 1) {
        setError(errorMessage, QStringLiteral("Invalid PMX textEncoding: %1").arg(doc.header.textEncoding));
        return false;
    }
    if (doc.header.additionalVec4Count > 4) {
        setError(errorMessage, QStringLiteral("Invalid PMX additionalVec4Count: %1").arg(doc.header.additionalVec4Count));
        return false;
    }
    if (!isValidIndexSize(doc.header.vertexIndexSize) ||
        !isValidIndexSize(doc.header.textureIndexSize) ||
        !isValidIndexSize(doc.header.materialIndexSize) ||
        !isValidIndexSize(doc.header.boneIndexSize) ||
        !isValidIndexSize(doc.header.morphIndexSize) ||
        !isValidIndexSize(doc.header.rigidBodyIndexSize)) {
        setError(errorMessage, QStringLiteral("Invalid index byte size in PMX globals"));
        return false;
    }

    // 4. Model Info
    if (!readText(buffer, offset, doc.header.textEncoding, doc.modelInfo.modelNameJp, errorMessage) ||
        !readText(buffer, offset, doc.header.textEncoding, doc.modelInfo.modelNameEn, errorMessage) ||
        !readText(buffer, offset, doc.header.textEncoding, doc.modelInfo.commentJp, errorMessage) ||
        !readText(buffer, offset, doc.header.textEncoding, doc.modelInfo.commentEn, errorMessage)) {
        return false;
    }

    // 5. Vertex Table
    int32_t vertexCount = 0;
    if (!readI32(buffer, offset, vertexCount, errorMessage) || vertexCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing vertexCount"));
        return false;
    }
    doc.vertices.resize(static_cast<size_t>(vertexCount));
    for (int32_t i = 0; i < vertexCount; ++i) {
        PmxVertex& v = doc.vertices[static_cast<size_t>(i)];
        if (!readVec3(buffer, offset, v.position, errorMessage) ||
            !readVec3(buffer, offset, v.normal, errorMessage) ||
            !readVec2(buffer, offset, v.uv, errorMessage)) {
            return false;
        }
        for (uint8_t u = 0; u < doc.header.additionalVec4Count; ++u) {
            if (!readVec4(buffer, offset, v.additionalUVs[u], errorMessage)) {
                return false;
            }
        }
        uint8_t weightTypeRaw = 0;
        if (!readU8(buffer, offset, weightTypeRaw, errorMessage)) {
            return false;
        }
        if (weightTypeRaw > 4 || (weightTypeRaw == 4 && doc.header.version < 2.1f)) {
            setError(errorMessage, QStringLiteral("Invalid vertex weightType: %1").arg(weightTypeRaw));
            return false;
        }
        v.weightType = static_cast<WeightType>(weightTypeRaw);
        switch (v.weightType) {
        case WeightType::BDEF1:
            if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, v.boneIndices[0], errorMessage)) {
                return false;
            }
            v.boneWeights = {1.0f, 0.0f, 0.0f, 0.0f};
            break;
        case WeightType::BDEF2:
            if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, v.boneIndices[0], errorMessage) ||
                !readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, v.boneIndices[1], errorMessage) ||
                !readFloat(buffer, offset, v.boneWeights[0], errorMessage)) {
                return false;
            }
            v.boneWeights[1] = 1.0f - v.boneWeights[0];
            break;
        case WeightType::BDEF4:
        case WeightType::QDEF:
            for (int b = 0; b < 4; ++b) {
                if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, v.boneIndices[b], errorMessage)) {
                    return false;
                }
            }
            for (int b = 0; b < 4; ++b) {
                if (!readFloat(buffer, offset, v.boneWeights[b], errorMessage)) {
                    return false;
                }
            }
            break;
        case WeightType::SDEF:
            if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, v.boneIndices[0], errorMessage) ||
                !readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, v.boneIndices[1], errorMessage) ||
                !readFloat(buffer, offset, v.boneWeights[0], errorMessage) ||
                !readVec3(buffer, offset, v.sdefC, errorMessage) ||
                !readVec3(buffer, offset, v.sdefR0, errorMessage) ||
                !readVec3(buffer, offset, v.sdefR1, errorMessage)) {
                return false;
            }
            v.boneWeights[1] = 1.0f - v.boneWeights[0];
            break;
        }
        if (!readFloat(buffer, offset, v.edgeScale, errorMessage)) {
            return false;
        }
    }

    // 6. Face Index Table
    int32_t faceVertexCount = 0;
    if (!readI32(buffer, offset, faceVertexCount, errorMessage) || faceVertexCount < 0 || (faceVertexCount % 3) != 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing faceVertexCount"));
        return false;
    }
    doc.faceIndices.resize(static_cast<size_t>(faceVertexCount));
    for (int32_t i = 0; i < faceVertexCount; ++i) {
        if (!readVertexIndex(buffer, offset, doc.header.vertexIndexSize, doc.faceIndices[static_cast<size_t>(i)], errorMessage)) {
            return false;
        }
    }

    // 7. Texture Table
    int32_t textureCount = 0;
    if (!readI32(buffer, offset, textureCount, errorMessage) || textureCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing textureCount"));
        return false;
    }
    doc.textures.resize(static_cast<size_t>(textureCount));
    for (int32_t i = 0; i < textureCount; ++i) {
        if (!readText(buffer, offset, doc.header.textEncoding, doc.textures[static_cast<size_t>(i)], errorMessage)) {
            return false;
        }
    }

    // 8. Material Table
    int32_t materialCount = 0;
    if (!readI32(buffer, offset, materialCount, errorMessage) || materialCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing materialCount"));
        return false;
    }
    doc.materials.resize(static_cast<size_t>(materialCount));
    for (int32_t i = 0; i < materialCount; ++i) {
        PmxMaterial& m = doc.materials[static_cast<size_t>(i)];
        if (!readText(buffer, offset, doc.header.textEncoding, m.nameJp, errorMessage) ||
            !readText(buffer, offset, doc.header.textEncoding, m.nameEn, errorMessage) ||
            !readVec4(buffer, offset, m.diffuse, errorMessage) ||
            !readVec3(buffer, offset, m.specular, errorMessage) ||
            !readFloat(buffer, offset, m.specularPower, errorMessage) ||
            !readVec3(buffer, offset, m.ambient, errorMessage) ||
            !readU8(buffer, offset, m.drawFlags, errorMessage) ||
            !readVec4(buffer, offset, m.edgeColor, errorMessage) ||
            !readFloat(buffer, offset, m.edgeSize, errorMessage) ||
            !readNonVertexIndex(buffer, offset, doc.header.textureIndexSize, m.textureIndex, errorMessage) ||
            !readNonVertexIndex(buffer, offset, doc.header.textureIndexSize, m.sphereTextureIndex, errorMessage) ||
            !readU8(buffer, offset, m.sphereMode, errorMessage) ||
            !readU8(buffer, offset, m.sharedToonFlag, errorMessage)) {
            return false;
        }
        if (m.sharedToonFlag == 0) {
            if (!readNonVertexIndex(buffer, offset, doc.header.textureIndexSize, m.toonIndex, errorMessage)) {
                return false;
            }
        } else if (m.sharedToonFlag == 1) {
            int8_t sharedToon = -1;
            if (!readI8(buffer, offset, sharedToon, errorMessage)) {
                return false;
            }
            m.toonIndex = static_cast<int32_t>(sharedToon);
        } else {
            setError(errorMessage, QStringLiteral("Invalid material sharedToonFlag: %1").arg(m.sharedToonFlag));
            return false;
        }
        if (!readText(buffer, offset, doc.header.textEncoding, m.memo, errorMessage) ||
            !readI32(buffer, offset, m.faceVertexCount, errorMessage) ||
            m.faceVertexCount < 0) {
            return false;
        }
    }

    // 9. Bone Table
    int32_t boneCount = 0;
    if (!readI32(buffer, offset, boneCount, errorMessage) || boneCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing boneCount"));
        return false;
    }
    doc.bones.resize(static_cast<size_t>(boneCount));
    for (int32_t i = 0; i < boneCount; ++i) {
        PmxBone& b = doc.bones[static_cast<size_t>(i)];
        if (!readText(buffer, offset, doc.header.textEncoding, b.nameJp, errorMessage) ||
            !readText(buffer, offset, doc.header.textEncoding, b.nameEn, errorMessage) ||
            !readVec3(buffer, offset, b.position, errorMessage) ||
            !readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, b.parentBoneIndex, errorMessage) ||
            !readI32(buffer, offset, b.deformTier, errorMessage) ||
            !readU16(buffer, offset, b.boneFlags, errorMessage)) {
            return false;
        }

        if ((b.boneFlags & 0x0001u) == 0u) {
            if (!readVec3(buffer, offset, b.tailOffset, errorMessage)) {
                return false;
            }
        } else {
            if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, b.tailBoneIndex, errorMessage)) {
                return false;
            }
        }

        if ((b.boneFlags & (0x0100u | 0x0200u)) != 0u) {
            if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, b.inheritParentIndex, errorMessage) ||
                !readFloat(buffer, offset, b.inheritWeight, errorMessage)) {
                return false;
            }
        }

        if ((b.boneFlags & 0x0400u) != 0u) {
            if (!readVec3(buffer, offset, b.fixedAxisVector, errorMessage)) {
                return false;
            }
        }

        if ((b.boneFlags & 0x0800u) != 0u) {
            if (!readVec3(buffer, offset, b.localAxisX, errorMessage) ||
                !readVec3(buffer, offset, b.localAxisZ, errorMessage)) {
                return false;
            }
        }

        if ((b.boneFlags & 0x2000u) != 0u) {
            if (!readI32(buffer, offset, b.externalKey, errorMessage)) {
                return false;
            }
        }

        if ((b.boneFlags & 0x0020u) != 0u) {
            int32_t ikLinkCount = 0;
            if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, b.ikTargetBoneIndex, errorMessage) ||
                !readI32(buffer, offset, b.ikLoopCount, errorMessage) ||
                !readFloat(buffer, offset, b.ikLimitAngle, errorMessage) ||
                !readI32(buffer, offset, ikLinkCount, errorMessage) ||
                ikLinkCount < 0) {
                return false;
            }
            b.ikLinks.resize(static_cast<size_t>(ikLinkCount));
            for (int32_t l = 0; l < ikLinkCount; ++l) {
                PmxIkLink& link = b.ikLinks[static_cast<size_t>(l)];
                if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, link.boneIndex, errorMessage) ||
                    !readU8(buffer, offset, link.hasAngleLimit, errorMessage)) {
                    return false;
                }
                if (link.hasAngleLimit != 0) {
                    if (!readVec3(buffer, offset, link.limitMin, errorMessage) ||
                        !readVec3(buffer, offset, link.limitMax, errorMessage)) {
                        return false;
                    }
                }
            }
        }
    }

    // 10. Morph Table
    int32_t morphCount = 0;
    if (!readI32(buffer, offset, morphCount, errorMessage) || morphCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing morphCount"));
        return false;
    }
    doc.morphs.resize(static_cast<size_t>(morphCount));
    for (int32_t i = 0; i < morphCount; ++i) {
        PmxMorph& m = doc.morphs[static_cast<size_t>(i)];
        int32_t offsetCount = 0;
        if (!readText(buffer, offset, doc.header.textEncoding, m.nameJp, errorMessage) ||
            !readText(buffer, offset, doc.header.textEncoding, m.nameEn, errorMessage) ||
            !readU8(buffer, offset, m.panelCategory, errorMessage) ||
            !readU8(buffer, offset, m.morphType, errorMessage) ||
            !readI32(buffer, offset, offsetCount, errorMessage) ||
            offsetCount < 0) {
            return false;
        }

        switch (m.morphType) {
        case 0: // Group
        case 9: // Flip (2.1)
            m.groupOffsets.resize(static_cast<size_t>(offsetCount));
            for (int32_t j = 0; j < offsetCount; ++j) {
                GroupMorphOffset& off = m.groupOffsets[static_cast<size_t>(j)];
                if (!readNonVertexIndex(buffer, offset, doc.header.morphIndexSize, off.morphIndex, errorMessage) ||
                    !readFloat(buffer, offset, off.weight, errorMessage)) {
                    return false;
                }
            }
            break;
        case 1: // Vertex
            m.vertexOffsets.resize(static_cast<size_t>(offsetCount));
            for (int32_t j = 0; j < offsetCount; ++j) {
                VertexMorphOffset& off = m.vertexOffsets[static_cast<size_t>(j)];
                if (!readVertexIndex(buffer, offset, doc.header.vertexIndexSize, off.vertexIndex, errorMessage) ||
                    !readVec3(buffer, offset, off.positionOffset, errorMessage)) {
                    return false;
                }
            }
            break;
        case 2: // Bone
            m.boneOffsets.resize(static_cast<size_t>(offsetCount));
            for (int32_t j = 0; j < offsetCount; ++j) {
                BoneMorphOffset& off = m.boneOffsets[static_cast<size_t>(j)];
                glm::vec4 q{0.0f};
                if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, off.boneIndex, errorMessage) ||
                    !readVec3(buffer, offset, off.translation, errorMessage) ||
                    !readVec4(buffer, offset, q, errorMessage)) {
                    return false;
                }
                off.rotation.x = q.x;
                off.rotation.y = q.y;
                off.rotation.z = q.z;
                off.rotation.w = q.w;
            }
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7: // UV / AdditionalUV1..4
            m.uvOffsets.resize(static_cast<size_t>(offsetCount));
            for (int32_t j = 0; j < offsetCount; ++j) {
                UvMorphOffset& off = m.uvOffsets[static_cast<size_t>(j)];
                if (!readVertexIndex(buffer, offset, doc.header.vertexIndexSize, off.vertexIndex, errorMessage) ||
                    !readVec4(buffer, offset, off.uvOffset, errorMessage)) {
                    return false;
                }
            }
            break;
        case 8: // Material
            m.materialOffsets.resize(static_cast<size_t>(offsetCount));
            for (int32_t j = 0; j < offsetCount; ++j) {
                MaterialMorphOffset& off = m.materialOffsets[static_cast<size_t>(j)];
                if (!readNonVertexIndex(buffer, offset, doc.header.materialIndexSize, off.materialIndex, errorMessage) ||
                    !readU8(buffer, offset, off.opType, errorMessage) ||
                    !readVec4(buffer, offset, off.diffuse, errorMessage) ||
                    !readVec3(buffer, offset, off.specular, errorMessage) ||
                    !readFloat(buffer, offset, off.specularPower, errorMessage) ||
                    !readVec3(buffer, offset, off.ambient, errorMessage) ||
                    !readVec4(buffer, offset, off.edgeColor, errorMessage) ||
                    !readFloat(buffer, offset, off.edgeSize, errorMessage) ||
                    !readVec4(buffer, offset, off.textureTint, errorMessage) ||
                    !readVec4(buffer, offset, off.sphereTint, errorMessage) ||
                    !readVec4(buffer, offset, off.toonTint, errorMessage)) {
                    return false;
                }
            }
            break;
        case 10: // Impulse (2.1)
            m.impulseOffsets.resize(static_cast<size_t>(offsetCount));
            for (int32_t j = 0; j < offsetCount; ++j) {
                ImpulseMorphOffset& off = m.impulseOffsets[static_cast<size_t>(j)];
                if (!readNonVertexIndex(buffer, offset, doc.header.rigidBodyIndexSize, off.rigidBodyIndex, errorMessage) ||
                    !readU8(buffer, offset, off.isLocal, errorMessage) ||
                    !readVec3(buffer, offset, off.velocity, errorMessage) ||
                    !readVec3(buffer, offset, off.torque, errorMessage)) {
                    return false;
                }
            }
            break;
        default:
            setError(errorMessage, QStringLiteral("Invalid morphType: %1").arg(m.morphType));
            return false;
        }
    }

    // 11. Display Frame Table
    int32_t displayFrameCount = 0;
    if (!readI32(buffer, offset, displayFrameCount, errorMessage) || displayFrameCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing displayFrameCount"));
        return false;
    }
    doc.displayFrames.resize(static_cast<size_t>(displayFrameCount));
    for (int32_t i = 0; i < displayFrameCount; ++i) {
        PmxDisplayFrame& df = doc.displayFrames[static_cast<size_t>(i)];
        int32_t elementCount = 0;
        if (!readText(buffer, offset, doc.header.textEncoding, df.nameJp, errorMessage) ||
            !readText(buffer, offset, doc.header.textEncoding, df.nameEn, errorMessage) ||
            !readU8(buffer, offset, df.specialFlag, errorMessage) ||
            !readI32(buffer, offset, elementCount, errorMessage) ||
            elementCount < 0) {
            return false;
        }
        df.elements.resize(static_cast<size_t>(elementCount));
        for (int32_t j = 0; j < elementCount; ++j) {
            PmxDisplayElement& el = df.elements[static_cast<size_t>(j)];
            if (!readU8(buffer, offset, el.targetType, errorMessage)) {
                return false;
            }
            if (el.targetType == 0) {
                if (!readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, el.index, errorMessage)) {
                    return false;
                }
            } else if (el.targetType == 1) {
                if (!readNonVertexIndex(buffer, offset, doc.header.morphIndexSize, el.index, errorMessage)) {
                    return false;
                }
            } else {
                setError(errorMessage, QStringLiteral("Invalid DisplayElement targetType: %1").arg(el.targetType));
                return false;
            }
        }
    }

    // 12. Rigid Body Table
    int32_t rigidBodyCount = 0;
    if (!readI32(buffer, offset, rigidBodyCount, errorMessage) || rigidBodyCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing rigidBodyCount"));
        return false;
    }
    doc.rigidBodies.resize(static_cast<size_t>(rigidBodyCount));
    for (int32_t i = 0; i < rigidBodyCount; ++i) {
        PmxRigidBody& rb = doc.rigidBodies[static_cast<size_t>(i)];
        if (!readText(buffer, offset, doc.header.textEncoding, rb.nameJp, errorMessage) ||
            !readText(buffer, offset, doc.header.textEncoding, rb.nameEn, errorMessage) ||
            !readNonVertexIndex(buffer, offset, doc.header.boneIndexSize, rb.boneIndex, errorMessage) ||
            !readU8(buffer, offset, rb.groupIndex, errorMessage) ||
            !readU16(buffer, offset, rb.nonCollisionMask, errorMessage) ||
            !readU8(buffer, offset, rb.shape, errorMessage) ||
            !readVec3(buffer, offset, rb.size, errorMessage) ||
            !readVec3(buffer, offset, rb.position, errorMessage) ||
            !readVec3(buffer, offset, rb.rotation, errorMessage) ||
            !readFloat(buffer, offset, rb.mass, errorMessage) ||
            !readFloat(buffer, offset, rb.linearDamping, errorMessage) ||
            !readFloat(buffer, offset, rb.angularDamping, errorMessage) ||
            !readFloat(buffer, offset, rb.restitution, errorMessage) ||
            !readFloat(buffer, offset, rb.friction, errorMessage) ||
            !readU8(buffer, offset, rb.physicsMode, errorMessage)) {
            return false;
        }
    }

    // 13. Joint Table
    int32_t jointCount = 0;
    if (!readI32(buffer, offset, jointCount, errorMessage) || jointCount < 0) {
        setError(errorMessage, QStringLiteral("Invalid or missing jointCount"));
        return false;
    }
    doc.joints.resize(static_cast<size_t>(jointCount));
    for (int32_t i = 0; i < jointCount; ++i) {
        PmxJoint& j = doc.joints[static_cast<size_t>(i)];
        if (!readText(buffer, offset, doc.header.textEncoding, j.nameJp, errorMessage) ||
            !readText(buffer, offset, doc.header.textEncoding, j.nameEn, errorMessage) ||
            !readU8(buffer, offset, j.jointType, errorMessage) ||
            !readNonVertexIndex(buffer, offset, doc.header.rigidBodyIndexSize, j.rigidBodyIndexA, errorMessage) ||
            !readNonVertexIndex(buffer, offset, doc.header.rigidBodyIndexSize, j.rigidBodyIndexB, errorMessage) ||
            !readVec3(buffer, offset, j.position, errorMessage) ||
            !readVec3(buffer, offset, j.rotation, errorMessage) ||
            !readVec3(buffer, offset, j.linearLowerLimit, errorMessage) ||
            !readVec3(buffer, offset, j.linearUpperLimit, errorMessage) ||
            !readVec3(buffer, offset, j.angularLowerLimit, errorMessage) ||
            !readVec3(buffer, offset, j.angularUpperLimit, errorMessage) ||
            !readVec3(buffer, offset, j.springLinearStiffness, errorMessage) ||
            !readVec3(buffer, offset, j.springAngularStiffness, errorMessage)) {
            return false;
        }
    }

    // 14. SoftBody Table (PMX 2.1 only)
    if (doc.header.version >= 2.1f) {
        int32_t softBodyCount = 0;
        if (!readI32(buffer, offset, softBodyCount, errorMessage) || softBodyCount < 0) {
            setError(errorMessage, QStringLiteral("Invalid or missing softBodyCount in PMX 2.1 stream"));
            return false;
        }
        doc.softBodies.resize(static_cast<size_t>(softBodyCount));
        for (int32_t i = 0; i < softBodyCount; ++i) {
            PmxSoftBody& sb = doc.softBodies[static_cast<size_t>(i)];
            if (!readText(buffer, offset, doc.header.textEncoding, sb.nameJp, errorMessage) ||
                !readText(buffer, offset, doc.header.textEncoding, sb.nameEn, errorMessage) ||
                !readU8(buffer, offset, sb.shape, errorMessage) ||
                !readNonVertexIndex(buffer, offset, doc.header.materialIndexSize, sb.materialIndex, errorMessage) ||
                !readU8(buffer, offset, sb.groupIndex, errorMessage) ||
                !readU16(buffer, offset, sb.nonCollisionMask, errorMessage) ||
                !readU8(buffer, offset, sb.flags, errorMessage) ||
                !readI32(buffer, offset, sb.blinkDistance, errorMessage) ||
                !readI32(buffer, offset, sb.clusterCount, errorMessage) ||
                !readFloat(buffer, offset, sb.totalMass, errorMessage) ||
                !readFloat(buffer, offset, sb.collisionMargin, errorMessage) ||
                !readI32(buffer, offset, sb.aeroModel, errorMessage)) {
                return false;
            }
            for (float& f : sb.configFloats) {
                if (!readFloat(buffer, offset, f, errorMessage)) {
                    return false;
                }
            }
            for (float& f : sb.clusterFloats) {
                if (!readFloat(buffer, offset, f, errorMessage)) {
                    return false;
                }
            }
            for (int32_t& iv : sb.iterationInts) {
                if (!readI32(buffer, offset, iv, errorMessage)) {
                    return false;
                }
            }
            for (int32_t& iv : sb.materialInts) {
                if (!readI32(buffer, offset, iv, errorMessage)) {
                    return false;
                }
            }
            int32_t anchorCount = 0;
            if (!readI32(buffer, offset, anchorCount, errorMessage) || anchorCount < 0) {
                return false;
            }
            sb.anchors.resize(static_cast<size_t>(anchorCount));
            for (int32_t a = 0; a < anchorCount; ++a) {
                PmxSoftBodyAnchor& anchor = sb.anchors[static_cast<size_t>(a)];
                if (!readNonVertexIndex(buffer, offset, doc.header.rigidBodyIndexSize, anchor.rigidBodyIndex, errorMessage) ||
                    !readVertexIndex(buffer, offset, doc.header.vertexIndexSize, anchor.vertexIndex, errorMessage) ||
                    !readU8(buffer, offset, anchor.nearMode, errorMessage)) {
                    return false;
                }
            }
            int32_t pinCount = 0;
            if (!readI32(buffer, offset, pinCount, errorMessage) || pinCount < 0) {
                return false;
            }
            sb.pinVertexIndices.resize(static_cast<size_t>(pinCount));
            for (int32_t p = 0; p < pinCount; ++p) {
                if (!readVertexIndex(buffer, offset, doc.header.vertexIndexSize, sb.pinVertexIndices[static_cast<size_t>(p)], errorMessage)) {
                    return false;
                }
            }
        }
    }

    outDocument = std::move(doc);
    return true;
}

bool PmxReader::readFromBuffer(const QByteArray& buffer,
                               PmxDocument& outDocument,
                               QString* errorMessage) {
    std::span<const uint8_t> span(reinterpret_cast<const uint8_t*>(buffer.constData()),
                                  static_cast<size_t>(buffer.size()));
    return readFromBuffer(span, outDocument, errorMessage);
}

bool PmxReader::readFromFile(const QString& filePath,
                             PmxDocument& outDocument,
                             QString* errorMessage) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(errorMessage, QStringLiteral("Failed to open PMX file for reading: %1").arg(filePath));
        return false;
    }
    const QByteArray data = file.readAll();
    if (!readFromBuffer(data, outDocument, errorMessage)) {
        return false;
    }
    outDocument.sourceFilePath = filePath;
    return true;
}

} // namespace pmx
