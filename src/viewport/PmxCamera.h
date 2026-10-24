#pragma once

#include <QObject>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pmx {

class PmxCamera : public QObject {
    Q_OBJECT
    Q_PROPERTY(float yaw READ yaw WRITE setYaw NOTIFY cameraChanged)
    Q_PROPERTY(float pitch READ pitch WRITE setPitch NOTIFY cameraChanged)
    Q_PROPERTY(float distance READ distance WRITE setDistance NOTIFY cameraChanged)
    Q_PROPERTY(float fov READ fov WRITE setFov NOTIFY cameraChanged)
    Q_PROPERTY(float nearPlane READ nearPlane WRITE setNearPlane NOTIFY cameraChanged)
    Q_PROPERTY(float farPlane READ farPlane WRITE setFarPlane NOTIFY cameraChanged)

public:
    explicit PmxCamera(QObject* parent = nullptr);
    ~PmxCamera() override = default;

    // Camera state getters
    glm::vec3 target() const { return m_target; }
    float distance() const { return m_distance; }
    float yaw() const { return m_yaw; }
    float pitch() const { return m_pitch; }
    float fov() const { return m_fov; }
    float nearPlane() const { return m_nearPlane; }
    float farPlane() const { return m_farPlane; }

    // Camera state setters with robust validation and clamping
    void setTarget(const glm::vec3& target);
    void setDistance(float distance);
    void setYaw(float yaw);
    void setPitch(float pitch);
    void setFov(float fov);
    void setNearPlane(float nearPlane);
    void setFarPlane(float farPlane);

    // Interactive operations
    void orbit(float deltaYaw, float deltaPitch);
    void pan(float dx, float dy);
    void zoom(float delta);
    Q_INVOKABLE void reset();

    // Derived matrices & positions
    glm::vec3 eyePosition() const;
    glm::mat4 viewMatrix() const;
    glm::mat4 projectionMatrix(float aspect) const;

    // Handedness conversion utilities (Section 5.1)
    static glm::vec3 toRightHanded(const glm::vec3& pLH);
    static glm::vec3 toLeftHanded(const glm::vec3& pRH);
    static glm::vec3 toRightHandedEuler(const glm::vec3& rLH);
    static glm::vec3 toLeftHandedEuler(const glm::vec3& rRH);
    static glm::quat toRightHandedQuat(const glm::quat& qLH);
    static glm::quat toLeftHandedQuat(const glm::quat& qRH);

signals:
    void cameraChanged();

private:
    glm::vec3 m_target{0.0f, 10.0f, 0.0f};
    float m_distance = 35.0f;
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_fov = 45.0f;
    float m_nearPlane = 0.1f;
    float m_farPlane = 1000.0f;
};

} // namespace pmx
