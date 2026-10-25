#include "PmxPhysicsEngine.h"
#include "PmxRuntimeScene.h"

#include <algorithm>
#include <cmath>

namespace pmx {

PmxPhysicsEngine::PmxPhysicsEngine() = default;

PmxPhysicsEngine::~PmxPhysicsEngine() {
    clear();
}

PmxPhysicsEngine::PmxPhysicsEngine(PmxPhysicsEngine&& other) noexcept
    : m_collisionConfiguration(std::move(other.m_collisionConfiguration)),
      m_dispatcher(std::move(other.m_dispatcher)),
      m_broadphase(std::move(other.m_broadphase)),
      m_solver(std::move(other.m_solver)),
      m_dynamicsWorld(std::move(other.m_dynamicsWorld)),
      m_collisionShapes(std::move(other.m_collisionShapes)),
      m_motionStates(std::move(other.m_motionStates)),
      m_rigidBodies(std::move(other.m_rigidBodies)),
      m_constraints(std::move(other.m_constraints)),
      m_bodyMetadata(std::move(other.m_bodyMetadata)) {}

PmxPhysicsEngine& PmxPhysicsEngine::operator=(PmxPhysicsEngine&& other) noexcept {
    if (this != &other) {
        clear();
        m_collisionConfiguration = std::move(other.m_collisionConfiguration);
        m_dispatcher = std::move(other.m_dispatcher);
        m_broadphase = std::move(other.m_broadphase);
        m_solver = std::move(other.m_solver);
        m_dynamicsWorld = std::move(other.m_dynamicsWorld);
        m_collisionShapes = std::move(other.m_collisionShapes);
        m_motionStates = std::move(other.m_motionStates);
        m_rigidBodies = std::move(other.m_rigidBodies);
        m_constraints = std::move(other.m_constraints);
        m_bodyMetadata = std::move(other.m_bodyMetadata);
    }
    return *this;
}

void PmxPhysicsEngine::clear() {
    if (m_dynamicsWorld) {
        for (auto& c : m_constraints) {
            m_dynamicsWorld->removeConstraint(c.get());
        }
        for (auto& rb : m_rigidBodies) {
            m_dynamicsWorld->removeRigidBody(rb.get());
        }
    }
    m_constraints.clear();
    m_rigidBodies.clear();
    m_motionStates.clear();
    m_collisionShapes.clear();
    m_bodyMetadata.clear();

    m_dynamicsWorld.reset();
    m_solver.reset();
    m_broadphase.reset();
    m_dispatcher.reset();
    m_collisionConfiguration.reset();
}

glm::mat4 PmxPhysicsEngine::btTransformToGlm(const btTransform& t) {
    glm::mat4 m(1.0f);
    const btMatrix3x3& basis = t.getBasis();
    const btVector3& origin = t.getOrigin();
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            m[c][r] = static_cast<float>(basis[r][c]);
        }
        m[3][r] = static_cast<float>(origin[r]);
    }
    return m;
}

btTransform PmxPhysicsEngine::glmToBtTransform(const glm::mat4& m) {
    btTransform t;
    btMatrix3x3 basis;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            basis[r][c] = static_cast<btScalar>(m[c][r]);
        }
    }
    t.setBasis(basis);
    t.setOrigin(btVector3(
        static_cast<btScalar>(m[3][0]),
        static_cast<btScalar>(m[3][1]),
        static_cast<btScalar>(m[3][2])
    ));
    return t;
}

