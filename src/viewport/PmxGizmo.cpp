#include "viewport/PmxGizmo.h"

#include <cmath>
#include <algorithm>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace pmx {

PmxGizmo::PmxGizmo() = default;

float PmxGizmo::computeDisplayScale(const glm::vec3& eyePos) const {
    return std::max(0.1f, glm::length(eyePos - m_position) * 0.15f);
}

glm::mat3 PmxGizmo::orientationMatrix() const {
    if (m_coordSpace == CoordSpace::Local) {
        return glm::mat3_cast(m_rotation);
    }
    return glm::mat3(1.0f);
}

glm::vec3 PmxGizmo::axisVector(GizmoAxis axis) const {
    glm::mat3 R = orientationMatrix();
    switch (axis) {
        case GizmoAxis::AxisX: return R * glm::vec3(1.0f, 0.0f, 0.0f);
        case GizmoAxis::AxisY: return R * glm::vec3(0.0f, 1.0f, 0.0f);
        case GizmoAxis::AxisZ: return R * glm::vec3(0.0f, 0.0f, 1.0f);
        default: return glm::vec3(0.0f);
    }
}

int PmxGizmo::findSymmetricVertex(const PmxDocument& doc, int vertexIndex, float tolerance) {
    if (vertexIndex < 0 || static_cast<size_t>(vertexIndex) >= doc.vertices.size()) {
        return -1;
    }
    const auto& vA = doc.vertices[vertexIndex];
    float targetX = -vA.position.x;
    float targetY = vA.position.y;
    float targetZ = vA.position.z;

    int bestMatch = -1;
    float bestDistSq = tolerance * tolerance;

    for (size_t i = 0; i < doc.vertices.size(); ++i) {
        if (static_cast<int>(i) == vertexIndex) continue;
        const auto& vB = doc.vertices[i];
        float dx = vB.position.x - targetX;
        float dy = vB.position.y - targetY;
        float dz = vB.position.z - targetZ;
        float distSq = dx * dx + dy * dy + dz * dz;
        if (distSq < bestDistSq) {
            bestDistSq = distSq;
            bestMatch = static_cast<int>(i);
        }
    }

    if (bestMatch != -1) {
        return bestMatch;
    }

    // If no distinct vertex was found, but vertex lies on the symmetry plane (|x| < tolerance)
    if (std::abs(vA.position.x) < tolerance) {
        return vertexIndex;
    }

    return -1;
}

int PmxGizmo::findSymmetricBone(const PmxDocument& doc, int boneIndex) {
    if (boneIndex < 0 || static_cast<size_t>(boneIndex) >= doc.bones.size()) {
        return -1;
    }
    const auto& bone = doc.bones[boneIndex];

    auto swapName = [](const QString& name, const QString& from, const QString& to) -> QString {
        if (name.contains(from)) {
            QString copy = name;
            copy.replace(from, to);
            return copy;
        }
        return QString();
    };

    // 1. Japanese naming: 左 <-> 右
    QString swappedJp;
    if (bone.nameJp.contains(QStringLiteral("左"))) {
        swappedJp = swapName(bone.nameJp, QStringLiteral("左"), QStringLiteral("右"));
    } else if (bone.nameJp.contains(QStringLiteral("右"))) {
        swappedJp = swapName(bone.nameJp, QStringLiteral("右"), QStringLiteral("左"));
    }

    if (!swappedJp.isEmpty()) {
        for (size_t i = 0; i < doc.bones.size(); ++i) {
            if (static_cast<int>(i) != boneIndex && doc.bones[i].nameJp == swappedJp) {
                return static_cast<int>(i);
            }
        }
    }

    // 2. English naming pairs
    static const QList<QPair<QString, QString>> pairs = {
        { QStringLiteral("_L"), QStringLiteral("_R") },
        { QStringLiteral(".L"), QStringLiteral(".R") },
        { QStringLiteral("_l"), QStringLiteral("_r") },
        { QStringLiteral(".l"), QStringLiteral(".r") },
        { QStringLiteral(" Left"), QStringLiteral(" Right") },
        { QStringLiteral(" left"), QStringLiteral(" right") },
        { QStringLiteral("Left"), QStringLiteral("Right") },
        { QStringLiteral("left"), QStringLiteral("right") }
    };

    for (const auto& p : pairs) {
        QString swappedEn;
        if (bone.nameEn.contains(p.first)) {
            swappedEn = swapName(bone.nameEn, p.first, p.second);
        } else if (bone.nameEn.contains(p.second)) {
            swappedEn = swapName(bone.nameEn, p.second, p.first);
        }

        if (!swappedEn.isEmpty()) {
            for (size_t i = 0; i < doc.bones.size(); ++i) {
                if (static_cast<int>(i) != boneIndex && doc.bones[i].nameEn == swappedEn) {
                    return static_cast<int>(i);
                }
            }
        }
    }

    return -1;
}

