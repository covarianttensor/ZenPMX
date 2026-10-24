#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <QString>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pmx {

struct PmxHeader {
    float version = 2.0f;
    uint8_t textEncoding = 0;       // 0 = UTF-16LE, 1 = UTF-8
    uint8_t additionalVec4Count = 0;// 0..4
    uint8_t vertexIndexSize = 2;    // 1 (uint8), 2 (uint16), 4 (int32)
    uint8_t textureIndexSize = 1;   // 1 (int8), 2 (int16), 4 (int32)
    uint8_t materialIndexSize = 1;
    uint8_t boneIndexSize = 2;
    uint8_t morphIndexSize = 2;
    uint8_t rigidBodyIndexSize = 2;
    bool preserveIndexSizesOnSave = false; // True during round-trip tests
};

struct PmxModelInfo {
    QString modelNameJp;
    QString modelNameEn;
    QString commentJp;
    QString commentEn;
};

enum class WeightType : uint8_t {
    BDEF1 = 0, BDEF2 = 1, BDEF4 = 2, SDEF = 3, QDEF = 4
};

struct PmxVertex {
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    std::array<glm::vec4, 4> additionalUVs{};
    WeightType weightType = WeightType::BDEF1;
    std::array<int32_t, 4> boneIndices{-1, -1, -1, -1};
    std::array<float, 4> boneWeights{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 sdefC{0.0f};
    glm::vec3 sdefR0{0.0f};
    glm::vec3 sdefR1{0.0f};
    float edgeScale = 1.0f;
};

struct PmxMaterial {
    QString nameJp;
    QString nameEn;
    glm::vec4 diffuse{1.0f};
    glm::vec3 specular{0.0f};
    float specularPower = 5.0f;
    glm::vec3 ambient{0.5f};
    uint8_t drawFlags = 0x1F;
    glm::vec4 edgeColor{0.0f, 0.0f, 0.0f, 1.0f};
    float edgeSize = 1.0f;
    int32_t textureIndex = -1;
    int32_t sphereTextureIndex = -1;
    uint8_t sphereMode = 0; // 0=None, 1=Mul, 2=Add, 3=SubTex
    uint8_t sharedToonFlag = 0;
    int32_t toonIndex = -1;
    QString memo;
    int32_t faceVertexCount = 0;
};

struct PmxIkLink {
    int32_t boneIndex = -1;
    uint8_t hasAngleLimit = 0;
    glm::vec3 limitMin{0.0f};
    glm::vec3 limitMax{0.0f};
};

struct PmxBone {
    QString nameJp;
    QString nameEn;
    glm::vec3 position{0.0f};
    int32_t parentBoneIndex = -1;
    int32_t deformTier = 0;
    uint16_t boneFlags = 0x001E;
    glm::vec3 tailOffset{0.0f};
    int32_t tailBoneIndex = -1;
    int32_t inheritParentIndex = -1;
    float inheritWeight = 1.0f;
    glm::vec3 fixedAxisVector{0.0f};
    glm::vec3 localAxisX{1.0f, 0.0f, 0.0f};
    glm::vec3 localAxisZ{0.0f, 0.0f, 1.0f};
    int32_t externalKey = 0;
    int32_t ikTargetBoneIndex = -1;
    int32_t ikLoopCount = 0;
    float ikLimitAngle = 0.0f;
    std::vector<PmxIkLink> ikLinks;
};

struct VertexMorphOffset { int32_t vertexIndex = -1; glm::vec3 positionOffset{0.0f}; };
struct UvMorphOffset { int32_t vertexIndex = -1; glm::vec4 uvOffset{0.0f}; };
struct BoneMorphOffset { int32_t boneIndex = -1; glm::vec3 translation{0.0f}; glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; };
struct GroupMorphOffset { int32_t morphIndex = -1; float weight = 0.0f; };
struct ImpulseMorphOffset { int32_t rigidBodyIndex = -1; uint8_t isLocal = 0; glm::vec3 velocity{0.0f}; glm::vec3 torque{0.0f}; };
struct MaterialMorphOffset {
    int32_t materialIndex = -1;
    uint8_t opType = 0; // 0 = Multiply, 1 = Add
    glm::vec4 diffuse{1.0f};
    glm::vec3 specular{1.0f};
    float specularPower = 1.0f;
    glm::vec3 ambient{1.0f};
    glm::vec4 edgeColor{1.0f};
    float edgeSize = 1.0f;
    glm::vec4 textureTint{1.0f};
    glm::vec4 sphereTint{1.0f};
    glm::vec4 toonTint{1.0f};
};

struct PmxMorph {
    QString nameJp;
    QString nameEn;
    uint8_t panelCategory = 4;
    uint8_t morphType = 1;
    std::vector<VertexMorphOffset> vertexOffsets;
    std::vector<UvMorphOffset> uvOffsets;
    std::vector<BoneMorphOffset> boneOffsets;
    std::vector<MaterialMorphOffset> materialOffsets;
    std::vector<GroupMorphOffset> groupOffsets;
    std::vector<ImpulseMorphOffset> impulseOffsets;
};

struct PmxDisplayElement {
    uint8_t targetType = 0; // 0 = Bone, 1 = Morph
    int32_t index = -1;
};

struct PmxDisplayFrame {
    QString nameJp;
    QString nameEn;
    uint8_t specialFlag = 0;
    std::vector<PmxDisplayElement> elements;
};

struct PmxRigidBody {
    QString nameJp;
    QString nameEn;
    int32_t boneIndex = -1;
    uint8_t groupIndex = 0;
    uint16_t nonCollisionMask = 0xFFFF;
    uint8_t shape = 0; // 0=Sphere, 1=Box, 2=Capsule
    glm::vec3 size{1.0f};
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    float mass = 1.0f;
    float linearDamping = 0.5f;
    float angularDamping = 0.5f;
    float restitution = 0.0f;
    float friction = 0.5f;
    uint8_t physicsMode = 0; // 0=FollowBone, 1=Physics, 2=PhysicsAndBone
};

struct PmxJoint {
    QString nameJp;
    QString nameEn;
    uint8_t jointType = 0;
    int32_t rigidBodyIndexA = -1;
    int32_t rigidBodyIndexB = -1;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 linearLowerLimit{0.0f};
    glm::vec3 linearUpperLimit{0.0f};
    glm::vec3 angularLowerLimit{0.0f};
    glm::vec3 angularUpperLimit{0.0f};
    glm::vec3 springLinearStiffness{0.0f};
    glm::vec3 springAngularStiffness{0.0f};
};

struct PmxSoftBodyAnchor { int32_t rigidBodyIndex = -1; int32_t vertexIndex = -1; uint8_t nearMode = 0; };

struct PmxSoftBody {
    QString nameJp;
    QString nameEn;
    uint8_t shape = 0;
    int32_t materialIndex = -1;
    uint8_t groupIndex = 0;
    uint16_t nonCollisionMask = 0xFFFF;
    uint8_t flags = 0;
    int32_t blinkDistance = 0;
    int32_t clusterCount = 0;
    float totalMass = 1.0f;
    float collisionMargin = 0.05f;
    int32_t aeroModel = 0;
    std::array<float, 12> configFloats{};
    std::array<float, 6> clusterFloats{};
    std::array<int32_t, 4> iterationInts{};
    std::array<int32_t, 3> materialInts{};
    std::vector<PmxSoftBodyAnchor> anchors;
    std::vector<int32_t> pinVertexIndices;
};

struct PmxDocument {
    QString sourceFilePath;
    PmxHeader header;
    PmxModelInfo modelInfo;
    std::vector<PmxVertex> vertices;
    std::vector<int32_t> faceIndices; // Size = 3 * triangleCount
    std::vector<QString> textures;
    std::vector<PmxMaterial> materials;
    std::vector<PmxBone> bones;
    std::vector<PmxMorph> morphs;
    std::vector<PmxDisplayFrame> displayFrames;
    std::vector<PmxRigidBody> rigidBodies;
    std::vector<PmxJoint> joints;
    std::vector<PmxSoftBody> softBodies;
};

} // namespace pmx
