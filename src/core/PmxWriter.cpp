#include "core/PmxWriter.h"

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

void writeU8(QByteArray& buffer, uint8_t val) {
    buffer.append(static_cast<char>(val));
}

void writeI8(QByteArray& buffer, int8_t val) {
    buffer.append(static_cast<char>(static_cast<uint8_t>(val)));
}

void writeU16(QByteArray& buffer, uint16_t val) {
    quint16 le = qToLittleEndian<quint16>(val);
    char bytes[sizeof(quint16)];
    std::memcpy(bytes, &le, sizeof(quint16));
    buffer.append(bytes, sizeof(quint16));
}

void writeI16(QByteArray& buffer, int16_t val) {
    qint16 le = qToLittleEndian<qint16>(val);
    char bytes[sizeof(qint16)];
    std::memcpy(bytes, &le, sizeof(qint16));
    buffer.append(bytes, sizeof(qint16));
}

void writeI32(QByteArray& buffer, int32_t val) {
    qint32 le = qToLittleEndian<qint32>(val);
    char bytes[sizeof(qint32)];
    std::memcpy(bytes, &le, sizeof(qint32));
    buffer.append(bytes, sizeof(qint32));
}

void writeFloat(QByteArray& buffer, float val) {
    quint32 rawBits = 0;
    std::memcpy(&rawBits, &val, sizeof(float));
    quint32 le = qToLittleEndian<quint32>(rawBits);
    char bytes[sizeof(quint32)];
    std::memcpy(bytes, &le, sizeof(quint32));
    buffer.append(bytes, sizeof(quint32));
}

void writeVec2(QByteArray& buffer, const glm::vec2& v) {
    writeFloat(buffer, v.x);
    writeFloat(buffer, v.y);
}

void writeVec3(QByteArray& buffer, const glm::vec3& v) {
    writeFloat(buffer, v.x);
    writeFloat(buffer, v.y);
    writeFloat(buffer, v.z);
}

void writeVec4(QByteArray& buffer, const glm::vec4& v) {
    writeFloat(buffer, v.x);
    writeFloat(buffer, v.y);
    writeFloat(buffer, v.z);
    writeFloat(buffer, v.w);
}

bool isValidIndexSize(uint8_t size) {
    return size == 1 || size == 2 || size == 4;
}

uint8_t selectVertexIndexSize(uint8_t currentSize, size_t count, bool preserve) {
    if (preserve && isValidIndexSize(currentSize)) {
        return currentSize;
    }
    uint8_t minRequired = 1;
    if (count > 65535) {
        minRequired = 4;
    } else if (count > 255) {
        minRequired = 2;
    }
    if (isValidIndexSize(currentSize) && currentSize >= minRequired) {
        return currentSize;
    }
    return minRequired;
}

uint8_t selectNonVertexIndexSize(uint8_t currentSize, size_t count, bool preserve) {
    if (preserve && isValidIndexSize(currentSize)) {
        return currentSize;
    }
    uint8_t minRequired = 1;
    if (count > 32767) {
        minRequired = 4;
    } else if (count > 127) {
        minRequired = 2;
    }
    if (isValidIndexSize(currentSize) && currentSize >= minRequired) {
        return currentSize;
    }
    return minRequired;
}

} // namespace

bool PmxWriter::writeVertexIndex(QByteArray& buffer,
                                 int32_t index,
                                 uint8_t indexSize,
                                 QString* errorMessage) {
    switch (indexSize) {
    case 1:
        if (index < 0 || index > 255) {
            setError(errorMessage, QStringLiteral("Vertex index %1 out of range for 1-byte uint8_t").arg(index));
            return false;
        }
        writeU8(buffer, static_cast<uint8_t>(index));
        return true;
    case 2:
        if (index < 0 || index > 65535) {
            setError(errorMessage, QStringLiteral("Vertex index %1 out of range for 2-byte uint16_t").arg(index));
            return false;
        }
        writeU16(buffer, static_cast<uint16_t>(index));
        return true;
    case 4:
        if (index < 0) {
            setError(errorMessage, QStringLiteral("Vertex index %1 out of range for 4-byte int32_t").arg(index));
            return false;
        }
        writeI32(buffer, index);
        return true;
    default:
        setError(errorMessage, QStringLiteral("Invalid vertexIndexSize: %1").arg(indexSize));
        return false;
    }
}