glm::vec3 PmxGizmo::mirrorDelta(const glm::vec3& delta) {
    return glm::vec3(-delta.x, delta.y, delta.z);
}

glm::quat PmxGizmo::mirrorRotation(const glm::quat& q) {
    // Reflection across X=0 plane in quaternion coordinates
    return glm::quat(q.w, q.x, -q.y, -q.z);
}

static float raySegmentDistance(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& segA,
    const glm::vec3& segB,
    float& outRayT,
    float& outSegS)
{
    glm::vec3 u = segB - segA;
    glm::vec3 v = rayDir;
    glm::vec3 w = segA - rayOrigin;

    float a = glm::dot(u, u);
    float b = glm::dot(u, v);
    float c = glm::dot(v, v);
    float d = glm::dot(u, w);
    float e = glm::dot(v, w);
    float D = a * c - b * b;

    float sc, tc;

    if (D < 1e-6f) {
        sc = 0.0f;
        tc = (b > c ? d / b : e / c);
    } else {
        sc = (b * e - c * d) / D;
        tc = (a * e - b * d) / D;
    }

    sc = std::clamp(sc, 0.0f, 1.0f);
    tc = std::max(0.0f, tc);

    outSegS = sc;
    outRayT = tc;

    glm::vec3 dP = w + (sc * u) - (tc * v);
    return glm::length(dP);
}

static bool intersectRayPlane(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& planePos,
    const glm::vec3& planeNormal,
    float& outT,
    glm::vec3& outPoint)
{
    float denom = glm::dot(rayDir, planeNormal);
    if (std::abs(denom) < 1e-6f) {
        return false;
    }
    float t = glm::dot(planePos - rayOrigin, planeNormal) / denom;
    if (t < 0.0f) {
        return false;
    }
    outT = t;
    outPoint = rayOrigin + t * rayDir;
    return true;
}

