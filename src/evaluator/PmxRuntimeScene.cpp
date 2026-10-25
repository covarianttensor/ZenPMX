#include "PmxRuntimeScene.h"
#include "PmxPhysicsEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace pmx {

PmxRuntimeScene::PmxRuntimeScene() = default;
PmxRuntimeScene::~PmxRuntimeScene() = default;
PmxRuntimeScene::PmxRuntimeScene(PmxRuntimeScene&&) noexcept = default;
PmxRuntimeScene& PmxRuntimeScene::operator=(PmxRuntimeScene&&) noexcept = default;

namespace {

const glm::mat4 s_identityMatrix{1.0f};
const RuntimeBone s_dummyBone{};
const EvaluatedMaterial s_dummyMaterial{};
const RuntimeVertex s_dummyRuntimeVertex{};

} // namespace

void PmxRuntimeScene::loadDocument(const PmxDocument& doc) {
    m_bones.clear();
    m_inverseBindMatrices.clear();
    m_evaluationOrder.clear();
    m_morphs.clear();
    m_morphWeights.clear();
    m_vertexOffsets.clear();
    m_uvOffsets.clear();
    m_runtimeVertices.clear();
    m_deformedPositions.clear();
    m_deformedNormals.clear();
    m_baseMaterials.clear();
    m_evaluatedMaterials.clear();

    m_ikChains.clear();
    m_children.clear();

    const size_t numBones = doc.bones.size();
    m_bones.reserve(numBones);
    m_inverseBindMatrices.reserve(numBones);

    for (size_t i = 0; i < numBones; ++i) {
        const auto& src = doc.bones[i];
        RuntimeBone rb;
        rb.index = static_cast<int32_t>(i);
        rb.parentIndex = src.parentBoneIndex;
        rb.nameJp = src.nameJp;
        rb.nameEn = src.nameEn;
        rb.restPosition = toRightHanded(src.position);
        rb.deformTier = src.deformTier;
        rb.boneFlags = src.boneFlags;
        rb.inheritParentIndex = src.inheritParentIndex;
        rb.inheritWeight = src.inheritWeight;

        rb.fixedAxisVector = toRightHanded(src.fixedAxisVector);
        if (glm::length(rb.fixedAxisVector) > 1e-6f) {
            rb.fixedAxisVector = glm::normalize(rb.fixedAxisVector);
        } else {
            rb.fixedAxisVector = glm::vec3(0.0f);
        }

        m_bones.push_back(rb);
        m_inverseBindMatrices.push_back(glm::translate(glm::mat4(1.0f), -rb.restPosition));

        // Parse IK chains: boneFlags & 0x0020
        if ((src.boneFlags & 0x0020u) != 0u) {
            IkChain chain;
            chain.ikBoneIndex = static_cast<int32_t>(i);
            chain.targetBoneIndex = src.ikTargetBoneIndex;
            chain.loopCount = (src.ikLoopCount > 0) ? src.ikLoopCount : 40;
            chain.limitAngle = (src.ikLimitAngle > 0.0f) ? src.ikLimitAngle : 0.5f;
            chain.enabled = true;
            chain.links.reserve(src.ikLinks.size());
            for (const auto& srcLink : src.ikLinks) {
                IkLinkConstraint link;
                link.boneIndex = srcLink.boneIndex;
                link.hasLimits = (srcLink.hasAngleLimit != 0);
                if (link.hasLimits) {
                    link.minAngle.x = -std::max(srcLink.limitMin.x, srcLink.limitMax.x);
                    link.maxAngle.x = -std::min(srcLink.limitMin.x, srcLink.limitMax.x);
                    link.minAngle.y = -std::max(srcLink.limitMin.y, srcLink.limitMax.y);
                    link.maxAngle.y = -std::min(srcLink.limitMin.y, srcLink.limitMax.y);
                    link.minAngle.z = -std::max(srcLink.limitMin.z, srcLink.limitMax.z);
                    link.maxAngle.z = -std::min(srcLink.limitMin.z, srcLink.limitMax.z);
                }
                chain.links.push_back(link);
            }
            m_ikChains.push_back(std::move(chain));
        }
    }

    rebuildChildrenHierarchy();

    // Sort evaluation order: (isPhysicsAfterDeform, deformTier, index)
    m_evaluationOrder.resize(numBones);
    std::iota(m_evaluationOrder.begin(), m_evaluationOrder.end(), 0);
    std::sort(m_evaluationOrder.begin(), m_evaluationOrder.end(), [this](int32_t a, int32_t b) {
        bool physA = (m_bones[static_cast<size_t>(a)].boneFlags & 0x1000) != 0;
        bool physB = (m_bones[static_cast<size_t>(b)].boneFlags & 0x1000) != 0;
        return std::tie(physA, m_bones[static_cast<size_t>(a)].deformTier, a) <
               std::tie(physB, m_bones[static_cast<size_t>(b)].deformTier, b);
    });

    // Morphs
    m_morphs = doc.morphs;
    m_morphWeights.assign(m_morphs.size(), 0.0f);

    // Vertices and UV offsets
    const size_t numVertices = doc.vertices.size();
    m_vertexOffsets.assign(numVertices, glm::vec3(0.0f));
    m_uvOffsets.assign(numVertices, glm::vec4(0.0f));

    m_runtimeVertices.resize(numVertices);
    m_deformedPositions.resize(numVertices);
    m_deformedNormals.resize(numVertices);

    for (size_t i = 0; i < numVertices; ++i) {
        const auto& srcV = doc.vertices[i];
        auto& dstV = m_runtimeVertices[i];
        dstV.restPosition = toRightHanded(srcV.position);
        dstV.restNormal = toRightHanded(srcV.normal);
        dstV.weightType = srcV.weightType;
        dstV.boneIndices = srcV.boneIndices;
        dstV.boneWeights = srcV.boneWeights;
        dstV.sdefC = toRightHanded(srcV.sdefC);
        dstV.sdefR0 = toRightHanded(srcV.sdefR0);
        dstV.sdefR1 = toRightHanded(srcV.sdefR1);
    }

    // Materials
    m_baseMaterials = doc.materials;
    m_evaluatedMaterials.resize(m_baseMaterials.size());
    for (size_t i = 0; i < m_baseMaterials.size(); ++i) {
        EvaluatedMaterial& em = m_evaluatedMaterials[i];
        const auto& bm = m_baseMaterials[i];
        em.diffuse = bm.diffuse;
        em.specular = bm.specular;
        em.specularPower = bm.specularPower;
        em.ambient = bm.ambient;
        em.edgeColor = bm.edgeColor;
        em.edgeSize = bm.edgeSize;
        em.textureTint = glm::vec4(1.0f);
        em.sphereTint = glm::vec4(1.0f);
        em.toonTint = glm::vec4(1.0f);
    }

    // Evaluate pre-physics rest pose
    evaluateMorphs();
    evaluateNodeGroup(false);

    // Initialize physics engine
    m_physicsEngine = std::make_unique<PmxPhysicsEngine>();
    m_physicsEngine->init(doc, *this);

    evaluateNodeGroup(false);
    evaluateNodeGroup(true);
    resetPhysicsToBones();

    evaluate(0.0f);
}