btTransform PmxPhysicsEngine::createBtTransform(const glm::vec3& posLH, const glm::vec3& rotLH) {
    // LH (Y-Up, +Z Forward) -> RH (Y-Up, -Z Forward)
    glm::vec3 posRH(posLH.x, posLH.y, -posLH.z);
    glm::vec3 rotRH(-rotLH.x, -rotLH.y, rotLH.z);

    btTransform t;
    t.setIdentity();
    glm::quat qx = glm::angleAxis(rotRH.x, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat qy = glm::angleAxis(rotRH.y, glm::vec3(0.0f, 1.0f, 0.0f));
    glm::quat qz = glm::angleAxis(rotRH.z, glm::vec3(0.0f, 0.0f, 1.0f));
    glm::quat q = glm::normalize(qy * qx * qz);
    t.setRotation(btQuaternion(
        static_cast<btScalar>(q.x),
        static_cast<btScalar>(q.y),
        static_cast<btScalar>(q.z),
        static_cast<btScalar>(q.w)
    ));
    t.setOrigin(btVector3(
        static_cast<btScalar>(posRH.x),
        static_cast<btScalar>(posRH.y),
        static_cast<btScalar>(posRH.z)
    ));
    return t;
}

void PmxPhysicsEngine::init(const PmxDocument& doc, const PmxRuntimeScene& scene) {
    clear();

    // 1. World Setup
    m_collisionConfiguration = std::make_unique<btDefaultCollisionConfiguration>();
    m_dispatcher = std::make_unique<btCollisionDispatcher>(m_collisionConfiguration.get());
    m_broadphase = std::make_unique<btDbvtBroadphase>();
    m_solver = std::make_unique<btSequentialImpulseConstraintSolver>();
    m_dynamicsWorld = std::make_unique<btDiscreteDynamicsWorld>(
        m_dispatcher.get(), m_broadphase.get(), m_solver.get(), m_collisionConfiguration.get());
    
    // Standard MMD gravity: g = (0.0f, -98.0f, 0.0f)
    m_dynamicsWorld->setGravity(btVector3(0.0f, -98.0f, 0.0f));

    // 2. Rigid Bodies
    const size_t numBodies = doc.rigidBodies.size();
    m_collisionShapes.reserve(numBodies);
    m_motionStates.reserve(numBodies);
    m_rigidBodies.reserve(numBodies);
    m_bodyMetadata.reserve(numBodies);

    for (size_t i = 0; i < numBodies; ++i) {
        const auto& src = doc.rigidBodies[i];

        // Create shape:
        // Shape 0: Sphere (radius size.x)
        // Shape 1: Box (extents size.x, size.y, size.z)
        // Shape 2: Capsule (radius size.x, height size.y)
        // Non-Zero Dimension Clamping
        float dimX = std::max(src.size.x, 1e-4f);
        float dimY = std::max(src.size.y, 1e-4f);
        float dimZ = std::max(src.size.z, 1e-4f);

        std::unique_ptr<btCollisionShape> shape;
        if (src.shape == 0) {
            shape = std::make_unique<btSphereShape>(static_cast<btScalar>(dimX));
        } else if (src.shape == 1) {
            shape = std::make_unique<btBoxShape>(btVector3(
                static_cast<btScalar>(dimX),
                static_cast<btScalar>(dimY),
                static_cast<btScalar>(dimZ)
            ));
        } else if (src.shape == 2) {
            shape = std::make_unique<btCapsuleShape>(
                static_cast<btScalar>(dimX),
                static_cast<btScalar>(dimY)
            );
        } else {
            shape = std::make_unique<btSphereShape>(static_cast<btScalar>(dimX));
        }

        // Rest transform in RH coordinates
        btTransform restBtTrans = createBtTransform(src.position, src.rotation);
        glm::mat4 restTransform = btTransformToGlm(restBtTrans);

        RuntimeRigidBody meta;
        meta.boneIndex = src.boneIndex;
        meta.physicsMode = src.physicsMode;
        meta.restTransform = restTransform;
        meta.restBtTransform = restBtTrans;

        if (src.boneIndex >= 0 && src.boneIndex < scene.boneCount()) {
            glm::mat4 boneRestWorld = scene.worldMatrix(src.boneIndex);
            meta.boneToBodyOffset = glm::inverse(boneRestWorld) * restTransform;
            meta.bodyToBoneOffset = glm::inverse(meta.boneToBodyOffset);
        } else {
            meta.boneToBodyOffset = restTransform;
            meta.bodyToBoneOffset = glm::inverse(restTransform);
        }

        // Mass and inertia calculation:
        // physicsMode 0: FollowBone (Kinematic) -> mass = 0.0f
        // physicsMode 1: Physics (Dynamic) -> mass = src.mass
        // physicsMode 2: PhysicsAndBone -> mass = src.mass
        btScalar mass = 0.0f;
        btVector3 localInertia(0.0f, 0.0f, 0.0f);
        if (src.physicsMode >= 1) {
            mass = static_cast<btScalar>(src.mass);
            if (mass > 0.0f) {
                shape->calculateLocalInertia(mass, localInertia);
            }
        }

        auto motionState = std::make_unique<btDefaultMotionState>(restBtTrans);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState.get(), shape.get(), localInertia);
        rbInfo.m_linearDamping = static_cast<btScalar>(src.linearDamping);
        rbInfo.m_angularDamping = static_cast<btScalar>(src.angularDamping);
        rbInfo.m_restitution = static_cast<btScalar>(src.restitution);
        rbInfo.m_friction = static_cast<btScalar>(src.friction);

        auto body = std::make_unique<btRigidBody>(rbInfo);

        if (src.physicsMode == 0) {
            body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            body->setActivationState(DISABLE_DEACTIVATION);
        } else {
            body->setActivationState(DISABLE_DEACTIVATION);
        }

        // Collision groups & masks
        int group = (src.groupIndex < 16) ? (1 << src.groupIndex) : 1;
        int mask = static_cast<int>(static_cast<uint16_t>(~src.nonCollisionMask));

        m_dynamicsWorld->addRigidBody(body.get(), group, mask);

        m_collisionShapes.push_back(std::move(shape));
        m_motionStates.push_back(std::move(motionState));
        m_rigidBodies.push_back(std::move(body));
        m_bodyMetadata.push_back(meta);
    }

    // 3. 6DoF Spring Constraints
    const size_t numJoints = doc.joints.size();
    m_constraints.reserve(numJoints);

    for (size_t i = 0; i < numJoints; ++i) {
        const auto& src = doc.joints[i];

        if (src.rigidBodyIndexA < 0 || src.rigidBodyIndexA >= static_cast<int32_t>(m_rigidBodies.size()) ||
            src.rigidBodyIndexB < 0 || src.rigidBodyIndexB >= static_cast<int32_t>(m_rigidBodies.size()) ||
            src.rigidBodyIndexA == src.rigidBodyIndexB) {
            continue;
        }

        auto* bodyA = m_rigidBodies[static_cast<size_t>(src.rigidBodyIndexA)].get();
        auto* bodyB = m_rigidBodies[static_cast<size_t>(src.rigidBodyIndexB)].get();

        btTransform jointWorldTrans = createBtTransform(src.position, src.rotation);

        btTransform frameInA = bodyA->getWorldTransform().inverse() * jointWorldTrans;
        btTransform frameInB = bodyB->getWorldTransform().inverse() * jointWorldTrans;

        auto constraint = std::make_unique<btGeneric6DofSpringConstraint>(*bodyA, *bodyB, frameInA, frameInB, true);

        // Linear limits:
        btVector3 linLower(
            static_cast<btScalar>(std::min(src.linearLowerLimit.x, src.linearUpperLimit.x)),
            static_cast<btScalar>(std::min(src.linearLowerLimit.y, src.linearUpperLimit.y)),
            static_cast<btScalar>(std::min(-src.linearUpperLimit.z, -src.linearLowerLimit.z))
        );
        btVector3 linUpper(
            static_cast<btScalar>(std::max(src.linearLowerLimit.x, src.linearUpperLimit.x)),
            static_cast<btScalar>(std::max(src.linearLowerLimit.y, src.linearUpperLimit.y)),
            static_cast<btScalar>(std::max(-src.linearUpperLimit.z, -src.linearLowerLimit.z))
        );
        constraint->setLinearLowerLimit(linLower);
        constraint->setLinearUpperLimit(linUpper);

        // Angular limits:
        // min X_RH = -max X_LH, max X_RH = -min X_LH
        // min Y_RH = -max Y_LH, max Y_RH = -min Y_LH
        // min Z_RH = min Z_LH, max Z_RH = max Z_LH
        btVector3 angMin(
            static_cast<btScalar>(-src.angularUpperLimit.x),
            static_cast<btScalar>(-src.angularUpperLimit.y),
            static_cast<btScalar>(src.angularLowerLimit.z)
        );
        btVector3 angMax(
            static_cast<btScalar>(-src.angularLowerLimit.x),
            static_cast<btScalar>(-src.angularLowerLimit.y),
            static_cast<btScalar>(src.angularUpperLimit.z)
        );
        btVector3 angLower(
            std::min(angMin.x(), angMax.x()),
            std::min(angMin.y(), angMax.y()),
            std::min(angMin.z(), angMax.z())
        );
        btVector3 angUpper(
            std::max(angMin.x(), angMax.x()),
            std::max(angMin.y(), angMax.y()),
            std::max(angMin.z(), angMax.z())
        );
        constraint->setAngularLowerLimit(angLower);
        constraint->setAngularUpperLimit(angUpper);

        // Springs
        // Linear spring axes (0 = X, 1 = Y, 2 = Z)
        const float linStiffness[3] = {src.springLinearStiffness.x, src.springLinearStiffness.y, src.springLinearStiffness.z};
        for (int axis = 0; axis < 3; ++axis) {
            if (linStiffness[axis] > 0.0f) {
                constraint->enableSpring(axis, true);
                constraint->setStiffness(axis, static_cast<btScalar>(linStiffness[axis]));
            }
        }

        // Angular spring axes (3 = X, 4 = Y, 5 = Z)
        const float angStiffness[3] = {src.springAngularStiffness.x, src.springAngularStiffness.y, src.springAngularStiffness.z};
        for (int axis = 0; axis < 3; ++axis) {
            if (angStiffness[axis] > 0.0f) {
                constraint->enableSpring(axis + 3, true);
                constraint->setStiffness(axis + 3, static_cast<btScalar>(angStiffness[axis]));
            }
        }

        constraint->setEquilibriumPoint();

        // Add constraint with linked body collision disabled
        m_dynamicsWorld->addConstraint(constraint.get(), true);
        m_constraints.push_back(std::move(constraint));
    }
}