bool PmxWriter::writeNonVertexIndex(QByteArray& buffer,
                                    int32_t index,
                                    uint8_t indexSize,
                                    QString* errorMessage) {
    switch (indexSize) {
    case 1:
        if (index < -128 || index > 127) {
            setError(errorMessage, QStringLiteral("Non-vertex index %1 out of range for 1-byte int8_t").arg(index));
            return false;
        }
        writeI8(buffer, static_cast<int8_t>(index));
        return true;
    case 2:
        if (index < -32768 || index > 32767) {
            setError(errorMessage, QStringLiteral("Non-vertex index %1 out of range for 2-byte int16_t").arg(index));
            return false;
        }
        writeI16(buffer, static_cast<int16_t>(index));
        return true;
    case 4:
        writeI32(buffer, index);
        return true;
    default:
        setError(errorMessage, QStringLiteral("Invalid non-vertex indexSize: %1").arg(indexSize));
        return false;
    }
}

bool PmxWriter::writeText(QByteArray& buffer,
                          const QString& text,
                          uint8_t textEncoding,
                          QString* errorMessage) {
    if (textEncoding == 0) {
        const qsizetype charCount = text.size();
        const int32_t byteLength = static_cast<int32_t>(charCount * 2);
        writeI32(buffer, byteLength);
        const ushort* utf16 = text.utf16();
        for (qsizetype i = 0; i < charCount; ++i) {
            writeU16(buffer, static_cast<uint16_t>(utf16[i]));
        }
        return true;
    } else if (textEncoding == 1) {
        const QByteArray utf8 = text.toUtf8();
        writeI32(buffer, static_cast<int32_t>(utf8.size()));
        buffer.append(utf8);
        return true;
    } else {
        setError(errorMessage, QStringLiteral("Invalid textEncoding: %1").arg(textEncoding));
        return false;
    }
}