GizmoAxis PmxGizmo::hitTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float gizmoScale) const {
    if (m_mode == GizmoMode::None || !std::isfinite(gizmoScale) || gizmoScale <= 0.0001f) {
        return GizmoAxis::None;
    }

    glm::vec3 uX = axisVector(GizmoAxis::AxisX);
    glm::vec3 uY = axisVector(GizmoAxis::AxisY);
    glm::vec3 uZ = axisVector(GizmoAxis::AxisZ);
    float L = gizmoScale;

    GizmoAxis bestAxis = GizmoAxis::None;
    float bestT = std::numeric_limits<float>::max();

    if (m_mode == GizmoMode::Translate) {
        // 1. Planar handles have priority if hit
        float planeMin = 0.1f * L;
        float planeMax = 0.45f * L;

        // Plane XY (Normal = uZ)
        float tXY;
        glm::vec3 pXY;
        if (intersectRayPlane(rayOrigin, rayDir, m_position, uZ, tXY, pXY)) {
            glm::vec3 delta = pXY - m_position;
            float px = glm::dot(delta, uX);
            float py = glm::dot(delta, uY);
            if (px >= planeMin && px <= planeMax && py >= planeMin && py <= planeMax) {
                if (tXY < bestT) {
                    bestT = tXY;
                    bestAxis = GizmoAxis::PlaneXY;
                }
            }
        }

        // Plane YZ (Normal = uX)
        float tYZ;
        glm::vec3 pYZ;
        if (intersectRayPlane(rayOrigin, rayDir, m_position, uX, tYZ, pYZ)) {
            glm::vec3 delta = pYZ - m_position;
            float py = glm::dot(delta, uY);
            float pz = glm::dot(delta, uZ);
            if (py >= planeMin && py <= planeMax && pz >= planeMin && pz <= planeMax) {
                if (tYZ < bestT) {
                    bestT = tYZ;
                    bestAxis = GizmoAxis::PlaneYZ;
                }
            }
        }

        // Plane XZ (Normal = uY)
        float tXZ;
        glm::vec3 pXZ;
        if (intersectRayPlane(rayOrigin, rayDir, m_position, uY, tXZ, pXZ)) {
            glm::vec3 delta = pXZ - m_position;
            float px = glm::dot(delta, uX);
            float pz = glm::dot(delta, uZ);
            if (px >= planeMin && px <= planeMax && pz >= planeMin && pz <= planeMax) {
                if (tXZ < bestT) {
                    bestT = tXZ;
                    bestAxis = GizmoAxis::PlaneXZ;
                }
            }
        }

        if (bestAxis != GizmoAxis::None) {
            return bestAxis;
        }

        // 2. Linear axis segments
        float hitThreshold = 0.15f * L;
        float rayT, segS;

        float distX = raySegmentDistance(rayOrigin, rayDir, m_position, m_position + uX * L, rayT, segS);
        if (distX < hitThreshold && rayT < bestT) {
            bestT = rayT;
            bestAxis = GizmoAxis::AxisX;
        }

        float distY = raySegmentDistance(rayOrigin, rayDir, m_position, m_position + uY * L, rayT, segS);
        if (distY < hitThreshold && rayT < bestT) {
            bestT = rayT;
            bestAxis = GizmoAxis::AxisY;
        }

        float distZ = raySegmentDistance(rayOrigin, rayDir, m_position, m_position + uZ * L, rayT, segS);
        if (distZ < hitThreshold && rayT < bestT) {
            bestT = rayT;
            bestAxis = GizmoAxis::AxisZ;
        }

        return bestAxis;
    }

    if (m_mode == GizmoMode::Rotate) {
        float ringRadius = 0.85f * L;
        float ringThick = 0.15f * L;

        // X ring (Normal = uX, in YZ plane)
        float tX;
        glm::vec3 pX;
        if (intersectRayPlane(rayOrigin, rayDir, m_position, uX, tX, pX)) {
            float r = glm::length(pX - m_position);
            if (std::abs(r - ringRadius) < ringThick && tX < bestT) {
                bestT = tX;
                bestAxis = GizmoAxis::AxisX;
            }
        }

        // Y ring (Normal = uY, in XZ plane)
        float tY;
        glm::vec3 pY;
        if (intersectRayPlane(rayOrigin, rayDir, m_position, uY, tY, pY)) {
            float r = glm::length(pY - m_position);
            if (std::abs(r - ringRadius) < ringThick && tY < bestT) {
                bestT = tY;
                bestAxis = GizmoAxis::AxisY;
            }
        }

        // Z ring (Normal = uZ, in XY plane)
        float tZ;
        glm::vec3 pZ;
        if (intersectRayPlane(rayOrigin, rayDir, m_position, uZ, tZ, pZ)) {
            float r = glm::length(pZ - m_position);
            if (std::abs(r - ringRadius) < ringThick && tZ < bestT) {
                bestT = tZ;
                bestAxis = GizmoAxis::AxisZ;
            }
        }

        return bestAxis;
    }

    if (m_mode == GizmoMode::Scale) {
        float hitThreshold = 0.18f * L;
        float rayT, segS;

        float distX = raySegmentDistance(rayOrigin, rayDir, m_position, m_position + uX * L, rayT, segS);
        if (distX < hitThreshold && rayT < bestT) {
            bestT = rayT;
            bestAxis = GizmoAxis::AxisX;
        }

        float distY = raySegmentDistance(rayOrigin, rayDir, m_position, m_position + uY * L, rayT, segS);
        if (distY < hitThreshold && rayT < bestT) {
            bestT = rayT;
            bestAxis = GizmoAxis::AxisY;
        }

        float distZ = raySegmentDistance(rayOrigin, rayDir, m_position, m_position + uZ * L, rayT, segS);
        if (distZ < hitThreshold && rayT < bestT) {
            bestT = rayT;
            bestAxis = GizmoAxis::AxisZ;
        }

        return bestAxis;
    }

    return GizmoAxis::None;
}

static float projectRayOntoLine(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& lineOrigin,
    const glm::vec3& lineDir)
{
    glm::vec3 w = rayOrigin - lineOrigin;
    float b = glm::dot(lineDir, rayDir);
    float d = glm::dot(lineDir, w);
    float e = glm::dot(rayDir, w);
    float denom = 1.0f - b * b;

    if (denom > 1e-4f) {
        return (d - b * e) / denom;
    }
    return d;
}

