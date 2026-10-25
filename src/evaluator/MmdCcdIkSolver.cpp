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

    for (const auto& link : chain.links) {
        if (link.boneIndex >= 0 && link.boneIndex < totalBones) {
            scene.setBoneIkRotation(link.boneIndex, glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            scene.updateSubtree(link.boneIndex);
        }
    }

    std::vector<float> planeAngles(chain.links.size(), 0.0f);

    for (int iter = 0; iter < loopCount; ++iter) {
        for (size_t linkIdx = 0; linkIdx < chain.links.size(); ++linkIdx) {
            const auto& link = chain.links[linkIdx];
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

            bool isHingeX = link.hasLimits &&
                            std::abs(link.minAngle.y - link.maxAngle.y) < 1e-4f &&
                            std::abs(link.minAngle.z - link.maxAngle.z) < 1e-4f;

            if (isHingeX) {
                glm::mat4 invLink = glm::inverse(scene.worldMatrix(link.boneIndex));
                glm::vec3 localE = glm::vec3(invLink * glm::vec4(pe, 1.0f));
                glm::vec3 localT = glm::vec3(invLink * glm::vec4(pt, 1.0f));
                float lenLocalE = glm::length(localE);
                float lenLocalT = glm::length(localT);
                if (!std::isfinite(lenLocalE) || !std::isfinite(lenLocalT) || lenLocalE < 1e-6f || lenLocalT < 1e-6f) {
                    continue;
                }
                glm::vec3 ve = localE / lenLocalE;
                glm::vec3 vt = localT / lenLocalT;

                float dot = glm::clamp(glm::dot(ve, vt), -1.0f, 1.0f);
                float angle = std::acos(dot);
                if (angle < 1e-6f) {
                    continue;
                }
                angle = std::min(angle, limitAngle);

                glm::quat rot1 = glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f));
                glm::quat rot2 = glm::angleAxis(-angle, glm::vec3(1.0f, 0.0f, 0.0f));
                float dot1 = glm::dot(rot1 * ve, vt);
                float dot2 = glm::dot(rot2 * ve, vt);

                float step = (dot1 > dot2) ? angle : -angle;
                float newAngle = planeAngles[linkIdx] + step;

                if (iter == 0) {
                    if ((newAngle < link.minAngle.x || newAngle > link.maxAngle.x) &&
                        (-newAngle >= link.minAngle.x && -newAngle <= link.maxAngle.x)) {
                        newAngle = -newAngle;
                    } else if (newAngle < link.minAngle.x || newAngle > link.maxAngle.x) {
                        float halfRad = (link.minAngle.x + link.maxAngle.x) * 0.5f;
                        if (std::abs(halfRad - newAngle) > std::abs(halfRad + newAngle)) {
                            newAngle = -newAngle;
                        }
                    }
                }

                newAngle = std::clamp(newAngle, link.minAngle.x, link.maxAngle.x);
                planeAngles[linkIdx] = newAngle;

                glm::quat q_desired = glm::angleAxis(newAngle, glm::vec3(1.0f, 0.0f, 0.0f));
                const auto& bone = scene.bone(link.boneIndex);
                glm::quat q_base = bone.userRotation * bone.morphRotation;
                if (glm::dot(q_base, q_base) > 1e-8f) {
                    q_base = glm::normalize(q_base);
                } else {
                    q_base = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                }
                glm::quat q_ik = glm::normalize(q_desired * glm::inverse(q_base));
                scene.setBoneIkRotation(link.boneIndex, q_ik);
                scene.updateSubtree(link.boneIndex);
            } else {
                glm::vec3 pl = scene.worldPosition(link.boneIndex);
                glm::vec3 ve = pe - pl;
                glm::vec3 vt = pt - pl;
                float lenE = glm::length(ve);
                float lenT = glm::length(vt);
                if (!std::isfinite(lenE) || !std::isfinite(lenT) || lenE < 1e-6f || lenT < 1e-6f) {
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
}

void MmdCcdIkSolver::solveAll(PmxRuntimeScene& scene, const std::vector<IkChain>& chains) {
    for (const auto& chain : chains) {
        if (chain.enabled) {
            solveChain(scene, chain);
        }
    }
}

} // namespace pmx
