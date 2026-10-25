#include "PmxAnimationPlayer.h"
#include "PmxRuntimeScene.h"

#include <algorithm>
#include <cmath>

namespace pmx {

PmxAnimationPlayer::PmxAnimationPlayer() = default;
PmxAnimationPlayer::~PmxAnimationPlayer() = default;

void PmxAnimationPlayer::loadMotion(const VmdMotion& motion) {
    m_motion = motion;
    m_maxFrame = motion.maxFrame;
    m_currentFrame = 0.0f;
    m_isPlaying = false;

    m_boneTracks.clear();
    for (const auto& kf : m_motion.boneKeyframes) {
        m_boneTracks[kf.boneName].push_back(kf);
        m_maxFrame = std::max(m_maxFrame, kf.frame);
    }
    for (auto it = m_boneTracks.begin(); it != m_boneTracks.end(); ++it) {
        auto& track = it.value();
        std::sort(track.begin(), track.end(), [](const auto& a, const auto& b) {
            return a.frame < b.frame;
        });
    }

    m_morphTracks.clear();
    for (const auto& kf : m_motion.morphKeyframes) {
        m_morphTracks[kf.morphName].push_back(kf);
        m_maxFrame = std::max(m_maxFrame, kf.frame);
    }
    for (auto it = m_morphTracks.begin(); it != m_morphTracks.end(); ++it) {
        auto& track = it.value();
        std::sort(track.begin(), track.end(), [](const auto& a, const auto& b) {
            return a.frame < b.frame;
        });
    }

    m_ikTrack = m_motion.ikKeyframes;
    for (const auto& kf : m_ikTrack) {
        m_maxFrame = std::max(m_maxFrame, kf.frame);
    }
    std::sort(m_ikTrack.begin(), m_ikTrack.end(), [](const auto& a, const auto& b) {
        return a.frame < b.frame;
    });
}

void PmxAnimationPlayer::loadPose(const VpdPose& pose) {
    m_motion = VmdMotion{};
    m_motion.modelName = pose.modelName;
    m_motion.maxFrame = 0;
    m_maxFrame = 0;
    m_currentFrame = 0.0f;
    m_isPlaying = false;

    m_boneTracks.clear();
    for (const auto& bp : pose.bones) {
        VmdBoneKeyframe kf;
        kf.boneName = bp.boneName;
        kf.frame = 0;
        kf.translation = bp.translation;
        kf.rotation = bp.rotation;
        m_boneTracks[bp.boneName].push_back(kf);
        m_motion.boneKeyframes.push_back(kf);
    }

    m_morphTracks.clear();
    for (const auto& mp : pose.morphs) {
        VmdMorphKeyframe kf;
        kf.morphName = mp.morphName;
        kf.frame = 0;
        kf.weight = mp.weight;
        m_morphTracks[mp.morphName].push_back(kf);
        m_motion.morphKeyframes.push_back(kf);
    }

    m_ikTrack.clear();
}

void PmxAnimationPlayer::setFrame(float frame) {
    if (std::isnan(frame) || frame < 0.0f) {
        m_currentFrame = 0.0f;
        return;
    }
    if (m_maxFrame > 0) {
        m_currentFrame = std::clamp(frame, 0.0f, static_cast<float>(m_maxFrame));
    } else {
        m_currentFrame = frame;
    }
}

void PmxAnimationPlayer::play() {
    if (m_maxFrame > 0 && m_currentFrame >= static_cast<float>(m_maxFrame) && !m_isLoop) {
        m_currentFrame = 0.0f;
    }
    m_isPlaying = true;
}

void PmxAnimationPlayer::pause() {
    m_isPlaying = false;
}

void PmxAnimationPlayer::stop() {
    m_isPlaying = false;
    m_currentFrame = 0.0f;
}

void PmxAnimationPlayer::update(float deltaTime) {
    if (!m_isPlaying || std::isnan(deltaTime) || deltaTime <= 0.0f) {
        return;
    }

    if (std::isnan(m_currentFrame)) {
        m_currentFrame = 0.0f;
    }

    m_currentFrame += deltaTime * 30.0f;

    if (m_maxFrame > 0) {
        float maxF = static_cast<float>(m_maxFrame);
        if (m_currentFrame >= maxF) {
            if (m_isLoop) {
                m_currentFrame = std::fmod(m_currentFrame, maxF);
            } else {
                m_currentFrame = maxF;
                m_isPlaying = false;
            }
        }
    } else {
        if (!m_isLoop) {
            m_isPlaying = false;
        }
    }
}

float PmxAnimationPlayer::evaluateBezier(const uint8_t interp[4], float t) {
    if (std::isnan(t) || t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }

    uint8_t x1_raw = interp[0];
    uint8_t y1_raw = interp[1];
    uint8_t x2_raw = interp[2];
    uint8_t y2_raw = interp[3];

    if (x1_raw == y1_raw && x2_raw == y2_raw) {
        return t;
    }

    float x1 = static_cast<float>(x1_raw) / 127.0f;
    float y1 = static_cast<float>(y1_raw) / 127.0f;
    float x2 = static_cast<float>(x2_raw) / 127.0f;
    float y2 = static_cast<float>(y2_raw) / 127.0f;

    float low = 0.0f;
    float high = 1.0f;
    float s = t;

    for (int i = 0; i < 16; ++i) {
        float oneMinusS = 1.0f - s;
        float x = 3.0f * oneMinusS * oneMinusS * s * x1 +
                  3.0f * oneMinusS * s * s * x2 +
                  s * s * s;
        float err = x - t;
        if (std::abs(err) < 1e-6f) {
            break;
        }
        if (err > 0.0f) {
            high = s;
        } else {
            low = s;
        }
        float dx = 3.0f * oneMinusS * oneMinusS * x1 +
                   6.0f * oneMinusS * s * (x2 - x1) +
                   3.0f * s * s * (1.0f - x2);
        if (std::abs(dx) > 1e-6f) {
            float sNext = s - err / dx;
            if (sNext > low && sNext < high) {
                s = sNext;
                continue;
            }
        }
        s = 0.5f * (low + high);
    }

    float oneMinusS = 1.0f - s;
    float y = 3.0f * oneMinusS * oneMinusS * s * y1 +
              3.0f * oneMinusS * s * s * y2 +
              s * s * s;
    return std::clamp(y, 0.0f, 1.0f);
}

glm::vec3 PmxAnimationPlayer::evaluateBoneTranslation(const QString& boneName, float frame) const {
    auto itTrack = m_boneTracks.find(boneName);
    if (itTrack == m_boneTracks.end() || itTrack->empty()) {
        return glm::vec3(0.0f);
    }
    const auto& track = *itTrack;
    if (track.size() == 1 || std::isnan(frame) || frame <= static_cast<float>(track.front().frame)) {
        return track.front().translation;
    }
    if (frame >= static_cast<float>(track.back().frame)) {
        return track.back().translation;
    }

    auto it = std::upper_bound(track.begin(), track.end(), frame,
                               [](float f, const VmdBoneKeyframe& kf) {
                                   return f < static_cast<float>(kf.frame);
                               });
    if (it == track.end()) {
        return track.back().translation;
    }
    if (it == track.begin()) {
        return track.front().translation;
    }

    const auto& kfB = *it;
    const auto& kfA = *(it - 1);

    float span = static_cast<float>(kfB.frame - kfA.frame);
    if (span <= 0.0f) {
        return kfA.translation;
    }

    float t = (frame - static_cast<float>(kfA.frame)) / span;
    t = std::clamp(t, 0.0f, 1.0f);

    float wx = evaluateBezier(kfB.interpX, t);
    float wy = evaluateBezier(kfB.interpY, t);
    float wz = evaluateBezier(kfB.interpZ, t);

    return glm::vec3(
        glm::mix(kfA.translation.x, kfB.translation.x, wx),
        glm::mix(kfA.translation.y, kfB.translation.y, wy),
        glm::mix(kfA.translation.z, kfB.translation.z, wz)
    );
}

glm::quat PmxAnimationPlayer::evaluateBoneRotation(const QString& boneName, float frame) const {
    auto itTrack = m_boneTracks.find(boneName);
    if (itTrack == m_boneTracks.end() || itTrack->empty()) {
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    const auto& track = *itTrack;
    if (track.size() == 1 || std::isnan(frame) || frame <= static_cast<float>(track.front().frame)) {
        return track.front().rotation;
    }
    if (frame >= static_cast<float>(track.back().frame)) {
        return track.back().rotation;
    }

    auto it = std::upper_bound(track.begin(), track.end(), frame,
                               [](float f, const VmdBoneKeyframe& kf) {
                                   return f < static_cast<float>(kf.frame);
                               });
    if (it == track.end()) {
        return track.back().rotation;
    }
    if (it == track.begin()) {
        return track.front().rotation;
    }

    const auto& kfB = *it;
    const auto& kfA = *(it - 1);

    float span = static_cast<float>(kfB.frame - kfA.frame);
    if (span <= 0.0f) {
        return kfA.rotation;
    }

    float t = (frame - static_cast<float>(kfA.frame)) / span;
    t = std::clamp(t, 0.0f, 1.0f);

    float wr = evaluateBezier(kfB.interpR, t);

    glm::quat qA = glm::normalize(kfA.rotation);
    glm::quat qB = glm::normalize(kfB.rotation);
    if (glm::dot(qA, qB) < 0.0f) {
        qB = -qB;
    }
    return glm::normalize(glm::slerp(qA, qB, wr));
}

float PmxAnimationPlayer::evaluateMorphWeight(const QString& morphName, float frame) const {
    auto itTrack = m_morphTracks.find(morphName);
    if (itTrack == m_morphTracks.end() || itTrack->empty()) {
        return 0.0f;
    }
    const auto& track = *itTrack;
    if (track.size() == 1 || std::isnan(frame) || frame <= static_cast<float>(track.front().frame)) {
        return track.front().weight;
    }
    if (frame >= static_cast<float>(track.back().frame)) {
        return track.back().weight;
    }

    auto it = std::upper_bound(track.begin(), track.end(), frame,
                               [](float f, const VmdMorphKeyframe& kf) {
                                   return f < static_cast<float>(kf.frame);
                               });
    if (it == track.end()) {
        return track.back().weight;
    }
    if (it == track.begin()) {
        return track.front().weight;
    }

    const auto& kfB = *it;
    const auto& kfA = *(it - 1);

    float span = static_cast<float>(kfB.frame - kfA.frame);
    if (span <= 0.0f) {
        return kfA.weight;
    }

    float t = (frame - static_cast<float>(kfA.frame)) / span;
    t = std::clamp(t, 0.0f, 1.0f);

    return glm::mix(kfA.weight, kfB.weight, t);
}

bool PmxAnimationPlayer::evaluateIkState(const QString& ikBoneName, float frame, bool defaultState) const {
    if (m_ikTrack.empty()) {
        return defaultState;
    }

    const VmdIkKeyframe* activeKf = nullptr;
    if (std::isnan(frame) || frame < static_cast<float>(m_ikTrack.front().frame)) {
        activeKf = &m_ikTrack.front();
    } else if (frame >= static_cast<float>(m_ikTrack.back().frame)) {
        activeKf = &m_ikTrack.back();
    } else {
        auto it = std::upper_bound(m_ikTrack.begin(), m_ikTrack.end(), frame,
                                   [](float f, const VmdIkKeyframe& kf) {
                                       return f < static_cast<float>(kf.frame);
                                   });
        if (it == m_ikTrack.end()) {
            activeKf = &m_ikTrack.back();
        } else if (it == m_ikTrack.begin()) {
            activeKf = &m_ikTrack.front();
        } else {
            activeKf = &(*(it - 1));
        }
    }

    for (const auto& toggle : activeKf->ikToggles) {
        if (toggle.ikBoneName == ikBoneName) {
            return toggle.enable;
        }
    }
    return defaultState;
}

void PmxAnimationPlayer::applyToScene(PmxRuntimeScene& scene) const {
    // 1. Bones
    for (auto it = m_boneTracks.begin(); it != m_boneTracks.end(); ++it) {
        const QString& boneName = it.key();
        int boneIdx = scene.findBoneByName(boneName);
        if (boneIdx >= 0) {
            glm::vec3 trans = evaluateBoneTranslation(boneName, m_currentFrame);
            glm::quat rot = evaluateBoneRotation(boneName, m_currentFrame);
            scene.setBoneTransform(boneIdx, trans, rot);
        }
    }

    // 2. Morphs
    for (auto it = m_morphTracks.begin(); it != m_morphTracks.end(); ++it) {
        const QString& morphName = it.key();
        int morphIdx = scene.findMorphByName(morphName);
        if (morphIdx >= 0) {
            float weight = evaluateMorphWeight(morphName, m_currentFrame);
            scene.setMorphWeight(morphIdx, weight);
        }
    }

    // 3. IK States
    if (!m_ikTrack.empty()) {
        const VmdIkKeyframe* activeKf = nullptr;
        if (std::isnan(m_currentFrame) || m_currentFrame < static_cast<float>(m_ikTrack.front().frame)) {
            activeKf = &m_ikTrack.front();
        } else if (m_currentFrame >= static_cast<float>(m_ikTrack.back().frame)) {
            activeKf = &m_ikTrack.back();
        } else {
            auto it = std::upper_bound(m_ikTrack.begin(), m_ikTrack.end(), m_currentFrame,
                                       [](float f, const VmdIkKeyframe& kf) {
                                           return f < static_cast<float>(kf.frame);
                                       });
            if (it == m_ikTrack.end()) {
                activeKf = &m_ikTrack.back();
            } else if (it == m_ikTrack.begin()) {
                activeKf = &m_ikTrack.front();
            } else {
                activeKf = &(*(it - 1));
            }
        }

        for (const auto& toggle : activeKf->ikToggles) {
            int boneIdx = scene.findBoneByName(toggle.ikBoneName);
            if (boneIdx >= 0) {
                scene.setIkEnabled(boneIdx, toggle.enable);
            }
        }
    }
}

} // namespace pmx
