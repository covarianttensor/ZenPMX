#include "PmxVertexDeformer.h"
#include "PmxRuntimeScene.h"

#include <cmath>

namespace pmx {

glm::vec3 PmxVertexDeformer::safeNormalizeNormal(const glm::vec3& n) {
    float lenSq = glm::dot(n, n);
    if (lenSq < 1e-10f || std::isnan(lenSq) || std::isinf(lenSq)) {
        return glm::vec3(0.0f, 1.0f, 0.0f);
    }
    return n * glm::inversesqrt(lenSq);
}

void PmxVertexDeformer::deformBdef1(const glm::vec3& pEff,
                                    const glm::vec3& nEff,
                                    const glm::mat4& m0,
                                    glm::vec3& outPos,
                                    glm::vec3& outNormal)
{
    outPos = glm::vec3(m0 * glm::vec4(pEff, 1.0f));
    outNormal = safeNormalizeNormal(glm::mat3(m0) * nEff);
}

void PmxVertexDeformer::deformBdef2(const glm::vec3& pEff,
                                    const glm::vec3& nEff,
                                    const glm::mat4& m0,
                                    const glm::mat4& m1,
                                    float w0,
                                    glm::vec3& outPos,
                                    glm::vec3& outNormal)
{
    float w1 = 1.0f - w0;
    glm::mat4 m = w0 * m0 + w1 * m1;
    outPos = glm::vec3(m * glm::vec4(pEff, 1.0f));
    outNormal = safeNormalizeNormal(glm::mat3(m) * nEff);
}

void PmxVertexDeformer::deformBdef4(const glm::vec3& pEff,
                                    const glm::vec3& nEff,
                                    const glm::mat4& m0,
                                    const glm::mat4& m1,
                                    const glm::mat4& m2,
                                    const glm::mat4& m3,
                                    const std::array<float, 4>& weights,
                                    glm::vec3& outPos,
                                    glm::vec3& outNormal)
{
    glm::mat4 m = weights[0] * m0 +
                  weights[1] * m1 +
                  weights[2] * m2 +
                  weights[3] * m3;
    outPos = glm::vec3(m * glm::vec4(pEff, 1.0f));
    outNormal = safeNormalizeNormal(glm::mat3(m) * nEff);
}

void PmxVertexDeformer::deformQdef(const glm::vec3& pEff,
                                   const glm::vec3& nEff,
                                   const glm::mat4& m0,
                                   const glm::mat4& m1,
                                   const glm::mat4& m2,
                                   const glm::mat4& m3,
                                   const std::array<float, 4>& weights,
                                   glm::vec3& outPos,
                                   glm::vec3& outNormal)
{
    deformBdef4(pEff, nEff, m0, m1, m2, m3, weights, outPos, outNormal);
}

void PmxVertexDeformer::deformSdef(const glm::vec3& pEff,
                                   const glm::vec3& nEff,
                                   const glm::mat4& m0,
                                   const glm::mat4& m1,
                                   float w0,
                                   const glm::vec3& sdefC,
                                   const glm::vec3& sdefR0,
                                   const glm::vec3& sdefR1,
                                   glm::vec3& outPos,
                                   glm::vec3& outNormal)
{
    float w1 = 1.0f - w0;

    // 1. Rotational quaternions from skinning matrices
    glm::quat q0 = glm::quat_cast(glm::mat3(m0));
    float lenQ0Sq = glm::dot(q0, q0);
    if (lenQ0Sq > 1e-8f) {
        q0 = glm::normalize(q0);
    } else {
        q0 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    glm::quat q1 = glm::quat_cast(glm::mat3(m1));
    float lenQ1Sq = glm::dot(q1, q1);
    if (lenQ1Sq > 1e-8f) {
        q1 = glm::normalize(q1);
    } else {
        q1 = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }

    // Shortest arc
    if (glm::dot(q0, q1) < 0.0f) {
        q1 = -q1;
    }

    // Interpolate rotation: slerp from q1 to q0 with weight w0
    glm::quat qSdef = glm::slerp(q1, q0, w0);
    float lenSdefSq = glm::dot(qSdef, qSdef);
    if (lenSdefSq > 1e-8f) {
        qSdef = glm::normalize(qSdef);
    } else {
        qSdef = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    glm::mat3 rSdef = glm::mat3_cast(qSdef);

    // 2. Position evaluation with SDEF center correction
    glm::vec3 c0 = glm::vec3(m0 * glm::vec4(sdefC, 1.0f));
    glm::vec3 c1 = glm::vec3(m1 * glm::vec4(sdefC, 1.0f));
    glm::vec3 cPrime = w0 * c0 + w1 * c1;

    // Directional bounds auxiliary correction per MASTER_SPECIFICATION.md §5.2
    glm::vec3 deltaR(0.0f);
    if (glm::dot(sdefR0, sdefR0) > 1e-8f || glm::dot(sdefR1, sdefR1) > 1e-8f) {
        glm::vec3 r0 = glm::mat3(m0) * sdefR0 - rSdef * sdefR0;
        glm::vec3 r1 = glm::mat3(m1) * sdefR1 - rSdef * sdefR1;
        deltaR = w0 * r0 + w1 * r1;
    }

    outPos = rSdef * (pEff - sdefC) + cPrime + deltaR;
    outNormal = safeNormalizeNormal(rSdef * nEff);
}

void PmxVertexDeformer::deformAll(const PmxRuntimeScene& scene,
                                  const std::vector<RuntimeVertex>& vertices,
                                  const std::vector<glm::vec3>& vertexOffsets,
                                  std::vector<glm::vec3>& outPositions,
                                  std::vector<glm::vec3>& outNormals)
{
    const size_t vertexCount = vertices.size();
    if (outPositions.size() != vertexCount) {
        outPositions.resize(vertexCount);
    }
    if (outNormals.size() != vertexCount) {
        outNormals.resize(vertexCount);
    }

    for (size_t i = 0; i < vertexCount; ++i) {
        const auto& rv = vertices[i];
        glm::vec3 morphOffset = (i < vertexOffsets.size()) ? vertexOffsets[i] : glm::vec3(0.0f);
        glm::vec3 pEff = rv.restPosition + morphOffset;
        const glm::vec3& nEff = rv.restNormal;

        const glm::mat4& m0 = scene.skinningMatrix(rv.boneIndices[0]);
        const glm::mat4& m1 = scene.skinningMatrix(rv.boneIndices[1]);
        const glm::mat4& m2 = scene.skinningMatrix(rv.boneIndices[2]);
        const glm::mat4& m3 = scene.skinningMatrix(rv.boneIndices[3]);

        switch (rv.weightType) {
        case WeightType::BDEF1:
            deformBdef1(pEff, nEff, m0, outPositions[i], outNormals[i]);
            break;
        case WeightType::BDEF2:
            deformBdef2(pEff, nEff, m0, m1, rv.boneWeights[0], outPositions[i], outNormals[i]);
            break;
        case WeightType::BDEF4:
            deformBdef4(pEff, nEff, m0, m1, m2, m3, rv.boneWeights, outPositions[i], outNormals[i]);
            break;
        case WeightType::SDEF:
            deformSdef(pEff, nEff, m0, m1, rv.boneWeights[0], rv.sdefC, rv.sdefR0, rv.sdefR1, outPositions[i], outNormals[i]);
            break;
        case WeightType::QDEF:
            deformQdef(pEff, nEff, m0, m1, m2, m3, rv.boneWeights, outPositions[i], outNormals[i]);
            break;
        default:
            deformBdef1(pEff, nEff, m0, outPositions[i], outNormals[i]);
            break;
        }
    }
}

} // namespace pmx
