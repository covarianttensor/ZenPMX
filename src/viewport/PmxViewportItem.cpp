#include "viewport/PmxViewportItem.h"
#include "viewport/PmxWireframeRenderer.h"
#include "models/PmxUndoCommands.h"
#include "evaluator/VmdImporter.h"
#include "evaluator/VpdImporter.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMatrix4x4>
#include <QUrl>
#include <cstring>
#include <algorithm>
#include <cmath>

inline void initShaders() {
    Q_INIT_RESOURCE(pmx_shaders);
}

namespace pmx {

inline bool isVec3Finite(const glm::vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

static QShader loadShaderFromResource(const QString& name) {
    initShaders();
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to load shader resource:" << name;
        return QShader();
    }
    return QShader::fromSerialized(f.readAll());
}

// -----------------------------------------------------------------------------
// PmxRhiRenderer
// -----------------------------------------------------------------------------

PmxRhiRenderer::PmxRhiRenderer()
    : m_meshDataDirty(true)
    , m_materialsDirty(true)
{
}

PmxRhiRenderer::~PmxRhiRenderer() {
    for (auto& gb : m_gpuBatches) {
        delete gb.srb;
        delete gb.ubo;
    }
    m_gpuBatches.clear();

    delete m_dummyLayoutUbo;
    delete m_dummyLayoutSrb;
    delete m_lineUbo;
    delete m_lineSrb;
    delete m_meshPipelineCullNone;
    delete m_meshPipelineCullBack;
    delete m_edgePipeline;
    delete m_linePipeline;
    delete m_heatmapPipeline;
    delete m_meshVertexBuffer;
    delete m_meshIndexBuffer;
    delete m_gridVertexBuffer;
    delete m_boneVertexBuffer;
    delete m_physicsVertexBuffer;
    delete m_jointVertexBuffer;
    delete m_gizmoVertexBuffer;
    delete m_heatmapVertexBuffer;
    delete m_brushCursorVertexBuffer;

    m_textureCache.clear();
}

void PmxRhiRenderer::initialize(QRhiCommandBuffer* cb) {
    QRhiResourceUpdateBatch* u = rhi()->nextResourceUpdateBatch();

    if (!m_dummyLayoutUbo) {
        m_dummyLayoutUbo = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(FrameAndMaterialUboData));
        m_dummyLayoutUbo->create();
    }
    if (!m_lineUbo) {
        m_lineUbo = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LineUboData));
        m_lineUbo->create();
    }

    QRhiTexture* dummyWhiteTex = m_textureCache.defaultWhiteTexture(rhi(), u);
    QRhiSampler* repSampler = m_textureCache.repeatSampler(rhi());
    QRhiSampler* clmpSampler = m_textureCache.clampSampler(rhi());

    if (!m_dummyLayoutSrb) {
        m_dummyLayoutSrb = rhi()->newShaderResourceBindings();
        m_dummyLayoutSrb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_dummyLayoutUbo),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, dummyWhiteTex, repSampler),
            QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, dummyWhiteTex, clmpSampler),
            QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, dummyWhiteTex, clmpSampler)
        });
        m_dummyLayoutSrb->create();
    }

    if (!m_lineSrb) {
        m_lineSrb = rhi()->newShaderResourceBindings();
        m_lineSrb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage, m_lineUbo)
        });
        m_lineSrb->create();
    }

    QShader meshVs = loadShaderFromResource(QStringLiteral(":/shaders/mesh.vert.qsb"));
    QShader meshFs = loadShaderFromResource(QStringLiteral(":/shaders/mesh.frag.qsb"));
    QShader linesVs = loadShaderFromResource(QStringLiteral(":/shaders/lines.vert.qsb"));
    QShader linesFs = loadShaderFromResource(QStringLiteral(":/shaders/lines.frag.qsb"));
    QShader edgeVs = loadShaderFromResource(QStringLiteral(":/shaders/pmx_edge.vert.qsb"));
    QShader edgeFs = loadShaderFromResource(QStringLiteral(":/shaders/pmx_edge.frag.qsb"));

    auto createMeshPipeline = [&](QRhiGraphicsPipeline::CullMode cullMode) -> QRhiGraphicsPipeline* {
        auto* pipe = rhi()->newGraphicsPipeline();
        pipe->setShaderStages({
            { QRhiShaderStage::Vertex, meshVs },
            { QRhiShaderStage::Fragment, meshFs }
        });
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { sizeof(MeshVertex) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float4, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float4, 16 },
            { 0, 2, QRhiVertexInputAttribute::Float4, 32 }
        });
        pipe->setVertexInputLayout(inputLayout);
        pipe->setShaderResourceBindings(m_dummyLayoutSrb);
        pipe->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        pipe->setTopology(QRhiGraphicsPipeline::Triangles);
        pipe->setCullMode(cullMode);
        pipe->setFrontFace(QRhiGraphicsPipeline::CW);
        pipe->setDepthTest(true);
        pipe->setDepthWrite(true);
        pipe->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        pipe->create();
        return pipe;
    };

    if (!m_meshPipelineCullNone) {
        m_meshPipelineCullNone = createMeshPipeline(QRhiGraphicsPipeline::None);
    }
    if (!m_meshPipelineCullBack) {
        m_meshPipelineCullBack = createMeshPipeline(QRhiGraphicsPipeline::Back);
    }

    if (!m_edgePipeline) {
        m_edgePipeline = rhi()->newGraphicsPipeline();
        m_edgePipeline->setShaderStages({
            { QRhiShaderStage::Vertex, edgeVs },
            { QRhiShaderStage::Fragment, edgeFs }
        });
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { sizeof(MeshVertex) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float4, 0 },
            { 0, 1, QRhiVertexInputAttribute::Float4, 16 },
            { 0, 2, QRhiVertexInputAttribute::Float4, 32 }
        });
        m_edgePipeline->setVertexInputLayout(inputLayout);
        m_edgePipeline->setShaderResourceBindings(m_dummyLayoutSrb);
        m_edgePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        m_edgePipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_edgePipeline->setCullMode(QRhiGraphicsPipeline::Front);
        m_edgePipeline->setFrontFace(QRhiGraphicsPipeline::CW);
        m_edgePipeline->setDepthTest(true);
        m_edgePipeline->setDepthWrite(false);
        m_edgePipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);

        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.opColor = QRhiGraphicsPipeline::Add;
        blend.srcAlpha = QRhiGraphicsPipeline::SrcAlpha;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.opAlpha = QRhiGraphicsPipeline::Add;
        m_edgePipeline->setTargetBlends({ blend });

        m_edgePipeline->create();
    }

    if (!m_linePipeline) {
        m_linePipeline = rhi()->newGraphicsPipeline();
        m_linePipeline->setShaderStages({
            { QRhiShaderStage::Vertex, linesVs },
            { QRhiShaderStage::Fragment, linesFs }
        });
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { sizeof(LineVertex) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(LineVertex, position) },
            { 0, 1, QRhiVertexInputAttribute::Float4, offsetof(LineVertex, color) }
        });
        m_linePipeline->setVertexInputLayout(inputLayout);
        m_linePipeline->setShaderResourceBindings(m_lineSrb);
        m_linePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        m_linePipeline->setTopology(QRhiGraphicsPipeline::Lines);
        m_linePipeline->setCullMode(QRhiGraphicsPipeline::None);
        m_linePipeline->setDepthTest(true);
        m_linePipeline->setDepthWrite(true);
        m_linePipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        m_linePipeline->create();
    }

    if (!m_heatmapPipeline) {
        m_heatmapPipeline = rhi()->newGraphicsPipeline();
        m_heatmapPipeline->setShaderStages({
            { QRhiShaderStage::Vertex, linesVs },
            { QRhiShaderStage::Fragment, linesFs }
        });
        QRhiVertexInputLayout inputLayout;
        inputLayout.setBindings({
            { sizeof(LineVertex) }
        });
        inputLayout.setAttributes({
            { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(LineVertex, position) },
            { 0, 1, QRhiVertexInputAttribute::Float4, offsetof(LineVertex, color) }
        });
        m_heatmapPipeline->setVertexInputLayout(inputLayout);
        m_heatmapPipeline->setShaderResourceBindings(m_lineSrb);
        m_heatmapPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        m_heatmapPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_heatmapPipeline->setCullMode(QRhiGraphicsPipeline::None);
        m_heatmapPipeline->setFrontFace(QRhiGraphicsPipeline::CW);
        m_heatmapPipeline->setDepthTest(true);
        m_heatmapPipeline->setDepthWrite(true);
        m_heatmapPipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        m_heatmapPipeline->create();
    }

    if (cb) {
        cb->resourceUpdate(u);
    }

    buildGrid(cb);
}

