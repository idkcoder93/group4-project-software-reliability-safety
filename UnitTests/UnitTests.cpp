#include "pch.h"
#include "CppUnitTest.h"
//#include "BitmapGenerator.h"
//#include "PacketDeserializer.h"
#include "../FOD-SERVER/BitmapGenerator.h"
#include "../FOD-SERVER/PacketDeserializer.h"
#include <cstring>
#include <chrono>
#include <ctime>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace FODServer;

namespace UnitTests
{
    // =========================================================
    // BitmapGenerator Tests
    // =========================================================
    TEST_CLASS(BitmapGeneratorTests)
    {
    public:

        // --- zoneToGrid ---

        TEST_METHOD(ZoneToGrid_ValidZone_A1_ReturnsRow0Col0)
        {
            int row = -1, col = -1;
            BitmapGenerator::zoneToGrid("A1", row, col);
            Assert::AreEqual(0, row);
            Assert::AreEqual(0, col);
        }

        TEST_METHOD(ZoneToGrid_ValidZone_F6_ReturnsRow5Col5)
        {
            int row = -1, col = -1;
            BitmapGenerator::zoneToGrid("F6", row, col);
            Assert::AreEqual(5, row);
            Assert::AreEqual(5, col);
        }

        TEST_METHOD(ZoneToGrid_LowercaseLetter_NormalisedCorrectly)
        {
            int row = -1, col = -1;
            BitmapGenerator::zoneToGrid("c3", row, col);
            Assert::AreEqual(2, row);
            Assert::AreEqual(2, col);
        }

        TEST_METHOD(ZoneToGrid_InvalidLetter_RowRemainsNegativeOne)
        {
            int row = -1, col = -1;
            BitmapGenerator::zoneToGrid("G1", row, col);
            Assert::AreEqual(-1, row);
            Assert::AreEqual(0, col);
        }

        TEST_METHOD(ZoneToGrid_InvalidDigit_ColRemainsNegativeOne)
        {
            int row = -1, col = -1;
            BitmapGenerator::zoneToGrid("A7", row, col);
            Assert::AreEqual(0, row);
            Assert::AreEqual(-1, col);
        }

        TEST_METHOD(ZoneToGrid_EmptyString_BothNegativeOne)
        {
            int row = -1, col = -1;
            BitmapGenerator::zoneToGrid("", row, col);
            Assert::AreEqual(-1, row);
            Assert::AreEqual(-1, col);
        }

        TEST_METHOD(ZoneToGrid_SingleChar_BothNegativeOne)
        {
            int row = -1, col = -1;
            BitmapGenerator::zoneToGrid("A", row, col);
            Assert::AreEqual(-1, row);  // no digit parsed
            Assert::AreEqual(-1, col);
        }

        // --- generateRunwayBitmap ---

        TEST_METHOD(GenerateBitmap_ReturnsSizeIsNonZero)
        {
            auto bmp = BitmapGenerator::generateRunwayBitmap("A1");
            Assert::IsTrue(bmp.size() > 0U);
        }

        TEST_METHOD(GenerateBitmap_StartsWithBMSignature)
        {
            auto bmp = BitmapGenerator::generateRunwayBitmap("B2");
            Assert::AreEqual('B', bmp[0]);
            Assert::AreEqual('M', bmp[1]);
        }

        TEST_METHOD(GenerateBitmap_FileSizeFieldMatchesVectorSize)
        {
            auto bmp = BitmapGenerator::generateRunwayBitmap("C3");
            int fileSize = 0;
            std::memcpy(&fileSize, &bmp[2], 4);
            Assert::AreEqual(static_cast<int>(bmp.size()), fileSize);
        }

        TEST_METHOD(GenerateBitmap_DataOffsetIs54)
        {
            // HEADER_SIZE = 14 (file header) + 40 (DIB) = 54
            auto bmp = BitmapGenerator::generateRunwayBitmap("A1");
            int offset = 0;
            std::memcpy(&offset, &bmp[10], 4);
            Assert::AreEqual(54, offset);
        }

        TEST_METHOD(GenerateBitmap_InvalidZone_DoesNotCrash)
        {
            // No zone should be highlighted, but generation should succeed
            auto bmp = BitmapGenerator::generateRunwayBitmap("Z9");
            Assert::IsTrue(bmp.size() > 0U);
        }

        TEST_METHOD(GenerateBitmap_EmptyZone_DoesNotCrash)
        {
            auto bmp = BitmapGenerator::generateRunwayBitmap("");
            Assert::IsTrue(bmp.size() > 0U);
        }
    };