bool PmxWriter::writeToBuffer(const PmxDocument& document,
                              QByteArray& outBuffer,
                              QString* errorMessage) {
    const PmxHeader& hdr = document.header;
    if (std::isnan(hdr.version) || std::isinf(hdr.version) ||
        (std::abs(hdr.version - 2.0f) > 1e-6f && std::abs(hdr.version - 2.1f) > 1e-6f)) {
        setError(errorMessage, QStringLiteral("Unsupported PMX version: %1").arg(hdr.version));
        return false;
    }
    if (hdr.textEncoding != 0 && hdr.textEncoding != 1) {
        setError(errorMessage, QStringLiteral("Invalid textEncoding: %1").arg(hdr.textEncoding));
        return false;
    }
    if (hdr.additionalVec4Count > 4) {
        setError(errorMessage, QStringLiteral("Invalid additionalVec4Count: %1").arg(hdr.additionalVec4Count));
        return false;
    }
    if ((document.faceIndices.size() % 3) != 0) {
        setError(errorMessage, QStringLiteral("faceIndices size must be a multiple of 3"));
        return false;
    }

    const uint8_t vertexIdxSize = selectVertexIndexSize(
        hdr.vertexIndexSize, document.vertices.size(), hdr.preserveIndexSizesOnSave);
    const uint8_t textureIdxSize = selectNonVertexIndexSize(
        hdr.textureIndexSize, document.textures.size(), hdr.preserveIndexSizesOnSave);
    const uint8_t materialIdxSize = selectNonVertexIndexSize(
        hdr.materialIndexSize, document.materials.size(), hdr.preserveIndexSizesOnSave);
    const uint8_t boneIdxSize = selectNonVertexIndexSize(
        hdr.boneIndexSize, document.bones.size(), hdr.preserveIndexSizesOnSave);
    const uint8_t morphIdxSize = selectNonVertexIndexSize(
        hdr.morphIndexSize, document.morphs.size(), hdr.preserveIndexSizesOnSave);
    const uint8_t rigidBodyIdxSize = selectNonVertexIndexSize(
        hdr.rigidBodyIndexSize, document.rigidBodies.size(), hdr.preserveIndexSizesOnSave);

    if (!isValidIndexSize(vertexIdxSize) ||
        !isValidIndexSize(textureIdxSize) ||
        !isValidIndexSize(materialIdxSize) ||
        !isValidIndexSize(boneIdxSize) ||
        !isValidIndexSize(morphIdxSize) ||
        !isValidIndexSize(rigidBodyIdxSize)) {
        setError(errorMessage, QStringLiteral("Invalid index byte size in PmxHeader"));
        return false;
    }

    QByteArray buf;
    buf.reserve(1024);

    // 1. Magic ("PMX ")
    buf.append("PMX ", 4);

    // 2. Version
    const float versionToWrite = (std::abs(hdr.version - 2.1f) <= 1e-6f) ? 2.1f : 2.0f;
    if (versionToWrite < 2.1f && !document.softBodies.empty()) {
        setError(errorMessage, QStringLiteral("SoftBody records require PMX 2.1"));
        return false;
    }
    writeFloat(buf, versionToWrite);

    // 3. Globals
    writeU8(buf, 8);
    writeU8(buf, hdr.textEncoding);
    writeU8(buf, hdr.additionalVec4Count);
    writeU8(buf, vertexIdxSize);
    writeU8(buf, textureIdxSize);
    writeU8(buf, materialIdxSize);
    writeU8(buf, boneIdxSize);
    writeU8(buf, morphIdxSize);
    writeU8(buf, rigidBodyIdxSize);

    // 4. Model Info
    if (!writeText(buf, document.modelInfo.modelNameJp, hdr.textEncoding, errorMessage) ||
        !writeText(buf, document.modelInfo.modelNameEn, hdr.textEncoding, errorMessage) ||
        !writeText(buf, document.modelInfo.commentJp, hdr.textEncoding, errorMessage) ||
        !writeText(buf, document.modelInfo.commentEn, hdr.textEncoding, errorMessage)) {
        return false;
    }

    // 5. Vertex Table
    writeI32(buf, static_cast<int32_t>(document.vertices.size()));
    for (const PmxVertex& v : document.vertices) {
        writeVec3(buf, v.position);
        writeVec3(buf, v.normal);
        writeVec2(buf, v.uv);
        for (uint8_t u = 0; u < hdr.additionalVec4Count; ++u) {
            writeVec4(buf, v.additionalUVs[u]);
        }
        writeU8(buf, static_cast<uint8_t>(v.weightType));
        switch (v.weightType) {
        case WeightType::BDEF1:
            if (!writeNonVertexIndex(buf, v.boneIndices[0], boneIdxSize, errorMessage)) {
                return false;
            }
            break;
        case WeightType::BDEF2:
            if (!writeNonVertexIndex(buf, v.boneIndices[0], boneIdxSize, errorMessage) ||
                !writeNonVertexIndex(buf, v.boneIndices[1], boneIdxSize, errorMessage)) {
                return false;
            }
            writeFloat(buf, v.boneWeights[0]);
            break;
        case WeightType::BDEF4:
            for (int b = 0; b < 4; ++b) {
                if (!writeNonVertexIndex(buf, v.boneIndices[b], boneIdxSize, errorMessage)) {
                    return false;
                }
            }
            for (int b = 0; b < 4; ++b) {
                writeFloat(buf, v.boneWeights[b]);
            }
            break;
        case WeightType::QDEF:
            if (versionToWrite < 2.1f) {
                setError(errorMessage, QStringLiteral("QDEF vertex weight type requires PMX 2.1"));
                return false;
            }
            for (int b = 0; b < 4; ++b) {
                if (!writeNonVertexIndex(buf, v.boneIndices[b], boneIdxSize, errorMessage)) {
                    return false;
                }
            }
            for (int b = 0; b < 4; ++b) {
                writeFloat(buf, v.boneWeights[b]);
            }
            break;
        case WeightType::SDEF:
            if (!writeNonVertexIndex(buf, v.boneIndices[0], boneIdxSize, errorMessage) ||
                !writeNonVertexIndex(buf, v.boneIndices[1], boneIdxSize, errorMessage)) {
                return false;
            }
            writeFloat(buf, v.boneWeights[0]);
            writeVec3(buf, v.sdefC);
            writeVec3(buf, v.sdefR0);
            writeVec3(buf, v.sdefR1);
            break;
        default:
            setError(errorMessage, QStringLiteral("Invalid vertex weightType: %1").arg(static_cast<uint8_t>(v.weightType)));
            return false;
        }
        writeFloat(buf, v.edgeScale);
    }

    // 6. Face Index Table
    writeI32(buf, static_cast<int32_t>(document.faceIndices.size()));
    for (int32_t idx : document.faceIndices) {
        if (!writeVertexIndex(buf, idx, vertexIdxSize, errorMessage)) {
            return false;
        }
    }

    // 7. Texture Table
    writeI32(buf, static_cast<int32_t>(document.textures.size()));
    for (const QString& tex : document.textures) {
        if (!writeText(buf, tex, hdr.textEncoding, errorMessage)) {
            return false;
        }
    }

    // 8. Material Table
    writeI32(buf, static_cast<int32_t>(document.materials.size()));
    for (const PmxMaterial& m : document.materials) {
        if (!writeText(buf, m.nameJp, hdr.textEncoding, errorMessage) ||
            !writeText(buf, m.nameEn, hdr.textEncoding, errorMessage)) {
            return false;
        }
        writeVec4(buf, m.diffuse);
        writeVec3(buf, m.specular);
        writeFloat(buf, m.specularPower);
        writeVec3(buf, m.ambient);
        writeU8(buf, m.drawFlags);
        writeVec4(buf, m.edgeColor);
        writeFloat(buf, m.edgeSize);
        if (!writeNonVertexIndex(buf, m.textureIndex, textureIdxSize, errorMessage) ||
            !writeNonVertexIndex(buf, m.sphereTextureIndex, textureIdxSize, errorMessage)) {
            return false;
        }
        if (m.sphereMode > 3) {
            setError(errorMessage, QStringLiteral("Invalid material sphereMode: %1").arg(m.sphereMode));
            return false;
        }
        writeU8(buf, m.sphereMode);
        writeU8(buf, m.sharedToonFlag);
        if (m.sharedToonFlag == 0) {
            if (!writeNonVertexIndex(buf, m.toonIndex, textureIdxSize, errorMessage)) {
                return false;
            }
        } else if (m.sharedToonFlag == 1) {
            if (!writeNonVertexIndex(buf, m.toonIndex, 1, errorMessage)) {
                return false;
            }
        } else {
            setError(errorMessage, QStringLiteral("Invalid material sharedToonFlag: %1").arg(m.sharedToonFlag));
            return false;
        }
        if (!writeText(buf, m.memo, hdr.textEncoding, errorMessage)) {
            return false;
        }
        if (m.faceVertexCount < 0 || (m.faceVertexCount % 3) != 0) {
            setError(errorMessage, QStringLiteral("Invalid material faceVertexCount: %1 (must be non-negative multiple of 3)").arg(m.faceVertexCount));
            return false;
        }
        writeI32(buf, m.faceVertexCount);
    }

    // 9. Bone Table
    writeI32(buf, static_cast<int32_t>(document.bones.size()));
    for (const PmxBone& b : document.bones) {
        if (!writeText(buf, b.nameJp, hdr.textEncoding, errorMessage) ||
            !writeText(buf, b.nameEn, hdr.textEncoding, errorMessage)) {
            return false;
        }
        writeVec3(buf, b.position);
        if (!writeNonVertexIndex(buf, b.parentBoneIndex, boneIdxSize, errorMessage)) {
            return false;
        }
        writeI32(buf, b.deformTier);
        writeU16(buf, b.boneFlags);

        if ((b.boneFlags & 0x0001u) == 0u) {
            writeVec3(buf, b.tailOffset);
        } else {
            if (!writeNonVertexIndex(buf, b.tailBoneIndex, boneIdxSize, errorMessage)) {
                return false;
            }
        }

        if ((b.boneFlags & (0x0100u | 0x0200u)) != 0u) {
            if (!writeNonVertexIndex(buf, b.inheritParentIndex, boneIdxSize, errorMessage)) {
                return false;
            }
            writeFloat(buf, b.inheritWeight);
        }

        if ((b.boneFlags & 0x0400u) != 0u) {
            writeVec3(buf, b.fixedAxisVector);
        }

        if ((b.boneFlags & 0x0800u) != 0u) {
            writeVec3(buf, b.localAxisX);
            writeVec3(buf, b.localAxisZ);
        }

        if ((b.boneFlags & 0x2000u) != 0u) {
            writeI32(buf, b.externalKey);
        }

        if ((b.boneFlags & 0x0020u) != 0u) {
            if (!writeNonVertexIndex(buf, b.ikTargetBoneIndex, boneIdxSize, errorMessage)) {
                return false;
            }
            writeI32(buf, b.ikLoopCount);
            writeFloat(buf, b.ikLimitAngle);
            writeI32(buf, static_cast<int32_t>(b.ikLinks.size()));
            for (const PmxIkLink& link : b.ikLinks) {
                if (!writeNonVertexIndex(buf, link.boneIndex, boneIdxSize, errorMessage)) {
                    return false;
                }
                writeU8(buf, link.hasAngleLimit);
                if (link.hasAngleLimit != 0) {
                    writeVec3(buf, link.limitMin);
                    writeVec3(buf, link.limitMax);
                }
            }
        }
    }

    // 10. Morph Table
    writeI32(buf, static_cast<int32_t>(document.morphs.size()));
    for (const PmxMorph& m : document.morphs) {
        if (!writeText(buf, m.nameJp, hdr.textEncoding, errorMessage) ||
            !writeText(buf, m.nameEn, hdr.textEncoding, errorMessage)) {
            return false;
        }
        writeU8(buf, m.panelCategory);
        if (m.morphType > 10) {
            setError(errorMessage, QStringLiteral("Invalid morphType: %1").arg(m.morphType));
            return false;
        }
        if ((m.morphType == 9 || m.morphType == 10) && versionToWrite < 2.1f) {
            setError(errorMessage, QStringLiteral("PMX 2.1 required for morphType: %1").arg(m.morphType));
            return false;
        }
        writeU8(buf, m.morphType);

        switch (m.morphType) {
        case 0:
        case 9:
            writeI32(buf, static_cast<int32_t>(m.groupOffsets.size()));
            for (const GroupMorphOffset& off : m.groupOffsets) {
                if (!writeNonVertexIndex(buf, off.morphIndex, morphIdxSize, errorMessage)) {
                    return false;
                }
                writeFloat(buf, off.weight);
            }
            break;
        case 1:
            writeI32(buf, static_cast<int32_t>(m.vertexOffsets.size()));
            for (const VertexMorphOffset& off : m.vertexOffsets) {
                if (!writeVertexIndex(buf, off.vertexIndex, vertexIdxSize, errorMessage)) {
                    return false;
                }
                writeVec3(buf, off.positionOffset);
            }
            break;
        case 2:
            writeI32(buf, static_cast<int32_t>(m.boneOffsets.size()));
            for (const BoneMorphOffset& off : m.boneOffsets) {
                if (!writeNonVertexIndex(buf, off.boneIndex, boneIdxSize, errorMessage)) {
                    return false;
                }
                writeVec3(buf, off.translation);
                writeVec4(buf, glm::vec4(off.rotation.x, off.rotation.y, off.rotation.z, off.rotation.w));
            }
            break;
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
            writeI32(buf, static_cast<int32_t>(m.uvOffsets.size()));
            for (const UvMorphOffset& off : m.uvOffsets) {
                if (!writeVertexIndex(buf, off.vertexIndex, vertexIdxSize, errorMessage)) {
                    return false;
                }
                writeVec4(buf, off.uvOffset);
            }
            break;
        case 8:
            writeI32(buf, static_cast<int32_t>(m.materialOffsets.size()));
            for (const MaterialMorphOffset& off : m.materialOffsets) {
                if (!writeNonVertexIndex(buf, off.materialIndex, materialIdxSize, errorMessage)) {
                    return false;
                }
                writeU8(buf, off.opType);
                writeVec4(buf, off.diffuse);
                writeVec3(buf, off.specular);
                writeFloat(buf, off.specularPower);
                writeVec3(buf, off.ambient);
                writeVec4(buf, off.edgeColor);
                writeFloat(buf, off.edgeSize);
                writeVec4(buf, off.textureTint);
                writeVec4(buf, off.sphereTint);
                writeVec4(buf, off.toonTint);
            }
            break;
        case 10:
            writeI32(buf, static_cast<int32_t>(m.impulseOffsets.size()));
            for (const ImpulseMorphOffset& off : m.impulseOffsets) {
                if (!writeNonVertexIndex(buf, off.rigidBodyIndex, rigidBodyIdxSize, errorMessage)) {
                    return false;
                }
                writeU8(buf, off.isLocal);
                writeVec3(buf, off.velocity);
                writeVec3(buf, off.torque);
            }
            break;
        default:
            setError(errorMessage, QStringLiteral("Invalid morphType: %1").arg(m.morphType));
            return false;
        }
    }

    // 11. Display Frame Table
    writeI32(buf, static_cast<int32_t>(document.displayFrames.size()));
    for (const PmxDisplayFrame& df : document.displayFrames) {
        if (!writeText(buf, df.nameJp, hdr.textEncoding, errorMessage) ||
            !writeText(buf, df.nameEn, hdr.textEncoding, errorMessage)) {
            return false;
        }
        writeU8(buf, df.specialFlag);
        writeI32(buf, static_cast<int32_t>(df.elements.size()));
        for (const PmxDisplayElement& el : df.elements) {
            writeU8(buf, el.targetType);
            if (el.targetType == 0) {
                if (!writeNonVertexIndex(buf, el.index, boneIdxSize, errorMessage)) {
                    return false;
                }
            } else if (el.targetType == 1) {
                if (!writeNonVertexIndex(buf, el.index, morphIdxSize, errorMessage)) {
                    return false;
                }
            } else {
                setError(errorMessage, QStringLiteral("Invalid DisplayElement targetType: %1").arg(el.targetType));
                return false;
            }
        }
    }

    // 12. Rigid Body Table
    writeI32(buf, static_cast<int32_t>(document.rigidBodies.size()));
    for (const PmxRigidBody& rb : document.rigidBodies) {
        if (!writeText(buf, rb.nameJp, hdr.textEncoding, errorMessage) ||
            !writeText(buf, rb.nameEn, hdr.textEncoding, errorMessage) ||
            !writeNonVertexIndex(buf, rb.boneIndex, boneIdxSize, errorMessage)) {
            return false;
        }
        writeU8(buf, rb.groupIndex);
        writeU16(buf, rb.nonCollisionMask);
        writeU8(buf, rb.shape);
        writeVec3(buf, rb.size);
        writeVec3(buf, rb.position);
        writeVec3(buf, rb.rotation);
        writeFloat(buf, rb.mass);
        writeFloat(buf, rb.linearDamping);
        writeFloat(buf, rb.angularDamping);
        writeFloat(buf, rb.restitution);
        writeFloat(buf, rb.friction);
        writeU8(buf, rb.physicsMode);
    }

    // 13. Joint Table
    writeI32(buf, static_cast<int32_t>(document.joints.size()));
    for (const PmxJoint& j : document.joints) {
        if (!writeText(buf, j.nameJp, hdr.textEncoding, errorMessage) ||
            !writeText(buf, j.nameEn, hdr.textEncoding, errorMessage)) {
            return false;
        }
        writeU8(buf, j.jointType);
        if (!writeNonVertexIndex(buf, j.rigidBodyIndexA, rigidBodyIdxSize, errorMessage) ||
            !writeNonVertexIndex(buf, j.rigidBodyIndexB, rigidBodyIdxSize, errorMessage)) {
            return false;
        }
        writeVec3(buf, j.position);
        writeVec3(buf, j.rotation);
        writeVec3(buf, j.linearLowerLimit);
        writeVec3(buf, j.linearUpperLimit);
        writeVec3(buf, j.angularLowerLimit);
        writeVec3(buf, j.angularUpperLimit);
        writeVec3(buf, j.springLinearStiffness);
        writeVec3(buf, j.springAngularStiffness);
    }

    // 14. SoftBody Table (PMX 2.1 only)
    if (versionToWrite >= 2.1f) {
        writeI32(buf, static_cast<int32_t>(document.softBodies.size()));
        for (const PmxSoftBody& sb : document.softBodies) {
            if (!writeText(buf, sb.nameJp, hdr.textEncoding, errorMessage) ||
                !writeText(buf, sb.nameEn, hdr.textEncoding, errorMessage)) {
                return false;
            }
            writeU8(buf, sb.shape);
            if (!writeNonVertexIndex(buf, sb.materialIndex, materialIdxSize, errorMessage)) {
                return false;
            }
            writeU8(buf, sb.groupIndex);
            writeU16(buf, sb.nonCollisionMask);
            writeU8(buf, sb.flags);
            writeI32(buf, sb.blinkDistance);
            writeI32(buf, sb.clusterCount);
            writeFloat(buf, sb.totalMass);
            writeFloat(buf, sb.collisionMargin);
            writeI32(buf, sb.aeroModel);
            for (float f : sb.configFloats) {
                writeFloat(buf, f);
            }
            for (float f : sb.clusterFloats) {
                writeFloat(buf, f);
            }
            for (int32_t iv : sb.iterationInts) {
                writeI32(buf, iv);
            }
            for (int32_t iv : sb.materialInts) {
                writeI32(buf, iv);
            }
            writeI32(buf, static_cast<int32_t>(sb.anchors.size()));
            for (const PmxSoftBodyAnchor& anchor : sb.anchors) {
                if (!writeNonVertexIndex(buf, anchor.rigidBodyIndex, rigidBodyIdxSize, errorMessage) ||
                    !writeVertexIndex(buf, anchor.vertexIndex, vertexIdxSize, errorMessage)) {
                    return false;
                }
                writeU8(buf, anchor.nearMode);
            }
            writeI32(buf, static_cast<int32_t>(sb.pinVertexIndices.size()));
            for (int32_t pinIdx : sb.pinVertexIndices) {
                if (!writeVertexIndex(buf, pinIdx, vertexIdxSize, errorMessage)) {
                    return false;
                }
            }
        }
    }

    outBuffer = std::move(buf);
    return true;
}

bool PmxWriter::writeToBuffer(const PmxDocument& document,
                              std::vector<uint8_t>& outBuffer,
                              QString* errorMessage) {
    QByteArray qbuf;
    if (!writeToBuffer(document, qbuf, errorMessage)) {
        return false;
    }
    outBuffer.assign(reinterpret_cast<const uint8_t*>(qbuf.constData()),
                     reinterpret_cast<const uint8_t*>(qbuf.constData()) + qbuf.size());
    return true;
}

bool PmxWriter::writeToFile(const QString& filePath,
                            const PmxDocument& document,
                            QString* errorMessage) {
    QByteArray qbuf;
    if (!writeToBuffer(document, qbuf, errorMessage)) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(errorMessage, QStringLiteral("Failed to open PMX file for writing: %1").arg(filePath));
        return false;
    }
    if (file.write(qbuf) != qbuf.size()) {
        setError(errorMessage, QStringLiteral("Incomplete write to PMX file: %1").arg(filePath));
        return false;
    }
    return true;
}

} // namespace pmx