void PmxRhiRenderer::buildGrid(QRhiCommandBuffer* cb) {
    std::vector<LineVertex> gridVertices;
    const int halfCount = 20;
    const float spacing = 1.0f;
    const glm::vec4 minorColor(0.25f, 0.25f, 0.28f, 1.0f);
    const glm::vec4 majorColor(0.4f, 0.4f, 0.45f, 1.0f);
    const glm::vec4 xAxisColor(0.8f, 0.2f, 0.2f, 1.0f);
    const glm::vec4 zAxisColor(0.2f, 0.3f, 0.8f, 1.0f);

    for (int i = -halfCount; i <= halfCount; ++i) {
        float pos = static_cast<float>(i) * spacing;
        // Line along Z (varying X)
        glm::vec4 cX = (i == 0) ? zAxisColor : ((i % 5 == 0) ? majorColor : minorColor);
        gridVertices.push_back({ glm::vec3(pos, 0.0f, -halfCount * spacing), cX });
        gridVertices.push_back({ glm::vec3(pos, 0.0f, halfCount * spacing), cX });

        // Line along X (varying Z)
        glm::vec4 cZ = (i == 0) ? xAxisColor : ((i % 5 == 0) ? majorColor : minorColor);
        gridVertices.push_back({ glm::vec3(-halfCount * spacing, 0.0f, pos), cZ });
        gridVertices.push_back({ glm::vec3(halfCount * spacing, 0.0f, pos), cZ });
    }

    m_gridVertexCount = static_cast<quint32>(gridVertices.size());
    quint32 bufferSize = static_cast<quint32>(gridVertices.size() * sizeof(LineVertex));
    if (!m_gridVertexBuffer || m_gridVertexBuffer->size() < bufferSize) {
        delete m_gridVertexBuffer;
        m_gridVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, bufferSize);
        m_gridVertexBuffer->create();
    }

    if (cb) {
        QRhiResourceUpdateBatch* u = rhi()->nextResourceUpdateBatch();
        u->uploadStaticBuffer(m_gridVertexBuffer, gridVertices.data());
        cb->resourceUpdate(u);
    }
}

void PmxRhiRenderer::synchronize(QQuickRhiItem* item) {
    auto* vp = static_cast<PmxViewportItem*>(item);
    m_aspect = static_cast<float>(vp->width()) / std::max(1.0f, static_cast<float>(vp->height()));
    m_viewMatrix = vp->camera()->viewMatrix();
    m_projMatrix = vp->camera()->projectionMatrix(m_aspect);
    m_eyePos = vp->camera()->eyePosition();
    m_showGrid = vp->showGrid();
    m_showBones = vp->showBones();
    m_showRigidBodies = vp->showRigidBodies();
    m_showJoints = vp->showJoints();
    m_wireframe = vp->wireframe();
    m_showEdgeOutlines = vp->showEdgeOutlines();
    bool modeChanged = (m_viewportMode != vp->viewportMode());
    m_viewportMode = vp->viewportMode();
    bool transformViewChanged = (m_syncedTransformViewMode != vp->isTransformViewMode());
    m_syncedTransformViewMode = vp->isTransformViewMode();

    int selBone = -1;
    int selRb = -1;
    int selJoint = -1;
    if (vp->controller() && vp->controller()->selectionManager()) {
        auto* sm = vp->controller()->selectionManager();
        auto bones = sm->selectedBones();
        if (!bones.isEmpty()) selBone = bones.first();
        auto rbs = sm->selectedRigidBodies();
        if (!rbs.isEmpty()) selRb = rbs.first();
        auto joints = sm->selectedJoints();
        if (!joints.isEmpty()) selJoint = joints.first();
    }

    bool runtimeDirty = (m_syncedTransformViewMode && m_syncedRuntimeRevision != vp->runtimeRevision());
    bool docDirty = (m_syncedRevision != vp->modelRevision());

    if (docDirty || runtimeDirty || transformViewChanged ||
        m_syncedSelectedBone != selBone ||
        m_syncedSelectedRigidBody != selRb ||
        m_syncedSelectedJoint != selJoint ||
        modeChanged) {
        m_syncedRevision = vp->modelRevision();
        m_syncedRuntimeRevision = vp->runtimeRevision();
        m_syncedSelectedBone = selBone;
        m_syncedSelectedRigidBody = selRb;
        m_syncedSelectedJoint = selJoint;

        m_stagedVertices.clear();
        m_stagedBoneLines.clear();
        m_stagedPhysicsLines.clear();
        m_stagedJointLines.clear();
        m_stagedHeatmapVertices.clear();

        if (vp->controller()) {
            const auto& doc = vp->controller()->document();
            if (m_syncedTransformViewMode) {
                m_stagedVertices = PmxViewportItem::stageDeformedVertices(doc, vp->runtimeScene());
                m_stagedBoneLines = PmxWireframeRenderer::buildBoneWireframes(doc, &vp->runtimeScene(), selBone);
                m_stagedPhysicsLines = PmxWireframeRenderer::buildRigidBodyWireframes(doc, &vp->runtimeScene(), selRb);
                m_stagedJointLines = PmxWireframeRenderer::buildJointWireframes(doc, &vp->runtimeScene(), selJoint);
            } else {
                m_stagedVertices = PmxViewportItem::stageVertices(doc);
                m_stagedBoneLines = PmxWireframeRenderer::buildBoneWireframes(doc, nullptr, selBone);
                m_stagedPhysicsLines = PmxWireframeRenderer::buildRigidBodyWireframes(doc, nullptr, selRb);
                m_stagedJointLines = PmxWireframeRenderer::buildJointWireframes(doc, nullptr, selJoint);
            }

            if (docDirty || transformViewChanged || m_stagedMaterials.empty()) {
                m_stagedMaterials = PmxViewportItem::partitionMaterialBatches(doc, m_stagedIndices);
                m_materialsDirty = true;
            }

            if (m_syncedTransformViewMode) {
                const auto& emList = vp->runtimeScene().evaluatedMaterials();
                for (auto& batch : m_stagedMaterials) {
                    if (batch.materialIndex >= 0 && static_cast<size_t>(batch.materialIndex) < emList.size()) {
                        const auto& em = emList[static_cast<size_t>(batch.materialIndex)];
                        batch.diffuse = em.diffuse;
                        batch.specular = glm::vec4(em.specular, em.specularPower);
                        batch.ambient = glm::vec4(em.ambient, 1.0f);
                        batch.edgeColor = em.edgeColor;
                        batch.edgeSize = em.edgeSize;
                        batch.texTintMul = em.textureTint;
                        batch.sphereTintMul = em.sphereTint;
                        batch.toonTintMul = em.toonTint;
                    }
                }
            }

            m_stagedHeatmapVertices.reserve(doc.vertices.size());
            for (const auto& v : doc.vertices) {
                float w = (selBone >= 0) ? PmxWeightPaintBrush::getVertexBoneWeight(v, selBone) : 0.0f;
                glm::vec4 col = PmxWeightPaintBrush::weightToColor(w);
                m_stagedHeatmapVertices.push_back({ PmxCamera::toRightHanded(v.position), col });
            }
        }

        m_meshDataDirty = true;
        m_heatmapDirty = true;
    }

    m_stagedGizmoLines.clear();
    if (!m_syncedTransformViewMode && vp->gizmo()->mode() != GizmoMode::None && m_viewportMode != 2) {
        float gizmoScale = vp->gizmo()->computeDisplayScale(m_eyePos);
        m_stagedGizmoLines = vp->gizmo()->buildGizmoGeometry(gizmoScale);
    }
    m_gizmoDirty = true;

    m_stagedBrushCursorLines = m_syncedTransformViewMode ? std::vector<LineVertex>{} : vp->brushCursorLines();
    m_brushCursorDirty = true;
}

