#pragma once

#include "core/PmxTypes.h"

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pmx {

class PmxRuntimeScene;

struct RuntimeVertex {
    glm::vec3 restPosition{0.0f};           // RH coordinates
    glm::vec3 restNormal{0.0f, 1.0f, 0.0f}; // RH coordinates
    WeightType weightType = WeightType::BDEF1;
    std::array<int32_t, 4> boneIndices{-1, -1, -1, -1};
    std::array<float, 4> boneWeights{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 sdefC{0.0f};                  // RH coordinates
    glm::vec3 sdefR0{0.0f};                 // RH coordinates
    glm::vec3 sdefR1{0.0f};                 // RH coordinates
};

class PmxVertexDeformer {
public:
    // Safe normal normalization helper (guarded against division by zero; fallback (0, 1, 0))
    static glm::vec3 safeNormalizeNormal(const glm::vec3& n);

    // Individual deformation methods
    static void deformBdef1(const glm::vec3& pEff,
                            const glm::vec3& nEff,
                            const glm::mat4& m0,
                            glm::vec3& outPos,
                            glm::vec3& outNormal);

    static void deformBdef2(const glm::vec3& pEff,
                            const glm::vec3& nEff,
                            const glm::mat4& m0,
                            const glm::mat4& m1,
                            float w0,
                            glm::vec3& outPos,
                            glm::vec3& outNormal);

    static void deformBdef4(const glm::vec3& pEff,
                            const glm::vec3& nEff,
                            const glm::mat4& m0,
                            const glm::mat4& m1,
                            const glm::mat4& m2,
                            const glm::mat4& m3,
                            const std::array<float, 4>& weights,
                            glm::vec3& outPos,
                            glm::vec3& outNormal);

    static void deformSdef(const glm::vec3& pEff,
                           const glm::vec3& nEff,
                           const glm::mat4& m0,
                           const glm::mat4& m1,
                           float w0,
                           const glm::vec3& sdefC,
                           const glm::vec3& sdefR0,
                           const glm::vec3& sdefR1,
                           glm::vec3& outPos,
                           glm::vec3& outNormal);

    static void deformQdef(const glm::vec3& pEff,
                           const glm::vec3& nEff,
                           const glm::mat4& m0,
                           const glm::mat4& m1,
                           const glm::mat4& m2,
                           const glm::mat4& m3,
                           const std::array<float, 4>& weights,
                           glm::vec3& outPos,
                           glm::vec3& outNormal);

    // Batch deformation helper
    static void deformAll(const PmxRuntimeScene& scene,
                          const std::vector<RuntimeVertex>& vertices,
                          const std::vector<glm::vec3>& vertexOffsets,
                          std::vector<glm::vec3>& outPositions,
                          std::vector<glm::vec3>& outNormals);
};

} // namespace pmx
