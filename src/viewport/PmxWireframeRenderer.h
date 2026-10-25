#pragma once

#include "core/PmxDocument.h"
#include "viewport/PmxGizmo.h"

#include <vector>

namespace pmx {

class PmxRuntimeScene;

class PmxWireframeRenderer {
public:
    static std::vector<LineVertex> buildBoneWireframes(const PmxDocument& doc, int selectedBoneIndex = -1);
    static std::vector<LineVertex> buildBoneWireframes(const PmxDocument& doc, const PmxRuntimeScene* scene, int selectedBoneIndex = -1);

    static std::vector<LineVertex> buildRigidBodyWireframes(const PmxDocument& doc, int selectedRigidBodyIndex = -1);
    static std::vector<LineVertex> buildRigidBodyWireframes(const PmxDocument& doc, const PmxRuntimeScene* scene, int selectedRigidBodyIndex = -1);

    static std::vector<LineVertex> buildJointWireframes(const PmxDocument& doc, int selectedJointIndex = -1);
    static std::vector<LineVertex> buildJointWireframes(const PmxDocument& doc, const PmxRuntimeScene* scene, int selectedJointIndex = -1);
};

} // namespace pmx
