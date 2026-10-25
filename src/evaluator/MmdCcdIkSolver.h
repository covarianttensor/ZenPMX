#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pmx {

class PmxRuntimeScene;

struct IkLinkConstraint {
    int32_t boneIndex = -1;
    bool hasLimits = false;
    glm::vec3 minAngle{0.0f}; // In RH radians
    glm::vec3 maxAngle{0.0f}; // In RH radians
};

struct IkChain {
    int32_t ikBoneIndex = -1;       // The IK controller bone (e.g. 左足ＩＫ)
    int32_t targetBoneIndex = -1;   // The effector bone (e.g. 左足首)
    int32_t loopCount = 40;
    float limitAngle = 0.5f;        // In radians
    std::vector<IkLinkConstraint> links;
    bool enabled = true;
};

class MmdCcdIkSolver {
public:
    static void solveChain(PmxRuntimeScene& scene, const IkChain& chain);
    static void solveAll(PmxRuntimeScene& scene, const std::vector<IkChain>& chains);

    // Euler conversion & clamping helpers (ZXY extrinsic / YXZ intrinsic)
    static glm::vec3 quatToZxyEuler(const glm::quat& q);
    static glm::quat zxyEulerToQuat(const glm::vec3& euler);
    static glm::quat clampEulerAngles(const glm::quat& q, const glm::vec3& minAngle, const glm::vec3& maxAngle);
};

} // namespace pmx