void PmxPhysicsEngine::stepSimulation(float deltaTime, int maxSubSteps, float fixedTimeStep) {
    if (m_dynamicsWorld && deltaTime > 0.0f) {
        m_dynamicsWorld->stepSimulation(
            static_cast<btScalar>(deltaTime),
            maxSubSteps,
            static_cast<btScalar>(fixedTimeStep)
        );
    }
}

void PmxPhysicsEngine::syncKinematicBodies(PmxRuntimeScene& scene) {
    for (size_t i = 0; i < m_rigidBodies.size(); ++i) {
        const auto& meta = m_bodyMetadata[i];
        if (meta.physicsMode == 0 && meta.boneIndex >= 0 && meta.boneIndex < scene.boneCount()) {
            glm::mat4 boneWorld = scene.worldMatrix(meta.boneIndex);
            glm::mat4 bodyWorld = boneWorld * meta.boneToBodyOffset;
            btTransform trans = glmToBtTransform(bodyWorld);

            auto* body = m_rigidBodies[i].get();
            if (body->getMotionState()) {
                body->getMotionState()->setWorldTransform(trans);
            }
            body->setWorldTransform(trans);
        }
    }
}

void PmxPhysicsEngine::syncDynamicBones(PmxRuntimeScene& scene) {
    for (size_t i = 0; i < m_rigidBodies.size(); ++i) {
        const auto& meta = m_bodyMetadata[i];
        if ((meta.physicsMode == 1 || meta.physicsMode == 2) &&
            meta.boneIndex >= 0 && meta.boneIndex < scene.boneCount()) {
            auto* body = m_rigidBodies[i].get();
            btTransform trans;
            if (body->getMotionState()) {
                body->getMotionState()->getWorldTransform(trans);
            } else {
                trans = body->getWorldTransform();
            }
            glm::mat4 bodyWorld = btTransformToGlm(trans);
            glm::mat4 boneWorld = bodyWorld * meta.bodyToBoneOffset;
            if (meta.physicsMode == 2) {
                // For PhysicsAndBone (2), preserve the bone's pre-physics translation
                // while applying only the physics rotation.
                glm::vec3 preservedTranslation = scene.worldPosition(meta.boneIndex);
                boneWorld[3] = glm::vec4(preservedTranslation, 1.0f);
            }
            scene.setBoneWorldMatrix(meta.boneIndex, boneWorld);
        }
    }
}

