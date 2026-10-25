#pragma once

#include "viewport/PmxCamera.h"
#include "viewport/PmxTextureCache.h"
#include "viewport/PmxGizmo.h"
#include "viewport/PmxWeightPaintBrush.h"
#include "models/PmxDocumentController.h"
#include "models/PmxUndoCommands.h"

#include <QtQuick/qquickrhiitem.h>
#include <rhi/qrhi.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace pmx {

class PmxViewportItem;

struct MeshVertex {
    glm::vec3 position{0.0f};
    float edgeScale = 1.0f;
    glm::vec3 normal{0.0f};
    float pad0 = 0.0f;
    glm::vec2 uv{0.0f};
    glm::vec2 pad1{0.0f};
};
static_assert(sizeof(MeshVertex) == 48, "MeshVertex must be 48 bytes");

struct StagedMaterialBatch {
    int materialIndex = -1;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    bool doubleSided = false;
    uint8_t sphereMode = 0;
    bool hasToon = false;
    int sharedToonIndex = -1;
    bool hasEdge = false;
    QString diffuseTexturePath;
    QString sphereTexturePath;
    QString toonTexturePath;
    glm::vec4 diffuse{1.0f};
    glm::vec4 specular{0.0f, 0.0f, 0.0f, 5.0f};
    glm::vec4 ambient{0.5f, 0.5f, 0.5f, 1.0f};
    glm::vec4 edgeColor{0.0f, 0.0f, 0.0f, 1.0f};
    float edgeSize = 1.0f;
};

class PmxRhiRenderer : public QQuickRhiItemRenderer {
public:
    PmxRhiRenderer();
    ~PmxRhiRenderer() override;

    void initialize(QRhiCommandBuffer* cb) override;
    void synchronize(QQuickRhiItem* item) override;
    void render(QRhiCommandBuffer* cb) override;

    void buildGrid(QRhiCommandBuffer* cb);

private:

    struct FrameAndMaterialUboData {
        glm::mat4 mvp{1.0f};
        glm::mat4 modelView{1.0f};
        glm::mat4 normalMatrix{1.0f};
        glm::vec4 lightDir{0.5f, 1.0f, 0.5f, 0.0f};
        glm::vec4 lightColor{0.6f, 0.6f, 0.6f, 1.0f};
        glm::vec4 cameraPos{0.0f};
        glm::vec4 diffuse{1.0f};
        glm::vec4 specular{0.0f, 0.0f, 0.0f, 5.0f};
        glm::vec4 ambient{0.5f, 0.5f, 0.5f, 1.0f};
        glm::vec4 edgeColor{0.0f, 0.0f, 0.0f, 1.0f};
        glm::vec4 edgeParams{1.0f, 600.0f, 0.0f, 0.0f};
        glm::vec4 texTintMul{1.0f};
        glm::vec4 texTintAdd{0.0f};
        glm::vec4 sphereTintMul{1.0f};
        glm::vec4 sphereTintAdd{0.0f};
        glm::vec4 toonTintMul{1.0f};
        glm::vec4 toonTintAdd{0.0f};
    };

    struct LineUboData {
        glm::mat4 mvp{1.0f};
    };

    struct GpuMaterialBatch {
        StagedMaterialBatch staged;
        QRhiBuffer* ubo = nullptr;
        QRhiShaderResourceBindings* srb = nullptr;
    };

    QRhiBuffer* m_dummyLayoutUbo = nullptr;
    QRhiShaderResourceBindings* m_dummyLayoutSrb = nullptr;
    QRhiBuffer* m_lineUbo = nullptr;
    QRhiShaderResourceBindings* m_lineSrb = nullptr;
    QRhiGraphicsPipeline* m_meshPipelineCullNone = nullptr;
    QRhiGraphicsPipeline* m_meshPipelineCullBack = nullptr;
    QRhiGraphicsPipeline* m_edgePipeline = nullptr;
    QRhiGraphicsPipeline* m_linePipeline = nullptr;
    QRhiGraphicsPipeline* m_heatmapPipeline = nullptr;

    QRhiBuffer* m_meshVertexBuffer = nullptr;
    QRhiBuffer* m_meshIndexBuffer = nullptr;
    quint32 m_meshIndexCount = 0;

    QRhiBuffer* m_gridVertexBuffer = nullptr;
    quint32 m_gridVertexCount = 0;

    QRhiBuffer* m_boneVertexBuffer = nullptr;
    quint32 m_boneVertexCount = 0;

    QRhiBuffer* m_physicsVertexBuffer = nullptr;
    quint32 m_physicsVertexCount = 0;

    QRhiBuffer* m_jointVertexBuffer = nullptr;
    quint32 m_jointVertexCount = 0;

    QRhiBuffer* m_gizmoVertexBuffer = nullptr;
    quint32 m_gizmoVertexCount = 0;