void PmxRhiRenderer::render(QRhiCommandBuffer* cb) {
    QRhiResourceUpdateBatch* u = rhi()->nextResourceUpdateBatch();

    if (m_meshDataDirty) {
        m_meshDataDirty = false;
        if (!m_stagedVertices.empty()) {
            quint32 vSize = static_cast<quint32>(m_stagedVertices.size() * sizeof(MeshVertex));
            if (!m_meshVertexBuffer || m_meshVertexBuffer->size() != vSize) {
                delete m_meshVertexBuffer;
                m_meshVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, vSize);
                m_meshVertexBuffer->create();
            }
            u->uploadStaticBuffer(m_meshVertexBuffer, 0, vSize, m_stagedVertices.data());
        }

        if (!m_stagedIndices.empty()) {
            quint32 iSize = static_cast<quint32>(m_stagedIndices.size() * sizeof(uint32_t));
            if (!m_meshIndexBuffer || m_meshIndexBuffer->size() != iSize) {
                delete m_meshIndexBuffer;
                m_meshIndexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::IndexBuffer, iSize);
                m_meshIndexBuffer->create();
            }
            u->uploadStaticBuffer(m_meshIndexBuffer, 0, iSize, m_stagedIndices.data());
            m_meshIndexCount = static_cast<quint32>(m_stagedIndices.size());
        } else {
            m_meshIndexCount = 0;
        }

        if (!m_stagedBoneLines.empty()) {
            quint32 bSize = static_cast<quint32>(m_stagedBoneLines.size() * sizeof(LineVertex));
            if (!m_boneVertexBuffer || m_boneVertexBuffer->size() != bSize) {
                delete m_boneVertexBuffer;
                m_boneVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, bSize);
                m_boneVertexBuffer->create();
            }
            u->uploadStaticBuffer(m_boneVertexBuffer, 0, bSize, m_stagedBoneLines.data());
            m_boneVertexCount = static_cast<quint32>(m_stagedBoneLines.size());
        } else {
            m_boneVertexCount = 0;
        }

        if (!m_stagedPhysicsLines.empty()) {
            quint32 pSize = static_cast<quint32>(m_stagedPhysicsLines.size() * sizeof(LineVertex));
            if (!m_physicsVertexBuffer || m_physicsVertexBuffer->size() != pSize) {
                delete m_physicsVertexBuffer;
                m_physicsVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, pSize);
                m_physicsVertexBuffer->create();
            }
            u->uploadStaticBuffer(m_physicsVertexBuffer, 0, pSize, m_stagedPhysicsLines.data());
            m_physicsVertexCount = static_cast<quint32>(m_stagedPhysicsLines.size());
        } else {
            m_physicsVertexCount = 0;
        }

        if (!m_stagedJointLines.empty()) {
            quint32 jSize = static_cast<quint32>(m_stagedJointLines.size() * sizeof(LineVertex));
            if (!m_jointVertexBuffer || m_jointVertexBuffer->size() != jSize) {
                delete m_jointVertexBuffer;
                m_jointVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, jSize);
                m_jointVertexBuffer->create();
            }
            u->uploadStaticBuffer(m_jointVertexBuffer, 0, jSize, m_stagedJointLines.data());
            m_jointVertexCount = static_cast<quint32>(m_stagedJointLines.size());
        } else {
            m_jointVertexCount = 0;
        }
    }

    if (m_gizmoDirty) {
        m_gizmoDirty = false;
        if (!m_stagedGizmoLines.empty()) {
            quint32 gSize = static_cast<quint32>(m_stagedGizmoLines.size() * sizeof(LineVertex));
            if (!m_gizmoVertexBuffer || m_gizmoVertexBuffer->size() != gSize) {
                delete m_gizmoVertexBuffer;
                m_gizmoVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, gSize);
                m_gizmoVertexBuffer->create();
            }
            u->uploadStaticBuffer(m_gizmoVertexBuffer, 0, gSize, m_stagedGizmoLines.data());
            m_gizmoVertexCount = static_cast<quint32>(m_stagedGizmoLines.size());
        } else {
            m_gizmoVertexCount = 0;
        }
    }

    if (m_heatmapDirty) {
        m_heatmapDirty = false;
        if (!m_stagedHeatmapVertices.empty()) {
            quint32 hSize = static_cast<quint32>(m_stagedHeatmapVertices.size() * sizeof(LineVertex));
            if (!m_heatmapVertexBuffer || m_heatmapVertexBuffer->size() != hSize) {
                delete m_heatmapVertexBuffer;
                m_heatmapVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, hSize);
                m_heatmapVertexBuffer->create();
            }
            u->uploadStaticBuffer(m_heatmapVertexBuffer, 0, hSize, m_stagedHeatmapVertices.data());
            m_heatmapVertexCount = static_cast<quint32>(m_stagedHeatmapVertices.size());
        } else {
            m_heatmapVertexCount = 0;
        }
    }

    if (m_brushCursorDirty) {
        m_brushCursorDirty = false;
        if (!m_stagedBrushCursorLines.empty()) {
            quint32 cSize = static_cast<quint32>(m_stagedBrushCursorLines.size() * sizeof(LineVertex));
            if (!m_brushCursorVertexBuffer || m_brushCursorVertexBuffer->size() != cSize) {
                delete m_brushCursorVertexBuffer;
                m_brushCursorVertexBuffer = rhi()->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, cSize);
                m_brushCursorVertexBuffer->create();
            }
            u->uploadStaticBuffer(m_brushCursorVertexBuffer, 0, cSize, m_stagedBrushCursorLines.data());
            m_brushCursorVertexCount = static_cast<quint32>(m_stagedBrushCursorLines.size());
        } else {
            m_brushCursorVertexCount = 0;
        }
    }

    if (m_materialsDirty || (m_gpuBatches.empty() && !m_stagedMaterials.empty())) {
        m_materialsDirty = false;
        for (auto& gb : m_gpuBatches) {
            delete gb.srb;
            delete gb.ubo;
        }
        m_gpuBatches.clear();

        QRhiSampler* repSampler = m_textureCache.repeatSampler(rhi());
        QRhiSampler* clmpSampler = m_textureCache.clampSampler(rhi());

        for (const auto& staged : m_stagedMaterials) {
            GpuMaterialBatch gb;
            gb.staged = staged;

            // Diffuse texture (Binding 1)
            QRhiTexture* diffTex = nullptr;
            if (!staged.diffuseTexturePath.isEmpty()) {
                diffTex = m_textureCache.loadTexture(rhi(), staged.diffuseTexturePath, u);
            }
            if (!diffTex) {
                diffTex = m_textureCache.defaultWhiteTexture(rhi(), u);
            }

            // Toon texture (Binding 2)
            QRhiTexture* toonTex = nullptr;
            if (staged.hasToon) {
                if (staged.sharedToonIndex >= 0) {
                    toonTex = m_textureCache.defaultToonTexture(rhi(), staged.sharedToonIndex, u);
                } else if (!staged.toonTexturePath.isEmpty()) {
                    toonTex = m_textureCache.loadTexture(rhi(), staged.toonTexturePath, u);
                }
            }
            if (!toonTex) {
                toonTex = m_textureCache.defaultToonTexture(rhi(), 0, u);
            }

            // Sphere texture (Binding 3)
            QRhiTexture* sphTex = nullptr;
            if (staged.sphereMode == 1 || staged.sphereMode == 2) {
                if (!staged.sphereTexturePath.isEmpty()) {
                    sphTex = m_textureCache.loadTexture(rhi(), staged.sphereTexturePath, u);
                }
            }
            if (!sphTex) {
                sphTex = m_textureCache.defaultWhiteTexture(rhi(), u);
            }

            gb.ubo = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(FrameAndMaterialUboData));
            gb.ubo->create();

            gb.srb = rhi()->newShaderResourceBindings();
            gb.srb->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, gb.ubo),
                QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage, diffTex, repSampler),
                QRhiShaderResourceBinding::sampledTexture(2, QRhiShaderResourceBinding::FragmentStage, toonTex, clmpSampler),
                QRhiShaderResourceBinding::sampledTexture(3, QRhiShaderResourceBinding::FragmentStage, sphTex, clmpSampler)
            });
            gb.srb->create();

            m_gpuBatches.push_back(gb);
        }
    }

    const QSize outputSize = renderTarget()->pixelSize();

    QMatrix4x4 corr = rhi()->clipSpaceCorrMatrix();
    const float* corrData = corr.constData();
    glm::mat4 corrMat;
    std::memcpy(&corrMat[0][0], corrData, 16 * sizeof(float));
    glm::mat4 mvp = corrMat * m_projMatrix * m_viewMatrix;
    glm::mat4 normalMatrix = glm::transpose(glm::inverse(m_viewMatrix));
    glm::vec4 lightDir = glm::normalize(m_viewMatrix * glm::vec4(0.5f, 1.0f, 0.5f, 0.0f));

    for (size_t i = 0; i < m_gpuBatches.size() && i < m_stagedMaterials.size(); ++i) {
        m_gpuBatches[i].staged = m_stagedMaterials[i];
    }

    for (auto& gb : m_gpuBatches) {
        FrameAndMaterialUboData uboData;
        uboData.mvp = mvp;
        uboData.modelView = m_viewMatrix;
        uboData.normalMatrix = normalMatrix;
        uboData.lightDir = lightDir;
        uboData.lightColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
        uboData.cameraPos = glm::vec4(m_eyePos, 1.0f);

        uboData.diffuse = gb.staged.diffuse;
        uboData.specular = gb.staged.specular;
        uboData.ambient = gb.staged.ambient;
        uboData.edgeColor = gb.staged.edgeColor;
        uboData.edgeParams = glm::vec4(
            gb.staged.edgeSize,
            static_cast<float>(outputSize.height()),
            static_cast<float>(gb.staged.sphereMode),
            gb.staged.hasToon ? 1.0f : 0.0f
        );
        uboData.texTintMul = gb.staged.texTintMul;
        uboData.texTintAdd = glm::vec4(0.0f);
        uboData.sphereTintMul = gb.staged.sphereTintMul;
        uboData.sphereTintAdd = glm::vec4(0.0f);
        uboData.toonTintMul = gb.staged.toonTintMul;
        uboData.toonTintAdd = glm::vec4(0.0f);

        u->updateDynamicBuffer(gb.ubo, 0, sizeof(uboData), &uboData);
    }

    LineUboData lineData;
    lineData.mvp = mvp;
    u->updateDynamicBuffer(m_lineUbo, 0, sizeof(lineData), &lineData);

    cb->beginPass(renderTarget(), QColor::fromRgbF(0.145f, 0.145f, 0.16f, 1.0f), { 1.0f, 0 }, u);
    cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));

    // 1. Ground Grid
    if (m_showGrid && m_gridVertexBuffer && m_gridVertexCount > 0 && m_linePipeline) {
        cb->setGraphicsPipeline(m_linePipeline);
        cb->setShaderResources(m_lineSrb);
        QRhiCommandBuffer::VertexInput vInput(m_gridVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput);
        cb->draw(m_gridVertexCount);
    }

    // 2. Mesh Triangles
    if (m_viewportMode == 2) {
        // Weight Paint Heatmap Pass
        if (m_heatmapVertexBuffer && m_meshIndexBuffer && m_meshIndexCount > 0 && m_heatmapPipeline) {
            cb->setGraphicsPipeline(m_heatmapPipeline);
            cb->setShaderResources(m_lineSrb);
            QRhiCommandBuffer::VertexInput vInput(m_heatmapVertexBuffer, 0);
            cb->setVertexInput(0, 1, &vInput, m_meshIndexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
            cb->drawIndexed(m_meshIndexCount);
        }
    } else {
        if (m_meshVertexBuffer && m_meshIndexBuffer && m_meshIndexCount > 0) {
            QRhiCommandBuffer::VertexInput vInput(m_meshVertexBuffer, 0);

            for (const auto& gb : m_gpuBatches) {
                if (gb.staged.indexCount == 0 || !gb.srb) continue;

                auto* pipe = (gb.staged.doubleSided || m_wireframe) ? m_meshPipelineCullNone : m_meshPipelineCullBack;
                if (!pipe) pipe = m_meshPipelineCullNone;

                cb->setGraphicsPipeline(pipe);
                cb->setShaderResources(gb.srb);
                cb->setVertexInput(0, 1, &vInput, m_meshIndexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
                cb->drawIndexed(gb.staged.indexCount, 1, gb.staged.indexOffset, 0);
            }

            // 3. Inverted-Hull Edge Outline Pass
            if (m_showEdgeOutlines && m_edgePipeline) {
                for (const auto& gb : m_gpuBatches) {
                    if (!gb.staged.hasEdge || gb.staged.indexCount == 0 || !gb.srb) continue;

                    cb->setGraphicsPipeline(m_edgePipeline);
                    cb->setShaderResources(gb.srb);
                    cb->setVertexInput(0, 1, &vInput, m_meshIndexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
                    cb->drawIndexed(gb.staged.indexCount, 1, gb.staged.indexOffset, 0);
                }
            }
        }
    }

    // 4. Bone Overlay
    if (m_showBones && m_boneVertexBuffer && m_boneVertexCount > 0 && m_linePipeline) {
        cb->setGraphicsPipeline(m_linePipeline);
        cb->setShaderResources(m_lineSrb);
        QRhiCommandBuffer::VertexInput vInput(m_boneVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput);
        cb->draw(m_boneVertexCount);
    }

    // 4b. Rigid Body (Physics) Wireframe Overlay
    if (m_showRigidBodies && m_physicsVertexBuffer && m_physicsVertexCount > 0 && m_linePipeline) {
        cb->setGraphicsPipeline(m_linePipeline);
        cb->setShaderResources(m_lineSrb);
        QRhiCommandBuffer::VertexInput vInput(m_physicsVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput);
        cb->draw(m_physicsVertexCount);
    }

    // 4c. Joint Wireframe Overlay
    if (m_showJoints && m_jointVertexBuffer && m_jointVertexCount > 0 && m_linePipeline) {
        cb->setGraphicsPipeline(m_linePipeline);
        cb->setShaderResources(m_lineSrb);
        QRhiCommandBuffer::VertexInput vInput(m_jointVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput);
        cb->draw(m_jointVertexCount);
    }

    // 5. Gizmo Overlay
    if (m_gizmoVertexBuffer && m_gizmoVertexCount > 0 && m_linePipeline) {
        cb->setGraphicsPipeline(m_linePipeline);
        cb->setShaderResources(m_lineSrb);
        QRhiCommandBuffer::VertexInput vInput(m_gizmoVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput);
        cb->draw(m_gizmoVertexCount);
    }

    // 6. Brush Cursor Wireframe Overlay
    if (m_viewportMode == 2 && m_brushCursorVertexBuffer && m_brushCursorVertexCount > 0 && m_linePipeline) {
        cb->setGraphicsPipeline(m_linePipeline);
        cb->setShaderResources(m_lineSrb);
        QRhiCommandBuffer::VertexInput vInput(m_brushCursorVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput);
        cb->draw(m_brushCursorVertexCount);
    }

    cb->endPass();
}

// -----------------------------------------------------------------------------
// PmxViewportItem
// -----------------------------------------------------------------------------

PmxViewportItem::PmxViewportItem(QQuickItem* parent)
    : QQuickRhiItem(parent)
    , m_camera(new PmxCamera(this))
{
    initShaders();
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);

    m_playbackTimer = new QTimer(this);
    m_playbackTimer->setInterval(16);
    m_playbackTimer->setTimerType(Qt::PreciseTimer);
    connect(m_playbackTimer, &QTimer::timeout, this, &PmxViewportItem::onPlaybackTick);

    connect(m_camera, &PmxCamera::cameraChanged, this, [this]() {
        update();
    });
}

PmxViewportItem::~PmxViewportItem() {
    if (m_playbackTimer) {
        m_playbackTimer->stop();
    }
}

QQuickRhiItemRenderer* PmxViewportItem::createRenderer() {
    return new PmxRhiRenderer();
}

std::vector<uint32_t> PmxViewportItem::filterValidTriangleIndices(const std::vector<int32_t>& rawIndices, size_t vertexCount) {
    std::vector<uint32_t> validIndices;
    if (rawIndices.size() < 3 || vertexCount == 0) {
        return validIndices;
    }

    validIndices.reserve(rawIndices.size());
    for (size_t i = 0; i + 2 < rawIndices.size(); i += 3) {
        int32_t i0 = rawIndices[i];
        int32_t i1 = rawIndices[i + 1];
        int32_t i2 = rawIndices[i + 2];

        if (i0 >= 0 && static_cast<size_t>(i0) < vertexCount &&
            i1 >= 0 && static_cast<size_t>(i1) < vertexCount &&
            i2 >= 0 && static_cast<size_t>(i2) < vertexCount) {
            validIndices.push_back(static_cast<uint32_t>(i0));
            validIndices.push_back(static_cast<uint32_t>(i1));
            validIndices.push_back(static_cast<uint32_t>(i2));
        }
    }

    return validIndices;
}

std::vector<MeshVertex> PmxViewportItem::stageVertices(const PmxDocument& doc) {
    std::vector<MeshVertex> staged;
    staged.reserve(doc.vertices.size());
    for (const auto& v : doc.vertices) {
        MeshVertex mv;
        mv.position = PmxCamera::toRightHanded(v.position);
        glm::vec3 n_rh = PmxCamera::toRightHanded(v.normal);
        float len = glm::length(n_rh);
        if (std::isfinite(len) && len > 1e-5f) {
            mv.normal = n_rh / len;
        } else {
            mv.normal = glm::vec3(0.0f);
        }
        mv.uv = v.uv;
        mv.edgeScale = std::isfinite(v.edgeScale) ? std::clamp(v.edgeScale, -100.0f, 100.0f) : 0.0f;
        staged.push_back(mv);
    }
    return staged;
}

std::vector<MeshVertex> PmxViewportItem::stageDeformedVertices(const PmxDocument& doc, const PmxRuntimeScene& scene) {
    std::vector<MeshVertex> staged;
    const size_t n = doc.vertices.size();
    staged.reserve(n);
    const auto& defPos = scene.deformedPositions();
    const auto& defNorm = scene.deformedNormals();
    const auto& uvOffsets = scene.uvOffsets();

    for (size_t i = 0; i < n; ++i) {
        MeshVertex mv;
        if (i < defPos.size() && isVec3Finite(defPos[i])) {
            mv.position = defPos[i];
        } else {
            mv.position = PmxCamera::toRightHanded(doc.vertices[i].position);
        }

        if (i < defNorm.size() && isVec3Finite(defNorm[i])) {
            mv.normal = defNorm[i];
        } else {
            glm::vec3 n_rh = PmxCamera::toRightHanded(doc.vertices[i].normal);
            float len = glm::length(n_rh);
            mv.normal = (std::isfinite(len) && len > 1e-5f) ? (n_rh / len) : glm::vec3(0.0f);
        }

        mv.uv = doc.vertices[i].uv;
        if (i < uvOffsets.size()) {
            const glm::vec4& uvo = uvOffsets[i];
            if (std::isfinite(uvo.x) && std::isfinite(uvo.y)) {
                mv.uv += glm::vec2(uvo.x, uvo.y);
            }
        }

        mv.edgeScale = std::isfinite(doc.vertices[i].edgeScale)
            ? std::clamp(doc.vertices[i].edgeScale, -100.0f, 100.0f)
            : 0.0f;

        staged.push_back(mv);
    }
    return staged;
}

std::vector<StagedMaterialBatch> PmxViewportItem::partitionMaterialBatches(
    const PmxDocument& doc,
    std::vector<uint32_t>& outStagedIndices)
{
    outStagedIndices.clear();
    std::vector<StagedMaterialBatch> batches;

    if (doc.materials.empty()) {
        outStagedIndices = filterValidTriangleIndices(doc.faceIndices, doc.vertices.size());
        if (!outStagedIndices.empty()) {
            StagedMaterialBatch b;
            b.indexOffset = 0;
            b.indexCount = static_cast<uint32_t>(outStagedIndices.size());
            b.doubleSided = true;
            b.hasToon = false;
            b.hasEdge = false;
            batches.push_back(b);
        }
        return batches;
    }

    QFileInfo modelInfo(doc.sourceFilePath);
    QDir modelDir = doc.sourceFilePath.isEmpty() ? QDir::current() : modelInfo.dir();

    auto resolveTexturePath = [&](int32_t texIdx) -> QString {
        if (texIdx < 0 || static_cast<size_t>(texIdx) >= doc.textures.size()) {
            return QString();
        }
        QString raw = doc.textures[static_cast<size_t>(texIdx)].trimmed();
        if (raw.isEmpty()) return QString();
        QString clean = raw;
        clean.replace(QLatin1Char('\\'), QLatin1Char('/'));
        if (QFileInfo(clean).isRelative()) {
            return modelDir.absoluteFilePath(clean);
        }
        return clean;
    };

    size_t rawIndexOffset = 0;
    const size_t totalRawIndices = doc.faceIndices.size();
    const size_t totalVertices = doc.vertices.size();

    for (size_t matIdx = 0; matIdx < doc.materials.size(); ++matIdx) {
        const auto& mat = doc.materials[matIdx];
        int32_t reqCount = mat.faceVertexCount;
        if (reqCount < 0) reqCount = 0;

        size_t availableCount = 0;
        if (rawIndexOffset < totalRawIndices) {
            availableCount = std::min(static_cast<size_t>(reqCount), totalRawIndices - rawIndexOffset);
        }

        std::vector<int32_t> matRawIndices;
        if (availableCount >= 3) {
            matRawIndices.assign(
                doc.faceIndices.begin() + rawIndexOffset,
                doc.faceIndices.begin() + rawIndexOffset + availableCount
            );
        }

        std::vector<uint32_t> validForMat = filterValidTriangleIndices(matRawIndices, totalVertices);

        StagedMaterialBatch batch;
        batch.materialIndex = static_cast<int>(matIdx);
        batch.indexOffset = static_cast<uint32_t>(outStagedIndices.size());
        batch.indexCount = static_cast<uint32_t>(validForMat.size());
        batch.doubleSided = (mat.drawFlags & 0x01) != 0;
        batch.sphereMode = mat.sphereMode;
        batch.diffuse = mat.diffuse;
        batch.specular = glm::vec4(mat.specular, mat.specularPower);
        batch.ambient = glm::vec4(mat.ambient, 1.0f);
        batch.edgeColor = mat.edgeColor;
        batch.edgeSize = mat.edgeSize;
        batch.hasEdge = ((mat.drawFlags & 0x10) != 0)
            && std::isfinite(mat.edgeSize) && (mat.edgeSize > 0.0f)
            && std::isfinite(mat.edgeColor.a) && (mat.edgeColor.a > 0.0f);

        // Diffuse texture
        batch.diffuseTexturePath = resolveTexturePath(mat.textureIndex);

        // Sphere texture
        batch.sphereTexturePath = resolveTexturePath(mat.sphereTextureIndex);

        // Toon texture
        if (mat.sharedToonFlag != 0) {
            batch.sharedToonIndex = mat.toonIndex;
            batch.hasToon = (mat.toonIndex >= 0);
        } else {
            batch.sharedToonIndex = -1;
            batch.toonTexturePath = resolveTexturePath(mat.toonIndex);
            batch.hasToon = (mat.toonIndex >= 0 && !batch.toonTexturePath.isEmpty());
        }

        outStagedIndices.insert(outStagedIndices.end(), validForMat.begin(), validForMat.end());
        batches.push_back(batch);

        rawIndexOffset += availableCount;
    }

    return batches;
}

void PmxViewportItem::setController(pmx::PmxDocumentController* controller) {
    if (m_controller == controller) return;

    if (m_controller) {
        disconnect(m_controller, &pmx::PmxDocumentController::documentLoaded, this, &PmxViewportItem::onDocumentChanged);
        disconnect(m_controller, &pmx::PmxDocumentController::documentSaved, this, &PmxViewportItem::onDocumentChanged);
        disconnect(m_controller, &pmx::PmxDocumentController::documentReset, this, &PmxViewportItem::onDocumentChanged);
        if (m_controller->undoStack()) {
            disconnect(m_controller->undoStack(), &QUndoStack::indexChanged, this, &PmxViewportItem::onDocumentChanged);
        }
        if (m_controller->selectionManager()) {
            disconnect(m_controller->selectionManager(), &pmx::PmxSelectionManager::anySelectionChanged, this, &PmxViewportItem::updateGizmoPosition);
        }
    }

    m_controller = controller;

    if (m_controller) {
        connect(m_controller, &pmx::PmxDocumentController::documentLoaded, this, &PmxViewportItem::onDocumentChanged);
        connect(m_controller, &pmx::PmxDocumentController::documentSaved, this, &PmxViewportItem::onDocumentChanged);
        connect(m_controller, &pmx::PmxDocumentController::documentReset, this, &PmxViewportItem::onDocumentChanged);
        if (m_controller->undoStack()) {
            connect(m_controller->undoStack(), &QUndoStack::indexChanged, this, &PmxViewportItem::onDocumentChanged);
        }
        if (m_controller->selectionManager()) {
            connect(m_controller->selectionManager(), &pmx::PmxSelectionManager::anySelectionChanged, this, &PmxViewportItem::updateGizmoPosition);
        }
    }

    onDocumentChanged();
    emit controllerChanged();
}

void PmxViewportItem::onDocumentChanged() {
    if (m_controller) {
        m_runtimeScene.loadDocument(m_controller->document());
        m_runtimeScene.setPhysicsEnabled(m_physicsEnabled);
        m_animationPlayer.applyToScene(m_runtimeScene);
        m_runtimeScene.evaluate(0.0f);
    }
    m_runtimeRevision++;
    m_modelRevision++;
    emit modelRevisionChanged();
    updateGizmoPosition();
    update();
}

bool PmxViewportItem::isPlaying() const {
    return m_animationPlayer.isPlaying();
}

float PmxViewportItem::currentFrame() const {
    return m_animationPlayer.currentFrame();
}

void PmxViewportItem::setCurrentFrame(float frame) {
    if (std::isnan(frame) || std::isinf(frame) || frame < 0.0f) {
        frame = 0.0f;
    }
    m_animationPlayer.setFrame(frame);
    m_animationPlayer.applyToScene(m_runtimeScene);
    m_runtimeScene.evaluate(0.0f);
    m_runtimeRevision++;
    emit currentFrameChanged();
    update();
}

int PmxViewportItem::maxFrame() const {
    return static_cast<int>(m_animationPlayer.maxFrame());
}

bool PmxViewportItem::loopPlayback() const {
    return m_animationPlayer.isLoop();
}

void PmxViewportItem::setLoopPlayback(bool loop) {
    if (m_animationPlayer.isLoop() != loop) {
        m_animationPlayer.setLoop(loop);
        emit loopPlaybackChanged();
    }
}

void PmxViewportItem::setPhysicsEnabled(bool enabled) {
    if (m_physicsEnabled != enabled) {
        m_physicsEnabled = enabled;
        m_runtimeScene.setPhysicsEnabled(enabled);
        emit physicsEnabledChanged();
        if (m_transformViewMode) {
            m_runtimeScene.evaluate(0.0f);
            m_runtimeRevision++;
            update();
        }
    }
}

void PmxViewportItem::setTransformViewMode(bool enabled) {
    if (m_transformViewMode != enabled) {
        m_transformViewMode = enabled;
        emit transformViewModeChanged();
        if (m_transformViewMode) {
            if (m_controller && m_runtimeScene.boneCount() == 0 && !m_controller->document().bones.empty()) {
                m_runtimeScene.loadDocument(m_controller->document());
                m_runtimeScene.setPhysicsEnabled(m_physicsEnabled);
            }
            m_animationPlayer.applyToScene(m_runtimeScene);
            m_runtimeScene.evaluate(0.0f);
            m_runtimeRevision++;
        } else {
            if (m_animationPlayer.isPlaying()) {
                pause();
            }
            updateGizmoPosition();
        }
        update();
    }
}

bool PmxViewportItem::loadVmdMotion(const QString& filePath) {
    QString cleanPath = filePath;
    if (cleanPath.startsWith(QLatin1String("file://"))) {
        cleanPath = QUrl(filePath).toLocalFile();
    }
    if (cleanPath.isEmpty()) {
        return false;
    }
    QString error;
    auto motionOpt = VmdImporter::loadFromFile(cleanPath, &error);
    if (!motionOpt.has_value()) {
        return false;
    }
    m_animationPlayer.loadMotion(*motionOpt);
    emit maxFrameChanged();
    emit currentFrameChanged();
    emit playingChanged();

    m_animationPlayer.applyToScene(m_runtimeScene);
    m_runtimeScene.evaluate(0.0f);
    m_runtimeRevision++;
    update();
    return true;
}

bool PmxViewportItem::loadVpdPose(const QString& filePath) {
    QString cleanPath = filePath;
    if (cleanPath.startsWith(QLatin1String("file://"))) {
        cleanPath = QUrl(filePath).toLocalFile();
    }
    if (cleanPath.isEmpty()) {
        return false;
    }
    QString error;
    auto poseOpt = VpdImporter::loadFromFile(cleanPath, &error);
    if (!poseOpt.has_value()) {
        return false;
    }
    m_animationPlayer.loadPose(*poseOpt);
    emit maxFrameChanged();
    emit currentFrameChanged();
    emit playingChanged();

    m_animationPlayer.applyToScene(m_runtimeScene);
    m_runtimeScene.evaluate(0.0f);
    m_runtimeRevision++;
    update();
    return true;
}

void PmxViewportItem::play() {
    m_animationPlayer.play();
    m_elapsedTimer.restart();
    if (m_playbackTimer) {
        m_playbackTimer->start();
    }
    emit playingChanged();
    update();
}

void PmxViewportItem::pause() {
    m_animationPlayer.pause();
    if (m_playbackTimer) {
        m_playbackTimer->stop();
    }
    emit playingChanged();
}

void PmxViewportItem::stop() {
    m_animationPlayer.stop();
    if (m_playbackTimer) {
        m_playbackTimer->stop();
    }
    m_animationPlayer.applyToScene(m_runtimeScene);
    m_runtimeScene.evaluate(0.0f);
    m_runtimeRevision++;
    emit playingChanged();
    emit currentFrameChanged();
    update();
}

void PmxViewportItem::resetPhysics() {
    m_runtimeScene.resetPhysics();
    m_runtimeScene.evaluate(0.0f);
    m_runtimeRevision++;
    update();
}

void PmxViewportItem::resetPose() {
    m_runtimeScene.resetTransforms();
    for (int i = 0; i < m_runtimeScene.morphCount(); ++i) {
        m_runtimeScene.setMorphWeight(i, 0.0f);
    }
    m_runtimeScene.resetPhysics();
    m_runtimeScene.evaluate(0.0f);
    m_runtimeRevision++;
    update();
}

void PmxViewportItem::onPlaybackTick() {
    if (!m_animationPlayer.isPlaying()) {
        if (m_playbackTimer) m_playbackTimer->stop();
        emit playingChanged();
        return;
    }
    qint64 elapsedNs = m_elapsedTimer.restart();
    float deltaTime = static_cast<float>(elapsedNs) * 1e-9f;
    if (deltaTime <= 0.0f || deltaTime > 0.1f) {
        deltaTime = 1.0f / 60.0f;
    }

    m_animationPlayer.update(deltaTime);
    m_animationPlayer.applyToScene(m_runtimeScene);
    m_runtimeScene.evaluate(deltaTime);
    m_runtimeRevision++;

    emit currentFrameChanged();

    if (!m_animationPlayer.isPlaying()) {
        if (m_playbackTimer) m_playbackTimer->stop();
        emit playingChanged();
    }

    update();
}

void PmxViewportItem::setShowGrid(bool show) {
    if (m_showGrid != show) {
        m_showGrid = show;
        emit showGridChanged();
        update();
    }
}

void PmxViewportItem::setShowBones(bool show) {
    if (m_showBones != show) {
        m_showBones = show;
        emit showBonesChanged();
        update();
    }
}

void PmxViewportItem::setShowRigidBodies(bool show) {
    if (m_showRigidBodies != show) {
        m_showRigidBodies = show;
        emit showRigidBodiesChanged();
        update();
    }
}

void PmxViewportItem::setShowJoints(bool show) {
    if (m_showJoints != show) {
        m_showJoints = show;
        emit showJointsChanged();
        update();
    }
}

void PmxViewportItem::setWireframe(bool wireframe) {
    if (m_wireframe != wireframe) {
        m_wireframe = wireframe;
        emit wireframeChanged();
        update();
    }
}

void PmxViewportItem::setShowEdgeOutlines(bool show) {
    if (m_showEdgeOutlines != show) {
        m_showEdgeOutlines = show;
        emit showEdgeOutlinesChanged();
        update();
    }
}

void PmxViewportItem::setGizmoMode(GizmoMode mode) {
    if (m_gizmo.mode() != mode) {
        m_gizmo.setMode(mode);
        emit gizmoModeChanged();
        updateGizmoPosition();
        update();
    }
}

void PmxViewportItem::setCoordSpace(CoordSpace space) {
    if (m_gizmo.coordSpace() != space) {
        m_gizmo.setCoordSpace(space);
        emit coordSpaceChanged();
        updateGizmoPosition();
        update();
    }
}

void PmxViewportItem::setSymmetricEditing(bool enabled) {
    if (m_gizmo.symmetricEditing() != enabled) {
        m_gizmo.setSymmetricEditing(enabled);
        emit symmetricEditingChanged();
        update();
    }
}

void PmxViewportItem::setSnapEnabled(bool enabled) {
    if (m_gizmo.snapEnabled() != enabled) {
        m_gizmo.setSnapEnabled(enabled);
        emit snapEnabledChanged();
        update();
    }
}

void PmxViewportItem::setViewportMode(int mode) {
    if (m_viewportMode != mode) {
        m_viewportMode = mode;
        if (m_viewportMode != 2) {
            m_brushCursorLines.clear();
        }
        emit viewportModeChanged();
        update();
    }
}

void PmxViewportItem::setBrushRadius(float radius) {
    if (std::abs(m_brushRadius - radius) > 1e-4f && std::isfinite(radius)) {
        m_brushRadius = std::max(0.001f, radius);
        emit brushRadiusChanged();
        if (m_hasBrushHit && m_viewportMode == 2) {
            m_brushCursorLines = PmxWeightPaintBrush::buildBrushCursor(m_brushHitPos, m_brushHitNormal, m_brushRadius, 32);
        }
        update();
    }
}

void PmxViewportItem::setBrushStrength(float strength) {
    if (std::abs(m_brushStrength - strength) > 1e-4f && std::isfinite(strength)) {
        m_brushStrength = std::clamp(strength, 0.0f, 1.0f);
        emit brushStrengthChanged();
    }
}

void PmxViewportItem::setBrushMode(int mode) {
    if (m_brushMode != mode) {
        m_brushMode = mode;
        emit brushModeChanged();
    }
}

void PmxViewportItem::setBrushFalloff(int falloff) {
    if (m_brushFalloff != falloff) {
        m_brushFalloff = falloff;
        emit brushFalloffChanged();
    }
}

static bool rayIntersectsTriangle(
    const glm::vec3& rayOrigin, const glm::vec3& rayDir,
    const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
    float& outT, glm::vec3& outHitPoint, glm::vec3& outHitNormal)
{
    const float EPSILON = 1e-7f;
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDir, edge2);
    float a = glm::dot(edge1, h);

    if (std::abs(a) < EPSILON) {
        return false;
    }

    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    float u = f * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f) {
        return false;
    }

    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(rayDir, q);

    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }

    float t = f * glm::dot(edge2, q);
    if (t > EPSILON && std::isfinite(t)) {
        outT = t;
        outHitPoint = rayOrigin + rayDir * t;
        glm::vec3 n = glm::cross(edge1, edge2);
        if (glm::length(n) > EPSILON) {
            outHitNormal = glm::normalize(n);
            if (a < 0.0f) {
                outHitNormal = -outHitNormal;
            }
        } else {
            outHitNormal = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        return true;
    }

    return false;
}

