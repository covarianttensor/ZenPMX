#include "VpdImporter.h"
#include "PmxRuntimeScene.h"

#include <QFile>
#include <QIODevice>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QStringEncoder>
#include <algorithm>
#include <cmath>

namespace pmx {

namespace {

void setError(QString* errorMessage, const QString& msg) {
    if (errorMessage != nullptr) {
        *errorMessage = msg;
    }
}

QByteArray encodeShiftJis(const QString& str) {
    auto encoding = QStringConverter::encodingForName("Shift-JIS");
    if (encoding.has_value()) {
        QStringEncoder encoder(encoding.value());
        return encoder(str);
    }
    return str.toUtf8();
}

QString decodeText(const QByteArray& data) {
    if (data.startsWith("\xEF\xBB\xBF")) {
        return QString::fromUtf8(data.constData() + 3, data.size() - 3);
    }
    auto encoding = QStringDecoder::encodingForName("Shift-JIS");
    if (encoding.has_value()) {
        QStringDecoder decoder(encoding.value());
        QString res = decoder(data);
        if (!decoder.hasError()) {
            return res;
        }
    }
    return QString::fromUtf8(data);
}

struct FloatToken {
    float value = 0.0f;
    qsizetype pos = 0;
    qsizetype length = 0;
};

std::vector<FloatToken> extractFloats(const QString& text) {
    static const QRegularExpression floatRegex(QStringLiteral(R"([+-]?(?:(?:\d+\.?\d*)|(?:\.\d+))(?:[eE][+-]?\d+)?)"));
    std::vector<FloatToken> tokens;
    auto it = floatRegex.globalMatch(text);
    while (it.hasNext()) {
        auto match = it.next();
        bool ok = false;
        float v = match.captured(0).toFloat(&ok);
        if (ok && !std::isnan(v) && !std::isinf(v)) {
            tokens.push_back({v, match.capturedStart(0), match.capturedLength(0)});
        }
    }
    return tokens;
}

} // namespace

std::optional<VpdPose> VpdImporter::loadFromDevice(QIODevice* device, QString* error) {
    if (!device || !device->isOpen() || !device->isReadable()) {
        setError(error, QStringLiteral("Device is not open or readable"));
        return std::nullopt;
    }
    QByteArray data = device->readAll();
    return loadFromByteArray(data, error);
}

std::optional<VpdPose> VpdImporter::loadFromFile(const QString& filePath, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Cannot open file: %1").arg(filePath));
        return std::nullopt;
    }
    return loadFromDevice(&file, error);
}

std::optional<VpdPose> VpdImporter::loadFromByteArray(const QByteArray& data, QString* error) {
    QString text = decodeText(data);
    if (text.isEmpty()) {
        setError(error, QStringLiteral("VPD data is empty"));
        return std::nullopt;
    }

    // Strip comments (from "//" to newline)
    static const QRegularExpression commentRegex(QStringLiteral(R"(//[^\r\n]*)"));
    text.replace(commentRegex, QStringLiteral(" "));

    // Header check
    if (!text.contains(QStringLiteral("Vocaloid Pose Data file"), Qt::CaseInsensitive)) {
        setError(error, QStringLiteral("Missing 'Vocaloid Pose Data file' header in VPD"));
        return std::nullopt;
    }

    VpdPose pose;

    // Optional model name: token ending with ';' after header
    static const QRegularExpression modelRegex(QStringLiteral(R"(Vocaloid Pose Data file[\r\n\s]*([^;\r\n]+);)"),
                                               QRegularExpression::CaseInsensitiveOption);
    auto modelMatch = modelRegex.match(text);
    if (modelMatch.hasMatch()) {
        pose.modelName = modelMatch.captured(1).trimmed();
    }

    // Match blocks: Bone<index>{...} or Morph<index>{...}
    static const QRegularExpression blockRegex(QStringLiteral(R"((Bone|Morph)\s*(\d*)\s*\{([^}]*)\})"),
                                               QRegularExpression::CaseInsensitiveOption);
    auto blockIt = blockRegex.globalMatch(text);

    while (blockIt.hasNext()) {
        auto match = blockIt.next();
        QString type = match.captured(1);
        QString inner = match.captured(3);

        if (type.compare(QStringLiteral("Bone"), Qt::CaseInsensitive) == 0) {
            // Inner content has two semicolon-delimited sections:
            // Section 0: boneName and 3 floats (tx, ty, tz)
            // Section 1: 4 floats (qx, qy, qz, qw)
            QStringList parts = inner.split(QLatin1Char(';'));
            if (parts.size() >= 2) {
                QString part0 = parts[0];
                QString part1 = parts[1];

                auto floats0 = extractFloats(part0);
                auto floats1 = extractFloats(part1);

                if (floats0.size() >= 3 && floats1.size() >= 4) {
                    qsizetype nameEnd = floats0[floats0.size() - 3].pos;
                    QString boneName = part0.left(nameEnd).trimmed();

                    float tx = floats0[floats0.size() - 3].value;
                    float ty = floats0[floats0.size() - 2].value;
                    float tz = floats0[floats0.size() - 1].value;

                    float qx = floats1[0].value;
                    float qy = floats1[1].value;
                    float qz = floats1[2].value;
                    float qw = floats1[3].value;

                    VpdBonePose bonePose;
                    bonePose.boneName = boneName;
                    // Left-Handed to Right-Handed: (tx, ty, -tz)
                    bonePose.translation = glm::vec3(tx, ty, -tz);

                    // Left-Handed to Right-Handed: (w, -x, -y, z)
                    glm::quat q(qw, -qx, -qy, qz);
                    if (glm::dot(q, q) > 1e-8f) {
                        bonePose.rotation = glm::normalize(q);
                    } else {
                        bonePose.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    }

                    pose.bones.push_back(std::move(bonePose));
                }
            }
        } else if (type.compare(QStringLiteral("Morph"), Qt::CaseInsensitive) == 0) {
            // Inner content has weight float before ';'
            QStringList parts = inner.split(QLatin1Char(';'));
            if (!parts.isEmpty()) {
                QString part0 = parts[0];
                auto floats0 = extractFloats(part0);
                if (!floats0.empty()) {
                    qsizetype nameEnd = floats0.back().pos;
                    QString morphName = part0.left(nameEnd).trimmed();
                    float weight = floats0.back().value;

                    VpdMorphPose morphPose;
                    morphPose.morphName = morphName;
                    morphPose.weight = weight;
                    pose.morphs.push_back(std::move(morphPose));
                }
            }
        }
    }

    return pose;
}