glm::vec3 PmxGizmo::computeTranslationDelta(
    GizmoAxis axis,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& lastRayOrigin,
    const glm::vec3& lastRayDir,
    float gizmoScale,
    bool snap) const
{
    Q_UNUSED(gizmoScale)
    glm::vec3 uX = axisVector(GizmoAxis::AxisX);
    glm::vec3 uY = axisVector(GizmoAxis::AxisY);
    glm::vec3 uZ = axisVector(GizmoAxis::AxisZ);

    if (axis == GizmoAxis::AxisX || axis == GizmoAxis::AxisY || axis == GizmoAxis::AxisZ) {
        glm::vec3 u = axisVector(axis);
        float s1 = projectRayOntoLine(rayOrigin, rayDir, m_position, u);
        float s0 = projectRayOntoLine(lastRayOrigin, lastRayDir, m_position, u);
        float delta = s1 - s0;

        if (snap && std::isfinite(m_translationSnap) && m_translationSnap > 1e-4f) {
            delta = std::round(delta / m_translationSnap) * m_translationSnap;
        }
        return u * delta;
    }

    if (axis == GizmoAxis::PlaneXY || axis == GizmoAxis::PlaneYZ || axis == GizmoAxis::PlaneXZ) {
        glm::vec3 n = (axis == GizmoAxis::PlaneXY) ? uZ : ((axis == GizmoAxis::PlaneYZ) ? uX : uY);
        float t1, t0;
        glm::vec3 p1, p0;
        if (intersectRayPlane(rayOrigin, rayDir, m_position, n, t1, p1) &&
            intersectRayPlane(lastRayOrigin, lastRayDir, m_position, n, t0, p0)) {
            glm::vec3 delta = p1 - p0;
            if (snap && std::isfinite(m_translationSnap) && m_translationSnap > 1e-4f) {
                glm::vec3 uA = (axis == GizmoAxis::PlaneYZ) ? uY : uX;
                glm::vec3 uB = (axis == GizmoAxis::PlaneXY) ? uY : uZ;
                float dA = glm::dot(delta, uA);
                float dB = glm::dot(delta, uB);
                dA = std::round(dA / m_translationSnap) * m_translationSnap;
                dB = std::round(dB / m_translationSnap) * m_translationSnap;
                return uA * dA + uB * dB;
            }
            return delta;
        }
    }

    return glm::vec3(0.0f);
}

