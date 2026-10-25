#include "viewport/PmxWeightPaintBrush.h"

#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

namespace pmx {

glm::vec4 PmxWeightPaintBrush::weightToColor(float weight) {
    if (!std::isfinite(weight)) {
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); // Blue fallback
    }

    if (weight <= 0.0f) {
        return glm::vec4(0.0f, 0.0f, 1.0f, 1.0f); // Blue
    } else if (weight <= 0.25f) {
        // Blue -> Cyan
        return glm::vec4(0.0f, 4.0f * weight, 1.0f, 1.0f);
    } else if (weight <= 0.5f) {
        // Cyan -> Green
        return glm::vec4(0.0f, 1.0f, 1.0f - 4.0f * (weight - 0.25f), 1.0f);
    } else if (weight <= 0.75f) {
        // Green -> Yellow
        return glm::vec4(4.0f * (weight - 0.5f), 1.0f, 0.0f, 1.0f);
    } else if (weight <= 1.0f) {
        // Yellow -> Red
        return glm::vec4(1.0f, 1.0f - 4.0f * (weight - 0.75f), 0.0f, 1.0f);
    } else {
        return glm::vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
    }
}

float PmxWeightPaintBrush::computeFalloff(float distance, float radius, BrushFalloff falloff) {
    if (!std::isfinite(distance) || !std::isfinite(radius) || radius <= 0.001f || distance < 0.0f || distance > radius) {
        return 0.0f;
    }

    float normDist = distance / radius;
    switch (falloff) {
    case BrushFalloff::Linear:
        return std::max(0.0f, 1.0f - normDist);
    case BrushFalloff::Gaussian:
        return std::exp(-3.0f * normDist * normDist);
    case BrushFalloff::Constant:
        return 1.0f;
    }
    return 0.0f;
}

float PmxWeightPaintBrush::getVertexBoneWeight(const PmxVertex& v, int boneIndex) {
    if (boneIndex < 0) {
        return 0.0f;
    }

    switch (v.weightType) {
    case WeightType::BDEF1:
        if (v.boneIndices[0] == boneIndex) {
            return 1.0f;
        }
        return 0.0f;
    case WeightType::BDEF2:
    case WeightType::SDEF: {
        float w = 0.0f;
        if (v.boneIndices[0] == boneIndex) {
            w += v.boneWeights[0];
        }
        if (v.boneIndices[1] == boneIndex) {
            w += (1.0f - v.boneWeights[0]);
        }
        return std::clamp(w, 0.0f, 1.0f);
    }
    case WeightType::BDEF4:
    case WeightType::QDEF: {
        float w = 0.0f;
        for (int i = 0; i < 4; ++i) {
            if (v.boneIndices[i] == boneIndex) {
                w += v.boneWeights[i];
            }
        }
        return std::clamp(w, 0.0f, 1.0f);
    }
    }
    return 0.0f;
}

