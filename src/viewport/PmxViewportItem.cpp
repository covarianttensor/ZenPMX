#include "viewport/PmxViewportItem.h"
#include <QFile>
#include <QDebug>
#include <QMatrix4x4>
#include <cstring>

namespace pmx {

static QShader loadShaderFromResource(const QString& name) {
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

PmxRhiRenderer::PmxRhiRenderer() = default;

PmxRhiRenderer::~PmxRhiRenderer() {
    delete m_meshUbo;
    delete m_lineUbo;
    delete m_meshSrb;
    delete m_lineSrb;
    delete m_meshPipeline;
    delete m_linePipeline;
    delete m_meshVertexBuffer;
    delete m_meshIndexBuffer;
    delete m_gridVertexBuffer;
    delete m_boneVertexBuffer;
}

void PmxRhiRenderer::initialize(QRhiCommandBuffer* cb) {
    if (!m_meshUbo) {
        m_meshUbo = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(FrameAndMaterialUboData));
        m_meshUbo->create();
    }
    if (!m_lineUbo) {
        m_lineUbo = rhi()->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(LineUboData));
        m_lineUbo->create();
    }

    if (!m_meshSrb) {
        m_meshSrb = rhi()->newShaderResourceBindings();
        m_meshSrb->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_meshUbo)
        });
        m_meshSrb->create();
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

    if (!m_meshPipeline) {
        m_meshPipeline = rhi()->newGraphicsPipeline();
        m_meshPipeline->setShaderStages({
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
            { 0, 2, QRhiVertexInputAttribute::Float2, offsetof(MeshVertex, uv) }
        });
        m_meshPipeline->setVertexInputLayout(inputLayout);
        m_meshPipeline->setShaderResourceBindings(m_meshSrb);
        m_meshPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
        m_meshPipeline->setTopology(QRhiGraphicsPipeline::Triangles);
        m_meshPipeline->setCullMode(QRhiGraphicsPipeline::None);
        m_meshPipeline->setDepthTest(true);
        m_meshPipeline->setDepthWrite(true);
        m_meshPipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
        m_meshPipeline->create();
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
        m_stagedBoneLines.clear();

        if (vp->controller()) {
            const auto& doc = vp->controller()->document();
            m_stagedVertices.reserve(doc.vertices.size());
            for (const auto& v : doc.vertices) {
                MeshVertex mv;
                mv.position = PmxCamera::toRightHanded(v.position);
                mv.normal = PmxCamera::toRightHanded(v.normal);
                mv.uv = v.uv;
                m_stagedVertices.push_back(mv);
            }
            m_stagedIndices = PmxViewportItem::filterValidTriangleIndices(doc.faceIndices, doc.vertices.size());

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

    const float* corrData = rhi()->clipSpaceCorrMatrix().constData();
    glm::mat4 corrMat;
    std::memcpy(&corrMat[0][0], corrData, 16 * sizeof(float));
    glm::mat4 mvp = corrMat * m_projMatrix * m_viewMatrix;

    FrameAndMaterialUboData meshData;
    meshData.mvp = mvp;
    meshData.modelView = m_viewMatrix;
    meshData.normalMatrix = glm::transpose(glm::inverse(m_viewMatrix));
    meshData.cameraPos = glm::vec4(m_eyePos, 1.0f);
    meshData.lightDir = glm::normalize(m_viewMatrix * glm::vec4(0.5f, 1.0f, 0.5f, 0.0f));
    u->updateDynamicBuffer(m_meshUbo, 0, sizeof(meshData), &meshData);

    LineUboData lineData;
    lineData.mvp = mvp;
    u->updateDynamicBuffer(m_lineUbo, 0, sizeof(lineData), &lineData);

    const QSize outputSize = renderTarget()->pixelSize();
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
    if (m_meshVertexBuffer && m_meshIndexBuffer && m_meshIndexCount > 0 && m_meshPipeline) {
        cb->setGraphicsPipeline(m_meshPipeline);
        cb->setShaderResources(m_meshSrb);
        QRhiCommandBuffer::VertexInput vInput(m_meshVertexBuffer, 0);
        cb->setVertexInput(0, 1, &vInput, m_meshIndexBuffer, 0, QRhiCommandBuffer::IndexUInt32);
        cb->drawIndexed(m_meshIndexCount);
    }

    // 3. Bone Overlay
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