    QRhiBuffer* m_heatmapVertexBuffer = nullptr;
    quint32 m_heatmapVertexCount = 0;

    QRhiBuffer* m_brushCursorVertexBuffer = nullptr;
    quint32 m_brushCursorVertexCount = 0;

    std::vector<MeshVertex> m_stagedVertices;
    std::vector<uint32_t> m_stagedIndices;
    std::vector<StagedMaterialBatch> m_stagedMaterials;
    std::vector<GpuMaterialBatch> m_gpuBatches;
    std::vector<LineVertex> m_stagedBoneLines;
    std::vector<LineVertex> m_stagedPhysicsLines;
    std::vector<LineVertex> m_stagedJointLines;
    std::vector<LineVertex> m_stagedGizmoLines;
    std::vector<LineVertex> m_stagedHeatmapVertices;
    std::vector<LineVertex> m_stagedBrushCursorLines;
    bool m_meshDataDirty = false;
    bool m_materialsDirty = false;
    bool m_gizmoDirty = false;
    bool m_heatmapDirty = false;
    bool m_brushCursorDirty = false;

    PmxTextureCache m_textureCache;

    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
    glm::vec3 m_eyePos{0.0f};
    float m_aspect = 1.0f;
    bool m_showGrid = true;
    bool m_showBones = true;
    bool m_showRigidBodies = true;
    bool m_showJoints = true;
    bool m_wireframe = false;
    bool m_showEdgeOutlines = true;
    int m_viewportMode = 0;
    quint64 m_syncedRevision = 0;
    int m_syncedSelectedBone = -1;
    int m_syncedSelectedRigidBody = -1;
    int m_syncedSelectedJoint = -1;
};

class PmxViewportItem : public QQuickRhiItem {
    Q_OBJECT
    Q_PROPERTY(pmx::PmxDocumentController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(pmx::PmxCamera* camera READ camera CONSTANT)
    Q_PROPERTY(quint64 modelRevision READ modelRevision NOTIFY modelRevisionChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showBones READ showBones WRITE setShowBones NOTIFY showBonesChanged)
    Q_PROPERTY(bool showRigidBodies READ showRigidBodies WRITE setShowRigidBodies NOTIFY showRigidBodiesChanged)
    Q_PROPERTY(bool showJoints READ showJoints WRITE setShowJoints NOTIFY showJointsChanged)
    Q_PROPERTY(bool wireframe READ wireframe WRITE setWireframe NOTIFY wireframeChanged)
    Q_PROPERTY(bool showEdgeOutlines READ showEdgeOutlines WRITE setShowEdgeOutlines NOTIFY showEdgeOutlinesChanged)
    Q_PROPERTY(int gizmoMode READ gizmoModeInt WRITE setGizmoModeInt NOTIFY gizmoModeChanged)
    Q_PROPERTY(int coordSpace READ coordSpaceInt WRITE setCoordSpaceInt NOTIFY coordSpaceChanged)
    Q_PROPERTY(bool symmetricEditing READ symmetricEditing WRITE setSymmetricEditing NOTIFY symmetricEditingChanged)
    Q_PROPERTY(bool snapEnabled READ snapEnabled WRITE setSnapEnabled NOTIFY snapEnabledChanged)
    Q_PROPERTY(int viewportMode READ viewportMode WRITE setViewportMode NOTIFY viewportModeChanged)
    Q_PROPERTY(float brushRadius READ brushRadius WRITE setBrushRadius NOTIFY brushRadiusChanged)
    Q_PROPERTY(float brushStrength READ brushStrength WRITE setBrushStrength NOTIFY brushStrengthChanged)
    Q_PROPERTY(int brushMode READ brushMode WRITE setBrushMode NOTIFY brushModeChanged)
    Q_PROPERTY(int brushFalloff READ brushFalloff WRITE setBrushFalloff NOTIFY brushFalloffChanged)

public:
    explicit PmxViewportItem(QQuickItem* parent = nullptr);
    ~PmxViewportItem() override;

    QQuickRhiItemRenderer* createRenderer() override;

    static std::vector<uint32_t> filterValidTriangleIndices(const std::vector<int32_t>& rawIndices, size_t vertexCount);
    static std::vector<StagedMaterialBatch> partitionMaterialBatches(const PmxDocument& doc, std::vector<uint32_t>& outStagedIndices);
    static std::vector<MeshVertex> stageVertices(const PmxDocument& doc);
    static inline glm::vec3 calculateEdgeExtrusion(const glm::vec3& position, const glm::vec3& normal, float edgeSize, float edgeScale) {
        float len = glm::length(normal);
        glm::vec3 safeNorm = (std::isfinite(len) && len > 1e-5f) ? (normal / len) : glm::vec3(0.0f);
        float clampedScale = std::isfinite(edgeScale) ? std::clamp(edgeScale, -100.0f, 100.0f) : 0.0f;
        float offset = std::clamp(edgeSize * clampedScale * 0.01f, -10.0f, 10.0f);
        return position + safeNorm * offset;
    }

    pmx::PmxDocumentController* controller() const { return m_controller; }
    void setController(pmx::PmxDocumentController* controller);

    pmx::PmxCamera* camera() const { return m_camera; }

    quint64 modelRevision() const { return m_modelRevision; }

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);

