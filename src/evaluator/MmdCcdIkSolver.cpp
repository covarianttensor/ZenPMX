#include "MmdCcdIkSolver.h"
#include "PmxRuntimeScene.h"

#include <algorithm>
#include <cmath>

namespace pmx {

glm::vec3 MmdCcdIkSolver::quatToZxyEuler(const glm::quat& q) {
    glm::mat3 m = glm::mat3_cast(glm::normalize(q));
    glm::vec3 euler(0.0f);
    float sinX = std::clamp(-m[2][1], -1.0f, 1.0f);
    euler.x = std::asin(sinX);

    if (std::abs(sinX) > 0.99999f) {
        // Gimbal lock
        euler.z = 0.0f;
        if (sinX > 0.0f) {
            euler.y = std::atan2(m[1][0], m[0][0]);
        } else {
            euler.y = std::atan2(-m[1][0], m[0][0]);
        }
    } else {
        euler.y = std::atan2(m[2][0], m[2][2]);
        euler.z = std::atan2(m[0][1], m[1][1]);
    }
    return euler;
}

glm::quat MmdCcdIkSolver::zxyEulerToQuat(const glm::vec3& euler) {
    glm::quat qx = glm::angleAxis(euler.x, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qy = glm::angleAxis(euler.y, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qz = glm::angleAxis(euler.z, glm::vec3(0.0f, 0.0f, 1.0f));
    return glm::normalize(qy * qx * qz);
}

glm::quat MmdCcdIkSolver::clampEulerAngles(const glm::quat& q, const glm::vec3& minAngle, const glm::vec3& maxAngle) {
    glm::vec3 euler = quatToZxyEuler(q);
    float minX = std::min(minAngle.x, maxAngle.x);
    float maxX = std::max(minAngle.x, maxAngle.x);
    euler.x = std::clamp(euler.x, minX, maxX);

    float minY = std::min(minAngle.y, maxAngle.y);
    float maxY = std::max(minAngle.y, maxAngle.y);
    euler.y = std::clamp(euler.y, minY, maxY);

    float minZ = std::min(minAngle.z, maxAngle.z);
    float maxZ = std::max(minAngle.z, maxAngle.z);
    euler.z = std::clamp(euler.z, minZ, maxZ);

    return zxyEulerToQuat(euler);
}

void MmdCcdIkSolver::solveChain(PmxRuntimeScene& scene, const IkChain& chain) {
    if (!chain.enabled || chain.links.empty()) {
        return;
    }
    const int totalBones = scene.boneCount();
    if (chain.targetBoneIndex < 0 || chain.targetBoneIndex >= totalBones ||
        chain.ikBoneIndex < 0 || chain.ikBoneIndex >= totalBones) {
        return;
    }

    const int loopCount = std::max(1, chain.loopCount);
    const float limitAngle = chain.limitAngle > 0.0f ? chain.limitAngle : 0.5f;

    for (int iter = 0; iter < loopCount; ++iter) {
        for (const auto& link : chain.links) {
            if (link.boneIndex < 0 || link.boneIndex >= totalBones) {
                continue;
            }

            // Current effector and target positions
            glm::vec3 pe = scene.worldPosition(chain.targetBoneIndex);
            glm::vec3 pt = scene.worldPosition(chain.ikBoneIndex);

            // Convergence check
            if (glm::distance(pe, pt) < 1e-4f) {
                return;
            }

            glm::vec3 pl = scene.worldPosition(link.boneIndex);
            glm::vec3 ve = pe - pl;
            glm::vec3 vt = pt - pl;
            float lenE = glm::length(ve);
            float lenT = glm::length(vt);
            if (lenE < 1e-6f || lenT < 1e-6f) {
                continue;
            }
            ve /= lenE;
            vt /= lenT;

            float c = std::clamp(glm::dot(ve, vt), -1.0f, 1.0f);
            float theta = std::acos(c);
            if (theta < 1e-6f) {
                continue;
            }

            float thetaClamped = std::min(theta, limitAngle);
            glm::vec3 aw = glm::cross(ve, vt);
            float lenAw = glm::length(aw);
            if (lenAw < 1e-6f) {
                continue;
            }
            aw /= lenAw;

            // Transform axis into link local space using inverse world orientation
            glm::mat3 R_world = glm::mat3(scene.worldMatrix(link.boneIndex));
            float det = glm::determinant(R_world);
            if (std::abs(det) < 1e-6f) {
                continue;
            }
            glm::vec3 a_local = glm::inverse(R_world) * aw;
            float lenLocal = glm::length(a_local);
            if (lenLocal < 1e-6f) {
                continue;
            }
            a_local /= lenLocal;

            glm::quat dq = glm::angleAxis(thetaClamped, a_local);
            glm::quat q_new = glm::normalize(scene.bone(link.boneIndex).ikRotation * dq);

            if (link.hasLimits) {
                q_new = clampEulerAngles(q_new, link.minAngle, link.maxAngle);
            }

            scene.setBoneIkRotation(link.boneIndex, q_new);
            scene.updateSubtree(link.boneIndex);
        }
    }
}

void MmdCcdIkSolver::solveAll(PmxRuntimeScene& scene, const std::vector<IkChain>& chains) {
    for (const auto& chain : chains) {
        if (chain.enabled) {
            solveChain(scene, chain);
        }
    }
}

} // namespace pmx