QByteArray VpdImporter::writeToByteArray(const VpdPose& pose) {
    QString out;
    out += QStringLiteral("Vocaloid Pose Data file\r\n\r\n");

    QString model = pose.modelName.isEmpty() ? QStringLiteral("model.osm") : pose.modelName;
    out += model + QStringLiteral("; // 親ファイル名\r\n");
    out += QString::number(pose.bones.size()) + QStringLiteral("; // 総ポーズボーン数\r\n\r\n");

    for (size_t i = 0; i < pose.bones.size(); ++i) {
        const auto& b = pose.bones[i];
        out += QStringLiteral("Bone%1{%2\r\n").arg(i).arg(b.boneName);

        // Right-Handed to Left-Handed translation: (x, y, -z)
        float tx = b.translation.x;
        float ty = b.translation.y;
        float tz = -b.translation.z;
        out += QStringLiteral("  %1,%2,%3;\r\n")
                   .arg(QString::number(tx, 'f', 6))
                   .arg(QString::number(ty, 'f', 6))
                   .arg(QString::number(tz, 'f', 6));

        // Right-Handed to Left-Handed rotation: (-x, -y, z, w)
        float qx = -b.rotation.x;
        float qy = -b.rotation.y;
        float qz = b.rotation.z;
        float qw = b.rotation.w;
        out += QStringLiteral("  %1,%2,%3,%4;\r\n}\r\n\r\n")
                   .arg(QString::number(qx, 'f', 6))
                   .arg(QString::number(qy, 'f', 6))
                   .arg(QString::number(qz, 'f', 6))
                   .arg(QString::number(qw, 'f', 6));
    }

    for (size_t i = 0; i < pose.morphs.size(); ++i) {
        const auto& m = pose.morphs[i];
        out += QStringLiteral("Morph%1{%2\r\n").arg(i).arg(m.morphName);
        out += QStringLiteral("  %1;\r\n}\r\n\r\n").arg(QString::number(m.weight, 'f', 6));
    }

    return encodeShiftJis(out);
}

bool VpdImporter::writeToFile(const QString& filePath, const VpdPose& pose, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Cannot open file for writing: %1").arg(filePath));
        return false;
    }
    QByteArray data = writeToByteArray(pose);
    if (file.write(data) != data.size()) {
        setError(error, QStringLiteral("Failed to write all bytes to file: %1").arg(filePath));
        return false;
    }
    return true;
}

void VpdImporter::applyToScene(const VpdPose& pose, PmxRuntimeScene& scene) {
    for (const auto& bone : pose.bones) {
        int idx = scene.findBoneByName(bone.boneName);
        if (idx >= 0) {
            scene.setBoneTransform(idx, bone.translation, bone.rotation);
        }
    }
    for (const auto& morph : pose.morphs) {
        int idx = scene.findMorphByName(morph.morphName);
        if (idx >= 0) {
            scene.setMorphWeight(idx, morph.weight);
        }
    }
}

} // namespace pmx