void PmxRuntimeScene::evaluate(float deltaTime) {
    evaluateMorphs();
    evaluateNodeGroup(false);
    if (m_physicsEnabled && m_physicsEngine) {
        m_physicsEngine->syncKinematicBodies(*this);
        if (deltaTime > 0.0f) {
            m_physicsEngine->stepSimulation(deltaTime);
            m_physicsEngine->syncDynamicBones(*this);
        } else {
            // Static pose or scrubbing: align dynamic rigid bodies to current bone transforms
            // to prevent syncDynamicBones from pulling bones to stale rest coordinates
            m_physicsEngine->resetToBoneTransforms(*this);
        }
    }
    evaluateNodeGroup(true);
    deformVertices();
}

void PmxRuntimeScene::deformVertices() {
    PmxVertexDeformer::deformAll(*this, m_runtimeVertices, m_vertexOffsets, m_deformedPositions, m_deformedNormals);
}

void PmxRuntimeScene::evaluateMorphs() {
    // 1. Reset all accumulators to default
    for (auto& bone : m_bones) {
        bone.morphTranslation = glm::vec3(0.0f);
        bone.morphRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    std::fill(m_vertexOffsets.begin(), m_vertexOffsets.end(), glm::vec3(0.0f));
    std::fill(m_uvOffsets.begin(), m_uvOffsets.end(), glm::vec4(0.0f));

    for (size_t i = 0; i < m_baseMaterials.size(); ++i) {
        EvaluatedMaterial& em = m_evaluatedMaterials[i];
        const auto& bm = m_baseMaterials[i];
        em.diffuse = bm.diffuse;
        em.specular = bm.specular;
        em.specularPower = bm.specularPower;
        em.ambient = bm.ambient;
        em.edgeColor = bm.edgeColor;
        em.edgeSize = bm.edgeSize;
        em.textureTint = glm::vec4(1.0f);
        em.sphereTint = glm::vec4(1.0f);
        em.toonTint = glm::vec4(1.0f);
    }

    if (m_morphs.empty()) {
        return;
    }

    // 2. Unpack Group Morphs recursively and compute effective weights
    std::vector<float> effectiveWeights(m_morphs.size(), 0.0f);

    auto accumulateWeight = [&](auto& self, int32_t morphIdx, float weight, int depth, std::vector<bool>& inStack) -> void {
        if (depth > 32 || morphIdx < 0 || morphIdx >= static_cast<int32_t>(m_morphs.size())) {
            return;
        }
        if (inStack[static_cast<size_t>(morphIdx)]) {
            return; // Cycle protection
        }
        const auto& morph = m_morphs[static_cast<size_t>(morphIdx)];
        if (morph.morphType == 0) { // Group morph
            inStack[static_cast<size_t>(morphIdx)] = true;
            for (const auto& child : morph.groupOffsets) {
                self(self, child.morphIndex, weight * child.weight, depth + 1, inStack);
            }
            inStack[static_cast<size_t>(morphIdx)] = false;
        } else {
            effectiveWeights[static_cast<size_t>(morphIdx)] += weight;
        }
    };

    for (size_t i = 0; i < m_morphs.size(); ++i) {
        float w = m_morphWeights[i];
        if (std::abs(w) < 1e-6f) {
            continue;
        }
        std::vector<bool> inStack(m_morphs.size(), false);
        accumulateWeight(accumulateWeight, static_cast<int32_t>(i), w, 0, inStack);
    }

    // 3. Accumulate Vertex, UV, and Bone Morphs
    for (size_t i = 0; i < m_morphs.size(); ++i) {
        float w = effectiveWeights[i];
        if (std::abs(w) < 1e-6f) {
            continue;
        }
        const auto& morph = m_morphs[i];

        if (morph.morphType == 1) { // Vertex morph
            for (const auto& off : morph.vertexOffsets) {
                if (off.vertexIndex >= 0 && off.vertexIndex < static_cast<int32_t>(m_vertexOffsets.size())) {
                    glm::vec3 rhOffset = toRightHanded(off.positionOffset);
                    m_vertexOffsets[static_cast<size_t>(off.vertexIndex)] += rhOffset * w;
                }
            }
        } else if (morph.morphType == 2) { // Bone morph
            for (const auto& off : morph.boneOffsets) {
                if (off.boneIndex >= 0 && off.boneIndex < static_cast<int32_t>(m_bones.size())) {
                    auto& bone = m_bones[static_cast<size_t>(off.boneIndex)];
                    glm::vec3 rhTrans = toRightHanded(off.translation);
                    bone.morphTranslation += rhTrans * w;

                    glm::quat rhRot = toRightHanded(off.rotation);
                    if (glm::dot(rhRot, rhRot) > 1e-8f) {
                        rhRot = glm::normalize(rhRot);
                    } else {
                        rhRot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                    }
                    glm::quat deltaRot = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), rhRot, w);
                    bone.morphRotation = glm::normalize(bone.morphRotation * deltaRot);
                }
            }
        } else if (morph.morphType >= 3 && morph.morphType <= 7) { // UV / Additional UV
            for (const auto& off : morph.uvOffsets) {
                if (off.vertexIndex >= 0 && off.vertexIndex < static_cast<int32_t>(m_uvOffsets.size())) {
                    m_uvOffsets[static_cast<size_t>(off.vertexIndex)] += off.uvOffset * w;
                }
            }
        }
    }

    // 4. Material Morphs: Multiply Pass, then Add Pass
    auto applyMaterialMultiply = [](EvaluatedMaterial& mat, const MaterialMorphOffset& off, float w) {
        mat.diffuse *= glm::mix(glm::vec4(1.0f), off.diffuse, w);
        mat.specular *= glm::mix(glm::vec3(1.0f), off.specular, w);
        mat.specularPower *= glm::mix(1.0f, off.specularPower, w);
        mat.ambient *= glm::mix(glm::vec3(1.0f), off.ambient, w);
        mat.edgeColor *= glm::mix(glm::vec4(1.0f), off.edgeColor, w);
        mat.edgeSize *= glm::mix(1.0f, off.edgeSize, w);
        mat.textureTint *= glm::mix(glm::vec4(1.0f), off.textureTint, w);
        mat.sphereTint *= glm::mix(glm::vec4(1.0f), off.sphereTint, w);
        mat.toonTint *= glm::mix(glm::vec4(1.0f), off.toonTint, w);
    };

    auto applyMaterialAdd = [](EvaluatedMaterial& mat, const MaterialMorphOffset& off, float w) {
        mat.diffuse += off.diffuse * w;
        mat.specular += off.specular * w;
        mat.specularPower += off.specularPower * w;
        mat.ambient += off.ambient * w;
        mat.edgeColor += off.edgeColor * w;
        mat.edgeSize += off.edgeSize * w;
        mat.textureTint += off.textureTint * w;
        mat.sphereTint += off.sphereTint * w;
        mat.toonTint += off.toonTint * w;
    };

    // Multiply pass (opType == 0)
    for (size_t i = 0; i < m_morphs.size(); ++i) {
        float w = effectiveWeights[i];
        if (std::abs(w) < 1e-6f) {
            continue;
        }
        const auto& morph = m_morphs[i];
        if (morph.morphType == 8) {
            for (const auto& off : morph.materialOffsets) {
                if (off.opType == 0) {
                    if (off.materialIndex < 0) {
                        for (auto& mat : m_evaluatedMaterials) {
                            applyMaterialMultiply(mat, off, w);
                        }
                    } else if (off.materialIndex < static_cast<int32_t>(m_evaluatedMaterials.size())) {
                        applyMaterialMultiply(m_evaluatedMaterials[static_cast<size_t>(off.materialIndex)], off, w);
                    }
                }
            }
        }
    }

    // Add pass (opType == 1)
    for (size_t i = 0; i < m_morphs.size(); ++i) {
        float w = effectiveWeights[i];
        if (std::abs(w) < 1e-6f) {
            continue;
        }
        const auto& morph = m_morphs[i];
        if (morph.morphType == 8) {
            for (const auto& off : morph.materialOffsets) {
                if (off.opType == 1) {
                    if (off.materialIndex < 0) {
                        for (auto& mat : m_evaluatedMaterials) {
                            applyMaterialAdd(mat, off, w);
                        }
                    } else if (off.materialIndex < static_cast<int32_t>(m_evaluatedMaterials.size())) {
                        applyMaterialAdd(m_evaluatedMaterials[static_cast<size_t>(off.materialIndex)], off, w);
                    }
                }
            }
        }
    }
}

