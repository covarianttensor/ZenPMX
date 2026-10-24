#pragma once

#include "core/PmxDocument.h"
#include "core/PmxTypes.h"

#include <QByteArray>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <span>

namespace pmx {

class PmxReader {
public:
    static bool readFromBuffer(std::span<const uint8_t> buffer,
                               PmxDocument& outDocument,
                               QString* errorMessage = nullptr);

    static bool readFromBuffer(const QByteArray& buffer,
                               PmxDocument& outDocument,
                               QString* errorMessage = nullptr);

    static bool readFromFile(const QString& filePath,
                             PmxDocument& outDocument,
                             QString* errorMessage = nullptr);

    // Low-level variable-width index decoders obeying the Binary Layout & Signedness Law
    static bool readVertexIndex(std::span<const uint8_t> buffer,
                                size_t& offset,
                                uint8_t indexSize,
                                int32_t& outIndex,
                                QString* errorMessage = nullptr);

    static bool readVertexIndex(const QByteArray& buffer,
                                size_t& offset,
                                uint8_t indexSize,
                                int32_t& outIndex,
                                QString* errorMessage = nullptr);

    static bool readNonVertexIndex(std::span<const uint8_t> buffer,
                                   size_t& offset,
                                   uint8_t indexSize,
                                   int32_t& outIndex,
                                   QString* errorMessage = nullptr);

    static bool readNonVertexIndex(const QByteArray& buffer,
                                   size_t& offset,
                                   uint8_t indexSize,
                                   int32_t& outIndex,
                                   QString* errorMessage = nullptr);

    static bool readText(std::span<const uint8_t> buffer,
                         size_t& offset,
                         uint8_t textEncoding,
                         QString& outText,
                         QString* errorMessage = nullptr);

    static bool readText(const QByteArray& buffer,
                         size_t& offset,
                         uint8_t textEncoding,
                         QString& outText,
                         QString* errorMessage = nullptr);
};

} // namespace pmx