bool PmxViewportItem::raycastMesh(const Ray& ray, glm::vec3& outHitPoint, glm::vec3& outHitNormal) const {
    if (!m_controller) return false;
    const auto& doc = m_controller->document();
    if (doc.vertices.empty() || doc.faceIndices.size() < 3) return false;

    float closestT = std::numeric_limits<float>::infinity();
    glm::vec3 bestHit(0.0f);
    glm::vec3 bestNormal(0.0f, 1.0f, 0.0f);
    bool found = false;

    for (size_t i = 0; i + 2 < doc.faceIndices.size(); i += 3) {
        int32_t i0 = doc.faceIndices[i];
        int32_t i1 = doc.faceIndices[i + 1];
        int32_t i2 = doc.faceIndices[i + 2];
        if (i0 < 0 || static_cast<size_t>(i0) >= doc.vertices.size() ||
            i1 < 0 || static_cast<size_t>(i1) >= doc.vertices.size() ||
            i2 < 0 || static_cast<size_t>(i2) >= doc.vertices.size()) {
            continue;
        }

        glm::vec3 v0 = PmxCamera::toRightHanded(doc.vertices[static_cast<size_t>(i0)].position);
        glm::vec3 v1 = PmxCamera::toRightHanded(doc.vertices[static_cast<size_t>(i1)].position);
        glm::vec3 v2 = PmxCamera::toRightHanded(doc.vertices[static_cast<size_t>(i2)].position);

        float t = 0.0f;
        glm::vec3 hit(0.0f);
        glm::vec3 norm(0.0f);
        if (rayIntersectsTriangle(ray.origin, ray.direction, v0, v1, v2, t, hit, norm)) {
            if (t < closestT) {
                closestT = t;
                bestHit = hit;
                bestNormal = norm;
                found = true;
            }
        }
    }

    if (found) {
        outHitPoint = bestHit;
        outHitNormal = bestNormal;
        return true;
    }
    return false;
}

