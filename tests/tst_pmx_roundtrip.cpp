#include "core/PmxDocument.h"
#include "core/PmxReader.h"
#include "core/PmxTypes.h"
#include "core/PmxWriter.h"

#include <QTemporaryDir>
#include <QtEndian>
#include <QtTest>
#include <cstring>
#include <limits>

using namespace pmx;

namespace {

void appendLeI32(QByteArray& buf, int32_t val) {
    qint32 le = qToLittleEndian<qint32>(val);
    char bytes[sizeof(qint32)];
    std::memcpy(bytes, &le, sizeof(qint32));
    buf.append(bytes, sizeof(qint32));
}

} // namespace

class TestPmxRoundtrip : public QObject {
    Q_OBJECT

private slots:
    void testPmx20HeaderAndInfoRoundTripUtf16Le() {
        PmxDocument original;
        original.header.version = 2.0f;
        original.header.textEncoding = 0; // UTF-16LE
        original.header.additionalVec4Count = 2;
        original.header.vertexIndexSize = 1;
        original.header.textureIndexSize = 2;
        original.header.materialIndexSize = 1;
        original.header.boneIndexSize = 4;
        original.header.morphIndexSize = 2;
        original.header.rigidBodyIndexSize = 4;
        original.header.preserveIndexSizesOnSave = true;

        original.modelInfo.modelNameJp = QStringLiteral("初音ミク_テストモデル");
        original.modelInfo.modelNameEn = QStringLiteral("Hatsune Miku Test Model");
        original.modelInfo.commentJp = QStringLiteral("1行目のコメント\r\n2行目の日本語コメント（UTF-16LE検証）");
        original.modelInfo.commentEn = QStringLiteral("Line 1 comment\r\nLine 2 English comment (UTF-16LE verification)");

        QByteArray inputBytes;
        QString writeErr;
        QVERIFY2(PmxWriter::writeToBuffer(original, inputBytes, &writeErr), qPrintable(writeErr));
        QVERIFY(!inputBytes.isEmpty());

        PmxDocument parsed;
        QString readErr;
        QVERIFY2(PmxReader::readFromBuffer(inputBytes, parsed, &readErr), qPrintable(readErr));

        QCOMPARE(parsed.header.version, 2.0f);
        QCOMPARE(parsed.header.textEncoding, uint8_t(0));
        QCOMPARE(parsed.header.additionalVec4Count, uint8_t(2));
        QCOMPARE(parsed.header.vertexIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.textureIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.materialIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.boneIndexSize, uint8_t(4));
        QCOMPARE(parsed.header.morphIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.rigidBodyIndexSize, uint8_t(4));
        QCOMPARE(parsed.header.preserveIndexSizesOnSave, true);

        QCOMPARE(parsed.modelInfo.modelNameJp, original.modelInfo.modelNameJp);
        QCOMPARE(parsed.modelInfo.modelNameEn, original.modelInfo.modelNameEn);
        QCOMPARE(parsed.modelInfo.commentJp, original.modelInfo.commentJp);
        QCOMPARE(parsed.modelInfo.commentEn, original.modelInfo.commentEn);

        QByteArray outputBytes;
        QVERIFY2(PmxWriter::writeToBuffer(parsed, outputBytes, &writeErr), qPrintable(writeErr));
        QCOMPARE(outputBytes, inputBytes);
    }

