#pragma once

#include <rhi/qrhi.h>
#include <QImage>
#include <QString>
#include <QMap>
#include <glm/glm.hpp>

namespace pmx {

class PmxTextureCache {
public:
    PmxTextureCache() = default;
    ~PmxTextureCache();

    PmxTextureCache(const PmxTextureCache&) = delete;
    PmxTextureCache& operator=(const PmxTextureCache&) = delete;
    PmxTextureCache(PmxTextureCache&&) noexcept;
    PmxTextureCache& operator=(PmxTextureCache&&) noexcept;

    QRhiTexture* defaultWhiteTexture(QRhi* rhi, QRhiResourceUpdateBatch* u = nullptr);
    QRhiTexture* defaultToonTexture(QRhi* rhi, int toonIndex, QRhiResourceUpdateBatch* u = nullptr);
    QRhiTexture* loadTexture(QRhi* rhi, const QString& filePath, QRhiResourceUpdateBatch* u = nullptr);
    QRhiSampler* repeatSampler(QRhi* rhi);
    QRhiSampler* clampSampler(QRhi* rhi);
    QRhiSampler* defaultSampler(QRhi* rhi);

    void clear();

    // Fallback QImage generation (for offline evaluation and test verification)
    static QImage createWhiteImage();
    static QImage createToonImage(int toonIndex);

    // MMD coordinate math helpers
    static glm::vec2 calculateSphereUV(const glm::vec3& normalView);
    static float calculateToonV(float NdotL);
    static float calculateToonCoordinate(float NdotL);

private:
    QRhiTexture* uploadImage(QRhi* rhi, const QImage& image, QRhiResourceUpdateBatch* u);

    QMap<QString, QRhiTexture*> m_textures;
    QMap<int, QRhiTexture*> m_defaultToonTextures;
    QRhiTexture* m_whiteTexture = nullptr;
    QRhiSampler* m_repeatSampler = nullptr;
    QRhiSampler* m_clampSampler = nullptr;
};

} // namespace pmx
