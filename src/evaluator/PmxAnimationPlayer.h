#pragma once

#include "VmdImporter.h"
#include "VpdImporter.h"

#include <QHash>
#include <QString>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace pmx {

class PmxRuntimeScene;

class PmxAnimationPlayer {
public:
    PmxAnimationPlayer();
    ~PmxAnimationPlayer();

    // Data ingestion
    void loadMotion(const VmdMotion& motion);
    void loadPose(const VpdPose& pose);

    // Playback timeline & controls
    void setFrame(float frame);
    float currentFrame() const { return m_currentFrame; }
    uint32_t maxFrame() const { return m_maxFrame; }

    void play();
    void pause();
    void stop();
    bool isPlaying() const { return m_isPlaying; }

    void setLoop(bool loop) { m_isLoop = loop; }
    bool isLoop() const { return m_isLoop; }

    void update(float deltaTime); // advances frame by deltaTime * 30.0f
    void applyToScene(PmxRuntimeScene& scene) const;

    // Track queries & evaluation helpers
    const VmdMotion& motion() const { return m_motion; }
    glm::vec3 evaluateBoneTranslation(const QString& boneName, float frame) const;
    glm::quat evaluateBoneRotation(const QString& boneName, float frame) const;
    float evaluateMorphWeight(const QString& morphName, float frame) const;
    bool evaluateIkState(const QString& ikBoneName, float frame, bool defaultState = true) const;

    // Cubic Bézier interpolation evaluator for MMD control points [x1, y1, x2, y2]
    static float evaluateBezier(const uint8_t interp[4], float t);

private:
    VmdMotion m_motion;
    QHash<QString, std::vector<VmdBoneKeyframe>> m_boneTracks;
    QHash<QString, std::vector<VmdMorphKeyframe>> m_morphTracks;
    std::vector<VmdIkKeyframe> m_ikTrack;

    float m_currentFrame = 0.0f;
    uint32_t m_maxFrame = 0;
    bool m_isPlaying = false;
    bool m_isLoop = false;
};

} // namespace pmx
