#pragma once

#include "core/PmxDocument.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <QString>
#include <QList>
#include <QPair>

namespace pmx {

enum class GizmoMode {
    None = 0,
    Translate = 1,
    Rotate = 2,
    Scale = 3
};

enum class GizmoAxis {
    None = 0,
    AxisX = 1,
    AxisY = 2,
    AxisZ = 3,
    PlaneXY = 4,
    PlaneYZ = 5,
    PlaneXZ = 6
};

enum class CoordSpace {
    World = 0,
    Local = 1
};

struct LineVertex {
    glm::vec3 position{0.0f};
    glm::vec4 color{1.0f};
};

class PmxGizmo {
public:
    PmxGizmo();
    ~PmxGizmo() = default;

    // State getters & setters
    glm::vec3 position() const { return m_position; }
    void setPosition(const glm::vec3& pos) { m_position = pos; }

    glm::quat rotation() const { return m_rotation; }
    void setRotation(const glm::quat& rot) { m_rotation = rot; }

    GizmoMode mode() const { return m_mode; }
    void setMode(GizmoMode mode) { m_mode = mode; }

    CoordSpace coordSpace() const { return m_coordSpace; }
    void setCoordSpace(CoordSpace space) { m_coordSpace = space; }

    GizmoAxis activeAxis() const { return m_activeAxis; }
    void setActiveAxis(GizmoAxis axis) { m_activeAxis = axis; }

    GizmoAxis hoveredAxis() const { return m_hoveredAxis; }
    void setHoveredAxis(GizmoAxis axis) { m_hoveredAxis = axis; }

    bool snapEnabled() const { return m_snapEnabled; }
    void setSnapEnabled(bool enabled) { m_snapEnabled = enabled; }

    float translationSnap() const { return m_translationSnap; }
    void setTranslationSnap(float snap) { m_translationSnap = snap; }

    float rotationSnap() const { return m_rotationSnap; }
    void setRotationSnap(float snap) { m_rotationSnap = snap; }

    float scaleSnap() const { return m_scaleSnap; }
    void setScaleSnap(float snap) { m_scaleSnap = snap; }

    bool symmetricEditing() const { return m_symmetricEditing; }
    void setSymmetricEditing(bool enabled) { m_symmetricEditing = enabled; }

    // Helpers
    float computeDisplayScale(const glm::vec3& eyePos) const;
    glm::mat3 orientationMatrix() const;
    glm::vec3 axisVector(GizmoAxis axis) const;

    // Symmetric editing helpers
    static int findSymmetricVertex(const PmxDocument& doc, int vertexIndex, float tolerance = 0.005f);
    static int findSymmetricBone(const PmxDocument& doc, int boneIndex);
    static glm::vec3 mirrorDelta(const glm::vec3& delta);
    static glm::quat mirrorRotation(const glm::quat& q);

    // Screen Ray Intersection & Picking
    GizmoAxis hitTest(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float gizmoScale) const;

    // Delta Calculations
    glm::vec3 computeTranslationDelta(
        GizmoAxis axis,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        const glm::vec3& lastRayOrigin,
        const glm::vec3& lastRayDir,
        float gizmoScale,
        bool snap) const;

    glm::quat computeRotationDelta(
        GizmoAxis axis,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        const glm::vec3& lastRayOrigin,
        const glm::vec3& lastRayDir,
        float gizmoScale,
        bool snap) const;

    glm::vec3 computeScaleDelta(
        GizmoAxis axis,
        const glm::vec3& rayOrigin,
        const glm::vec3& rayDir,
        const glm::vec3& lastRayOrigin,
        const glm::vec3& lastRayDir,
        float gizmoScale,
        bool snap) const;

    // Line Geometry Generation for rendering
    std::vector<LineVertex> buildGizmoGeometry(float scale) const;

private:
    glm::vec3 m_position{0.0f};
    glm::quat m_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    GizmoMode m_mode = GizmoMode::Translate;
    CoordSpace m_coordSpace = CoordSpace::World;
    GizmoAxis m_activeAxis = GizmoAxis::None;
    GizmoAxis m_hoveredAxis = GizmoAxis::None;

    bool m_snapEnabled = false;
    float m_translationSnap = 0.5f;
    float m_rotationSnap = 0.261799f; // 15 deg in rad
    float m_scaleSnap = 0.1f;
    bool m_symmetricEditing = false;
};

} // namespace pmx
