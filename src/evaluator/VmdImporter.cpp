#include "VmdImporter.h"

#include <QFile>
#include <QIODevice>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <span>

namespace pmx {

namespace {

void setError(QString* errorMessage, const QString& msg) {
    if (errorMessage != nullptr) {
        *errorMessage = msg;
    }
}

QString decodeShiftJis(const char* data, size_t maxLen) {
    size_t len = 0;
    while (len < maxLen && data[len] != '\0') {
        ++len;
    }
    auto encoding = QStringDecoder::encodingForName("Shift-JIS");
    if (encoding.has_value()) {
        QStringDecoder decoder(encoding.value());
        QString res = decoder(QByteArrayView(data, static_cast<qsizetype>(len)));
        if (!decoder.hasError()) {
            return res;
        }
    }
    return QString::fromUtf8(data, static_cast<qsizetype>(len));
}

QByteArray encodeShiftJis(const QString& str, size_t fixedLength = 0) {
    QByteArray bytes;
    auto encoding = QStringConverter::encodingForName("Shift-JIS");
    if (encoding.has_value()) {
        QStringEncoder encoder(encoding.value());
        bytes = encoder(str);
    } else {
        bytes = str.toUtf8();
    }
    if (fixedLength > 0) {
        if (static_cast<size_t>(bytes.size()) > fixedLength) {
            bytes.truncate(static_cast<qsizetype>(fixedLength));
        } else if (static_cast<size_t>(bytes.size()) < fixedLength) {
            bytes.append(QByteArray(static_cast<qsizetype>(fixedLength - bytes.size()), '\0'));
        }
    }
    return bytes;
}

bool hasRemaining(std::span<const uint8_t> buffer, size_t offset, size_t count) {
    return offset <= buffer.size() && count <= (buffer.size() - offset);
}

bool readU8(std::span<const uint8_t> buffer, size_t& offset, uint8_t& out) {
    if (!hasRemaining(buffer, offset, sizeof(uint8_t))) {
        return false;
    }
    out = buffer[offset];
    offset += sizeof(uint8_t);
    return true;
}

bool readU32(std::span<const uint8_t> buffer, size_t& offset, uint32_t& out) {
    if (!hasRemaining(buffer, offset, sizeof(uint32_t))) {
        return false;
    }
    quint32 raw = 0;
    std::memcpy(&raw, buffer.data() + offset, sizeof(quint32));
    out = static_cast<uint32_t>(qFromLittleEndian(raw));
    offset += sizeof(uint32_t);
    return true;
}

bool readFloat(std::span<const uint8_t> buffer, size_t& offset, float& out) {
    if (!hasRemaining(buffer, offset, sizeof(uint32_t))) {
        return false;
    }
    quint32 rawBits = 0;
    std::memcpy(&rawBits, buffer.data() + offset, sizeof(quint32));
    rawBits = qFromLittleEndian(rawBits);
    std::memcpy(&out, &rawBits, sizeof(float));
    offset += sizeof(float);
    return true;
}

} // namespace

std::optional<VmdMotion> VmdImporter::loadFromDevice(QIODevice* device, QString* error) {
    if (!device || !device->isOpen() || !device->isReadable()) {
        setError(error, QStringLiteral("Device is not open or readable"));
        return std::nullopt;
    }
    QByteArray data = device->readAll();
    return loadFromByteArray(data, error);
}

std::optional<VmdMotion> VmdImporter::loadFromFile(const QString& filePath, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Cannot open file: %1").arg(filePath));
        return std::nullopt;
    }
    return loadFromDevice(&file, error);
}