void PmxViewportItem::updateGizmoPosition() {
    if (!m_controller) return;

    const auto& doc = m_controller->document();
    const auto selectedBones = m_controller->selectionManager()->selectedBones();
    const auto selectedVertices = m_controller->selectionManager()->selectedVertices();

    if (!selectedBones.empty()) {
        glm::vec3 sum(0.0f);
        int validCount = 0;
        for (int bIdx : selectedBones) {
            if (bIdx >= 0 && static_cast<size_t>(bIdx) < doc.bones.size()) {
                sum += doc.bones[bIdx].position;
                validCount++;
            }
        }
        if (validCount > 0) {
            glm::vec3 centroidLH = sum / static_cast<float>(validCount);
            m_gizmo.setPosition(PmxCamera::toRightHanded(centroidLH));
            m_gizmo.setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        }
    } else if (!selectedVertices.empty()) {
        glm::vec3 sum(0.0f);
        int validCount = 0;
        for (int vIdx : selectedVertices) {
            if (vIdx >= 0 && static_cast<size_t>(vIdx) < doc.vertices.size()) {
                sum += doc.vertices[vIdx].position;
                validCount++;
            }
        }
        if (validCount > 0) {
            glm::vec3 centroidLH = sum / static_cast<float>(validCount);
            m_gizmo.setPosition(PmxCamera::toRightHanded(centroidLH));
            m_gizmo.setRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        }
    }

    update();
}