glm::quat PmxGizmo::computeRotationDelta(
    GizmoAxis axis,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& lastRayOrigin,
    const glm::vec3& lastRayDir,
    float gizmoScale,
    bool snap) const
{
    Q_UNUSED(gizmoScale)
    glm::vec3 n = axisVector(axis);
    if (glm::length(n) < 1e-4f) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    float t1, t0;
    glm::vec3 p1, p0;
    if (intersectRayPlane(rayOrigin, rayDir, m_position, n, t1, p1) &&
        intersectRayPlane(lastRayOrigin, lastRayDir, m_position, n, t0, p0)) {
        glm::vec3 v0 = p0 - m_position;
        glm::vec3 v1 = p1 - m_position;
        if (glm::length(v0) < 1e-4f || glm::length(v1) < 1e-4f) {
            return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
        v0 = glm::normalize(v0);
        v1 = glm::normalize(v1);

        float angle = std::atan2(glm::dot(n, glm::cross(v0, v1)), glm::dot(v0, v1));
        if (snap && std::isfinite(m_rotationSnap) && m_rotationSnap > 1e-4f) {
            angle = std::round(angle / m_rotationSnap) * m_rotationSnap;
        }
        return glm::angleAxis(angle, n);
    }

    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

glm::vec3 PmxGizmo::computeScaleDelta(
    GizmoAxis axis,
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDir,
    const glm::vec3& lastRayOrigin,
    const glm::vec3& lastRayDir,
    float gizmoScale,
    bool snap) const
{
    glm::vec3 u = axisVector(axis);
    if (glm::length(u) < 1e-4f) {
        return glm::vec3(1.0f);
    }

    float s1 = projectRayOntoLine(rayOrigin, rayDir, m_position, u);
    float s0 = projectRayOntoLine(lastRayOrigin, lastRayDir, m_position, u);
    float delta = s1 - s0;
    float safeScale = std::max(0.1f, gizmoScale);
    float factor = 1.0f + delta / safeScale;

    if (snap && std::isfinite(m_scaleSnap) && m_scaleSnap > 1e-4f) {
        float deltaFactor = factor - 1.0f;
        deltaFactor = std::round(deltaFactor / m_scaleSnap) * m_scaleSnap;
        factor = 1.0f + deltaFactor;
    }

    if (axis == GizmoAxis::AxisX) return glm::vec3(factor, 1.0f, 1.0f);
    if (axis == GizmoAxis::AxisY) return glm::vec3(1.0f, factor, 1.0f);
    if (axis == GizmoAxis::AxisZ) return glm::vec3(1.0f, 1.0f, factor);

    return glm::vec3(factor);
}

std::vector<LineVertex> PmxGizmo::buildGizmoGeometry(float scale) const {
    std::vector<LineVertex> lines;
    if (m_mode == GizmoMode::None || scale <= 0.0001f) {
        return lines;
    }

    glm::vec3 uX = axisVector(GizmoAxis::AxisX);
    glm::vec3 uY = axisVector(GizmoAxis::AxisY);
    glm::vec3 uZ = axisVector(GizmoAxis::AxisZ);
    glm::vec3 P = m_position;
    float L = scale;

    const glm::vec4 yellow(1.0f, 1.0f, 0.1f, 1.0f);
    const glm::vec4 colX = (m_activeAxis == GizmoAxis::AxisX || m_hoveredAxis == GizmoAxis::AxisX) ? yellow : glm::vec4(1.0f, 0.15f, 0.15f, 1.0f);
    const glm::vec4 colY = (m_activeAxis == GizmoAxis::AxisY || m_hoveredAxis == GizmoAxis::AxisY) ? yellow : glm::vec4(0.15f, 1.0f, 0.15f, 1.0f);
    const glm::vec4 colZ = (m_activeAxis == GizmoAxis::AxisZ || m_hoveredAxis == GizmoAxis::AxisZ) ? yellow : glm::vec4(0.2f, 0.4f, 1.0f, 1.0f);

    auto addLine = [&](const glm::vec3& p0, const glm::vec3& p1, const glm::vec4& color) {
        lines.push_back({ p0, color });
        lines.push_back({ p1, color });
    };

    if (m_mode == GizmoMode::Translate) {
        // X Axis Line & Arrow
        addLine(P, P + uX * L, colX);
        addLine(P + uX * L, P + uX * (L * 0.85f) + uY * (L * 0.06f), colX);
        addLine(P + uX * L, P + uX * (L * 0.85f) - uY * (L * 0.06f), colX);
        addLine(P + uX * L, P + uX * (L * 0.85f) + uZ * (L * 0.06f), colX);
        addLine(P + uX * L, P + uX * (L * 0.85f) - uZ * (L * 0.06f), colX);

        // Y Axis Line & Arrow
        addLine(P, P + uY * L, colY);
        addLine(P + uY * L, P + uY * (L * 0.85f) + uX * (L * 0.06f), colY);
        addLine(P + uY * L, P + uY * (L * 0.85f) - uX * (L * 0.06f), colY);
        addLine(P + uY * L, P + uY * (L * 0.85f) + uZ * (L * 0.06f), colY);
        addLine(P + uY * L, P + uY * (L * 0.85f) - uZ * (L * 0.06f), colY);

        // Z Axis Line & Arrow
        addLine(P, P + uZ * L, colZ);
        addLine(P + uZ * L, P + uZ * (L * 0.85f) + uX * (L * 0.06f), colZ);
        addLine(P + uZ * L, P + uZ * (L * 0.85f) - uX * (L * 0.06f), colZ);
        addLine(P + uZ * L, P + uZ * (L * 0.85f) + uY * (L * 0.06f), colZ);
        addLine(P + uZ * L, P + uZ * (L * 0.85f) - uY * (L * 0.06f), colZ);

        // Planar quad handles
        float p0 = 0.15f * L;
        float p1 = 0.40f * L;

        glm::vec4 colXY = (m_activeAxis == GizmoAxis::PlaneXY || m_hoveredAxis == GizmoAxis::PlaneXY) ? yellow : glm::vec4(0.8f, 0.8f, 0.2f, 0.8f);
        addLine(P + uX * p0 + uY * p0, P + uX * p1 + uY * p0, colXY);
        addLine(P + uX * p1 + uY * p0, P + uX * p1 + uY * p1, colXY);
        addLine(P + uX * p1 + uY * p1, P + uX * p0 + uY * p1, colXY);
        addLine(P + uX * p0 + uY * p1, P + uX * p0 + uY * p0, colXY);

        glm::vec4 colYZ = (m_activeAxis == GizmoAxis::PlaneYZ || m_hoveredAxis == GizmoAxis::PlaneYZ) ? yellow : glm::vec4(0.2f, 0.8f, 0.8f, 0.8f);
        addLine(P + uY * p0 + uZ * p0, P + uY * p1 + uZ * p0, colYZ);
        addLine(P + uY * p1 + uZ * p0, P + uY * p1 + uZ * p1, colYZ);
        addLine(P + uY * p1 + uZ * p1, P + uY * p0 + uZ * p1, colYZ);
        addLine(P + uY * p0 + uZ * p1, P + uY * p0 + uZ * p0, colYZ);

        glm::vec4 colXZ = (m_activeAxis == GizmoAxis::PlaneXZ || m_hoveredAxis == GizmoAxis::PlaneXZ) ? yellow : glm::vec4(0.8f, 0.2f, 0.8f, 0.8f);
        addLine(P + uX * p0 + uZ * p0, P + uX * p1 + uZ * p0, colXZ);
        addLine(P + uX * p1 + uZ * p0, P + uX * p1 + uZ * p1, colXZ);
        addLine(P + uX * p1 + uZ * p1, P + uX * p0 + uZ * p1, colXZ);
        addLine(P + uX * p0 + uZ * p1, P + uX * p0 + uZ * p0, colXZ);
    } else if (m_mode == GizmoMode::Rotate) {
        const int segments = 48;
        float R = 0.85f * L;

        // X ring (YZ plane)
        for (int i = 0; i < segments; ++i) {
            float a0 = static_cast<float>(i) * 2.0f * M_PI / segments;
            float a1 = static_cast<float>(i + 1) * 2.0f * M_PI / segments;
            glm::vec3 pt0 = P + (uY * std::cos(a0) + uZ * std::sin(a0)) * R;
            glm::vec3 pt1 = P + (uY * std::cos(a1) + uZ * std::sin(a1)) * R;
            addLine(pt0, pt1, colX);
        }

        // Y ring (XZ plane)
        for (int i = 0; i < segments; ++i) {
            float a0 = static_cast<float>(i) * 2.0f * M_PI / segments;
            float a1 = static_cast<float>(i + 1) * 2.0f * M_PI / segments;
            glm::vec3 pt0 = P + (uX * std::cos(a0) + uZ * std::sin(a0)) * R;
            glm::vec3 pt1 = P + (uX * std::cos(a1) + uZ * std::sin(a1)) * R;
            addLine(pt0, pt1, colY);
        }

        // Z ring (XY plane)
        for (int i = 0; i < segments; ++i) {
            float a0 = static_cast<float>(i) * 2.0f * M_PI / segments;
            float a1 = static_cast<float>(i + 1) * 2.0f * M_PI / segments;
            glm::vec3 pt0 = P + (uX * std::cos(a0) + uY * std::sin(a0)) * R;
            glm::vec3 pt1 = P + (uX * std::cos(a1) + uY * std::sin(a1)) * R;
            addLine(pt0, pt1, colZ);
        }
    } else if (m_mode == GizmoMode::Scale) {
        addLine(P, P + uX * L, colX);
        addLine(P, P + uY * L, colY);
        addLine(P, P + uZ * L, colZ);

        float b = 0.05f * L;
        auto addCube = [&](const glm::vec3& c, const glm::vec4& color) {
            addLine(c + glm::vec3(-b,-b,-b), c + glm::vec3(b,-b,-b), color);
            addLine(c + glm::vec3(b,-b,-b), c + glm::vec3(b,b,-b), color);
            addLine(c + glm::vec3(b,b,-b), c + glm::vec3(-b,b,-b), color);
            addLine(c + glm::vec3(-b,b,-b), c + glm::vec3(-b,-b,-b), color);

            addLine(c + glm::vec3(-b,-b,b), c + glm::vec3(b,-b,b), color);
            addLine(c + glm::vec3(b,-b,b), c + glm::vec3(b,b,b), color);
            addLine(c + glm::vec3(b,b,b), c + glm::vec3(-b,b,b), color);
            addLine(c + glm::vec3(-b,b,b), c + glm::vec3(-b,-b,b), color);

            addLine(c + glm::vec3(-b,-b,-b), c + glm::vec3(-b,-b,b), color);
            addLine(c + glm::vec3(b,-b,-b), c + glm::vec3(b,-b,b), color);
            addLine(c + glm::vec3(b,b,-b), c + glm::vec3(b,b,b), color);
            addLine(c + glm::vec3(-b,b,-b), c + glm::vec3(-b,b,b), color);
        };

        addCube(P + uX * L, colX);
        addCube(P + uY * L, colY);
        addCube(P + uZ * L, colZ);
    }

    return lines;
}

} // namespace pmx