std::optional<VmdMotion> VmdImporter::loadFromByteArray(const QByteArray& data, QString* error) {
    if (data.size() < 50) {
        setError(error, QStringLiteral("VMD data too small (expected at least 50 bytes header)"));
        return std::nullopt;
    }

    std::span<const uint8_t> buffer(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size()));
    size_t offset = 0;

    // 1. Header (30 bytes)
    const char* rawHeader = reinterpret_cast<const char*>(buffer.data());
    if (std::memcmp(rawHeader, "Vocaloid Motion Data", 20) != 0) {
        setError(error, QStringLiteral("Invalid VMD header signature"));
        return std::nullopt;
    }
    VmdMotion motion;
    motion.version = decodeShiftJis(rawHeader, 30);
    offset += 30;

    // 2. Model Name (20 bytes)
    motion.modelName = decodeShiftJis(reinterpret_cast<const char*>(buffer.data() + offset), 20);
    offset += 20;

    // 3. Bone Keyframes
    uint32_t boneCount = 0;
    if (!readU32(buffer, offset, boneCount)) {
        setError(error, QStringLiteral("Unexpected EOF while reading bone keyframe count"));
        return std::nullopt;
    }

    constexpr size_t boneRecordSize = 111;
    size_t maxPossibleBones = (buffer.size() >= offset) ? ((buffer.size() - offset) / boneRecordSize) : 0;
    if (boneCount > maxPossibleBones) {
        setError(error, QStringLiteral("Unexpected EOF while reading %1 bone keyframes").arg(boneCount));
        return std::nullopt;
    }

    motion.boneKeyframes.reserve(boneCount);
    for (uint32_t i = 0; i < boneCount; ++i) {
        VmdBoneKeyframe kf;
        kf.boneName = decodeShiftJis(reinterpret_cast<const char*>(buffer.data() + offset), 15);
        offset += 15;

        readU32(buffer, offset, kf.frame);

        glm::vec3 posLH{0.0f};
        readFloat(buffer, offset, posLH.x);
        readFloat(buffer, offset, posLH.y);
        readFloat(buffer, offset, posLH.z);
        // Left-Handed to Right-Handed: (x, y, -z)
        kf.translation = glm::vec3(posLH.x, posLH.y, -posLH.z);

        glm::vec4 rotLH{0.0f}; // x, y, z, w
        readFloat(buffer, offset, rotLH.x);
        readFloat(buffer, offset, rotLH.y);
        readFloat(buffer, offset, rotLH.z);
        readFloat(buffer, offset, rotLH.w);
        // Left-Handed to Right-Handed: (w, -x, -y, z)
        glm::quat q(rotLH.w, -rotLH.x, -rotLH.y, rotLH.z);
        if (glm::dot(q, q) > 1e-8f) {
            kf.rotation = glm::normalize(q);
        } else {
            kf.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }

        // 64-byte interpolation curves
        const uint8_t* rawInterp = buffer.data() + offset;
        kf.interpX[0] = rawInterp[0];  kf.interpX[1] = rawInterp[4];
        kf.interpX[2] = rawInterp[8];  kf.interpX[3] = rawInterp[12];

        kf.interpY[0] = rawInterp[1];  kf.interpY[1] = rawInterp[5];
        kf.interpY[2] = rawInterp[9];  kf.interpY[3] = rawInterp[13];

        kf.interpZ[0] = rawInterp[2];  kf.interpZ[1] = rawInterp[6];
        kf.interpZ[2] = rawInterp[10]; kf.interpZ[3] = rawInterp[14];

        kf.interpR[0] = rawInterp[3];  kf.interpR[1] = rawInterp[7];
        kf.interpR[2] = rawInterp[11]; kf.interpR[3] = rawInterp[15];
        offset += 64;

        motion.maxFrame = std::max(motion.maxFrame, kf.frame);
        motion.boneKeyframes.push_back(std::move(kf));
    }

    // 4. Morph Keyframes
    uint32_t morphCount = 0;
    if (readU32(buffer, offset, morphCount)) {
        constexpr size_t morphRecordSize = 23;
        size_t maxPossibleMorphs = (buffer.size() >= offset) ? ((buffer.size() - offset) / morphRecordSize) : 0;
        if (morphCount > maxPossibleMorphs) {
            setError(error, QStringLiteral("Unexpected EOF while reading %1 morph keyframes").arg(morphCount));
            return std::nullopt;
        }

        motion.morphKeyframes.reserve(morphCount);
        for (uint32_t i = 0; i < morphCount; ++i) {
            VmdMorphKeyframe kf;
            kf.morphName = decodeShiftJis(reinterpret_cast<const char*>(buffer.data() + offset), 15);
            offset += 15;

            readU32(buffer, offset, kf.frame);
            readFloat(buffer, offset, kf.weight);

            motion.maxFrame = std::max(motion.maxFrame, kf.frame);
            motion.morphKeyframes.push_back(std::move(kf));
        }
    }

    // 5. Optional Camera Keyframes
    uint32_t cameraCount = 0;
    if (readU32(buffer, offset, cameraCount)) {
        constexpr size_t cameraRecordSize = 61;
        size_t maxPossible = (buffer.size() >= offset) ? ((buffer.size() - offset) / cameraRecordSize) : 0;
        if (cameraCount > maxPossible) {
            setError(error, QStringLiteral("Unexpected EOF while reading %1 camera keyframes").arg(cameraCount));
            return std::nullopt;
        }
        offset += static_cast<size_t>(cameraCount) * cameraRecordSize;
    }

    // 6. Optional Light Keyframes
    uint32_t lightCount = 0;
    if (readU32(buffer, offset, lightCount)) {
        constexpr size_t lightRecordSize = 28;
        size_t maxPossible = (buffer.size() >= offset) ? ((buffer.size() - offset) / lightRecordSize) : 0;
        if (lightCount > maxPossible) {
            setError(error, QStringLiteral("Unexpected EOF while reading %1 light keyframes").arg(lightCount));
            return std::nullopt;
        }
        offset += static_cast<size_t>(lightCount) * lightRecordSize;
    }

    // 7. Optional Self Shadow Keyframes
    uint32_t shadowCount = 0;
    if (readU32(buffer, offset, shadowCount)) {
        constexpr size_t shadowRecordSize = 9;
        size_t maxPossible = (buffer.size() >= offset) ? ((buffer.size() - offset) / shadowRecordSize) : 0;
        if (shadowCount > maxPossible) {
            setError(error, QStringLiteral("Unexpected EOF while reading %1 shadow keyframes").arg(shadowCount));
            return std::nullopt;
        }
        offset += static_cast<size_t>(shadowCount) * shadowRecordSize;
    }

    // 8. Optional IK Keyframes
    uint32_t ikCount = 0;
    if (readU32(buffer, offset, ikCount)) {
        constexpr size_t minIkRecordSize = 9; // 4 frame + 1 showModel + 4 toggleCount
        size_t maxPossibleIk = (buffer.size() >= offset) ? ((buffer.size() - offset) / minIkRecordSize) : 0;
        if (ikCount > maxPossibleIk) {
            setError(error, QStringLiteral("Unexpected EOF while reading %1 IK keyframes").arg(ikCount));
            return std::nullopt;
        }
        motion.ikKeyframes.reserve(ikCount);
        for (uint32_t i = 0; i < ikCount; ++i) {
            if (!hasRemaining(buffer, offset, 9)) {
                setError(error, QStringLiteral("Truncated IK keyframe record %1").arg(i));
                return std::nullopt;
            }
            VmdIkKeyframe kf;
            readU32(buffer, offset, kf.frame);
            uint8_t showModel = 1;
            readU8(buffer, offset, showModel);
            kf.showModel = (showModel != 0);

            uint32_t toggleCount = 0;
            readU32(buffer, offset, toggleCount);

            constexpr size_t toggleRecordSize = 21; // 20 name + 1 enable
            size_t maxPossibleToggles = (buffer.size() >= offset) ? ((buffer.size() - offset) / toggleRecordSize) : 0;
            if (toggleCount > maxPossibleToggles) {
                setError(error, QStringLiteral("Unexpected EOF while reading %1 IK toggles in keyframe %2").arg(toggleCount).arg(i));
                return std::nullopt;
            }

            kf.ikToggles.reserve(toggleCount);
            for (uint32_t j = 0; j < toggleCount; ++j) {
                QString ikBoneName = decodeShiftJis(reinterpret_cast<const char*>(buffer.data() + offset), 20);
                offset += 20;
                uint8_t enable = 1;
                readU8(buffer, offset, enable);
                kf.ikToggles.push_back({ikBoneName, (enable != 0)});
            }

            motion.maxFrame = std::max(motion.maxFrame, kf.frame);
            motion.ikKeyframes.push_back(std::move(kf));
        }
    }

    return motion;
}

