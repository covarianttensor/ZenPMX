#pragma once

#include "core/PmxDocument.h"
#include "core/PmxTypes.h"

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <vector>

namespace pmx {

class PmxWriter {
public:
    static bool writeToBuffer(const PmxDocument& document,
                              QByteArray& outBuffer,
                              QString* errorMessage = nullptr);

    static bool writeToBuffer(const PmxDocument& document,
                              std::vector<uint8_t>& outBuffer,
                              QString* errorMessage = nullptr);

    static bool writeToFile(const QString& filePath,
                            const PmxDocument& document,
                            QString* errorMessage = nullptr);

    // Low-level variable-width index encoders obeying the Binary Layout & Signedness Law
    static bool writeVertexIndex(QByteArray& buffer,
                                 int32_t index,
                                 uint8_t indexSize,
                                 QString* errorMessage = nullptr);

    static bool writeNonVertexIndex(QByteArray& buffer,
                                    int32_t index,
                                    uint8_t indexSize,
                                    QString* errorMessage = nullptr);

    static bool writeText(QByteArray& buffer,
                          const QString& text,
                          uint8_t textEncoding,
                          QString* errorMessage = nullptr);
};

} // namespace pmx
