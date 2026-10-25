#pragma once

#include "core/PmxDocument.h"

#include <QString>
#include <memory>

namespace pmx {

struct AssimpImportOptions {
    float scale = 1.0f;
    bool generateNormals = true;
    bool flipUVs = true;
    bool createRootBone = true;
    QString rootBoneNameJp = QString::fromUtf8("全ての親");
    QString rootBoneNameEn = QStringLiteral("Root");
};

class AssimpImporter {
public:
    static std::unique_ptr<PmxDocument> importFile(
        const QString& filePath,
        const AssimpImportOptions& options = {},
        QString* errorMessage = nullptr);

    static std::unique_ptr<PmxDocument> importFromMemory(
        const void* data,
        size_t length,
        const QString& hintFormat = "obj",
        const AssimpImportOptions& options = {},
        QString* errorMessage = nullptr);
};

} // namespace pmx

namespace Pmx = pmx;
