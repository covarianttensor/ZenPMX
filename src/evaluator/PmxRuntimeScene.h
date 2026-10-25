#pragma once

#include "core/PmxDocument.h"
#include "core/PmxTypes.h"
#include "MmdCcdIkSolver.h"
#include "PmxVertexDeformer.h"

#include <QString>
#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pmx {

class PmxPhysicsEngine;

struct RuntimeBone {
    int32_t index = -1;
    int32_t parentIndex = -1;
    QString nameJp;
    QString nameEn;
    glm::vec3 restPosition{0.0f}; // RH coordinates
    int32_t deformTier = 0;
    uint16_t boneFlags = 0;
    int32_t inheritParentIndex = -1;
    float inheritWeight = 1.0f;
    glm::vec3 fixedAxisVector{0.0f};

    // Dynamic State per frame:
    glm::vec3 userTranslation{0.0f};
    glm::quat userRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 morphTranslation{0.0f};
    glm::quat morphRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 appendTranslation{0.0f};
    glm::quat appendRotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::quat ikRotation{1.0f, 0.0f, 0.0f, 0.0f};

    glm::mat4 localMatrix{1.0f};
    glm::mat4 worldMatrix{1.0f};
    glm::mat4 skinningMatrix{1.0f}; // worldMatrix * inverseBindMatrix
};

struct EvaluatedMaterial {
    glm::vec4 diffuse{1.0f};
    glm::vec3 specular{0.0f};
    float specularPower = 5.0f;
    glm::vec3 ambient{0.5f};
    glm::vec4 edgeColor{0.0f, 0.0f, 0.0f, 1.0f};
    float edgeSize = 1.0f;
    glm::vec4 textureTint{1.0f};
    glm::vec4 sphereTint{1.0f};
    glm::vec4 toonTint{1.0f};
};

class PmxRuntimeScene {
public:
    PmxRuntimeScene();
    ~PmxRuntimeScene();
    PmxRuntimeScene(const PmxRuntimeScene&) = delete;
    PmxRuntimeScene& operator=(const PmxRuntimeScene&) = delete;
    PmxRuntimeScene(PmxRuntimeScene&&) noexcept;
    PmxRuntimeScene& operator=(PmxRuntimeScene&&) noexcept;

    // Lifecycle & Execution
    void loadDocument(const PmxDocument& doc);
    void evaluate(float deltaTime = 1.0f / 60.0f);
    void evaluateMorphs();
    void evaluateBones();
    void evaluatePrePhysicsBones();
    void evaluatePostPhysicsBones();
    void deformVertices();

    // Physics Engine Controls & Accessors
    PmxPhysicsEngine* physicsEngine() { return m_physicsEngine.get(); }
    const PmxPhysicsEngine* physicsEngine() const { return m_physicsEngine.get(); }
    void setPhysicsEnabled(bool enabled) { m_physicsEnabled = enabled; }
    bool isPhysicsEnabled() const { return m_physicsEnabled; }
    void resetPhysics();
    void setBoneWorldMatrix(int boneIndex, const glm::mat4& worldMatrix);

    // Transform & Morph manipulation
    void setBoneTransform(int boneIndex, const glm::vec3& translation, const glm::quat& rotation);
    void resetTransforms();

    void setMorphWeight(int morphIndex, float weight);
    float morphWeight(int morphIndex) const;

    // Bone accessors
    const std::vector<RuntimeBone>& bones() const { return m_bones; }
    const RuntimeBone& bone(int boneIndex) const;
    const glm::mat4& worldMatrix(int boneIndex) const;
    const glm::mat4& skinningMatrix(int boneIndex) const;
    const glm::mat4& localMatrix(int boneIndex) const;
    const glm::mat4& inverseBindMatrix(int boneIndex) const;
    glm::vec3 worldPosition(int boneIndex) const;

    // Evaluation schedule accessor
    const std::vector<int32_t>& evaluationOrder() const { return m_evaluationOrder; }

    // Vertex & UV offset accessors
    const std::vector<glm::vec3>& vertexOffsets() const { return m_vertexOffsets; }
    glm::vec3 vertexOffset(int vertexIndex) const;

    const std::vector<glm::vec4>& uvOffsets() const { return m_uvOffsets; }
    glm::vec4 uvOffset(int vertexIndex) const;

    // Deformed vertex & normal accessors
    const std::vector<glm::vec3>& deformedPositions() const { return m_deformedPositions; }
    const std::vector<glm::vec3>& deformedNormals() const { return m_deformedNormals; }
    glm::vec3 deformedPosition(int vertexIndex) const;
    glm::vec3 deformedNormal(int vertexIndex) const;

    const std::vector<RuntimeVertex>& runtimeVertices() const { return m_runtimeVertices; }
    const RuntimeVertex& runtimeVertex(int vertexIndex) const;

    // Material accessors
    const std::vector<EvaluatedMaterial>& evaluatedMaterials() const { return m_evaluatedMaterials; }
    const EvaluatedMaterial& evaluatedMaterial(int materialIndex) const;

    // Counts
    int boneCount() const { return static_cast<int>(m_bones.size()); }
    int morphCount() const { return static_cast<int>(m_morphWeights.size()); }
    int vertexCount() const { return static_cast<int>(m_vertexOffsets.size()); }
    int materialCount() const { return static_cast<int>(m_evaluatedMaterials.size()); }

    // IK Controls
    void solveIk();
    void setIkEnabled(int boneIndex, bool enabled);
    bool isIkEnabled(int boneIndex) const;
    int ikChainCount() const;
    void addIkChain(const IkChain& chain);
    void clearIkChains();
    const std::vector<IkChain>& ikChains() const { return m_ikChains; }
    std::vector<IkChain>& ikChains() { return m_ikChains; }

    void setBoneIkRotation(int boneIndex, const glm::quat& rotation);
    const glm::quat& boneIkRotation(int boneIndex) const;
    void updateSubtree(int32_t boneIndex, int depth = 0);
    void rebuildChildrenHierarchy();

    // Coordinate conversion utilities
    static glm::vec3 toRightHanded(const glm::vec3& vLH) {
        return glm::vec3(vLH.x, vLH.y, -vLH.z);
    }
    static glm::quat toRightHanded(const glm::quat& qLH) {
        return glm::quat(qLH.w, -qLH.x, -qLH.y, qLH.z);
    }
    static glm::vec3 toLeftHanded(const glm::vec3& vRH) {
        return glm::vec3(vRH.x, vRH.y, -vRH.z);
    }
    static glm::quat toLeftHanded(const glm::quat& qRH) {
        return glm::quat(qRH.w, -qRH.x, -qRH.y, qRH.z);
    }

private:
    void evaluateSingleBone(int32_t idx);

    std::vector<RuntimeBone> m_bones;
    std::vector<glm::mat4> m_inverseBindMatrices;
    std::vector<int32_t> m_evaluationOrder;

    std::vector<PmxMorph> m_morphs;
    std::vector<float> m_morphWeights;

    std::vector<glm::vec3> m_vertexOffsets;
    std::vector<glm::vec4> m_uvOffsets;

    std::vector<RuntimeVertex> m_runtimeVertices;
    std::vector<glm::vec3> m_deformedPositions;
    std::vector<glm::vec3> m_deformedNormals;

    std::vector<PmxMaterial> m_baseMaterials;
    std::vector<EvaluatedMaterial> m_evaluatedMaterials;

    std::vector<IkChain> m_ikChains;
    std::vector<std::vector<int32_t>> m_children;

    std::unique_ptr<PmxPhysicsEngine> m_physicsEngine;
    bool m_physicsEnabled = true;
};

} // namespace pmx