void PmxWeightPaintBrush::applyBrushToVertex(PmxVertex& v, int targetBoneIndex, float factor, float strength, BrushMode mode) {
    if (targetBoneIndex < 0) {
        return;
    }
    if (!std::isfinite(factor) || !std::isfinite(strength)) {
        return;
    }

    float f = std::clamp(factor, 0.0f, 1.0f);
    float s = std::clamp(strength, 0.0f, 1.0f);
    if (f <= 0.0f || s <= 0.0f) {
        return;
    }

    float delta = s * f;

    // 1. BDEF1 handling
    if (v.weightType == WeightType::BDEF1) {
        if (v.boneIndices[0] == targetBoneIndex) {
            // Already 100% on target bone. Cannot add further.
            return;
        } else if (v.boneIndices[0] < 0) {
            // Unbound vertex, bind directly to target bone
            v.boneIndices[0] = targetBoneIndex;
            v.boneWeights = { 1.0f, 0.0f, 0.0f, 0.0f };
            return;
        } else {
            // Different bone: elevate to BDEF2 on Add or Set
            if (mode == BrushMode::Subtract) {
                return; // Target bone weight is already 0
            }
            float targetW = (mode == BrushMode::Add) ? delta : (s * f);
            targetW = std::clamp(targetW, 0.0f, 1.0f);

            v.weightType = WeightType::BDEF2;
            v.boneIndices[1] = targetBoneIndex;
            v.boneWeights[0] = 1.0f - targetW;
            v.boneWeights[1] = targetW;
            return;
        }
    }

    // 2. BDEF2 handling
    if (v.weightType == WeightType::BDEF2) {
        int matchIdx = -1;
        if (v.boneIndices[0] == targetBoneIndex) matchIdx = 0;
        else if (v.boneIndices[1] == targetBoneIndex) matchIdx = 1;

        if (matchIdx >= 0) {
            float oldW = (matchIdx == 0) ? v.boneWeights[0] : (1.0f - v.boneWeights[0]);
            float newW = oldW;
            if (mode == BrushMode::Add) {
                newW = std::min(1.0f, oldW + delta);
            } else if (mode == BrushMode::Subtract) {
                newW = std::max(0.0f, oldW - delta);
            } else if (mode == BrushMode::Set) {
                newW = std::clamp(oldW + (s - oldW) * f, 0.0f, 1.0f);
            } else if (mode == BrushMode::Smooth) {
                newW = std::clamp(oldW + (0.5f - oldW) * delta, 0.0f, 1.0f);
            }

            if (matchIdx == 0) {
                v.boneWeights[0] = newW;
                v.boneWeights[1] = 1.0f - newW;
            } else {
                v.boneWeights[0] = 1.0f - newW;
                v.boneWeights[1] = newW;
            }
            return;
        } else {
            // Neither slot is targetBoneIndex
            if (mode == BrushMode::Subtract) {
                return; // Already 0
            }

            if (v.boneWeights[0] <= 1e-5f || v.boneIndices[0] < 0) {
                v.boneIndices[0] = targetBoneIndex;
                v.boneWeights[0] = 0.0f;
                v.boneWeights[1] = 1.0f;
                // Re-apply to slot 0
                applyBrushToVertex(v, targetBoneIndex, factor, strength, mode);
                return;
            } else if (v.boneWeights[1] <= 1e-5f || v.boneIndices[1] < 0) {
                v.boneIndices[1] = targetBoneIndex;
                v.boneWeights[0] = 1.0f;
                v.boneWeights[1] = 0.0f;
                // Re-apply to slot 1
                applyBrushToVertex(v, targetBoneIndex, factor, strength, mode);
                return;
            } else {
                // Both slots occupied with non-zero weights: elevate to BDEF4
                v.boneWeights[1] = 1.0f - v.boneWeights[0];
                v.weightType = WeightType::BDEF4;
                v.boneIndices[2] = targetBoneIndex;
                v.boneWeights[2] = 0.0f;
                v.boneIndices[3] = -1;
                v.boneWeights[3] = 0.0f;
                // Falls through to BDEF4 logic below
            }
        }
    }

    // 3. SDEF handling (strictly 2 bones)
    if (v.weightType == WeightType::SDEF) {
        int matchIdx = -1;
        if (v.boneIndices[0] == targetBoneIndex) matchIdx = 0;
        else if (v.boneIndices[1] == targetBoneIndex) matchIdx = 1;

        if (matchIdx >= 0) {
            float oldW = (matchIdx == 0) ? v.boneWeights[0] : (1.0f - v.boneWeights[0]);
            float newW = oldW;
            if (mode == BrushMode::Add) {
                newW = std::min(1.0f, oldW + delta);
            } else if (mode == BrushMode::Subtract) {
                newW = std::max(0.0f, oldW - delta);
            } else if (mode == BrushMode::Set) {
                newW = std::clamp(oldW + (s - oldW) * f, 0.0f, 1.0f);
            } else if (mode == BrushMode::Smooth) {
                newW = std::clamp(oldW + (0.5f - oldW) * delta, 0.0f, 1.0f);
            }

            if (matchIdx == 0) {
                v.boneWeights[0] = newW;
                v.boneWeights[1] = 1.0f - newW;
            } else {
                v.boneWeights[0] = 1.0f - newW;
                v.boneWeights[1] = newW;
            }
            return;
        } else {
            if (mode == BrushMode::Subtract) {
                return;
            }
            // Replace the bone slot with the smaller weight to preserve SDEF structure
            int replaceIdx = (v.boneWeights[0] <= v.boneWeights[1]) ? 0 : 1;
            v.boneIndices[replaceIdx] = targetBoneIndex;
            if (replaceIdx == 0) {
                v.boneWeights[0] = 0.0f;
                v.boneWeights[1] = 1.0f;
            } else {
                v.boneWeights[0] = 1.0f;
                v.boneWeights[1] = 0.0f;
            }
            applyBrushToVertex(v, targetBoneIndex, factor, strength, mode);
            return;
        }
    }

    // 4. BDEF4 and QDEF handling
    if (v.weightType == WeightType::BDEF4 || v.weightType == WeightType::QDEF) {
        int matchIdx = -1;
        for (int i = 0; i < 4; ++i) {
            if (v.boneIndices[i] == targetBoneIndex) {
                matchIdx = i;
                break;
            }
        }

        if (matchIdx < 0) {
            if (mode == BrushMode::Subtract) {
                return; // Nothing to subtract
            }
            // Find empty or near-zero slot
            for (int i = 0; i < 4; ++i) {
                if (v.boneIndices[i] < 0 || v.boneWeights[i] <= 1e-5f) {
                    matchIdx = i;
                    v.boneIndices[i] = targetBoneIndex;
                    v.boneWeights[i] = 0.0f;
                    break;
                }
            }
            // If all 4 slots are occupied, replace the minimum weight slot
            if (matchIdx < 0) {
                int minIdx = 0;
                float minW = v.boneWeights[0];
                for (int i = 1; i < 4; ++i) {
                    if (v.boneWeights[i] < minW) {
                        minW = v.boneWeights[i];
                        minIdx = i;
                    }
                }
                matchIdx = minIdx;
                v.boneIndices[minIdx] = targetBoneIndex;
                v.boneWeights[minIdx] = 0.0f;
            }
        }

        float oldW = v.boneWeights[matchIdx];
        float newW = oldW;
        if (mode == BrushMode::Add) {
            newW = std::min(1.0f, oldW + delta);
        } else if (mode == BrushMode::Subtract) {
            newW = std::max(0.0f, oldW - delta);
        } else if (mode == BrushMode::Set) {
            newW = std::clamp(oldW + (s - oldW) * f, 0.0f, 1.0f);
        } else if (mode == BrushMode::Smooth) {
            int activeCount = 0;
            for (int i = 0; i < 4; ++i) {
                if (v.boneIndices[i] >= 0 && v.boneWeights[i] > 1e-4f) {
                    activeCount++;
                }
            }
            float targetShare = (activeCount > 0) ? (1.0f / static_cast<float>(activeCount)) : 0.25f;
            newW = std::clamp(oldW + (targetShare - oldW) * delta, 0.0f, 1.0f);
        }

        // Adjust other bones proportionally so sum remains 1.0f
        float otherOldSum = 0.0f;
        for (int i = 0; i < 4; ++i) {
            if (i != matchIdx) otherOldSum += v.boneWeights[i];
        }
        float otherNewSum = 1.0f - newW;

        if (otherOldSum > 1e-6f) {
            float scale = otherNewSum / otherOldSum;
            for (int i = 0; i < 4; ++i) {
                if (i != matchIdx) {
                    v.boneWeights[i] = std::max(0.0f, v.boneWeights[i] * scale);
                }
            }
        } else {
            // Distribute among other valid slots
            int otherValidCount = 0;
            for (int i = 0; i < 4; ++i) {
                if (i != matchIdx && v.boneIndices[i] >= 0) otherValidCount++;
            }
            if (otherValidCount > 0) {
                float share = otherNewSum / static_cast<float>(otherValidCount);
                for (int i = 0; i < 4; ++i) {
                    if (i != matchIdx && v.boneIndices[i] >= 0) {
                        v.boneWeights[i] = share;
                    }
                }
            }
        }
        v.boneWeights[matchIdx] = newW;

        // Zero floating-point accumulation drift: primary bone gets 1.0 - sum(others)
        float otherSumFinal = 0.0f;
        for (int i = 0; i < 4; ++i) {
            if (i != matchIdx) otherSumFinal += v.boneWeights[i];
        }
        v.boneWeights[matchIdx] = std::clamp(1.0f - otherSumFinal, 0.0f, 1.0f);
    }
}