void PmxPhysicsEngine::reset() {
    for (size_t i = 0; i < m_rigidBodies.size(); ++i) {
        auto* body = m_rigidBodies[i].get();
        const auto& meta = m_bodyMetadata[i];

        body->setWorldTransform(meta.restBtTransform);
        if (body->getMotionState()) {
            body->getMotionState()->setWorldTransform(meta.restBtTransform);
        }
        body->setLinearVelocity(btVector3(0.0f, 0.0f, 0.0f));
        body->setAngularVelocity(btVector3(0.0f, 0.0f, 0.0f));
        body->clearForces();

        if (meta.physicsMode >= 1) {
            body->activate(true);
        }
    }

    for (auto& constraint : m_constraints) {
        constraint->setEquilibriumPoint();
    }
}

btRigidBody* PmxPhysicsEngine::rigidBody(int index) const {
    if (index >= 0 && index < static_cast<int>(m_rigidBodies.size())) {
        return m_rigidBodies[static_cast<size_t>(index)].get();
    }
    return nullptr;
}

const RuntimeRigidBody* PmxPhysicsEngine::runtimeRigidBody(int index) const {
    if (index >= 0 && index < static_cast<int>(m_bodyMetadata.size())) {
        return &m_bodyMetadata[static_cast<size_t>(index)];
    }
    return nullptr;
}