void PmxViewportItem::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->position();

    if (!m_transformViewMode && event->button() == Qt::LeftButton && m_viewportMode == 2 && !(event->modifiers() & Qt::AltModifier) && m_controller) {
        float w = static_cast<float>(width());
        float h = static_cast<float>(height());
        Ray ray = m_camera->screenPointToRay(event->position(), w, h);
        if (raycastMesh(ray, m_brushHitPos, m_brushHitNormal)) {
            m_hasBrushHit = true;
            m_isPaintingWeights = true;
            m_paintStrokeDeltas.clear();
            m_paintSessionId++;

            m_brushCursorLines = PmxWeightPaintBrush::buildBrushCursor(m_brushHitPos, m_brushHitNormal, m_brushRadius, 32);

            int activeBone = -1;
            if (m_controller->selectionManager()) {
                auto sel = m_controller->selectionManager()->selectedBones();
                if (!sel.isEmpty()) activeBone = sel.first();
            }

            if (activeBone >= 0) {
                auto& doc = m_controller->documentInternal();
                for (size_t i = 0; i < doc.vertices.size(); ++i) {
                    glm::vec3 vRH = PmxCamera::toRightHanded(doc.vertices[i].position);
                    float dist = glm::distance(vRH, m_brushHitPos);
                    if (dist <= m_brushRadius) {
                        float factor = PmxWeightPaintBrush::computeFalloff(dist, m_brushRadius, static_cast<BrushFalloff>(m_brushFalloff));
                        if (factor > 0.0f) {
                            int vIdx = static_cast<int>(i);
                            if (m_paintStrokeDeltas.find(vIdx) == m_paintStrokeDeltas.end()) {
                                VertexWeightDelta d;
                                d.vertexIndex = vIdx;
                                d.oldWeightType = doc.vertices[i].weightType;
                                d.oldBoneIndices = doc.vertices[i].boneIndices;
                                d.oldBoneWeights = doc.vertices[i].boneWeights;
                                d.oldSdefC = doc.vertices[i].sdefC;
                                d.oldSdefR0 = doc.vertices[i].sdefR0;
                                d.oldSdefR1 = doc.vertices[i].sdefR1;
                                m_paintStrokeDeltas[vIdx] = d;
                            }
                            PmxWeightPaintBrush::applyBrushToVertex(doc.vertices[i], activeBone, factor, m_brushStrength, static_cast<BrushMode>(m_brushMode));
                            auto& d = m_paintStrokeDeltas[vIdx];
                            d.newWeightType = doc.vertices[i].weightType;
                            d.newBoneIndices = doc.vertices[i].boneIndices;
                            d.newBoneWeights = doc.vertices[i].boneWeights;
                            d.newSdefC = doc.vertices[i].sdefC;
                            d.newSdefR0 = doc.vertices[i].sdefR0;
                            d.newSdefR1 = doc.vertices[i].sdefR1;
                        }
                    }
                }
                m_modelRevision++;
                emit modelRevisionChanged();
            }
            update();
            event->accept();
            return;
        }
    }

    if (!m_transformViewMode && event->button() == Qt::LeftButton && m_gizmo.mode() != GizmoMode::None && m_controller) {
        float w = static_cast<float>(width());
        float h = static_cast<float>(height());
        Ray ray = m_camera->screenPointToRay(event->position(), w, h);
        float gizmoScale = m_gizmo.computeDisplayScale(m_camera->eyePosition());
        GizmoAxis hit = m_gizmo.hitTest(ray.origin, ray.direction, gizmoScale);
        if (hit != GizmoAxis::None) {
            m_isGizmoDragging = true;
            m_gizmo.setActiveAxis(hit);
            m_dragStartRay = ray;
            m_dragLastRay = ray;
            m_dragStartGizmoPos = m_gizmo.position();
            m_dragSessionId++;

            const auto& doc = m_controller->document();
            const auto selBones = m_controller->selectionManager()->selectedBones();
            const auto selVerts = m_controller->selectionManager()->selectedVertices();

            m_dragSelectedBoneIndices.clear();
            m_dragOriginalBonePositions.clear();
            m_dragSelectedVertexIndices.clear();
            m_dragOriginalVertexPositions.clear();
            m_dragOriginalVertexNormals.clear();

            if (!selBones.empty()) {
                for (int bIdx : selBones) {
                    if (bIdx >= 0 && static_cast<size_t>(bIdx) < doc.bones.size()) {
                        m_dragSelectedBoneIndices.push_back(bIdx);
                        m_dragOriginalBonePositions.push_back(doc.bones[bIdx].position);
                    }
                }
            } else if (!selVerts.empty()) {
                for (int vIdx : selVerts) {
                    if (vIdx >= 0 && static_cast<size_t>(vIdx) < doc.vertices.size()) {
                        m_dragSelectedVertexIndices.push_back(vIdx);
                        m_dragOriginalVertexPositions.push_back(doc.vertices[vIdx].position);
                        m_dragOriginalVertexNormals.push_back(doc.vertices[vIdx].normal);
                    }
                }
            }
            event->accept();
            return;
        }
    }

    if (event->button() == Qt::MiddleButton && (event->modifiers() & Qt::ShiftModifier)) {
        m_isPanning = true;
    } else if (event->button() == Qt::MiddleButton ||
              (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        m_isOrbiting = true;
    }
    event->accept();
}

