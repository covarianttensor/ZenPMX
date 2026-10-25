#include "viewport/PmxWireframeRenderer.h"
#include "viewport/PmxCamera.h"

#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace pmx {

namespace {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;

inline bool isVec3Finite(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline glm::mat3 makeEulerRotationMatrix(const glm::vec3& pmxEuler) {
    glm::vec3 rRH = PmxCamera::toRightHandedEuler(pmxEuler);
    glm::mat3 rotZ = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.z, glm::vec3(0.0f, 0.0f, 1.0f)));
    glm::mat3 rotX = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.x, glm::vec3(1.0f, 0.0f, 0.0f)));
    glm::mat3 rotY = glm::mat3(glm::rotate(glm::mat4(1.0f), rRH.y, glm::vec3(0.0f, 1.0f, 0.0f)));
    return rotY * rotX * rotZ;
}

} // namespace

std::vector<LineVertex> PmxWireframeRenderer::buildBoneWireframes(const PmxDocument& doc, int selectedBoneIndex) {
    std::vector<LineVertex> lines;
    if (doc.bones.empty()) {
        return lines;
    }

    // Reserve space estimate: ~26 vertices per bone
    lines.reserve(doc.bones.size() * 26);

    for (size_t i = 0; i < doc.bones.size(); ++i) {
        const auto& b = doc.bones[i];
        if (!isVec3Finite(b.position)) {
            continue;
        }

        const glm::vec3 pHead = PmxCamera::toRightHanded(b.position);
        glm::vec3 pTail = pHead;
        bool hasValidTail = false;

        if ((b.boneFlags & 0x0001) != 0) { // Tail is bone
            if (b.tailBoneIndex >= 0 && static_cast<size_t>(b.tailBoneIndex) < doc.bones.size() &&
                b.tailBoneIndex != static_cast<int>(i)) {
                const auto& child = doc.bones[static_cast<size_t>(b.tailBoneIndex)];
                if (isVec3Finite(child.position)) {
                    pTail = PmxCamera::toRightHanded(child.position);
                    hasValidTail = true;
                }
            }
        } else if (isVec3Finite(b.tailOffset) && glm::length(b.tailOffset) > 0.001f) {
            pTail = PmxCamera::toRightHanded(b.position + b.tailOffset);
            hasValidTail = true;
        }

        const glm::vec4 boneColor = (static_cast<int>(i) == selectedBoneIndex)
            ? glm::vec4(1.0f, 0.8f, 0.1f, 1.0f)
            : glm::vec4(0.2f, 0.8f, 1.0f, 0.9f);

        // Connecting line from parent head to bone head
        if (b.parentBoneIndex >= 0 && static_cast<size_t>(b.parentBoneIndex) < doc.bones.size() &&
            b.parentBoneIndex != static_cast<int>(i)) {
            const auto& parentBone = doc.bones[static_cast<size_t>(b.parentBoneIndex)];
            if (isVec3Finite(parentBone.position)) {
                glm::vec3 pParent = PmxCamera::toRightHanded(parentBone.position);
                lines.push_back({ pParent, boneColor });
                lines.push_back({ pHead, boneColor });
            }
        }

        const glm::vec3 v = pTail - pHead;
        const float L = glm::length(v);

        if (hasValidTail && L > 0.001f) {
            const glm::vec3 dir = v / L;
            const glm::vec3 pBase = pHead + v * 0.2f; // 20% along shaft
            const float rBase = std::min(0.25f, L * 0.15f);

            glm::vec3 up = (std::abs(dir.y) < 0.99f) ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            glm::vec3 u = glm::normalize(glm::cross(dir, up)) * rBase;
            glm::vec3 w = glm::normalize(glm::cross(dir, u)) * rBase;

            glm::vec3 d0 = pBase + u;
            glm::vec3 d1 = pBase + w;
            glm::vec3 d2 = pBase - u;
            glm::vec3 d3 = pBase - w;

            // 4 lines from pHead to base diamond points
            lines.push_back({ pHead, boneColor }); lines.push_back({ d0, boneColor });
            lines.push_back({ pHead, boneColor }); lines.push_back({ d1, boneColor });
            lines.push_back({ pHead, boneColor }); lines.push_back({ d2, boneColor });
            lines.push_back({ pHead, boneColor }); lines.push_back({ d3, boneColor });

            // 4 lines connecting diamond ring
            lines.push_back({ d0, boneColor }); lines.push_back({ d1, boneColor });
            lines.push_back({ d1, boneColor }); lines.push_back({ d2, boneColor });
            lines.push_back({ d2, boneColor }); lines.push_back({ d3, boneColor });
            lines.push_back({ d3, boneColor }); lines.push_back({ d0, boneColor });

            // 4 lines from base diamond points to pTail
            lines.push_back({ d0, boneColor }); lines.push_back({ pTail, boneColor });
            lines.push_back({ d1, boneColor }); lines.push_back({ pTail, boneColor });
            lines.push_back({ d2, boneColor }); lines.push_back({ pTail, boneColor });
            lines.push_back({ d3, boneColor }); lines.push_back({ pTail, boneColor });
        } else {
            // Localized octahedron of radius 0.2f centered at pHead
            const float r = 0.2f;
            glm::vec3 top = pHead + glm::vec3(0.0f, r, 0.0f);
            glm::vec3 bot = pHead - glm::vec3(0.0f, r, 0.0f);
            glm::vec3 r0 = pHead + glm::vec3(r, 0.0f, 0.0f);
            glm::vec3 r1 = pHead + glm::vec3(0.0f, 0.0f, r);
            glm::vec3 r2 = pHead - glm::vec3(r, 0.0f, 0.0f);
            glm::vec3 r3 = pHead - glm::vec3(0.0f, 0.0f, r);

            // 4 lines from top to ring
            lines.push_back({ top, boneColor }); lines.push_back({ r0, boneColor });
            lines.push_back({ top, boneColor }); lines.push_back({ r1, boneColor });
            lines.push_back({ top, boneColor }); lines.push_back({ r2, boneColor });
            lines.push_back({ top, boneColor }); lines.push_back({ r3, boneColor });

            // 4 lines around the ring
            lines.push_back({ r0, boneColor }); lines.push_back({ r1, boneColor });
            lines.push_back({ r1, boneColor }); lines.push_back({ r2, boneColor });
            lines.push_back({ r2, boneColor }); lines.push_back({ r3, boneColor });
            lines.push_back({ r3, boneColor }); lines.push_back({ r0, boneColor });

            // 4 lines from bottom to ring
            lines.push_back({ bot, boneColor }); lines.push_back({ r0, boneColor });
            lines.push_back({ bot, boneColor }); lines.push_back({ r1, boneColor });
            lines.push_back({ bot, boneColor }); lines.push_back({ r2, boneColor });
            lines.push_back({ bot, boneColor }); lines.push_back({ r3, boneColor });
        }
    }

    return lines;
}

