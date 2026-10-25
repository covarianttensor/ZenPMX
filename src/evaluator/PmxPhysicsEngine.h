#pragma once

#include "core/PmxDocument.h"
#include "core/PmxTypes.h"

#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <vector>

namespace pmx {

class PmxRuntimeScene;

struct RuntimeRigidBody {
    int32_t boneIndex = -1;
    uint8_t physicsMode = 0; // 0=FollowBone (Kinematic), 1=Physics (Dynamic), 2=PhysicsAndBone
    glm::mat4 restTransform{1.0f};    // RH model-space rest transform
    glm::mat4 boneToBodyOffset{1.0f}; // T_bone->body = W_bone_rest^-1 * W_body_rest
    glm::mat4 bodyToBoneOffset{1.0f}; // T_bone->body^-1
    btTransform restBtTransform;      // Cached rest pose in Bullet format
};

class PmxPhysicsEngine {
public:
    PmxPhysicsEngine();
    ~PmxPhysicsEngine();

    // Non-copyable, movable
    PmxPhysicsEngine(const PmxPhysicsEngine&) = delete;
    PmxPhysicsEngine& operator=(const PmxPhysicsEngine&) = delete;
    PmxPhysicsEngine(PmxPhysicsEngine&&) noexcept;
    PmxPhysicsEngine& operator=(PmxPhysicsEngine&&) noexcept;

    void init(const PmxDocument& doc, const PmxRuntimeScene& scene);
    void reset(PmxRuntimeScene* scene = nullptr);
    void resetToBoneTransforms(PmxRuntimeScene& scene);

    void stepSimulation(float deltaTime, int maxSubSteps = 10, float fixedTimeStep = 1.0f / 60.0f);
    void syncKinematicBodies(PmxRuntimeScene& scene);
    void syncDynamicBones(PmxRuntimeScene& scene);

    // Accessors
    int rigidBodyCount() const { return static_cast<int>(m_rigidBodies.size()); }
    btRigidBody* rigidBody(int index) const;
    const RuntimeRigidBody* runtimeRigidBody(int index) const;
    glm::mat4 rigidBodyTransform(int index) const;
    glm::vec3 rigidBodyPosition(int index) const;
    glm::vec3 rigidBodyLinearVelocity(int index) const;
    glm::vec3 rigidBodyAngularVelocity(int index) const;

    int constraintCount() const { return static_cast<int>(m_constraints.size()); }
    btGeneric6DofSpringConstraint* constraint(int index) const;

    btDiscreteDynamicsWorld* dynamicsWorld() const { return m_dynamicsWorld.get(); }

    void setGravity(const glm::vec3& gravity);
    glm::vec3 gravity() const;

    // Coordinate conversion utilities
    static glm::mat4 btTransformToGlm(const btTransform& t);
    static btTransform glmToBtTransform(const glm::mat4& m);
    static btTransform createBtTransform(const glm::vec3& posLH, const glm::vec3& rotLH);

private:
    void clear();

    std::unique_ptr<btDefaultCollisionConfiguration> m_collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> m_dispatcher;
    std::unique_ptr<btBroadphaseInterface> m_broadphase;
    std::unique_ptr<btSequentialImpulseConstraintSolver> m_solver;
    std::unique_ptr<btDiscreteDynamicsWorld> m_dynamicsWorld;

    std::vector<std::unique_ptr<btCollisionShape>> m_collisionShapes;
    std::vector<std::unique_ptr<btDefaultMotionState>> m_motionStates;
    std::vector<std::unique_ptr<btRigidBody>> m_rigidBodies;
    std::vector<std::unique_ptr<btGeneric6DofSpringConstraint>> m_constraints;
    std::vector<RuntimeRigidBody> m_bodyMetadata;
};

} // namespace pmx