void PmxRuntimeScene::evaluateBones() {
    evaluateNodeGroup(false);
    evaluateNodeGroup(true);
}

void PmxRuntimeScene::evaluatePrePhysicsBones() {
    evaluateNodeGroup(false);
}

void PmxRuntimeScene::evaluatePostPhysicsBones() {
    evaluateNodeGroup(true);
}

void PmxRuntimeScene::updateBoneLocalTransform(int32_t idx) {
    if (idx < 0 || idx >= static_cast<int32_t>(m_bones.size())) {
        return;
    }
    auto& bone = m_bones[static_cast<size_t>(idx)];
    glm::vec3 t = bone.userTranslation + bone.morphTranslation + bone.appendTranslation;
    glm::quat q_base = bone.userRotation * bone.morphRotation;
    if (glm::dot(q_base, q_base) > 1e-8f) {
        q_base = glm::normalize(q_base);
    } else {
        q_base = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    glm::quat q = bone.ikRotation * q_base * bone.appendRotation;
    if (glm::dot(q, q) > 1e-8f) {
        q = glm::normalize(q);
    } else {
        q = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    // Fixed Axis constraint (0x0400)
    if ((bone.boneFlags & 0x0400) && glm::length(bone.fixedAxisVector) > 1e-6f) {
        glm::vec3 axis = glm::normalize(bone.fixedAxisVector);
        glm::vec3 v(q.x, q.y, q.z);
        glm::vec3 p = glm::dot(v, axis) * axis;
        glm::quat twist(q.w, p.x, p.y, p.z);
        if (glm::dot(twist, twist) > 1e-8f) {
            q = glm::normalize(twist);
        } else {
            q = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }

    bone.localMatrix = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q);
}

void PmxRuntimeScene::propagateWorldTransform(int32_t boneIndex, bool afterPhysics, int depth) {
    if (depth > 256 || boneIndex < 0 || boneIndex >= static_cast<int32_t>(m_bones.size())) {
        return;
    }
    auto& bone = m_bones[static_cast<size_t>(boneIndex)];
    if (bone.parentIndex < 0 || bone.parentIndex >= static_cast<int32_t>(m_bones.size()) || bone.parentIndex == bone.index) {
        bone.worldMatrix = glm::translate(glm::mat4(1.0f), bone.restPosition) * bone.localMatrix;
    } else {
        const auto& parent = m_bones[static_cast<size_t>(bone.parentIndex)];
        glm::vec3 deltaRest = bone.restPosition - parent.restPosition;
        bone.worldMatrix = parent.worldMatrix * glm::translate(glm::mat4(1.0f), deltaRest) * bone.localMatrix;
    }
    bone.skinningMatrix = bone.worldMatrix * m_inverseBindMatrices[static_cast<size_t>(boneIndex)];

    if (static_cast<size_t>(boneIndex) < m_children.size()) {
        for (int32_t childIndex : m_children[static_cast<size_t>(boneIndex)]) {
            bool childAfter = (m_bones[static_cast<size_t>(childIndex)].boneFlags & 0x1000) != 0;
            if (childAfter == afterPhysics) {
                propagateWorldTransform(childIndex, afterPhysics, depth + 1);
            }
        }
    }
}

void PmxRuntimeScene::evaluateSingleBone(int32_t idx) {
    updateSubtree(idx);
}

void PmxRuntimeScene::evaluateNodeGroup(bool afterPhysics) {
    // Pass 1 (Local Base Transforms):
    for (int32_t idx : m_evaluationOrder) {
        auto& bone = m_bones[static_cast<size_t>(idx)];
        bool boneAfterPhysics = (bone.boneFlags & 0x1000) != 0;
        if (boneAfterPhysics != afterPhysics) {
            continue;
        }

        bone.appendTranslation = glm::vec3(0.0f);
        bone.appendRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        bone.ikRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        updateBoneLocalTransform(idx);
    }

    // Pass 2 (Root Hierarchy Propagation):
    for (int32_t idx : m_evaluationOrder) {
        const auto& bone = m_bones[static_cast<size_t>(idx)];
        bool boneAfterPhysics = (bone.boneFlags & 0x1000) != 0;
        if (boneAfterPhysics != afterPhysics) {
            continue;
        }
        bool isRootInGroup = (bone.parentIndex < 0 ||
                              bone.parentIndex >= static_cast<int32_t>(m_bones.size()) ||
                              bone.parentIndex == bone.index ||
                              ((m_bones[static_cast<size_t>(bone.parentIndex)].boneFlags & 0x1000) != (bone.boneFlags & 0x1000)));
        if (isRootInGroup) {
            propagateWorldTransform(idx, afterPhysics);
        }
    }

    // Pass 3 (Interleaved Deform Tier Loop):
    for (int32_t idx : m_evaluationOrder) {
        auto& bone = m_bones[static_cast<size_t>(idx)];
        bool boneAfterPhysics = (bone.boneFlags & 0x1000) != 0;
        if (boneAfterPhysics != afterPhysics) {
            continue;
        }

        // If bone has an enabled IK chain (bone is chain.ikBoneIndex):
        for (const auto& chain : m_ikChains) {
            if (chain.ikBoneIndex == idx && chain.enabled) {
                MmdCcdIkSolver::solveChain(*this, chain);
            }
        }

        // If bone has Append Rotation (0x0100) or Append Translation (0x0200):
        if (((bone.boneFlags & 0x0100) || (bone.boneFlags & 0x0200)) &&
            bone.inheritParentIndex >= 0 && bone.inheritParentIndex < static_cast<int32_t>(m_bones.size())) {
            const auto& src = m_bones[static_cast<size_t>(bone.inheritParentIndex)];

            glm::quat appendQ(1.0f, 0.0f, 0.0f, 0.0f);
            if (bone.boneFlags & 0x0100) {
                glm::quat q_srcBase = src.userRotation * src.morphRotation;
                if (glm::dot(q_srcBase, q_srcBase) > 1e-8f) {
                    q_srcBase = glm::normalize(q_srcBase);
                } else {
                    q_srcBase = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                }
                glm::quat R_src = glm::normalize(src.ikRotation * q_srcBase);
                if (bone.inheritWeight >= 0.0f) {
                    appendQ = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), R_src, bone.inheritWeight);
                } else {
                    appendQ = glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::conjugate(R_src), -bone.inheritWeight);
                }
            }

            glm::vec3 appendT(0.0f);
            if (bone.boneFlags & 0x0200) {
                glm::vec3 T_src = src.userTranslation + src.morphTranslation;
                appendT = T_src * bone.inheritWeight;
            }

            bone.appendRotation = appendQ;
            bone.appendTranslation = appendT;

            updateSubtree(idx);
        }
    }

    // Pass 4 (Final Hierarchy Synchronization):
    for (int32_t idx : m_evaluationOrder) {
        const auto& bone = m_bones[static_cast<size_t>(idx)];
        bool boneAfterPhysics = (bone.boneFlags & 0x1000) != 0;
        if (boneAfterPhysics != afterPhysics) {
            continue;
        }
        bool isRootInGroup = (bone.parentIndex < 0 ||
                              bone.parentIndex >= static_cast<int32_t>(m_bones.size()) ||
                              bone.parentIndex == bone.index ||
                              ((m_bones[static_cast<size_t>(bone.parentIndex)].boneFlags & 0x1000) != (bone.boneFlags & 0x1000)));
        if (isRootInGroup) {
            propagateWorldTransform(idx, afterPhysics);
        }
    }
}

