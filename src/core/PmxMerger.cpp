#include "core/PmxMerger.h"

#include <algorithm>

namespace pmx {

namespace {

uint8_t requiredVertexIndexSize(size_t count) {
    if (count > 65535) {
        return 4;
    }
    if (count > 255) {
        return 2;
    }
    return 1;
}

uint8_t requiredNonVertexIndexSize(size_t count) {
    if (count > 32767) {
        return 4;
    }
    if (count > 127) {
        return 2;
    }
    return 1;
}

} // namespace

bool PmxMerger::merge(PmxDocument& baseDoc, const PmxDocument& incomingDoc, const MergeOptions& options, QString* errorMessage) {
    if (errorMessage) {
        errorMessage->clear();
    }

    const int32_t baseVertexCount = static_cast<int32_t>(baseDoc.vertices.size());
    const int32_t baseMaterialCount = static_cast<int32_t>(baseDoc.materials.size());
    const int32_t baseBoneCount = static_cast<int32_t>(baseDoc.bones.size());
    const int32_t baseMorphCount = static_cast<int32_t>(baseDoc.morphs.size());
    const int32_t baseRigidBodyCount = static_cast<int32_t>(baseDoc.rigidBodies.size());

    // 1. Texture Remapping
    // Check if incoming texture path exists in baseDoc.textures; if found, reuse index; if not, append to baseDoc.textures.
    std::vector<int32_t> textureMap(incomingDoc.textures.size(), -1);
    for (size_t i = 0; i < incomingDoc.textures.size(); ++i) {
        const QString& incomingPath = incomingDoc.textures[i];
        int32_t foundIdx = -1;
        for (size_t j = 0; j < baseDoc.textures.size(); ++j) {
            if (baseDoc.textures[j] == incomingPath) {
                foundIdx = static_cast<int32_t>(j);
                break;
            }
        }
        if (foundIdx >= 0) {
            textureMap[i] = foundIdx;
        } else {
            int32_t newIdx = static_cast<int32_t>(baseDoc.textures.size());
            baseDoc.textures.push_back(incomingPath);
            textureMap[i] = newIdx;
        }
    }

    // 2. Bone Remapping & Deduplication
    std::vector<int32_t> boneMap(incomingDoc.bones.size(), -1);
    std::vector<int32_t> newlyAppendedBoneIndices;

    for (size_t i = 0; i < incomingDoc.bones.size(); ++i) {
        const auto& incomingBone = incomingDoc.bones[i];
        int32_t matchIdx = -1;

        if (options.mergeIdenticalBones) {
            for (int32_t b = 0; b < baseBoneCount; ++b) {
                const auto& baseBone = baseDoc.bones[static_cast<size_t>(b)];
                if (!incomingBone.nameJp.isEmpty() && !baseBone.nameJp.isEmpty()) {
                    if (baseBone.nameJp == incomingBone.nameJp) {
                        matchIdx = b;
                        break;
                    }
                } else if (!incomingBone.nameEn.isEmpty() && !baseBone.nameEn.isEmpty()) {
                    if (baseBone.nameEn == incomingBone.nameEn) {
                        matchIdx = b;
                        break;
                    }
                }
            }
        }

        if (matchIdx >= 0) {
            boneMap[i] = matchIdx;
        } else {
            PmxBone newBone = incomingBone;
            newBone.position += options.offsetPosition;
            int32_t newBoneIdx = static_cast<int32_t>(baseDoc.bones.size());
            baseDoc.bones.push_back(newBone);
            boneMap[i] = newBoneIdx;
            newlyAppendedBoneIndices.push_back(newBoneIdx);
        }
    }

    auto remapBone = [&](int32_t idx) -> int32_t {
        if (idx >= 0 && idx < static_cast<int32_t>(boneMap.size())) {
            return boneMap[static_cast<size_t>(idx)];
        }
        return -1;
    };

    // Remap references on newly appended bones
    for (int32_t newBoneIdx : newlyAppendedBoneIndices) {
        auto& bone = baseDoc.bones[static_cast<size_t>(newBoneIdx)];
        bone.parentBoneIndex = remapBone(bone.parentBoneIndex);
        if (bone.tailBoneIndex >= 0) {
            bone.tailBoneIndex = remapBone(bone.tailBoneIndex);
        }
        bone.inheritParentIndex = remapBone(bone.inheritParentIndex);
        bone.ikTargetBoneIndex = remapBone(bone.ikTargetBoneIndex);
        for (auto& link : bone.ikLinks) {
            link.boneIndex = remapBone(link.boneIndex);
        }
    }

    // 3. Vertex & Face Remapping
    baseDoc.vertices.reserve(baseDoc.vertices.size() + incomingDoc.vertices.size());
    for (const auto& incomingVertex : incomingDoc.vertices) {
        PmxVertex v = incomingVertex;
        v.position += options.offsetPosition;

        switch (v.weightType) {
        case WeightType::BDEF1:
            v.boneIndices[0] = remapBone(v.boneIndices[0]);
            break;
        case WeightType::BDEF2:
            v.boneIndices[0] = remapBone(v.boneIndices[0]);
            v.boneIndices[1] = remapBone(v.boneIndices[1]);
            break;
        case WeightType::BDEF4:
        case WeightType::QDEF:
            for (int k = 0; k < 4; ++k) {
                v.boneIndices[k] = remapBone(v.boneIndices[k]);
            }
            break;
        case WeightType::SDEF:
            v.boneIndices[0] = remapBone(v.boneIndices[0]);
            v.boneIndices[1] = remapBone(v.boneIndices[1]);
            if (options.offsetPosition != glm::vec3(0.0f)) {
                v.sdefC += options.offsetPosition;
            }
            break;
        }

        baseDoc.vertices.push_back(v);
    }

    baseDoc.faceIndices.reserve(baseDoc.faceIndices.size() + incomingDoc.faceIndices.size());
    for (int32_t faceIndex : incomingDoc.faceIndices) {
        baseDoc.faceIndices.push_back(faceIndex + baseVertexCount);
    }

    // 4. Material Remapping
    baseDoc.materials.reserve(baseDoc.materials.size() + incomingDoc.materials.size());
    for (const auto& incomingMat : incomingDoc.materials) {
        PmxMaterial mat = incomingMat;
        mat.textureIndex = (mat.textureIndex >= 0 && mat.textureIndex < static_cast<int32_t>(textureMap.size()))
            ? textureMap[static_cast<size_t>(mat.textureIndex)] : -1;
        mat.sphereTextureIndex = (mat.sphereTextureIndex >= 0 && mat.sphereTextureIndex < static_cast<int32_t>(textureMap.size()))
            ? textureMap[static_cast<size_t>(mat.sphereTextureIndex)] : -1;
        if (mat.sharedToonFlag == 0) {
            mat.toonIndex = (mat.toonIndex >= 0 && mat.toonIndex < static_cast<int32_t>(textureMap.size()))
                ? textureMap[static_cast<size_t>(mat.toonIndex)] : -1;
        }
        baseDoc.materials.push_back(mat);
    }

    // 5. Morph Remapping
    if (options.importMorphs) {
        baseDoc.morphs.reserve(baseDoc.morphs.size() + incomingDoc.morphs.size());
        for (const auto& incomingMorph : incomingDoc.morphs) {
            PmxMorph morph = incomingMorph;
            for (auto& vo : morph.vertexOffsets) {
                if (vo.vertexIndex >= 0) {
                    vo.vertexIndex += baseVertexCount;
                }
            }
            for (auto& uvo : morph.uvOffsets) {
                if (uvo.vertexIndex >= 0) {
                    uvo.vertexIndex += baseVertexCount;
                }
            }
            for (auto& bo : morph.boneOffsets) {
                bo.boneIndex = remapBone(bo.boneIndex);
            }
            for (auto& mo : morph.materialOffsets) {
                if (mo.materialIndex >= 0) {
                    mo.materialIndex += baseMaterialCount;
                }
            }
            for (auto& go : morph.groupOffsets) {
                if (go.morphIndex >= 0) {
                    go.morphIndex += baseMorphCount;
                }
            }
            for (auto& io : morph.impulseOffsets) {
                if (options.importPhysics && io.rigidBodyIndex >= 0) {
                    io.rigidBodyIndex += baseRigidBodyCount;
                } else {
                    io.rigidBodyIndex = -1;
                }
            }
            baseDoc.morphs.push_back(morph);
        }
    }

    // 6. Display Frame Remapping
    if (options.importDisplayFrames) {
        for (const auto& incomingFrame : incomingDoc.displayFrames) {
            std::vector<PmxDisplayElement> remappedElements;
            remappedElements.reserve(incomingFrame.elements.size());
            for (const auto& item : incomingFrame.elements) {
                PmxDisplayElement elem = item;
                if (elem.targetType == 0) { // Bone
                    elem.index = remapBone(elem.index);
                } else if (elem.targetType == 1) { // Morph
                    if (options.importMorphs && elem.index >= 0 && elem.index < static_cast<int32_t>(incomingDoc.morphs.size())) {
                        elem.index = baseMorphCount + elem.index;
                    } else {
                        elem.index = -1;
                    }
                }
                if (elem.index >= 0) {
                    bool alreadyInRemapped = false;
                    for (const auto& existing : remappedElements) {
                        if (existing.targetType == elem.targetType && existing.index == elem.index) {
                            alreadyInRemapped = true;
                            break;
                        }
                    }
                    if (!alreadyInRemapped) {
                        remappedElements.push_back(elem);
                    }
                }
            }

            int32_t matchFrameIdx = -1;
            for (size_t f = 0; f < baseDoc.displayFrames.size(); ++f) {
                const auto& baseFrame = baseDoc.displayFrames[f];
                if (!incomingFrame.nameJp.isEmpty() && !baseFrame.nameJp.isEmpty()) {
                    if (baseFrame.nameJp == incomingFrame.nameJp) {
                        matchFrameIdx = static_cast<int32_t>(f);
                        break;
                    }
                } else if (!incomingFrame.nameEn.isEmpty() && !baseFrame.nameEn.isEmpty()) {
                    if (baseFrame.nameEn == incomingFrame.nameEn) {
                        matchFrameIdx = static_cast<int32_t>(f);
                        break;
                    }
                }
            }

            if (matchFrameIdx >= 0) {
                auto& destFrame = baseDoc.displayFrames[static_cast<size_t>(matchFrameIdx)];
                for (const auto& elem : remappedElements) {
                    bool alreadyExists = false;
                    for (const auto& existing : destFrame.elements) {
                        if (existing.targetType == elem.targetType && existing.index == elem.index) {
                            alreadyExists = true;
                            break;
                        }
                    }
                    if (!alreadyExists) {
                        destFrame.elements.push_back(elem);
                    }
                }
            } else {
                PmxDisplayFrame newFrame = incomingFrame;
                newFrame.elements = std::move(remappedElements);
                baseDoc.displayFrames.push_back(newFrame);
            }
        }
    }

    // 7. Physics Remapping
    if (options.importPhysics) {
        baseDoc.rigidBodies.reserve(baseDoc.rigidBodies.size() + incomingDoc.rigidBodies.size());
        for (const auto& incomingRb : incomingDoc.rigidBodies) {
            PmxRigidBody rb = incomingRb;
            rb.boneIndex = remapBone(rb.boneIndex);
            rb.position += options.offsetPosition;
            baseDoc.rigidBodies.push_back(rb);
        }

        baseDoc.joints.reserve(baseDoc.joints.size() + incomingDoc.joints.size());
        for (const auto& incomingJoint : incomingDoc.joints) {
            PmxJoint joint = incomingJoint;
            joint.rigidBodyIndexA = (joint.rigidBodyIndexA >= 0 && joint.rigidBodyIndexA < static_cast<int32_t>(incomingDoc.rigidBodies.size()))
                ? (baseRigidBodyCount + joint.rigidBodyIndexA) : -1;
            joint.rigidBodyIndexB = (joint.rigidBodyIndexB >= 0 && joint.rigidBodyIndexB < static_cast<int32_t>(incomingDoc.rigidBodies.size()))
                ? (baseRigidBodyCount + joint.rigidBodyIndexB) : -1;
            joint.position += options.offsetPosition;
            baseDoc.joints.push_back(joint);
        }

        baseDoc.softBodies.reserve(baseDoc.softBodies.size() + incomingDoc.softBodies.size());
        for (const auto& incomingSb : incomingDoc.softBodies) {
            PmxSoftBody sb = incomingSb;
            sb.materialIndex = (sb.materialIndex >= 0 && sb.materialIndex < static_cast<int32_t>(incomingDoc.materials.size()))
                ? (baseMaterialCount + sb.materialIndex) : -1;
            for (auto& anchor : sb.anchors) {
                anchor.rigidBodyIndex = (anchor.rigidBodyIndex >= 0 && anchor.rigidBodyIndex < static_cast<int32_t>(incomingDoc.rigidBodies.size()))
                    ? (baseRigidBodyCount + anchor.rigidBodyIndex) : -1;
                anchor.vertexIndex = (anchor.vertexIndex >= 0 && anchor.vertexIndex < static_cast<int32_t>(incomingDoc.vertices.size()))
                    ? (baseVertexCount + anchor.vertexIndex) : -1;
            }
            for (auto& pinIdx : sb.pinVertexIndices) {
                if (pinIdx >= 0 && pinIdx < static_cast<int32_t>(incomingDoc.vertices.size())) {
                    pinIdx += baseVertexCount;
                } else {
                    pinIdx = -1;
                }
            }
            baseDoc.softBodies.push_back(sb);
        }
    }

    // 8. Header Recalculation
    baseDoc.header.vertexIndexSize = std::max({baseDoc.header.vertexIndexSize, incomingDoc.header.vertexIndexSize, requiredVertexIndexSize(baseDoc.vertices.size())});
    baseDoc.header.textureIndexSize = std::max({baseDoc.header.textureIndexSize, incomingDoc.header.textureIndexSize, requiredNonVertexIndexSize(baseDoc.textures.size())});
    baseDoc.header.materialIndexSize = std::max({baseDoc.header.materialIndexSize, incomingDoc.header.materialIndexSize, requiredNonVertexIndexSize(baseDoc.materials.size())});
    baseDoc.header.boneIndexSize = std::max({baseDoc.header.boneIndexSize, incomingDoc.header.boneIndexSize, requiredNonVertexIndexSize(baseDoc.bones.size())});
    baseDoc.header.morphIndexSize = std::max({baseDoc.header.morphIndexSize, incomingDoc.header.morphIndexSize, requiredNonVertexIndexSize(baseDoc.morphs.size())});
    baseDoc.header.rigidBodyIndexSize = std::max({baseDoc.header.rigidBodyIndexSize, incomingDoc.header.rigidBodyIndexSize, requiredNonVertexIndexSize(baseDoc.rigidBodies.size())});
    baseDoc.header.additionalVec4Count = std::max(baseDoc.header.additionalVec4Count, incomingDoc.header.additionalVec4Count);
    baseDoc.header.version = std::max(baseDoc.header.version, incomingDoc.header.version);
    if (!baseDoc.softBodies.empty() && baseDoc.header.version < 2.1f) {
        baseDoc.header.version = 2.1f;
    }
    for (const auto& v : incomingDoc.vertices) {
        if (v.weightType == WeightType::QDEF) {
            baseDoc.header.version = std::max(baseDoc.header.version, 2.1f);
            break;
        }
    }
    if (options.importMorphs) {
        for (const auto& m : incomingDoc.morphs) {
            if (m.morphType == 9 || m.morphType == 10) {
                baseDoc.header.version = std::max(baseDoc.header.version, 2.1f);
                break;
            }
        }
    }

    return true;
}

} // namespace pmx
