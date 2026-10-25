#pragma once

#include "core/PmxDocument.h"
#include <glm/vec3.hpp>
#include <QString>

namespace pmx {

struct MergeOptions {
    bool mergeIdenticalBones = true;     // Deduplicate bones with matching nameJp / nameEn
    glm::vec3 offsetPosition{0.0f};      // Translation offset applied to incoming vertices and bones
    bool importPhysics = true;           // Merge rigid bodies and joints
    bool importMorphs = true;            // Merge morphs
    bool importDisplayFrames = true;     // Merge display frames
};

class PmxMerger {
public:
    // Merges incomingDoc into baseDoc in-place, returning true on success
    static bool merge(PmxDocument& baseDoc, const PmxDocument& incomingDoc, const MergeOptions& options = {}, QString* errorMessage = nullptr);
};

} // namespace pmx