std::vector<LineVertex> PmxWireframeRenderer::buildRigidBodyWireframes(const PmxDocument& doc, int selectedRigidBodyIndex) {
    std::vector<LineVertex> lines;
    if (doc.rigidBodies.empty()) {
        return lines;
    }

    lines.reserve(doc.rigidBodies.size() * 144);

    for (size_t i = 0; i < doc.rigidBodies.size(); ++i) {
        const auto& rb = doc.rigidBodies[i];
        if (!isVec3Finite(rb.position) || !isVec3Finite(rb.rotation) || !isVec3Finite(rb.size)) {
            continue;
        }

        const glm::vec3 p = PmxCamera::toRightHanded(rb.position);
        const glm::mat3 R = makeEulerRotationMatrix(rb.rotation);

        glm::vec4 color;
        if (static_cast<int>(i) == selectedRigidBodyIndex) {
            color = glm::vec4(1.0f, 0.95f, 0.3f, 1.0f);
        } else {
            switch (rb.physicsMode) {
            case 0:
                color = glm::vec4(0.2f, 0.55f, 1.0f, 0.85f); // Sky Blue (Static / FollowBone)
                break;
            case 1:
                color = glm::vec4(1.0f, 0.5f, 0.05f, 0.85f); // Bright Orange (Dynamic)
                break;
            case 2:
                color = glm::vec4(1.0f, 0.85f, 0.1f, 0.85f); // Golden Yellow (Dynamic + Bone alignment)
                break;
            default:
                color = glm::vec4(0.2f, 0.55f, 1.0f, 0.85f);
                break;
            }
        }

        if (rb.shape == 0) { // Sphere
            const float r = std::max(0.0001f, std::abs(rb.size.x));
            constexpr int SEGMENTS = 24;

            // XY circle
            for (int s = 0; s < SEGMENTS; ++s) {
                float th0 = TWO_PI * s / SEGMENTS;
                float th1 = TWO_PI * (s + 1) / SEGMENTS;
                glm::vec3 pt0 = p + R * glm::vec3(r * std::cos(th0), r * std::sin(th0), 0.0f);
                glm::vec3 pt1 = p + R * glm::vec3(r * std::cos(th1), r * std::sin(th1), 0.0f);
                lines.push_back({ pt0, color });
                lines.push_back({ pt1, color });
            }

            // XZ circle
            for (int s = 0; s < SEGMENTS; ++s) {
                float th0 = TWO_PI * s / SEGMENTS;
                float th1 = TWO_PI * (s + 1) / SEGMENTS;
                glm::vec3 pt0 = p + R * glm::vec3(r * std::cos(th0), 0.0f, r * std::sin(th0));
                glm::vec3 pt1 = p + R * glm::vec3(r * std::cos(th1), 0.0f, r * std::sin(th1));
                lines.push_back({ pt0, color });
                lines.push_back({ pt1, color });
            }

            // YZ circle
            for (int s = 0; s < SEGMENTS; ++s) {
                float th0 = TWO_PI * s / SEGMENTS;
                float th1 = TWO_PI * (s + 1) / SEGMENTS;
                glm::vec3 pt0 = p + R * glm::vec3(0.0f, r * std::cos(th0), r * std::sin(th0));
                glm::vec3 pt1 = p + R * glm::vec3(0.0f, r * std::cos(th1), r * std::sin(th1));
                lines.push_back({ pt0, color });
                lines.push_back({ pt1, color });
            }
        } else if (rb.shape == 1) { // Box
            const float ex = std::max(0.0001f, std::abs(rb.size.x));
            const float ey = std::max(0.0001f, std::abs(rb.size.y));
            const float ez = std::max(0.0001f, std::abs(rb.size.z));

            glm::vec3 c[8] = {
                p + R * glm::vec3(-ex, -ey, -ez),
                p + R * glm::vec3( ex, -ey, -ez),
                p + R * glm::vec3( ex,  ey, -ez),
                p + R * glm::vec3(-ex,  ey, -ez),
                p + R * glm::vec3(-ex, -ey,  ez),
                p + R * glm::vec3( ex, -ey,  ez),
                p + R * glm::vec3( ex,  ey,  ez),
                p + R * glm::vec3(-ex,  ey,  ez),
            };

            const int edges[12][2] = {
                {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Bottom face
                {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Top face
                {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Vertical pillars
            };

            for (int e = 0; e < 12; ++e) {
                lines.push_back({ c[edges[e][0]], color });
                lines.push_back({ c[edges[e][1]], color });
            }
        } else if (rb.shape == 2) { // Capsule
            const float r = std::max(0.0001f, std::abs(rb.size.x));
            const float h = std::max(0.0001f, std::abs(rb.size.y));
            const float hh = h * 0.5f;
            constexpr int SEGMENTS = 24;

            // 2 circular rings at y = +hh and y = -hh
            for (int s = 0; s < SEGMENTS; ++s) {
                float th0 = TWO_PI * s / SEGMENTS;
                float th1 = TWO_PI * (s + 1) / SEGMENTS;

                glm::vec3 ptTop0 = p + R * glm::vec3(r * std::cos(th0), hh, r * std::sin(th0));
                glm::vec3 ptTop1 = p + R * glm::vec3(r * std::cos(th1), hh, r * std::sin(th1));
                lines.push_back({ ptTop0, color });
                lines.push_back({ ptTop1, color });

                glm::vec3 ptBot0 = p + R * glm::vec3(r * std::cos(th0), -hh, r * std::sin(th0));
                glm::vec3 ptBot1 = p + R * glm::vec3(r * std::cos(th1), -hh, r * std::sin(th1));
                lines.push_back({ ptBot0, color });
                lines.push_back({ ptBot1, color });
            }

            // 4 vertical lines along cylinder sides
            const glm::vec3 vSides[4] = {
                glm::vec3( r, 0.0f, 0.0f),
                glm::vec3(-r, 0.0f, 0.0f),
                glm::vec3(0.0f, 0.0f,  r),
                glm::vec3(0.0f, 0.0f, -r),
            };

            for (int v = 0; v < 4; ++v) {
                glm::vec3 p0 = p + R * (vSides[v] + glm::vec3(0.0f, -hh, 0.0f));
                glm::vec3 p1 = p + R * (vSides[v] + glm::vec3(0.0f,  hh, 0.0f));
                lines.push_back({ p0, color });
                lines.push_back({ p1, color });
            }

            // Hemispherical arcs at top dome (+hh) and bottom dome (-hh)
            constexpr int ARC_SEGS = 12;
            for (int s = 0; s < ARC_SEGS; ++s) {
                float a0 = PI * s / ARC_SEGS;
                float a1 = PI * (s + 1) / ARC_SEGS;

                // Top dome XY arc
                glm::vec3 txy0 = p + R * glm::vec3(r * std::cos(a0), hh + r * std::sin(a0), 0.0f);
                glm::vec3 txy1 = p + R * glm::vec3(r * std::cos(a1), hh + r * std::sin(a1), 0.0f);
                lines.push_back({ txy0, color });
                lines.push_back({ txy1, color });

                // Top dome ZY arc
                glm::vec3 tzy0 = p + R * glm::vec3(0.0f, hh + r * std::sin(a0), r * std::cos(a0));
                glm::vec3 tzy1 = p + R * glm::vec3(0.0f, hh + r * std::sin(a1), r * std::cos(a1));
                lines.push_back({ tzy0, color });
                lines.push_back({ tzy1, color });

                // Bottom dome XY arc
                glm::vec3 bxy0 = p + R * glm::vec3(r * std::cos(a0), -hh - r * std::sin(a0), 0.0f);
                glm::vec3 bxy1 = p + R * glm::vec3(r * std::cos(a1), -hh - r * std::sin(a1), 0.0f);
                lines.push_back({ bxy0, color });
                lines.push_back({ bxy1, color });

                // Bottom dome ZY arc
                glm::vec3 bzy0 = p + R * glm::vec3(0.0f, -hh - r * std::sin(a0), r * std::cos(a0));
                glm::vec3 bzy1 = p + R * glm::vec3(0.0f, -hh - r * std::sin(a1), r * std::cos(a1));
                lines.push_back({ bzy0, color });
                lines.push_back({ bzy1, color });
            }
        }
    }

    return lines;
}

std::vector<LineVertex> PmxWireframeRenderer::buildJointWireframes(const PmxDocument& doc, int selectedJointIndex) {
    std::vector<LineVertex> lines;
    if (doc.joints.empty()) {
        return lines;
    }

    lines.reserve(doc.joints.size() * 64);

    for (size_t i = 0; i < doc.joints.size(); ++i) {
        const auto& j = doc.joints[i];
        if (!isVec3Finite(j.position) || !isVec3Finite(j.rotation)) {
            continue;
        }

        const glm::vec3 p = PmxCamera::toRightHanded(j.position);
        const glm::mat3 R = makeEulerRotationMatrix(j.rotation);

        const bool isSelected = (static_cast<int>(i) == selectedJointIndex);

        // 3-axis coordinate cross (length 0.4f)
        const glm::vec4 colorX = isSelected ? glm::vec4(1.0f, 0.4f, 0.4f, 1.0f) : glm::vec4(1.0f, 0.15f, 0.15f, 1.0f);
        const glm::vec4 colorY = isSelected ? glm::vec4(0.4f, 1.0f, 0.4f, 1.0f) : glm::vec4(0.15f, 0.9f, 0.15f, 1.0f);
        const glm::vec4 colorZ = isSelected ? glm::vec4(0.4f, 0.6f, 1.0f, 1.0f) : glm::vec4(0.2f, 0.4f, 1.0f, 1.0f);

        lines.push_back({ p, colorX }); lines.push_back({ p + R * glm::vec3(0.4f, 0.0f, 0.0f), colorX });
        lines.push_back({ p, colorY }); lines.push_back({ p + R * glm::vec3(0.0f, 0.4f, 0.0f), colorY });
        lines.push_back({ p, colorZ }); lines.push_back({ p + R * glm::vec3(0.0f, 0.0f, 0.4f), colorZ });

        // Rotational limit arcs
        const float rArc = 0.5f;
        constexpr int ARC_SEGS = 12;

        // X limit arc (YZ plane)
        float minX = -j.angularUpperLimit.x;
        float maxX = -j.angularLowerLimit.x;
        if (std::isfinite(minX) && std::isfinite(maxX)) {
            if (minX > maxX) std::swap(minX, maxX);
            if (std::abs(maxX - minX) > 0.001f) {
                const glm::vec4 arcColor(1.0f, 0.4f, 0.4f, 0.8f);
                for (int s = 0; s < ARC_SEGS; ++s) {
                    float a0 = minX + (maxX - minX) * s / ARC_SEGS;
                    float a1 = minX + (maxX - minX) * (s + 1) / ARC_SEGS;
                    glm::vec3 pt0 = p + R * glm::vec3(0.0f, rArc * std::cos(a0), rArc * std::sin(a0));
                    glm::vec3 pt1 = p + R * glm::vec3(0.0f, rArc * std::cos(a1), rArc * std::sin(a1));
                    lines.push_back({ pt0, arcColor });
                    lines.push_back({ pt1, arcColor });
                }
            }
        }

        // Y limit arc (ZX plane)
        float minY = -j.angularUpperLimit.y;
        float maxY = -j.angularLowerLimit.y;
        if (std::isfinite(minY) && std::isfinite(maxY)) {
            if (minY > maxY) std::swap(minY, maxY);
            if (std::abs(maxY - minY) > 0.001f) {
                const glm::vec4 arcColor(0.4f, 1.0f, 0.4f, 0.8f);
                for (int s = 0; s < ARC_SEGS; ++s) {
                    float a0 = minY + (maxY - minY) * s / ARC_SEGS;
                    float a1 = minY + (maxY - minY) * (s + 1) / ARC_SEGS;
                    glm::vec3 pt0 = p + R * glm::vec3(rArc * std::sin(a0), 0.0f, rArc * std::cos(a0));
                    glm::vec3 pt1 = p + R * glm::vec3(rArc * std::sin(a1), 0.0f, rArc * std::cos(a1));
                    lines.push_back({ pt0, arcColor });
                    lines.push_back({ pt1, arcColor });
                }
            }
        }

        // Z limit arc (XY plane)
        float minZ = j.angularLowerLimit.z;
        float maxZ = j.angularUpperLimit.z;
        if (std::isfinite(minZ) && std::isfinite(maxZ)) {
            if (minZ > maxZ) std::swap(minZ, maxZ);
            if (std::abs(maxZ - minZ) > 0.001f) {
                const glm::vec4 arcColor(0.4f, 0.6f, 1.0f, 0.8f);
                for (int s = 0; s < ARC_SEGS; ++s) {
                    float a0 = minZ + (maxZ - minZ) * s / ARC_SEGS;
                    float a1 = minZ + (maxZ - minZ) * (s + 1) / ARC_SEGS;
                    glm::vec3 pt0 = p + R * glm::vec3(rArc * std::cos(a0), rArc * std::sin(a0), 0.0f);
                    glm::vec3 pt1 = p + R * glm::vec3(rArc * std::cos(a1), rArc * std::sin(a1), 0.0f);
                    lines.push_back({ pt0, arcColor });
                    lines.push_back({ pt1, arcColor });
                }
            }
        }

        // Anchor lines to rigid bodies A and B
        const glm::vec4 anchorColor = isSelected
            ? glm::vec4(1.0f, 0.4f, 1.0f, 1.0f)
            : glm::vec4(0.85f, 0.2f, 0.85f, 0.8f);

        if (j.rigidBodyIndexA >= 0 && static_cast<size_t>(j.rigidBodyIndexA) < doc.rigidBodies.size()) {
            const auto& rbA = doc.rigidBodies[static_cast<size_t>(j.rigidBodyIndexA)];
            if (isVec3Finite(rbA.position)) {
                glm::vec3 pA = PmxCamera::toRightHanded(rbA.position);
                lines.push_back({ p, anchorColor });
                lines.push_back({ pA, anchorColor });
            }
        }

        if (j.rigidBodyIndexB >= 0 && static_cast<size_t>(j.rigidBodyIndexB) < doc.rigidBodies.size()) {
            const auto& rbB = doc.rigidBodies[static_cast<size_t>(j.rigidBodyIndexB)];
            if (isVec3Finite(rbB.position)) {
                glm::vec3 pB = PmxCamera::toRightHanded(rbB.position);
                lines.push_back({ p, anchorColor });
                lines.push_back({ pB, anchorColor });
            }
        }
    }

    return lines;
}

} // namespace pmx
