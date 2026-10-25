#include "viewport/PmxViewportItem.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMatrix4x4>
#include <cstring>
#include <algorithm>
#include <cmath>

inline void initShaders() {
    Q_INIT_RESOURCE(pmx_shaders);
}

namespace pmx {

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
    delete m_meshVertexBuffer;
    delete m_meshIndexBuffer;
    delete m_gridVertexBuffer;
    delete m_boneVertexBuffer;

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
            { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, position) },
            { 0, 1, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, normal) },
            { 0, 2, QRhiVertexInputAttribute::Float2, offsetof(MeshVertex, uv) },
            { 0, 3, QRhiVertexInputAttribute::Float, offsetof(MeshVertex, edgeScale) }
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
            { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, position) },
            { 0, 1, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, normal) },
            { 0, 2, QRhiVertexInputAttribute::Float2, offsetof(MeshVertex, uv) },
            { 0, 3, QRhiVertexInputAttribute::Float, offsetof(MeshVertex, edgeScale) }
        });
        m_edgePipeline->setVertexInputLayout(inputLayout);
        m_edgePipeline->setShaderResourceBindings(m_dummyLayoutSrb);
        m_edgePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        m_edgePipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_edgePipeline->setCullMode(QRhiGraphicsPipeline::Front);
        m_edgePipeline->setFrontFace(QRhiGraphicsPipeline::CW);
        m_edgePipeline->setDepthTest(true);
        m_edgePipeline->setDepthWrite(true);
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
    m_wireframe = vp->wireframe();

    if (m_syncedRevision != vp->modelRevision()) {
        m_syncedRevision = vp->modelRevision();
        m_stagedVertices.clear();
        m_stagedIndices.clear();
        m_stagedMaterials.clear();
        m_stagedBoneLines.clear();

        if (vp->controller()) {
            const auto& doc = vp->controller()->document();
            m_stagedVertices = PmxViewportItem::stageVertices(doc);
            m_stagedMaterials = PmxViewportItem::partitionMaterialBatches(doc, m_stagedIndices);

            // Build bone overlay lines
            for (size_t bi = 0; bi < doc.bones.size(); ++bi) {
                const auto& b = doc.bones[bi];
                glm::vec3 bPos = PmxCamera::toRightHanded(b.position);
                if (b.boneFlags & 0x0001) { // TailIsBone
                    if (b.tailBoneIndex >= 0 && static_cast<size_t>(b.tailBoneIndex) < doc.bones.size()) {
                        glm::vec3 tailPos = PmxCamera::toRightHanded(doc.bones[b.tailBoneIndex].position);
                        m_stagedBoneLines.push_back({ bPos, glm::vec4(0.2f, 0.8f, 1.0f, 1.0f) });
                        m_stagedBoneLines.push_back({ tailPos, glm::vec4(0.2f, 0.8f, 1.0f, 1.0f) });
                    }
                } else if (glm::length(b.tailOffset) > 0.001f) {
                    glm::vec3 tailPos = PmxCamera::toRightHanded(b.position + b.tailOffset);
                    m_stagedBoneLines.push_back({ bPos, glm::vec4(0.2f, 0.8f, 1.0f, 1.0f) });
                    m_stagedBoneLines.push_back({ tailPos, glm::vec4(0.2f, 0.8f, 1.0f, 1.0f) });
                }

                // Cross marker at bone position
                float s = 0.2f;
                m_stagedBoneLines.push_back({ bPos - glm::vec3(s,0,0), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f) });
                m_stagedBoneLines.push_back({ bPos + glm::vec3(s,0,0), glm::vec4(1.0f, 0.2f, 0.2f, 1.0f) });
                m_stagedBoneLines.push_back({ bPos - glm::vec3(0,s,0), glm::vec4(0.2f, 1.0f, 0.2f, 1.0f) });
                m_stagedBoneLines.push_back({ bPos + glm::vec3(0,s,0), glm::vec4(0.2f, 1.0f, 0.2f, 1.0f) });
                m_stagedBoneLines.push_back({ bPos - glm::vec3(0,0,s), glm::vec4(0.2f, 0.2f, 1.0f, 1.0f) });
                m_stagedBoneLines.push_back({ bPos + glm::vec3(0,0,s), glm::vec4(0.2f, 0.2f, 1.0f, 1.0f) });
            }
        }

        m_meshDataDirty = true;
        m_materialsDirty = true;
    }
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
        uboData.texTintMul = glm::vec4(1.0f);
        uboData.texTintAdd = glm::vec4(0.0f);
        uboData.sphereTintMul = glm::vec4(1.0f);
        uboData.sphereTintAdd = glm::vec4(0.0f);
        uboData.toonTintMul = glm::vec4(1.0f);
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

    // 2. Mesh Triangles (Partitioned per-material)
    if (m_meshVertexBuffer && m_meshIndexBuffer && m_meshIndexCount > 0) {
        QRhiCommandBuffer::VertexInput vInput(m_meshVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput, m_meshIndexBuffer, 0, QRhiCommandBuffer::IndexUInt32);

        for (const auto& gb : m_gpuBatches) {
            if (gb.staged.indexCount == 0) continue;

            QRhiGraphicsPipeline* pipeline = gb.staged.doubleSided
                ? m_meshPipelineCullNone
                : m_meshPipelineCullBack;
            if (!pipeline) continue;

            cb->setGraphicsPipeline(pipeline);
            cb->setShaderResources(gb.srb);
            cb->drawIndexed(gb.staged.indexCount, 1, gb.staged.indexOffset, 0);
        }

        // 3. Inverted-Hull Edge Outline Pass
        if (m_edgePipeline) {
            for (const auto& gb : m_gpuBatches) {
                if (!gb.staged.hasEdge || gb.staged.indexCount == 0) continue;

                cb->setGraphicsPipeline(m_edgePipeline);
                cb->setShaderResources(gb.srb);
                cb->drawIndexed(gb.staged.indexCount, 1, gb.staged.indexOffset, 0);
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

    connect(m_camera, &PmxCamera::cameraChanged, this, [this]() {
        update();
    });
}

PmxViewportItem::~PmxViewportItem() = default;

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
        mv.normal = PmxCamera::toRightHanded(v.normal);
        mv.uv = v.uv;
        mv.edgeScale = v.edgeScale;
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
    }

    m_controller = controller;

    if (m_controller) {
        connect(m_controller, &pmx::PmxDocumentController::documentLoaded, this, &PmxViewportItem::onDocumentChanged);
        connect(m_controller, &pmx::PmxDocumentController::documentSaved, this, &PmxViewportItem::onDocumentChanged);
        connect(m_controller, &pmx::PmxDocumentController::documentReset, this, &PmxViewportItem::onDocumentChanged);
        if (m_controller->undoStack()) {
            connect(m_controller->undoStack(), &QUndoStack::indexChanged, this, &PmxViewportItem::onDocumentChanged);
        }
    }

    onDocumentChanged();
    emit controllerChanged();
}

void PmxViewportItem::onDocumentChanged() {
    m_modelRevision++;
    emit modelRevisionChanged();
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

void PmxViewportItem::setWireframe(bool wireframe) {
    if (m_wireframe != wireframe) {
        m_wireframe = wireframe;
        emit wireframeChanged();
        update();
    }
}

void PmxViewportItem::mousePressEvent(QMouseEvent* event) {
    m_lastMousePos = event->position();
    if (event->button() == Qt::MiddleButton && (event->modifiers() & Qt::ShiftModifier)) {
        m_isPanning = true;
    } else if (event->button() == Qt::MiddleButton ||
              (event->button() == Qt::LeftButton && (event->modifiers() & Qt::AltModifier))) {
        m_isOrbiting = true;
    }
    event->accept();
}

void PmxViewportItem::mouseMoveEvent(QMouseEvent* event) {
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
    m_isPanning = false;
    m_isOrbiting = false;
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
