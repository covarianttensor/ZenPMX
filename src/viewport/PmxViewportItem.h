#pragma once

#include "viewport/PmxCamera.h"
#include "viewport/PmxTextureCache.h"
#include "models/PmxDocumentController.h"

#include <QtQuick/qquickrhiitem.h>
#include <rhi/qrhi.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <vector>
#include <cstdint>

namespace pmx {

class PmxViewportItem;

struct StagedMaterialBatch {
    int materialIndex = -1;
    uint32_t indexOffset = 0;
    uint32_t indexCount = 0;
    bool doubleSided = false;
    uint8_t sphereMode = 0;
    bool hasToon = false;
    int sharedToonIndex = -1;
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
    struct MeshVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    struct LineVertex {
        glm::vec3 position;
        glm::vec4 color;
    };

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
    QRhiGraphicsPipeline* m_linePipeline = nullptr;

    QRhiBuffer* m_meshVertexBuffer = nullptr;
    QRhiBuffer* m_meshIndexBuffer = nullptr;
    quint32 m_meshIndexCount = 0;

    QRhiBuffer* m_gridVertexBuffer = nullptr;
    quint32 m_gridVertexCount = 0;

    QRhiBuffer* m_boneVertexBuffer = nullptr;
    quint32 m_boneVertexCount = 0;

    std::vector<MeshVertex> m_stagedVertices;
    std::vector<uint32_t> m_stagedIndices;
    std::vector<StagedMaterialBatch> m_stagedMaterials;
    std::vector<GpuMaterialBatch> m_gpuBatches;
    std::vector<LineVertex> m_stagedBoneLines;
    bool m_meshDataDirty = false;
    bool m_materialsDirty = false;

    PmxTextureCache m_textureCache;

    glm::mat4 m_viewMatrix{1.0f};
    glm::mat4 m_projMatrix{1.0f};
    glm::vec3 m_eyePos{0.0f};
    float m_aspect = 1.0f;
    bool m_showGrid = true;
    bool m_showBones = true;
    bool m_wireframe = false;
    quint64 m_syncedRevision = 0;
};

class PmxViewportItem : public QQuickRhiItem {
    Q_OBJECT
    Q_PROPERTY(pmx::PmxDocumentController* controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(pmx::PmxCamera* camera READ camera CONSTANT)
    Q_PROPERTY(quint64 modelRevision READ modelRevision NOTIFY modelRevisionChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showBones READ showBones WRITE setShowBones NOTIFY showBonesChanged)
    Q_PROPERTY(bool wireframe READ wireframe WRITE setWireframe NOTIFY wireframeChanged)

public:
    explicit PmxViewportItem(QQuickItem* parent = nullptr);
    ~PmxViewportItem() override;

    QQuickRhiItemRenderer* createRenderer() override;

    static std::vector<uint32_t> filterValidTriangleIndices(const std::vector<int32_t>& rawIndices, size_t vertexCount);
    static std::vector<StagedMaterialBatch> partitionMaterialBatches(const PmxDocument& doc, std::vector<uint32_t>& outStagedIndices);

    pmx::PmxDocumentController* controller() const { return m_controller; }
    void setController(pmx::PmxDocumentController* controller);

    pmx::PmxCamera* camera() const { return m_camera; }

    quint64 modelRevision() const { return m_modelRevision; }

    bool showGrid() const { return m_showGrid; }
    void setShowGrid(bool show);

    bool showBones() const { return m_showBones; }
    void setShowBones(bool show);

    bool wireframe() const { return m_wireframe; }
    void setWireframe(bool wireframe);

signals:
    void controllerChanged();
    void modelRevisionChanged();
    void showGridChanged();
    void showBonesChanged();
    void wireframeChanged();

public slots:
    void onDocumentChanged();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    pmx::PmxDocumentController* m_controller = nullptr;
    pmx::PmxCamera* m_camera = nullptr;
    quint64 m_modelRevision = 0;
    bool m_showGrid = true;
    bool m_showBones = true;
    bool m_wireframe = false;

    bool m_isOrbiting = false;
    bool m_isPanning = false;
    QPointF m_lastMousePos;
};

} // namespace pmx