    void testPmx21HeaderAndInfoRoundTripUtf8() {
        PmxDocument original;
        original.header.version = 2.1f;
        original.header.textEncoding = 1; // UTF-8
        original.header.additionalVec4Count = 4;
        original.header.vertexIndexSize = 4;
        original.header.textureIndexSize = 1;
        original.header.materialIndexSize = 2;
        original.header.boneIndexSize = 2;
        original.header.morphIndexSize = 1;
        original.header.rigidBodyIndexSize = 2;
        original.header.preserveIndexSizesOnSave = true;

        original.modelInfo.modelNameJp = QStringLiteral("PMX2.1_標準テストモデル");
        original.modelInfo.modelNameEn = QStringLiteral("PMX 2.1 Standard Test Model");
        original.modelInfo.commentJp = QStringLiteral("UTF-8 エンコーディング\nSoftBodyセクション含む");
        original.modelInfo.commentEn = QStringLiteral("UTF-8 Encoding\nIncludes trailing SoftBody count");

        QByteArray inputBytes;
        QString err;
        QVERIFY2(PmxWriter::writeToBuffer(original, inputBytes, &err), qPrintable(err));

        PmxDocument parsed;
        QVERIFY2(PmxReader::readFromBuffer(inputBytes, parsed, &err), qPrintable(err));

        QCOMPARE(parsed.header.version, 2.1f);
        QCOMPARE(parsed.header.textEncoding, uint8_t(1));
        QCOMPARE(parsed.header.additionalVec4Count, uint8_t(4));
        QCOMPARE(parsed.header.vertexIndexSize, uint8_t(4));
        QCOMPARE(parsed.header.textureIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.materialIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.boneIndexSize, uint8_t(2));
        QCOMPARE(parsed.header.morphIndexSize, uint8_t(1));
        QCOMPARE(parsed.header.rigidBodyIndexSize, uint8_t(2));
        QCOMPARE(parsed.softBodies.size(), size_t(0));

        QCOMPARE(parsed.modelInfo.modelNameJp, original.modelInfo.modelNameJp);
        QCOMPARE(parsed.modelInfo.modelNameEn, original.modelInfo.modelNameEn);
        QCOMPARE(parsed.modelInfo.commentJp, original.modelInfo.commentJp);
        QCOMPARE(parsed.modelInfo.commentEn, original.modelInfo.commentEn);

        QByteArray outputBytes;
        QVERIFY2(PmxWriter::writeToBuffer(parsed, outputBytes, &err), qPrintable(err));
        QCOMPARE(outputBytes, inputBytes);

        // Also verify file I/O round-trip
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString tempFilePath = tempDir.filePath(QStringLiteral("roundtrip_21.pmx"));
        QVERIFY2(PmxWriter::writeToFile(tempFilePath, parsed, &err), qPrintable(err));

        PmxDocument fromFile;
        QVERIFY2(PmxReader::readFromFile(tempFilePath, fromFile, &err), qPrintable(err));
        QCOMPARE(fromFile.sourceFilePath, tempFilePath);

        QByteArray fileRoundTripBytes;
        QVERIFY2(PmxWriter::writeToBuffer(fromFile, fileRoundTripBytes, &err), qPrintable(err));
        QCOMPARE(fileRoundTripBytes, inputBytes);
    }