QByteArray VmdImporter::writeToByteArray(const VmdMotion& motion) {
    QByteArray bytes;

    // 1. Header (30 bytes)
    QByteArray versionBytes = motion.version.toUtf8();
    if (versionBytes.isEmpty()) {
        versionBytes = "Vocaloid Motion Data 0002";
    }
    if (versionBytes.size() > 30) {
        versionBytes.truncate(30);
    } else {
        versionBytes.append(QByteArray(30 - versionBytes.size(), '\0'));
    }
    bytes.append(versionBytes);

    // 2. Model Name (20 bytes)
    bytes.append(encodeShiftJis(motion.modelName, 20));

    // 3. Bone Keyframes
    quint32 boneCount = qToLittleEndian(static_cast<quint32>(motion.boneKeyframes.size()));
    bytes.append(reinterpret_cast<const char*>(&boneCount), sizeof(quint32));

    for (const auto& kf : motion.boneKeyframes) {
        bytes.append(encodeShiftJis(kf.boneName, 15));

        quint32 frame = qToLittleEndian(static_cast<quint32>(kf.frame));
        bytes.append(reinterpret_cast<const char*>(&frame), sizeof(quint32));

        // Right-Handed to Left-Handed translation: (x, y, -z)
        float px = kf.translation.x;
        float py = kf.translation.y;
        float pz = -kf.translation.z;

        quint32 rawPx = qToLittleEndian(*reinterpret_cast<quint32*>(&px));
        quint32 rawPy = qToLittleEndian(*reinterpret_cast<quint32*>(&py));
        quint32 rawPz = qToLittleEndian(*reinterpret_cast<quint32*>(&pz));
        bytes.append(reinterpret_cast<const char*>(&rawPx), sizeof(quint32));
        bytes.append(reinterpret_cast<const char*>(&rawPy), sizeof(quint32));
        bytes.append(reinterpret_cast<const char*>(&rawPz), sizeof(quint32));

        // Right-Handed to Left-Handed rotation: (-x, -y, z, w)
        float rx = -kf.rotation.x;
        float ry = -kf.rotation.y;
        float rz = kf.rotation.z;
        float rw = kf.rotation.w;

        quint32 rawRx = qToLittleEndian(*reinterpret_cast<quint32*>(&rx));
        quint32 rawRy = qToLittleEndian(*reinterpret_cast<quint32*>(&ry));
        quint32 rawRz = qToLittleEndian(*reinterpret_cast<quint32*>(&rz));
        quint32 rawRw = qToLittleEndian(*reinterpret_cast<quint32*>(&rw));
        bytes.append(reinterpret_cast<const char*>(&rawRx), sizeof(quint32));
        bytes.append(reinterpret_cast<const char*>(&rawRy), sizeof(quint32));
        bytes.append(reinterpret_cast<const char*>(&rawRz), sizeof(quint32));
        bytes.append(reinterpret_cast<const char*>(&rawRw), sizeof(quint32));

        // 64-byte interpolation curves
        uint8_t rawInterp[64]{};
        rawInterp[0] = kf.interpX[0];  rawInterp[4] = kf.interpX[1];
        rawInterp[8] = kf.interpX[2];  rawInterp[12] = kf.interpX[3];

        rawInterp[1] = kf.interpY[0];  rawInterp[5] = kf.interpY[1];
        rawInterp[9] = kf.interpY[2];  rawInterp[13] = kf.interpY[3];

        rawInterp[2] = kf.interpZ[0];  rawInterp[6] = kf.interpZ[1];
        rawInterp[10] = kf.interpZ[2]; rawInterp[14] = kf.interpZ[3];

        rawInterp[3] = kf.interpR[0];  rawInterp[7] = kf.interpR[1];
        rawInterp[11] = kf.interpR[2]; rawInterp[15] = kf.interpR[3];

        for (int b = 1; b < 4; ++b) {
            for (int j = 0; j < 16; ++j) {
                rawInterp[b * 16 + j] = rawInterp[j];
            }
        }
        bytes.append(reinterpret_cast<const char*>(rawInterp), 64);
    }

    // 4. Morph Keyframes
    quint32 morphCount = qToLittleEndian(static_cast<quint32>(motion.morphKeyframes.size()));
    bytes.append(reinterpret_cast<const char*>(&morphCount), sizeof(quint32));

    for (const auto& kf : motion.morphKeyframes) {
        bytes.append(encodeShiftJis(kf.morphName, 15));

        quint32 frame = qToLittleEndian(static_cast<quint32>(kf.frame));
        bytes.append(reinterpret_cast<const char*>(&frame), sizeof(quint32));

        float w = kf.weight;
        quint32 rawW = qToLittleEndian(*reinterpret_cast<quint32*>(&w));
        bytes.append(reinterpret_cast<const char*>(&rawW), sizeof(quint32));
    }

    // 5. Camera count (0)
    quint32 zero = 0;
    bytes.append(reinterpret_cast<const char*>(&zero), sizeof(quint32));

    // 6. Light count (0)
    bytes.append(reinterpret_cast<const char*>(&zero), sizeof(quint32));

    // 7. Self shadow count (0)
    bytes.append(reinterpret_cast<const char*>(&zero), sizeof(quint32));

    // 8. IK Keyframes
    quint32 ikCount = qToLittleEndian(static_cast<quint32>(motion.ikKeyframes.size()));
    bytes.append(reinterpret_cast<const char*>(&ikCount), sizeof(quint32));

    for (const auto& kf : motion.ikKeyframes) {
        quint32 frame = qToLittleEndian(static_cast<quint32>(kf.frame));
        bytes.append(reinterpret_cast<const char*>(&frame), sizeof(quint32));

        uint8_t show = kf.showModel ? 1 : 0;
        bytes.append(reinterpret_cast<const char*>(&show), sizeof(uint8_t));

        quint32 toggleCount = qToLittleEndian(static_cast<quint32>(kf.ikToggles.size()));
        bytes.append(reinterpret_cast<const char*>(&toggleCount), sizeof(quint32));

        for (const auto& toggle : kf.ikToggles) {
            bytes.append(encodeShiftJis(toggle.ikBoneName, 20));
            uint8_t enable = toggle.enable ? 1 : 0;
            bytes.append(reinterpret_cast<const char*>(&enable), sizeof(uint8_t));
        }
    }

    return bytes;
}

bool VmdImporter::writeToFile(const QString& filePath, const VmdMotion& motion, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Cannot open file for writing: %1").arg(filePath));
        return false;
    }
    QByteArray data = writeToByteArray(motion);
    if (file.write(data) != data.size()) {
        setError(error, QStringLiteral("Failed to write all bytes to file: %1").arg(filePath));
        return false;
    }
    return true;
}

} // namespace pmx