    bool showBones() const { return m_showBones; }
    void setShowBones(bool show);

    bool showRigidBodies() const { return m_showRigidBodies; }
    void setShowRigidBodies(bool show);

    bool showJoints() const { return m_showJoints; }
    void setShowJoints(bool show);

    bool wireframe() const { return m_wireframe; }
    void setWireframe(bool wireframe);

    bool showEdgeOutlines() const { return m_showEdgeOutlines; }
    void setShowEdgeOutlines(bool show);

    int viewportMode() const { return m_viewportMode; }
    void setViewportMode(int mode);

    float brushRadius() const { return m_brushRadius; }
    void setBrushRadius(float radius);

    float brushStrength() const { return m_brushStrength; }
    void setBrushStrength(float strength);

    int brushMode() const { return m_brushMode; }
    void setBrushMode(int mode);

    int brushFalloff() const { return m_brushFalloff; }
    void setBrushFalloff(int falloff);

    const std::vector<LineVertex>& brushCursorLines() const { return m_brushCursorLines; }
    bool raycastMesh(const Ray& ray, glm::vec3& outHitPoint, glm::vec3& outHitNormal) const;

    GizmoMode gizmoMode() const { return m_gizmo.mode(); }
    void setGizmoMode(GizmoMode mode);
    int gizmoModeInt() const { return static_cast<int>(m_gizmo.mode()); }
    void setGizmoModeInt(int m) { setGizmoMode(static_cast<GizmoMode>(m)); }

    CoordSpace coordSpace() const { return m_gizmo.coordSpace(); }
    void setCoordSpace(CoordSpace space);
    int coordSpaceInt() const { return static_cast<int>(m_gizmo.coordSpace()); }
    void setCoordSpaceInt(int s) { setCoordSpace(static_cast<CoordSpace>(s)); }

    bool symmetricEditing() const { return m_gizmo.symmetricEditing(); }
    void setSymmetricEditing(bool enabled);

    bool snapEnabled() const { return m_gizmo.snapEnabled(); }
    void setSnapEnabled(bool enabled);

    PmxGizmo* gizmo() { return &m_gizmo; }
    const PmxGizmo* gizmo() const { return &m_gizmo; }

    void updateGizmoPosition();

signals:
    void controllerChanged();
    void modelRevisionChanged();
    void showGridChanged();
    void showBonesChanged();
    void showRigidBodiesChanged();
    void showJointsChanged();
    void wireframeChanged();
    void showEdgeOutlinesChanged();
    void viewportModeChanged();
    void brushRadiusChanged();
    void brushStrengthChanged();
    void brushModeChanged();
    void brushFalloffChanged();
    void gizmoModeChanged();
    void coordSpaceChanged();
    void symmetricEditingChanged();
    void snapEnabledChanged();

public slots:
    void onDocumentChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;

private:
    pmx::PmxDocumentController* m_controller = nullptr;
    pmx::PmxCamera* m_camera = nullptr;
    quint64 m_modelRevision = 0;
    bool m_showGrid = true;
    bool m_showBones = true;
    bool m_showRigidBodies = true;
    bool m_showJoints = true;
    bool m_wireframe = false;
    bool m_showEdgeOutlines = true;
    int m_viewportMode = 0;
    float m_brushRadius = 1.0f;
    float m_brushStrength = 0.2f;
    int m_brushMode = 0;
    int m_brushFalloff = 0;

    bool m_isPaintingWeights = false;
    int m_paintSessionId = 0;
    std::unordered_map<int, VertexWeightDelta> m_paintStrokeDeltas;
    std::vector<LineVertex> m_brushCursorLines;
    glm::vec3 m_brushHitPos{0.0f};
    glm::vec3 m_brushHitNormal{0.0f, 1.0f, 0.0f};
    bool m_hasBrushHit = false;

    PmxGizmo m_gizmo;
    bool m_isGizmoDragging = false;
    Ray m_dragStartRay;
    Ray m_dragLastRay;
    glm::vec3 m_dragStartGizmoPos{0.0f};
    int m_dragSessionId = 0;
    std::vector<int> m_dragSelectedVertexIndices;
    std::vector<glm::vec3> m_dragOriginalVertexPositions;
    std::vector<glm::vec3> m_dragOriginalVertexNormals;
    std::vector<int> m_dragSelectedBoneIndices;
    std::vector<glm::vec3> m_dragOriginalBonePositions;

    bool m_isOrbiting = false;
    bool m_isPanning = false;
    QPointF m_lastMousePos;
};

} // namespace pmx