glm::mat4 PmxPhysicsEngine::rigidBodyTransform(int index) const {
    if (index >= 0 && index < static_cast<int>(m_rigidBodies.size())) {
        btTransform trans;
        if (m_rigidBodies[static_cast<size_t>(index)]->getMotionState()) {
            m_rigidBodies[static_cast<size_t>(index)]->getMotionState()->getWorldTransform(trans);
        } else {
            trans = m_rigidBodies[static_cast<size_t>(index)]->getWorldTransform();
        }
        return btTransformToGlm(trans);
    }
    return glm::mat4(1.0f);
}

glm::vec3 PmxPhysicsEngine::rigidBodyPosition(int index) const {
    if (index >= 0 && index < static_cast<int>(m_rigidBodies.size())) {
        btTransform trans;
        if (m_rigidBodies[static_cast<size_t>(index)]->getMotionState()) {
            m_rigidBodies[static_cast<size_t>(index)]->getMotionState()->getWorldTransform(trans);
        } else {
            trans = m_rigidBodies[static_cast<size_t>(index)]->getWorldTransform();
        }
        const btVector3& o = trans.getOrigin();
        return glm::vec3(static_cast<float>(o.x()), static_cast<float>(o.y()), static_cast<float>(o.z()));
    }
    return glm::vec3(0.0f);
}

glm::vec3 PmxPhysicsEngine::rigidBodyLinearVelocity(int index) const {
    if (index >= 0 && index < static_cast<int>(m_rigidBodies.size())) {
        const btVector3& v = m_rigidBodies[static_cast<size_t>(index)]->getLinearVelocity();
        return glm::vec3(static_cast<float>(v.x()), static_cast<float>(v.y()), static_cast<float>(v.z()));
    }
    return glm::vec3(0.0f);
}

glm::vec3 PmxPhysicsEngine::rigidBodyAngularVelocity(int index) const {
    if (index >= 0 && index < static_cast<int>(m_rigidBodies.size())) {
        const btVector3& w = m_rigidBodies[static_cast<size_t>(index)]->getAngularVelocity();
        return glm::vec3(static_cast<float>(w.x()), static_cast<float>(w.y()), static_cast<float>(w.z()));
    }
    return glm::vec3(0.0f);
}

btGeneric6DofSpringConstraint* PmxPhysicsEngine::constraint(int index) const {
    if (index >= 0 && index < static_cast<int>(m_constraints.size())) {
        return m_constraints[static_cast<size_t>(index)].get();
    }
    return nullptr;
}

void PmxPhysicsEngine::setGravity(const glm::vec3& gravity) {
    if (m_dynamicsWorld) {
        m_dynamicsWorld->setGravity(btVector3(
            static_cast<btScalar>(gravity.x),
            static_cast<btScalar>(gravity.y),
            static_cast<btScalar>(gravity.z)
        ));
    }
}

glm::vec3 PmxPhysicsEngine::gravity() const {
    if (m_dynamicsWorld) {
        btVector3 g = m_dynamicsWorld->getGravity();
        return glm::vec3(static_cast<float>(g.x()), static_cast<float>(g.y()), static_cast<float>(g.z()));
    }
    return glm::vec3(0.0f, -98.0f, 0.0f);
}

} // namespace pmx