std::vector<LineVertex> PmxWeightPaintBrush::buildBrushCursor(
    const glm::vec3& center,
    const glm::vec3& normal,
    float radius,
    int segments)
{
    if (!std::isfinite(radius) || radius <= 0.001f ||
        !std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z)) {
        return {};
    }

    if (segments < 3) segments = 32;
    if (segments > 128) segments = 128;

    glm::vec3 n(0.0f, 1.0f, 0.0f);
    if (std::isfinite(normal.x) && std::isfinite(normal.y) && std::isfinite(normal.z) &&
        glm::length(normal) > 1e-4f) {
        n = glm::normalize(normal);
    }

    glm::vec3 u;
    if (std::abs(n.z) < 0.999f) {
        u = glm::normalize(glm::cross(n, glm::vec3(0.0f, 0.0f, 1.0f)));
    } else {
        u = glm::normalize(glm::cross(n, glm::vec3(0.0f, 1.0f, 0.0f)));
    }
    glm::vec3 v = glm::normalize(glm::cross(n, u));

    // Highlight Yellow (1.0, 0.95, 0.2, 1.0)
    const glm::vec4 cursorColor(1.0f, 0.95f, 0.2f, 1.0f);

    std::vector<LineVertex> lines;
    lines.reserve(static_cast<size_t>(segments * 2));

    for (int i = 0; i < segments; ++i) {
        float theta1 = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
        float theta2 = 2.0f * glm::pi<float>() * static_cast<float>(i + 1) / static_cast<float>(segments);

        glm::vec3 p1 = center + radius * (std::cos(theta1) * u + std::sin(theta1) * v);
        glm::vec3 p2 = center + radius * (std::cos(theta2) * u + std::sin(theta2) * v);

        lines.push_back({ p1, cursorColor });
        lines.push_back({ p2, cursorColor });
    }

    return lines;
}

} // namespace pmx