void PmxViewportItem::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPaintingWeights && m_controller) {
        float w = static_cast<float>(width());
        float h = static_cast<float>(height());
        Ray ray = m_camera->screenPointToRay(event->position(), w, h);
        if (raycastMesh(ray, m_brushHitPos, m_brushHitNormal)) {
            m_hasBrushHit = true;
            m_brushCursorLines = PmxWeightPaintBrush::buildBrushCursor(m_brushHitPos, m_brushHitNormal, m_brushRadius, 32);

            int activeBone = -1;
            if (m_controller->selectionManager()) {
                auto sel = m_controller->selectionManager()->selectedBones();
                if (!sel.isEmpty()) activeBone = sel.first();
            }

            if (activeBone >= 0) {
                auto& doc = m_controller->documentInternal();
                for (size_t i = 0; i < doc.vertices.size(); ++i) {
                    glm::vec3 vRH = PmxCamera::toRightHanded(doc.vertices[i].position);
                    float dist = glm::distance(vRH, m_brushHitPos);
                    if (dist <= m_brushRadius) {
                        float factor = PmxWeightPaintBrush::computeFalloff(dist, m_brushRadius, static_cast<BrushFalloff>(m_brushFalloff));
                        if (factor > 0.0f) {
                            int vIdx = static_cast<int>(i);
                            if (m_paintStrokeDeltas.find(vIdx) == m_paintStrokeDeltas.end()) {
                                VertexWeightDelta d;
                                d.vertexIndex = vIdx;
                                d.oldWeightType = doc.vertices[i].weightType;
                                d.oldBoneIndices = doc.vertices[i].boneIndices;
                                d.oldBoneWeights = doc.vertices[i].boneWeights;
                                d.oldSdefC = doc.vertices[i].sdefC;
                                d.oldSdefR0 = doc.vertices[i].sdefR0;
                                d.oldSdefR1 = doc.vertices[i].sdefR1;
                                m_paintStrokeDeltas[vIdx] = d;
                            }
                            PmxWeightPaintBrush::applyBrushToVertex(doc.vertices[i], activeBone, factor, m_brushStrength, static_cast<BrushMode>(m_brushMode));
                            auto& d = m_paintStrokeDeltas[vIdx];
                            d.newWeightType = doc.vertices[i].weightType;
                            d.newBoneIndices = doc.vertices[i].boneIndices;
                            d.newBoneWeights = doc.vertices[i].boneWeights;
                            d.newSdefC = doc.vertices[i].sdefC;
                            d.newSdefR0 = doc.vertices[i].sdefR0;
                            d.newSdefR1 = doc.vertices[i].sdefR1;
                        }
                    }
                }
                m_modelRevision++;
                emit modelRevisionChanged();
            }
            update();
            event->accept();
            return;
        }
    }

    if (m_isGizmoDragging && m_controller) {
        float w = static_cast<float>(width());
        float h = static_cast<float>(height());
        Ray ray = m_camera->screenPointToRay(event->position(), w, h);
        float gizmoScale = m_gizmo.computeDisplayScale(m_camera->eyePosition());
        bool snap = m_gizmo.snapEnabled();
        int cmdId = PmxVertexTransformCommand::CommandId + (m_dragSessionId & 0xFFFF);
        const auto& doc = m_controller->document();

        glm::vec3 deltaRH(0.0f);
        glm::quat rotRH(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale(1.0f);

        if (m_gizmo.mode() == GizmoMode::Translate) {
            deltaRH = m_gizmo.computeTranslationDelta(
                m_gizmo.activeAxis(),
                ray.origin, ray.direction,
                m_dragStartRay.origin, m_dragStartRay.direction,
                gizmoScale, snap
            );
        } else if (m_gizmo.mode() == GizmoMode::Rotate) {
            rotRH = m_gizmo.computeRotationDelta(
                m_gizmo.activeAxis(),
                ray.origin, ray.direction,
                m_dragStartRay.origin, m_dragStartRay.direction,
                gizmoScale, snap
            );
        } else if (m_gizmo.mode() == GizmoMode::Scale) {
            scale = m_gizmo.computeScaleDelta(
                m_gizmo.activeAxis(),
                ray.origin, ray.direction,
                m_dragStartRay.origin, m_dragStartRay.direction,
                gizmoScale, snap
            );
        }

        if (!m_dragSelectedBoneIndices.empty()) {
            std::vector<BoneTransformDelta> deltas;
            std::vector<int> handledBones;

            for (size_t i = 0; i < m_dragSelectedBoneIndices.size(); ++i) {
                int bIdx = m_dragSelectedBoneIndices[i];
                glm::vec3 origLH = m_dragOriginalBonePositions[i];
                glm::vec3 origRH = PmxCamera::toRightHanded(origLH);
                glm::vec3 newRH = origRH;

                if (m_gizmo.mode() == GizmoMode::Translate) {
                    newRH = origRH + deltaRH;
                } else if (m_gizmo.mode() == GizmoMode::Rotate) {
                    newRH = m_dragStartGizmoPos + rotRH * (origRH - m_dragStartGizmoPos);
                } else if (m_gizmo.mode() == GizmoMode::Scale) {
                    newRH = m_dragStartGizmoPos + scale * (origRH - m_dragStartGizmoPos);
                }

                glm::vec3 newLH = PmxCamera::toLeftHanded(newRH);
                if (m_gizmo.symmetricEditing() && std::abs(origLH.x) < 0.005f) {
                    newLH.x = origLH.x;
                }

                BoneTransformDelta d;
                d.boneIndex = bIdx;
                d.oldPos = origLH;
                d.newPos = newLH;
                deltas.push_back(d);
                handledBones.push_back(bIdx);

                if (m_gizmo.symmetricEditing()) {
                    int symIdx = PmxGizmo::findSymmetricBone(doc, bIdx);
                    if (symIdx >= 0 && symIdx != bIdx &&
                        std::find(m_dragSelectedBoneIndices.begin(), m_dragSelectedBoneIndices.end(), symIdx) == m_dragSelectedBoneIndices.end() &&
                        std::find(handledBones.begin(), handledBones.end(), symIdx) == handledBones.end()) {
                        glm::vec3 symOrigLH = doc.bones[symIdx].position;
                        glm::vec3 deltaLH = newLH - origLH;
                        glm::vec3 symDeltaLH = PmxGizmo::mirrorDelta(deltaLH);

                        BoneTransformDelta symD;
                        symD.boneIndex = symIdx;
                        symD.oldPos = symOrigLH;
                        symD.newPos = symOrigLH + symDeltaLH;
                        deltas.push_back(symD);
                        handledBones.push_back(symIdx);
                    }
                }
            }

            if (!deltas.empty()) {
                auto* cmd = new PmxBoneTransformCommand(m_controller, deltas, cmdId);
                m_controller->pushCommand(cmd);
                if (m_gizmo.mode() == GizmoMode::Translate) {
                    m_gizmo.setPosition(m_dragStartGizmoPos + deltaRH);
                }
                m_modelRevision++;
                emit modelRevisionChanged();
                update();
            }
        } else if (!m_dragSelectedVertexIndices.empty()) {
            std::vector<VertexTransformDelta> deltas;
            std::vector<int> handledVertices;

            for (size_t i = 0; i < m_dragSelectedVertexIndices.size(); ++i) {
                int vIdx = m_dragSelectedVertexIndices[i];
                glm::vec3 origLH = m_dragOriginalVertexPositions[i];
                glm::vec3 origNormLH = m_dragOriginalVertexNormals[i];
                glm::vec3 origRH = PmxCamera::toRightHanded(origLH);
                glm::vec3 origNormRH = PmxCamera::toRightHanded(origNormLH);

                glm::vec3 newRH = origRH;
                glm::vec3 newNormRH = origNormRH;

                if (m_gizmo.mode() == GizmoMode::Translate) {
                    newRH = origRH + deltaRH;
                } else if (m_gizmo.mode() == GizmoMode::Rotate) {
                    newRH = m_dragStartGizmoPos + rotRH * (origRH - m_dragStartGizmoPos);
                    newNormRH = rotRH * origNormRH;
                } else if (m_gizmo.mode() == GizmoMode::Scale) {
                    newRH = m_dragStartGizmoPos + scale * (origRH - m_dragStartGizmoPos);
                }

                glm::vec3 newLH = PmxCamera::toLeftHanded(newRH);
                glm::vec3 newNormLH = PmxCamera::toLeftHanded(newNormRH);

                if (m_gizmo.symmetricEditing() && std::abs(origLH.x) < 0.005f) {
                    newLH.x = origLH.x;
                    newNormLH.x = 0.0f;
                }

                VertexTransformDelta d;
                d.vertexIndex = vIdx;
                d.oldPos = origLH;
                d.newPos = newLH;
                d.oldNormal = origNormLH;
                d.newNormal = newNormLH;
                deltas.push_back(d);
                handledVertices.push_back(vIdx);

                if (m_gizmo.symmetricEditing()) {
                    int symIdx = PmxGizmo::findSymmetricVertex(doc, vIdx);
                    if (symIdx >= 0 && symIdx != vIdx &&
                        std::find(m_dragSelectedVertexIndices.begin(), m_dragSelectedVertexIndices.end(), symIdx) == m_dragSelectedVertexIndices.end() &&
                        std::find(handledVertices.begin(), handledVertices.end(), symIdx) == handledVertices.end()) {
                        glm::vec3 symOrigLH = doc.vertices[symIdx].position;
                        glm::vec3 symOrigNormLH = doc.vertices[symIdx].normal;
                        glm::vec3 deltaLH = newLH - origLH;
                        glm::vec3 symDeltaLH = PmxGizmo::mirrorDelta(deltaLH);

                        VertexTransformDelta symD;
                        symD.vertexIndex = symIdx;
                        symD.oldPos = symOrigLH;
                        symD.newPos = symOrigLH + symDeltaLH;
                        symD.oldNormal = symOrigNormLH;
                        symD.newNormal = glm::vec3(-newNormLH.x, newNormLH.y, newNormLH.z);
                        deltas.push_back(symD);
                        handledVertices.push_back(symIdx);
                    }
                }
            }

            if (!deltas.empty()) {
                auto* cmd = new PmxVertexTransformCommand(m_controller, deltas, cmdId);
                m_controller->pushCommand(cmd);
                if (m_gizmo.mode() == GizmoMode::Translate) {
                    m_gizmo.setPosition(m_dragStartGizmoPos + deltaRH);
                }
                m_modelRevision++;
                emit modelRevisionChanged();
                update();
            }
        }
        event->accept();
        return;
    }

    QPointF currentPos = event->position();
    float dx = static_cast<float>(currentPos.x() - m_lastMousePos.x());
    float dy = static_cast<float>(currentPos.y() - m_lastMousePos.y());
    m_lastMousePos = currentPos;

    if (m_isPanning) {
        m_camera->pan(dx, -dy);
        update();
    } else if (m_isOrbiting) {
        m_camera->orbit(dx * 0.5f, -dy * 0.5f);
        update();
    }
    event->accept();
}