    void testVertexVsNonVertexIndexSignednessLaw() {
        QString err;

        // 1. Non-Vertex Index: -1 must encode to 0xFF (1B), 0xFFFF (2B), 0xFFFFFFFF (4B) and decode back to -1
        {
            QByteArray buf1;
            QVERIFY2(PmxWriter::writeNonVertexIndex(buf1, -1, 1, &err), qPrintable(err));
            QCOMPARE(buf1.size(), qsizetype(1));
            QCOMPARE(static_cast<uint8_t>(buf1[0]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readNonVertexIndex(buf1, offset, 1, decoded, &err), qPrintable(err));
            QCOMPARE(offset, size_t(1));
            QCOMPARE(decoded, -1);
        }
        {
            QByteArray buf2;
            QVERIFY2(PmxWriter::writeNonVertexIndex(buf2, -1, 2, &err), qPrintable(err));
            QCOMPARE(buf2.size(), qsizetype(2));
            QCOMPARE(static_cast<uint8_t>(buf2[0]), uint8_t(0xFF));
            QCOMPARE(static_cast<uint8_t>(buf2[1]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readNonVertexIndex(buf2, offset, 2, decoded, &err), qPrintable(err));
            QCOMPARE(offset, size_t(2));
            QCOMPARE(decoded, -1);
        }
        {
            QByteArray buf4;
            QVERIFY2(PmxWriter::writeNonVertexIndex(buf4, -1, 4, &err), qPrintable(err));
            QCOMPARE(buf4.size(), qsizetype(4));
            for (int i = 0; i < 4; ++i) {
                QCOMPARE(static_cast<uint8_t>(buf4[i]), uint8_t(0xFF));
            }

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readNonVertexIndex(buf4, offset, 4, decoded, &err), qPrintable(err));
            QCOMPARE(offset, size_t(4));
            QCOMPARE(decoded, -1);
        }

        // 2. Vertex Index: 0xFF (size 1) MUST decode to +255, and 0xFFFF (size 2) MUST decode to +65535
        {
            QByteArray vbuf1;
            QVERIFY2(PmxWriter::writeVertexIndex(vbuf1, 255, 1, &err), qPrintable(err));
            QCOMPARE(vbuf1.size(), qsizetype(1));
            QCOMPARE(static_cast<uint8_t>(vbuf1[0]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readVertexIndex(vbuf1, offset, 1, decoded, &err), qPrintable(err));
            QCOMPARE(decoded, 255);
        }
        {
            QByteArray vbuf2;
            QVERIFY2(PmxWriter::writeVertexIndex(vbuf2, 65535, 2, &err), qPrintable(err));
            QCOMPARE(vbuf2.size(), qsizetype(2));
            QCOMPARE(static_cast<uint8_t>(vbuf2[0]), uint8_t(0xFF));
            QCOMPARE(static_cast<uint8_t>(vbuf2[1]), uint8_t(0xFF));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readVertexIndex(vbuf2, offset, 2, decoded, &err), qPrintable(err));
            QCOMPARE(decoded, 65535);
        }
        {
            QByteArray vbuf4;
            QVERIFY2(PmxWriter::writeVertexIndex(vbuf4, 123456789, 4, &err), qPrintable(err));
            QCOMPARE(vbuf4.size(), qsizetype(4));

            size_t offset = 0;
            int32_t decoded = 0;
            QVERIFY2(PmxReader::readVertexIndex(vbuf4, offset, 4, decoded, &err), qPrintable(err));
            QCOMPARE(decoded, 123456789);
        }

        // 3. Boundary & out-of-range checks for Vertex vs Non-Vertex indices
        {
            QByteArray dummy;
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, -1, 1));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, 256, 1));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, -1, 2));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, 65536, 2));
            QVERIFY(!PmxWriter::writeVertexIndex(dummy, -1, 4));

            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, -129, 1));
            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, 128, 1));
            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, -32769, 2));
            QVERIFY(!PmxWriter::writeNonVertexIndex(dummy, 32768, 2));
        }
    }

    void testAdversarialMalformedStreams() {
        PmxDocument validDoc;
        validDoc.header.version = 2.0f;
        validDoc.header.textEncoding = 0;
        validDoc.header.preserveIndexSizesOnSave = true;
        validDoc.modelInfo.modelNameJp = QStringLiteral("テスト");

        QByteArray validBytes;
        QVERIFY(PmxWriter::writeToBuffer(validDoc, validBytes));

        PmxDocument outDoc;

        // 1. Empty buffer
        QVERIFY(!PmxReader::readFromBuffer(QByteArray(), outDoc));

        // 2. Truncated magic & truncated header at every byte position up to validBytes.size() - 1
        for (qsizetype len = 1; len < validBytes.size(); ++len) {
            QByteArray truncated = validBytes.left(len);
            QVERIFY2(!PmxReader::readFromBuffer(truncated, outDoc),
                     qPrintable(QStringLiteral("Expected failure on truncated buffer of length %1").arg(len)));
        }

        // 3. Wrong magic ("PMD ")
        {
            QByteArray badMagic = validBytes;
            badMagic[0] = 'P';
            badMagic[1] = 'M';
            badMagic[2] = 'D';
            badMagic[3] = ' ';
            QVERIFY(!PmxReader::readFromBuffer(badMagic, outDoc));
        }

        // 4. Unsupported versions (1.0f, 3.0f, NaN)
        for (float badVer : {1.0f, 3.0f, std::numeric_limits<float>::quiet_NaN()}) {
            QByteArray badVersionBytes = validBytes;
            quint32 bits = 0;
            std::memcpy(&bits, &badVer, sizeof(float));
            bits = qToLittleEndian(bits);
            std::memcpy(badVersionBytes.data() + 4, &bits, sizeof(uint32_t));
            QVERIFY(!PmxReader::readFromBuffer(badVersionBytes, outDoc));
        }

        // 5. globalsCount != 8
        for (uint8_t badGlobalsCount : {uint8_t(0), uint8_t(7), uint8_t(9), uint8_t(255)}) {
            QByteArray badGlobals = validBytes;
            badGlobals[8] = static_cast<char>(badGlobalsCount);
            QVERIFY(!PmxReader::readFromBuffer(badGlobals, outDoc));
        }

        // 6. Invalid textEncoding (2) and invalid additionalVec4Count (5)
        {
            QByteArray badEncoding = validBytes;
            badEncoding[9] = 2;
            QVERIFY(!PmxReader::readFromBuffer(badEncoding, outDoc));

            QByteArray badAddUv = validBytes;
            badAddUv[10] = 5;
            QVERIFY(!PmxReader::readFromBuffer(badAddUv, outDoc));
        }

        // 7. Invalid index byte sizes (0, 3, 8) across all 6 index size slots (globals[2..7] at offsets 11..16)
        for (int slot = 11; slot <= 16; ++slot) {
            for (uint8_t badIdxSize : {uint8_t(0), uint8_t(3), uint8_t(8)}) {
                QByteArray mutated = validBytes;
                mutated[slot] = static_cast<char>(badIdxSize);
                QVERIFY(!PmxReader::readFromBuffer(mutated, outDoc));
            }
        }

        // 8. Negative string length (at offset 17, first PmxText byteLength)
        {
            QByteArray negStr = validBytes.left(17);
            appendLeI32(negStr, -4);
            QVERIFY(!PmxReader::readFromBuffer(negStr, outDoc));
        }

        // 9. Odd UTF-16LE byteLength (textEncoding == 0, byteLength == 3)
        {
            QByteArray oddUtf16 = validBytes.left(17);
            appendLeI32(oddUtf16, 3);
            oddUtf16.append("abc", 3);
            QVERIFY(!PmxReader::readFromBuffer(oddUtf16, outDoc));
        }

        // 10. String byteLength exceeding buffer end
        {
            QByteArray hugeStr = validBytes.left(17);
            appendLeI32(hugeStr, 4096);
            hugeStr.append("abcd", 4);
            QVERIFY(!PmxReader::readFromBuffer(hugeStr, outDoc));
        }
    }
};

QTEST_MAIN(TestPmxRoundtrip)
#include "tst_pmx_roundtrip.moc"
