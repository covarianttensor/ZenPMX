#pragma once

#include "core/PmxDocument.h"

#include <QByteArray>
#include <QString>
#include <span>

namespace pmx {

class PmdImporter {
public:
    static bool importFromFile(const QString& filePath, PmxDocument& outDoc, QString* errorMessage = nullptr);
    static bool importFromMemory(std::span<const uint8_t> data, PmxDocument& outDoc, QString* errorMessage = nullptr);
    static bool importFromMemory(const QByteArray& data, PmxDocument& outDoc, QString* errorMessage = nullptr);
};

} // namespace pmx