void PmxRuntimeScene::setBoneTransform(int boneIndex, const glm::vec3& translation, const glm::quat& rotation) {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        m_bones[static_cast<size_t>(boneIndex)].userTranslation = translation;
        if (glm::dot(rotation, rotation) > 1e-8f) {
            m_bones[static_cast<size_t>(boneIndex)].userRotation = glm::normalize(rotation);
        } else {
            m_bones[static_cast<size_t>(boneIndex)].userRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }
}

void PmxRuntimeScene::resetTransforms() {
    for (auto& bone : m_bones) {
        bone.userTranslation = glm::vec3(0.0f);
        bone.userRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        bone.morphTranslation = glm::vec3(0.0f);
        bone.morphRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        bone.appendTranslation = glm::vec3(0.0f);
        bone.appendRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        bone.ikRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    std::fill(m_morphWeights.begin(), m_morphWeights.end(), 0.0f);
    resetPhysics();
    evaluate(0.0f);
}

void PmxRuntimeScene::resetPhysics() {
    evaluateBones();
    resetPhysicsToBones();
}

void PmxRuntimeScene::resetPhysicsToBones() {
    if (m_physicsEngine) {
        m_physicsEngine->resetToBoneTransforms(*this);
    }
}

void PmxRuntimeScene::setBoneWorldMatrix(int boneIndex, const glm::mat4& worldMatrix) {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        m_bones[static_cast<size_t>(boneIndex)].worldMatrix = worldMatrix;
        m_bones[static_cast<size_t>(boneIndex)].skinningMatrix =
            worldMatrix * m_inverseBindMatrices[static_cast<size_t>(boneIndex)];
    }
}

