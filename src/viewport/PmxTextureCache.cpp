#include "viewport/PmxTextureCache.h"
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

namespace pmx {

PmxTextureCache::~PmxTextureCache() {
    clear();
}

PmxTextureCache::PmxTextureCache(PmxTextureCache&& other) noexcept
    : m_textures(std::move(other.m_textures))
    , m_defaultToonTextures(std::move(other.m_defaultToonTextures))
    , m_whiteTexture(other.m_whiteTexture)
    , m_repeatSampler(other.m_repeatSampler)
    , m_clampSampler(other.m_clampSampler)
{
    other.m_whiteTexture = nullptr;
    other.m_repeatSampler = nullptr;
    other.m_clampSampler = nullptr;
}

PmxTextureCache& PmxTextureCache::operator=(PmxTextureCache&& other) noexcept {
    if (this != &other) {
        clear();
        m_textures = std::move(other.m_textures);
        m_defaultToonTextures = std::move(other.m_defaultToonTextures);
        m_whiteTexture = other.m_whiteTexture;
        m_repeatSampler = other.m_repeatSampler;
        m_clampSampler = other.m_clampSampler;
        other.m_whiteTexture = nullptr;
        other.m_repeatSampler = nullptr;
        other.m_clampSampler = nullptr;
    }
    return *this;
}

void PmxTextureCache::clear() {
    for (auto* tex : m_textures) {
        delete tex;
    }
    m_textures.clear();

    for (auto* tex : m_defaultToonTextures) {
        delete tex;
    }
    m_defaultToonTextures.clear();

    delete m_whiteTexture;
    m_whiteTexture = nullptr;

    delete m_repeatSampler;
    m_repeatSampler = nullptr;

    delete m_clampSampler;
    m_clampSampler = nullptr;
}

glm::vec2 PmxTextureCache::calculateSphereUV(const glm::vec3& normalView) {
    return glm::vec2(0.5f + 0.5f * normalView.x, 0.5f - 0.5f * normalView.y);
}

float PmxTextureCache::calculateToonV(float NdotL) {
    return std::clamp(0.5f - 0.5f * NdotL, 0.0f, 1.0f);
}

float PmxTextureCache::calculateToonCoordinate(float NdotL) {
    return calculateToonV(NdotL);
}

QImage PmxTextureCache::createWhiteImage() {
    QImage img(1, 1, QImage::Format_RGBA8888);
    img.fill(QColor(255, 255, 255, 255));
    return img;
}

QImage PmxTextureCache::createToonImage(int toonIndex) {
    int idx = toonIndex;
    if (idx < 0 || idx > 9) {
        idx = 0;
    }

    const int width = 16;
    const int height = 16;
    QImage img(width, height, QImage::Format_RGBA8888);

    struct ToonGradient {
        QColor lit;
        QColor shadow;
        float threshold;
        float softness;
    };

    static const ToonGradient presets[10] = {
        { QColor(255, 255, 255), QColor(170, 170, 170), 0.5f, 0.15f }, // toon01: Neutral gray shadow
        { QColor(255, 255, 255), QColor(230, 195, 185), 0.5f, 0.15f }, // toon02: Warm skin shadow
        { QColor(255, 255, 255), QColor(130, 130, 130), 0.5f, 0.15f }, // toon03: Darker cloth shadow
        { QColor(255, 255, 255), QColor(140, 140, 140), 0.5f, 0.02f }, // toon04: High contrast sharp cel
        { QColor(255, 255, 255), QColor(200, 200, 200), 0.5f, 0.20f }, // toon05: Soft ambient
        { QColor(255, 255, 255), QColor(170, 180, 205), 0.5f, 0.15f }, // toon06: Cool bluish shadow
        { QColor(255, 255, 255), QColor(205, 190, 170), 0.5f, 0.15f }, // toon07: Warm golden shadow
        { QColor(255, 255, 255), QColor(90, 90, 90),    0.5f, 0.10f }, // toon08: Dark shadow
        { QColor(255, 255, 255), QColor(220, 220, 220), 0.5f, 0.25f }, // toon09: Subtle ramp
        { QColor(255, 255, 255), QColor(160, 160, 160), 0.5f, 0.001f } // toon10: 2-tone step anime cel
    };

    const auto& grad = presets[idx];

    for (int y = 0; y < height; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(height - 1);
        float factor;
        if (grad.softness <= 0.005f) {
            factor = (t >= grad.threshold) ? 1.0f : 0.0f;
        } else {
            float edge0 = grad.threshold - grad.softness;
            float edge1 = grad.threshold + grad.softness;
            float clamped = std::clamp((t - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            factor = clamped * clamped * (3.0f - 2.0f * clamped);
        }

        int r = static_cast<int>(std::round(grad.lit.red()   * (1.0f - factor) + grad.shadow.red()   * factor));
        int g = static_cast<int>(std::round(grad.lit.green() * (1.0f - factor) + grad.shadow.green() * factor));
        int b = static_cast<int>(std::round(grad.lit.blue()  * (1.0f - factor) + grad.shadow.blue()  * factor));
        QColor col(r, g, b, 255);

        for (int x = 0; x < width; ++x) {
            img.setPixelColor(x, y, col);
        }
    }

    return img;
}

QRhiTexture* PmxTextureCache::uploadImage(QRhi* rhi, const QImage& image, QRhiResourceUpdateBatch* u) {
    if (!rhi || image.isNull()) return nullptr;
    QImage formatted = image.convertToFormat(QImage::Format_RGBA8888);
    QRhiTexture* tex = rhi->newTexture(QRhiTexture::RGBA8, formatted.size());
    if (!tex->create()) {
        delete tex;
        return nullptr;
    }
    if (u) {
        u->uploadTexture(tex, formatted);
    }
    return tex;
}

QRhiTexture* PmxTextureCache::defaultWhiteTexture(QRhi* rhi, QRhiResourceUpdateBatch* u) {
    if (m_whiteTexture) return m_whiteTexture;
    if (!rhi) return nullptr;

    m_whiteTexture = uploadImage(rhi, createWhiteImage(), u);
    return m_whiteTexture;
}

QRhiTexture* PmxTextureCache::defaultToonTexture(QRhi* rhi, int toonIndex, QRhiResourceUpdateBatch* u) {
    int idx = toonIndex;
    if (idx < 0 || idx > 9) idx = 0;

    auto it = m_defaultToonTextures.find(idx);
    if (it != m_defaultToonTextures.end() && it.value()) {
        return it.value();
    }
    if (!rhi) return nullptr;

    QRhiTexture* tex = uploadImage(rhi, createToonImage(idx), u);
    if (tex) {
        m_defaultToonTextures.insert(idx, tex);
    }
    return tex ? tex : defaultWhiteTexture(rhi, u);
}

QRhiTexture* PmxTextureCache::loadTexture(QRhi* rhi, const QString& filePath, QRhiResourceUpdateBatch* u) {
    if (filePath.isEmpty() || !rhi) {
        return defaultWhiteTexture(rhi, u);
    }

    auto it = m_textures.find(filePath);
    if (it != m_textures.end() && it.value()) {
        return it.value();
    }

    QImage img;
    if (QFile::exists(filePath)) {
        img.load(filePath);
        if (img.isNull()) {
            if (filePath.endsWith(QStringLiteral(".sph"), Qt::CaseInsensitive) ||
                filePath.endsWith(QStringLiteral(".spa"), Qt::CaseInsensitive)) {
                img.load(filePath, "BMP");
                if (img.isNull()) {
                    img.load(filePath, "PNG");
                }
            }
        }
    }

    if (img.isNull()) {
        return defaultWhiteTexture(rhi, u);
    }

    QRhiTexture* tex = uploadImage(rhi, img, u);
    if (tex) {
        m_textures.insert(filePath, tex);
        return tex;
    }
    return defaultWhiteTexture(rhi, u);
}

QRhiSampler* PmxTextureCache::repeatSampler(QRhi* rhi) {
    if (m_repeatSampler) return m_repeatSampler;
    if (!rhi) return nullptr;

    m_repeatSampler = rhi->newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::Repeat,
        QRhiSampler::Repeat
    );
    if (!m_repeatSampler->create()) {
        delete m_repeatSampler;
        m_repeatSampler = nullptr;
    }
    return m_repeatSampler;
}

QRhiSampler* PmxTextureCache::clampSampler(QRhi* rhi) {
    if (m_clampSampler) return m_clampSampler;
    if (!rhi) return nullptr;

    m_clampSampler = rhi->newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge
    );
    if (!m_clampSampler->create()) {
        delete m_clampSampler;
        m_clampSampler = nullptr;
    }
    return m_clampSampler;
}

QRhiSampler* PmxTextureCache::defaultSampler(QRhi* rhi) {
    return repeatSampler(rhi);
}

} // namespace pmx
