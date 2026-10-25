#pragma once

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class QIODevice;

namespace pmx {

struct VmdBoneKeyframe {
    QString boneName;
    uint32_t frame = 0;
    glm::vec3 translation{0.0f}; // RH coordinates
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // RH coordinates
    // 4 Bézier interpolation curves (X, Y, Z, R): control points [x1, y1, x2, y2]
    uint8_t interpX[4]{20, 20, 107, 107};
    uint8_t interpY[4]{20, 20, 107, 107};
    uint8_t interpZ[4]{20, 20, 107, 107};
    uint8_t interpR[4]{20, 20, 107, 107};
};

struct VmdMorphKeyframe {
    QString morphName;
    uint32_t frame = 0;
    float weight = 0.0f;
};

struct VmdIkToggle {
    QString ikBoneName;
    bool enable = true;
};

struct VmdIkKeyframe {
    uint32_t frame = 0;
    bool showModel = true;
    std::vector<VmdIkToggle> ikToggles;
};

struct VmdMotion {
    QString version;
    QString modelName;
    std::vector<VmdBoneKeyframe> boneKeyframes;
    std::vector<VmdMorphKeyframe> morphKeyframes;
    std::vector<VmdIkKeyframe> ikKeyframes;
    uint32_t maxFrame = 0;
};

class VmdImporter {
public:
    static std::optional<VmdMotion> loadFromDevice(QIODevice* device, QString* error = nullptr);
    static std::optional<VmdMotion> loadFromByteArray(const QByteArray& data, QString* error = nullptr);
    static std::optional<VmdMotion> loadFromFile(const QString& filePath, QString* error = nullptr);

    static QByteArray writeToByteArray(const VmdMotion& motion);
    static bool writeToFile(const QString& filePath, const VmdMotion& motion, QString* error = nullptr);
};

} // namespace pmx
