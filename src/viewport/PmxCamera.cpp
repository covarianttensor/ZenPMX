#include "viewport/PmxCamera.h"

namespace pmx {

PmxCamera::PmxCamera(QObject* parent)
    : QObject(parent)
{
}

void PmxCamera::setTarget(const glm::vec3& target) {
    if (std::isfinite(target.x) && std::isfinite(target.y) && std::isfinite(target.z)) {
        m_target = target;
        emit cameraChanged();
    }
}

void PmxCamera::setDistance(float distance) {
    if (std::isfinite(distance)) {
        m_distance = std::clamp(distance, 0.1f, 10000.0f);
        emit cameraChanged();
    }
}

void PmxCamera::setYaw(float yaw) {
    if (std::isfinite(yaw)) {
        m_yaw = std::remainder(yaw, 360.0f);
        emit cameraChanged();
    }
}

void PmxCamera::setPitch(float pitch) {
    if (std::isfinite(pitch)) {
        m_pitch = std::clamp(pitch, -89.0f, 89.0f);
        emit cameraChanged();
    }
}

void PmxCamera::setFov(float fov) {
    if (std::isfinite(fov) && fov > 1.0f && fov < 179.0f) {
        m_fov = fov;
        emit cameraChanged();
    }
}

void PmxCamera::setNearPlane(float nearPlane) {
    if (std::isfinite(nearPlane) && nearPlane > 0.0001f && nearPlane < m_farPlane) {
        m_nearPlane = nearPlane;
        emit cameraChanged();
    }
}

void PmxCamera::setFarPlane(float farPlane) {
    if (std::isfinite(farPlane) && farPlane > m_nearPlane) {
        m_farPlane = farPlane;
        emit cameraChanged();
    }
}

void PmxCamera::orbit(float deltaYaw, float deltaPitch) {
    if (std::isfinite(deltaYaw) && std::isfinite(deltaPitch)) {
        m_yaw = std::remainder(m_yaw + deltaYaw, 360.0f);
        m_pitch = std::clamp(m_pitch + deltaPitch, -89.0f, 89.0f);
        emit cameraChanged();
    }
}

void PmxCamera::pan(float dx, float dy) {
    if (!std::isfinite(dx) || !std::isfinite(dy)) {
        return;
    }

    glm::vec3 forward = glm::normalize(m_target - eyePosition());
    glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
    if (glm::length(right) < 1e-4f) {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        right = glm::normalize(right);
    }
    glm::vec3 up = glm::normalize(glm::cross(right, forward));

    float factor = m_distance * 0.001f;
    m_target += (right * dx + up * dy) * factor;
    emit cameraChanged();
}

void PmxCamera::zoom(float delta) {
    if (std::isfinite(delta)) {
        float factor = 1.0f - delta * 0.1f;
        if (factor < 0.1f) factor = 0.1f;
        setDistance(m_distance * factor);
    }
}

void PmxCamera::reset() {
    m_target = glm::vec3(0.0f, 10.0f, 0.0f);
    m_distance = 35.0f;
    m_yaw = 0.0f;
    m_pitch = 0.0f;
    m_fov = 45.0f;
    m_nearPlane = 0.1f;
    m_farPlane = 1000.0f;
    emit cameraChanged();
}

glm::vec3 PmxCamera::eyePosition() const {
    float radYaw = glm::radians(m_yaw);
    float radPitch = glm::radians(m_pitch);
    glm::vec3 offset(
        m_distance * std::cos(radPitch) * std::sin(radYaw),
        m_distance * std::sin(radPitch),
        m_distance * std::cos(radPitch) * std::cos(radYaw)
    );
    return m_target + offset;
}

glm::mat4 PmxCamera::viewMatrix() const {
    return glm::lookAt(eyePosition(), m_target, glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 PmxCamera::projectionMatrix(float aspect) const {
    float safeNear = std::max(0.0001f, m_nearPlane);
    float safeFar = std::max(safeNear + 0.001f, m_farPlane);
    float safeAspect = (std::isfinite(aspect) && aspect > 0.0001f) ? aspect : 1.0f;
    return glm::perspective(glm::radians(m_fov), safeAspect, safeNear, safeFar);
}

glm::vec3 PmxCamera::toRightHanded(const glm::vec3& pLH) {
    return glm::vec3(pLH.x, pLH.y, -pLH.z);
}

glm::vec3 PmxCamera::toLeftHanded(const glm::vec3& pRH) {
    return glm::vec3(pRH.x, pRH.y, -pRH.z);
}

glm::vec3 PmxCamera::toRightHandedEuler(const glm::vec3& rLH) {
    return glm::vec3(-rLH.x, -rLH.y, rLH.z);
}

glm::vec3 PmxCamera::toLeftHandedEuler(const glm::vec3& rRH) {
    return glm::vec3(-rRH.x, -rRH.y, rRH.z);
}

glm::quat PmxCamera::toRightHandedQuat(const glm::quat& qLH) {
    return glm::quat(qLH.w, -qLH.x, -qLH.y, qLH.z);
}

glm::quat PmxCamera::toLeftHandedQuat(const glm::quat& qRH) {
    return glm::quat(qRH.w, -qRH.x, -qRH.y, qRH.z);
}

} // namespace pmx