void PmxRuntimeScene::setMorphWeight(int morphIndex, float weight) {
    if (morphIndex >= 0 && morphIndex < static_cast<int>(m_morphWeights.size())) {
        m_morphWeights[static_cast<size_t>(morphIndex)] = weight;
    }
}

float PmxRuntimeScene::morphWeight(int morphIndex) const {
    if (morphIndex >= 0 && morphIndex < static_cast<int>(m_morphWeights.size())) {
        return m_morphWeights[static_cast<size_t>(morphIndex)];
    }
    return 0.0f;
}

int PmxRuntimeScene::findBoneByName(const QString& name) const {
    if (name.isEmpty()) {
        return -1;
    }
    for (size_t i = 0; i < m_bones.size(); ++i) {
        if (m_bones[i].nameJp == name || m_bones[i].nameEn == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int PmxRuntimeScene::findMorphByName(const QString& name) const {
    if (name.isEmpty()) {
        return -1;
    }
    for (size_t i = 0; i < m_morphs.size(); ++i) {
        if (m_morphs[i].nameJp == name || m_morphs[i].nameEn == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

const RuntimeBone& PmxRuntimeScene::bone(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        return m_bones[static_cast<size_t>(boneIndex)];
    }
    return s_dummyBone;
}

const glm::mat4& PmxRuntimeScene::worldMatrix(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        return m_bones[static_cast<size_t>(boneIndex)].worldMatrix;
    }
    return s_identityMatrix;
}

const glm::mat4& PmxRuntimeScene::skinningMatrix(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        return m_bones[static_cast<size_t>(boneIndex)].skinningMatrix;
    }
    return s_identityMatrix;
}

const glm::mat4& PmxRuntimeScene::localMatrix(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        return m_bones[static_cast<size_t>(boneIndex)].localMatrix;
    }
    return s_identityMatrix;
}

const glm::mat4& PmxRuntimeScene::inverseBindMatrix(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_inverseBindMatrices.size())) {
        return m_inverseBindMatrices[static_cast<size_t>(boneIndex)];
    }
    return s_identityMatrix;
}

