#pragma once

#include "core/PmxTypes.h"
#include "viewport/PmxGizmo.h"

#include <glm/glm.hpp>
#include <vector>

namespace pmx {

enum class BrushMode {
    Add = 0,
    Subtract = 1,
    Set = 2,
    Smooth = 3
};

enum class BrushFalloff {
    Linear = 0,
    Gaussian = 1,
    Constant = 2
};

class PmxWeightPaintBrush {
public:
    static glm::vec4 weightToColor(float weight);
    static float computeFalloff(float distance, float radius, BrushFalloff falloff);
    static float getVertexBoneWeight(const PmxVertex& v, int boneIndex);
    static void applyBrushToVertex(PmxVertex& v, int targetBoneIndex, float factor, float strength, BrushMode mode);
    static std::vector<LineVertex> buildBrushCursor(const glm::vec3& center, const glm::vec3& normal, float radius, int segments = 32);
};

} // namespace pmx
