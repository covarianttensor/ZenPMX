#pragma once

#include <QByteArray>
#include <QString>
#include <optional>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class QIODevice;

namespace pmx {

class PmxRuntimeScene;

struct VpdBonePose {
    QString boneName;
    glm::vec3 translation{0.0f}; // RH coordinates
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // RH coordinates
};

struct VpdMorphPose {
    QString morphName;
    float weight = 0.0f;
};

struct VpdPose {
    QString modelName;
    std::vector<VpdBonePose> bones;
    std::vector<VpdMorphPose> morphs;
};

class VpdImporter {
public:
    static std::optional<VpdPose> loadFromDevice(QIODevice* device, QString* error = nullptr);
    static std::optional<VpdPose> loadFromByteArray(const QByteArray& data, QString* error = nullptr);
    static std::optional<VpdPose> loadFromFile(const QString& filePath, QString* error = nullptr);

    static QByteArray writeToByteArray(const VpdPose& pose);
    static bool writeToFile(const QString& filePath, const VpdPose& pose, QString* error = nullptr);

    static void applyToScene(const VpdPose& pose, PmxRuntimeScene& scene);
};

} // namespace pmx