glm::vec3 PmxRuntimeScene::worldPosition(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        return glm::vec3(m_bones[static_cast<size_t>(boneIndex)].worldMatrix[3]);
    }
    return glm::vec3(0.0f);
}

glm::vec3 PmxRuntimeScene::vertexOffset(int vertexIndex) const {
    if (vertexIndex >= 0 && vertexIndex < static_cast<int>(m_vertexOffsets.size())) {
        return m_vertexOffsets[static_cast<size_t>(vertexIndex)];
    }
    return glm::vec3(0.0f);
}

glm::vec4 PmxRuntimeScene::uvOffset(int vertexIndex) const {
    if (vertexIndex >= 0 && vertexIndex < static_cast<int>(m_uvOffsets.size())) {
        return m_uvOffsets[static_cast<size_t>(vertexIndex)];
    }
    return glm::vec4(0.0f);
}

glm::vec3 PmxRuntimeScene::deformedPosition(int vertexIndex) const {
    if (vertexIndex >= 0 && vertexIndex < static_cast<int>(m_deformedPositions.size())) {
        return m_deformedPositions[static_cast<size_t>(vertexIndex)];
    }
    return glm::vec3(0.0f);
}

glm::vec3 PmxRuntimeScene::deformedNormal(int vertexIndex) const {
    if (vertexIndex >= 0 && vertexIndex < static_cast<int>(m_deformedNormals.size())) {
        return m_deformedNormals[static_cast<size_t>(vertexIndex)];
    }
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

const RuntimeVertex& PmxRuntimeScene::runtimeVertex(int vertexIndex) const {
    if (vertexIndex >= 0 && vertexIndex < static_cast<int>(m_runtimeVertices.size())) {
        return m_runtimeVertices[static_cast<size_t>(vertexIndex)];
    }
    return s_dummyRuntimeVertex;
}

const EvaluatedMaterial& PmxRuntimeScene::evaluatedMaterial(int materialIndex) const {
    if (materialIndex >= 0 && materialIndex < static_cast<int>(m_evaluatedMaterials.size())) {
        return m_evaluatedMaterials[static_cast<size_t>(materialIndex)];
    }
    return s_dummyMaterial;
}

void PmxRuntimeScene::solveIk() {
    // Reset ikRotation for all links in all chains before solving
    for (const auto& chain : m_ikChains) {
        for (const auto& link : chain.links) {
            if (link.boneIndex >= 0 && link.boneIndex < static_cast<int32_t>(m_bones.size())) {
                m_bones[static_cast<size_t>(link.boneIndex)].ikRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
                updateSubtree(link.boneIndex);
            }
        }
    }

    for (const auto& chain : m_ikChains) {
        if (chain.enabled) {
            MmdCcdIkSolver::solveChain(*this, chain);
        }
    }
}

void PmxRuntimeScene::setIkEnabled(int boneIndex, bool enabled) {
    for (auto& chain : m_ikChains) {
        if (chain.ikBoneIndex == boneIndex) {
            chain.enabled = enabled;
        }
    }
}

bool PmxRuntimeScene::isIkEnabled(int boneIndex) const {
    for (const auto& chain : m_ikChains) {
        if (chain.ikBoneIndex == boneIndex) {
            return chain.enabled;
        }
    }
    return false;
}

int PmxRuntimeScene::ikChainCount() const {
    return static_cast<int>(m_ikChains.size());
}

void PmxRuntimeScene::addIkChain(const IkChain& chain) {
    m_ikChains.push_back(chain);
}

void PmxRuntimeScene::clearIkChains() {
    m_ikChains.clear();
}

void PmxRuntimeScene::setBoneIkRotation(int boneIndex, const glm::quat& rotation) {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        if (glm::dot(rotation, rotation) > 1e-8f) {
            m_bones[static_cast<size_t>(boneIndex)].ikRotation = glm::normalize(rotation);
        } else {
            m_bones[static_cast<size_t>(boneIndex)].ikRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        }
    }
}