    // =========================================================
    // PacketDeserializer Tests
    // =========================================================
    TEST_CLASS(PacketDeserializerTests)
    {
        // Helper: write a little-endian int into a buffer at pos
        static void writeInt(char* buf, int pos, int val)
        {
            std::memcpy(&buf[pos], &val, 4);
        }

        // Helper: write a length-prefixed string
        static int writeString(char* buf, int pos, const char* str)
        {
            const int len = static_cast<int>(std::strlen(str));
            writeInt(buf, pos, len);
            std::memcpy(&buf[pos + 4], str, static_cast<size_t>(len));
            return pos + 4 + len;
        }

        // Build a minimal valid header buffer and return its size.
        // Layout: packetTypeId(4) | hazardType(4) | locationZone(4+N) |
        //         severityLevel(4) | officerName(4+M) | timestamp(8) |
        //         descLength(4) | checkSum(4)
        static std::vector<char> buildHeaderBuffer(
            int packetTypeId,
            int hazardType,
            const char* zone,
            int severity,
            const char* officer,
            std::time_t timestamp,
            int descLength,
            int checkSum)
        {
            // max size estimate
            std::vector<char> buf(512, '\0');
            int pos = 0;

            writeInt(buf.data(), pos, packetTypeId);   pos += 4;
            writeInt(buf.data(), pos, hazardType);     pos += 4;
            pos = writeString(buf.data(), pos, zone);
            writeInt(buf.data(), pos, severity);       pos += 4;
            pos = writeString(buf.data(), pos, officer);
            std::memcpy(&buf[pos], &timestamp, 8);    pos += 8;
            writeInt(buf.data(), pos, descLength);     pos += 4;
            writeInt(buf.data(), pos, checkSum);       pos += 4;

            buf.resize(static_cast<size_t>(pos));
            return buf;
        }

    public:

        // --- deserializeHeader ---

        TEST_METHOD(DeserializeHeader_ValidBuffer_ReturnsTrue)
        {
            const std::time_t ts = 1700000000;
            auto buf = buildHeaderBuffer(1, 0, "B3", 2, "Smith", ts, 50, 999);
            FODHeader h{};
            const bool ok = PacketDeserializer::deserializeHeader(buf.data(), static_cast<int>(buf.size()), h);
            Assert::IsTrue(ok);
        }

        TEST_METHOD(DeserializeHeader_ValidBuffer_FieldsCorrect)
        {
            const std::time_t ts = 1700000000;
            auto buf = buildHeaderBuffer(7, 1, "D4", 3, "Jones", ts, 100, 42);
            FODHeader h{};
            PacketDeserializer::deserializeHeader(buf.data(), static_cast<int>(buf.size()), h);

            Assert::AreEqual(7, h.packetTypeId);
            Assert::IsTrue(static_cast<int>(HazardType::FOD_HAZARD_TYPE_ANIMAL) == static_cast<int>(h.hazardType));
            Assert::AreEqual(std::string("D4"), h.locationZone);
            Assert::AreEqual(3, h.severityLevel);
            Assert::AreEqual(std::string("Jones"), h.officerName);
            Assert::AreEqual(100, h.descLength);
            Assert::AreEqual(42, h.checkSum);
        }

        TEST_METHOD(DeserializeHeader_TruncatedBuffer_ReturnsFalse)
        {
            // Only 4 bytes — not enough for the full header
            char buf[4] = { 0x01, 0x00, 0x00, 0x00 };
            FODHeader h{};
            const bool ok = PacketDeserializer::deserializeHeader(buf, 4, h);
            Assert::IsFalse(ok);
        }

        TEST_METHOD(DeserializeHeader_EmptyBuffer_ReturnsFalse)
        {
            FODHeader h{};
            const bool ok = PacketDeserializer::deserializeHeader(nullptr, 0, h);
            Assert::IsFalse(ok);
        }

        // --- deserializeDescription ---

        TEST_METHOD(DeserializeDescription_ValidBuffer_ReturnsTrue)
        {
            // Layout: packetTypeId(4) | checksum(4) | descLen(4) | <text>
            const char* text = "FOD on runway";
            const int textLen = static_cast<int>(std::strlen(text));
            std::vector<char> buf(12 + textLen, '\0');
            writeInt(buf.data(), 0, 2);          // packetTypeId
            writeInt(buf.data(), 4, 999);         // checksum placeholder
            writeInt(buf.data(), 8, textLen);
            std::memcpy(&buf[12], text, static_cast<size_t>(textLen));

            FODDescription d{};
            const bool ok = PacketDeserializer::deserializeDescription(buf.data(), static_cast<int>(buf.size()), d);
            Assert::IsTrue(ok);
            PacketDeserializer::freeDescription(d);
        }

        TEST_METHOD(DeserializeDescription_ValidBuffer_TextCorrect)
        {
            const char* text = "Debris near taxiway";
            const int textLen = static_cast<int>(std::strlen(text));
            std::vector<char> buf(12 + textLen, '\0');
            writeInt(buf.data(), 0, 2);
            writeInt(buf.data(), 4, 0);
            writeInt(buf.data(), 8, textLen);
            std::memcpy(&buf[12], text, static_cast<size_t>(textLen));

            FODDescription d{};
            PacketDeserializer::deserializeDescription(buf.data(), static_cast<int>(buf.size()), d);
            Assert::AreEqual(std::string(text), std::string(d.description));
            PacketDeserializer::freeDescription(d);
        }

        TEST_METHOD(DeserializeDescription_TruncatedBuffer_ReturnsFalse)
        {
            char buf[4] = { 0x01, 0x00, 0x00, 0x00 };
            FODDescription d{};
            const bool ok = PacketDeserializer::deserializeDescription(buf, 4, d);
            Assert::IsFalse(ok);
        }

        // --- freeDescription ---

        TEST_METHOD(FreeDescription_SetsPointerToNullptr)
        {
            const char* text = "test";
            const int textLen = static_cast<int>(std::strlen(text));
            std::vector<char> buf(12 + textLen, '\0');
            writeInt(buf.data(), 0, 2);
            writeInt(buf.data(), 4, 0);
            writeInt(buf.data(), 8, textLen);
            std::memcpy(&buf[12], text, static_cast<size_t>(textLen));

            FODDescription d{};
            PacketDeserializer::deserializeDescription(buf.data(), static_cast<int>(buf.size()), d);
            PacketDeserializer::freeDescription(d);
            Assert::IsNull(d.description);
        }

        // --- verifyDescriptionChecksum ---

        TEST_METHOD(VerifyDescriptionChecksum_CorrectChecksum_ReturnsTrue)
        {
            // Compute expected checksum manually for "ABC" = 65+66+67 = 198
            const char* text = "ABC";
            const int textLen = static_cast<int>(std::strlen(text));
            std::vector<char> buf(12 + textLen, '\0');
            writeInt(buf.data(), 0, 2);
            writeInt(buf.data(), 4, 198);   // correct checksum
            writeInt(buf.data(), 8, textLen);
            std::memcpy(&buf[12], text, static_cast<size_t>(textLen));

            FODDescription d{};
            PacketDeserializer::deserializeDescription(buf.data(), static_cast<int>(buf.size()), d);
            const bool valid = PacketDeserializer::verifyDescriptionChecksum(d);
            PacketDeserializer::freeDescription(d);
            Assert::IsTrue(valid);
        }

        TEST_METHOD(VerifyDescriptionChecksum_WrongChecksum_ReturnsFalse)
        {
            const char* text = "ABC";
            const int textLen = static_cast<int>(std::strlen(text));
            std::vector<char> buf(12 + textLen, '\0');
            writeInt(buf.data(), 0, 2);
            writeInt(buf.data(), 4, 0);     // deliberately wrong
            writeInt(buf.data(), 8, textLen);
            std::memcpy(&buf[12], text, static_cast<size_t>(textLen));

            FODDescription d{};
            PacketDeserializer::deserializeDescription(buf.data(), static_cast<int>(buf.size()), d);
            const bool valid = PacketDeserializer::verifyDescriptionChecksum(d);
            PacketDeserializer::freeDescription(d);
            Assert::IsFalse(valid);
        }

        TEST_METHOD(VerifyDescriptionChecksum_NullDescription_ReturnsFalse)
        {
            FODDescription d{};
            d.description = nullptr;
            d.checksum = 0;
            const bool valid = PacketDeserializer::verifyDescriptionChecksum(d);
            Assert::IsFalse(valid);
        }
    };
}