void PmxViewportItem::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isPaintingWeights) {
        m_isPaintingWeights = false;
        if (!m_paintStrokeDeltas.empty() && m_controller) {
            std::vector<VertexWeightDelta> deltas;
            deltas.reserve(m_paintStrokeDeltas.size());
            for (const auto& pair : m_paintStrokeDeltas) {
                deltas.push_back(pair.second);
            }
            int cmdId = PmxVertexWeightCommand::CommandId + (m_paintSessionId & 0xFFFF);
            auto* cmd = new PmxVertexWeightCommand(m_controller, deltas, cmdId);
            m_controller->pushCommand(cmd);
            m_paintStrokeDeltas.clear();
            m_modelRevision++;
            emit modelRevisionChanged();
        }
        update();
        event->accept();
        return;
    }

    if (m_isGizmoDragging) {
        m_isGizmoDragging = false;
        m_gizmo.setActiveAxis(GizmoAxis::None);
        updateGizmoPosition();
        update();
        event->accept();
        return;
    }
    m_isPanning = false;
    m_isOrbiting = false;
    event->accept();
}

void PmxViewportItem::hoverMoveEvent(QHoverEvent* event) {
    if (m_viewportMode == 2) {
        float w = static_cast<float>(width());
        float h = static_cast<float>(height());
        Ray ray = m_camera->screenPointToRay(event->position(), w, h);
        if (raycastMesh(ray, m_brushHitPos, m_brushHitNormal)) {
            m_hasBrushHit = true;
            m_brushCursorLines = PmxWeightPaintBrush::buildBrushCursor(m_brushHitPos, m_brushHitNormal, m_brushRadius, 32);
        } else {
            m_hasBrushHit = false;
            m_brushCursorLines.clear();
        }
        update();
        event->accept();
        return;
    }

    if (m_gizmo.mode() != GizmoMode::None && !m_isGizmoDragging) {
        float w = static_cast<float>(width());
        float h = static_cast<float>(height());
        Ray ray = m_camera->screenPointToRay(event->position(), w, h);
        float gizmoScale = m_gizmo.computeDisplayScale(m_camera->eyePosition());
        GizmoAxis hit = m_gizmo.hitTest(ray.origin, ray.direction, gizmoScale);
        if (hit != m_gizmo.hoveredAxis()) {
            m_gizmo.setHoveredAxis(hit);
            update();
        }
    }
    event->accept();
}

void PmxViewportItem::wheelEvent(QWheelEvent* event) {
    float numDegrees = event->angleDelta().y() / 8.0f;
    float numSteps = numDegrees / 15.0f;
    m_camera->zoom(numSteps);
    update();
    event->accept();
}

} // namespace pmx