static const glm::quat s_identityQuat(1.0f, 0.0f, 0.0f, 0.0f);

const glm::quat& PmxRuntimeScene::boneIkRotation(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bones.size())) {
        return m_bones[static_cast<size_t>(boneIndex)].ikRotation;
    }
    return s_identityQuat;
}

void PmxRuntimeScene::rebuildChildrenHierarchy() {
    m_children.assign(m_bones.size(), {});
    for (size_t i = 0; i < m_bones.size(); ++i) {
        int32_t p = m_bones[i].parentIndex;
        if (p >= 0 && p < static_cast<int32_t>(m_bones.size()) && p != static_cast<int32_t>(i)) {
            m_children[static_cast<size_t>(p)].push_back(static_cast<int32_t>(i));
        }
    }
}

void PmxRuntimeScene::updateSubtree(int32_t boneIndex, int depth) {
    if (depth > 256 || boneIndex < 0 || boneIndex >= static_cast<int32_t>(m_bones.size())) {
        return;
    }
    updateBoneLocalTransform(boneIndex);
    auto& bone = m_bones[static_cast<size_t>(boneIndex)];
    if (bone.parentIndex < 0 || bone.parentIndex >= static_cast<int32_t>(m_bones.size()) || bone.parentIndex == bone.index) {
        bone.worldMatrix = glm::translate(glm::mat4(1.0f), bone.restPosition) * bone.localMatrix;
    } else {
        const auto& parent = m_bones[static_cast<size_t>(bone.parentIndex)];
        glm::vec3 deltaRest = bone.restPosition - parent.restPosition;
        bone.worldMatrix = parent.worldMatrix * glm::translate(glm::mat4(1.0f), deltaRest) * bone.localMatrix;
    }
    bone.skinningMatrix = bone.worldMatrix * m_inverseBindMatrices[static_cast<size_t>(boneIndex)];

    if (m_children.size() != m_bones.size()) {
        rebuildChildrenHierarchy();
    }
    if (static_cast<size_t>(boneIndex) < m_children.size()) {
        for (int32_t childIndex : m_children[static_cast<size_t>(boneIndex)]) {
            updateSubtree(childIndex, depth + 1);
        }
    }
}

} // namespace pmx
