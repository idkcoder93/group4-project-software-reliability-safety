#include "pch.h"
#include "CppUnitTest.h"
#include "../FOD-SERVER/BitmapGenerator.h"
#include <cstring>
#include <cstdint>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    // =========================================================
    // Large Data Transfer Tests
    //
    // Primary coverage: REQ-SYS-070 (system shall have at least
    // one command that initiates a large data transfer of a
    // bitmap-like object >= 1 MB from Server to Client).
    //
    // The server's generateRunwayBitmap() is the artifact that
    // satisfies REQ-SYS-070. These tests verify:
    //   1. The generated bitmap is >= 1 MB (the hard requirement)
    //   2. The bitmap is a well-formed BMP that will decode on
    //      the receiving client (integrity of the transfer payload)
    //   3. Size consistency across zones (the transfer size is
    //      deterministic - important for buffer sizing on the
    //      client side, which uses a 5 MB upper bound check)
    //   4. The bitmap data is not all zeros, proving pixel data
    //      is actually generated rather than just a header
    //
    // Secondary coverage: REQ-SYS-010 (pre-defined structure for
    // data transferred between Client and Server - BMP file
    // header is the pre-defined structure for the bitmap packet).
    // =========================================================
    TEST_CLASS(LargeTransferTests)
    {
    private:
        static constexpr size_t ONE_MEGABYTE = 1024U * 1024U;  // 1,048,576 bytes
        static constexpr size_t FIVE_MEGABYTES = 5U * 1024U * 1024U;

        // Helpers for reading little-endian fields from the BMP header
        static int readInt32LE(const std::vector<char>& buf, size_t offset)
        {
            int value = 0;
            std::memcpy(&value, &buf[offset], 4);
            return value;
        }

        static short readInt16LE(const std::vector<char>& buf, size_t offset)
        {
            short value = 0;
            std::memcpy(&value, &buf[offset], 2);
            return value;
        }

    public:
        // --- REQ-SYS-070: size requirement ---

        TEST_METHOD(GenerateBitmap_SizeMeetsOneMegabyteRequirement)
        {
            // The core REQ-SYS-070 assertion: the generated bitmap
            // MUST be at least 1 MB so it qualifies as a large
            // data transfer per the system requirements.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            Assert::IsTrue(bmp.size() >= ONE_MEGABYTE,
                L"Generated bitmap must be at least 1 MB (1,048,576 bytes) "
                L"to satisfy REQ-SYS-070.");
        }

        TEST_METHOD(GenerateBitmap_SizeBelowClientUpperBound)
        {
            // The client's receiveBitmap() rejects any payload above
            // 5 MB as a safety check. Verify the server never
            // generates a payload that would trip that guard.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            Assert::IsTrue(bmp.size() <= FIVE_MEGABYTES,
                L"Generated bitmap must stay under the 5 MB client-side "
                L"receive limit defined in ClientSession::receiveBitmap.");
        }

        TEST_METHOD(GenerateBitmap_SizeIsDeterministicAcrossZones)
        {
            // Every generated bitmap must be the same size regardless
            // of which zone is highlighted. The client reads an int32
            // length prefix and allocates exactly that many bytes -
            // if the size varied, buffer handling could desynchronize.
            const auto a = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            const auto b = FODServer::BitmapGenerator::generateRunwayBitmap("F6");
            const auto c = FODServer::BitmapGenerator::generateRunwayBitmap("C3");
            Assert::AreEqual(a.size(), b.size());
            Assert::AreEqual(a.size(), c.size());
        }

        TEST_METHOD(GenerateBitmap_SizeIsDeterministicForInvalidZones)
        {
            // Even when no zone is highlighted, the full BMP must be
            // the same size - so the client can still receive a valid,
            // fixed-length bitmap payload.
            const auto valid = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            const auto invalid = FODServer::BitmapGenerator::generateRunwayBitmap("Z9");
            const auto empty = FODServer::BitmapGenerator::generateRunwayBitmap("");
            Assert::AreEqual(valid.size(), invalid.size());
            Assert::AreEqual(valid.size(), empty.size());
        }

        // --- REQ-SYS-010: pre-defined structure integrity ---

        TEST_METHOD(GenerateBitmap_StartsWithBMSignature)
        {
            // BMP file signature must be the first two bytes so
            // the client (or any image viewer) can recognize the
            // payload as a bitmap.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("B2");
            Assert::IsTrue(bmp.size() >= 2U);
            Assert::AreEqual('B', bmp[0]);
            Assert::AreEqual('M', bmp[1]);
        }

        TEST_METHOD(GenerateBitmap_FileSizeFieldMatchesVectorSize)
        {
            // BMP header's 4-byte file-size field at offset 2 must
            // equal the actual buffer size - otherwise the client
            // (or downstream tooling) will truncate or over-read.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("C3");
            const int headerReportedSize = readInt32LE(bmp, 2);
            Assert::AreEqual(static_cast<int>(bmp.size()), headerReportedSize);
        }

        TEST_METHOD(GenerateBitmap_DataOffsetIsFiftyFour)
        {
            // Pixel-data offset must equal 54 (14-byte file header +
            // 40-byte DIB header). The client relies on this to
            // parse the bitmap after receive.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            const int dataOffset = readInt32LE(bmp, 10);
            Assert::AreEqual(54, dataOffset);
        }

        TEST_METHOD(GenerateBitmap_DIBHeaderSizeIsForty)
        {
            // BITMAPINFOHEADER (40 bytes) is the DIB variant used
            // by this project.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            const int dibSize = readInt32LE(bmp, 14);
            Assert::AreEqual(40, dibSize);
        }

        TEST_METHOD(GenerateBitmap_WidthAndHeightAreSixHundred)
        {
            // Width and height must match the configured grid
            // dimensions - these drive the >=1 MB pixel-data size.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            Assert::AreEqual(600, readInt32LE(bmp, 18));  // width
            Assert::AreEqual(600, readInt32LE(bmp, 22));  // height
        }

        TEST_METHOD(GenerateBitmap_PlanesAndBppCorrect)
        {
            // Color planes = 1 and bits-per-pixel = 24 are what the
            // BMP spec requires for a 24-bit uncompressed image.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            Assert::AreEqual(static_cast<short>(1), readInt16LE(bmp, 26));
            Assert::AreEqual(static_cast<short>(24), readInt16LE(bmp, 28));
        }

        // --- Payload integrity: prove pixel data is actually generated ---

        TEST_METHOD(GenerateBitmap_PixelDataIsNotAllZeros)
        {
            // A header-only BMP would technically satisfy the size
            // requirement (via zero-fill) but would be useless to
            // the client. Verify real pixel content exists.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            Assert::IsTrue(bmp.size() > 54U);

            bool foundNonZero = false;
            for (size_t i = 54U; i < bmp.size(); ++i)
            {
                if (bmp[i] != '\0')
                {
                    foundNonZero = true;
                    break;
                }
            }
            Assert::IsTrue(foundNonZero,
                L"Pixel data region must contain non-zero bytes.");
        }

        TEST_METHOD(GenerateBitmap_HighlightedZoneHasRedPixels)
        {
            // The affected zone should be rendered in red (R=200, G=40, B=40).
            // Find at least one pixel in the expected zone area that
            // matches the red highlight color. This proves the
            // zone-highlighting feature is actually working, not just
            // that a file of sufficient size was produced.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");

            // "A1" maps to gridRow=0, gridCol=0.
            // In BMP bottom-up order, gridRow 0 covers y in [500, 599].
            // gridCol 0 covers x in [0, 99]. Check a pixel deep inside
            // that cell (away from grid-line pixels).
            const int stride = 600 * 3; // 600 width, 3 bytes per pixel, no padding
            const int y = 550;
            const int x = 50;
            const size_t offset = 54U +
                (static_cast<size_t>(y) * static_cast<size_t>(stride)) +
                (static_cast<size_t>(x) * 3U);

            // BMP byte order is B, G, R
            const unsigned char blue = static_cast<unsigned char>(bmp[offset]);
            const unsigned char green = static_cast<unsigned char>(bmp[offset + 1U]);
            const unsigned char red = static_cast<unsigned char>(bmp[offset + 2U]);

            Assert::AreEqual(static_cast<unsigned char>(200U), red);
            Assert::AreEqual(static_cast<unsigned char>(40U), green);
            Assert::AreEqual(static_cast<unsigned char>(40U), blue);
        }

        TEST_METHOD(GenerateBitmap_NonHighlightedZoneHasGreenPixels)
        {
            // Zones other than the affected one should be green
            // (R=45, G=120, B=45). This confirms the full runway
            // is rendered, not just the hazard cell.
            const auto bmp = FODServer::BitmapGenerator::generateRunwayBitmap("A1");

            // Check a pixel in zone F6 (gridRow 5, gridCol 5),
            // which should NOT be highlighted when A1 is the
            // affected zone.
            // In bottom-up BMP, gridRow 5 covers y in [0, 99].
            // gridCol 5 covers x in [500, 599].
            const int stride = 600 * 3;
            const int y = 50;
            const int x = 550;
            const size_t offset = 54U +
                (static_cast<size_t>(y) * static_cast<size_t>(stride)) +
                (static_cast<size_t>(x) * 3U);

            const unsigned char blue = static_cast<unsigned char>(bmp[offset]);
            const unsigned char green = static_cast<unsigned char>(bmp[offset + 1U]);
            const unsigned char red = static_cast<unsigned char>(bmp[offset + 2U]);

            Assert::AreEqual(static_cast<unsigned char>(45U), red);
            Assert::AreEqual(static_cast<unsigned char>(120U), green);
            Assert::AreEqual(static_cast<unsigned char>(45U), blue);
        }

        // --- Boundary / robustness ---

        TEST_METHOD(GenerateBitmap_CalledMultipleTimes_ProducesIdenticalOutput)
        {
            // Deterministic output is important for reproducible
            // integration testing and for consistent client-side
            // rendering across reports.
            const auto first = FODServer::BitmapGenerator::generateRunwayBitmap("B2");
            const auto second = FODServer::BitmapGenerator::generateRunwayBitmap("B2");
            Assert::AreEqual(first.size(), second.size());

            // byte-for-byte comparison
            const int cmp = std::memcmp(first.data(), second.data(), first.size());
            Assert::AreEqual(0, cmp,
                L"Two calls with the same zone must produce byte-identical output.");
        }

        TEST_METHOD(GenerateBitmap_DifferentZones_ProduceDifferentOutput)
        {
            // Two different highlighted zones must render
            // differently - otherwise the zone-highlight feature
            // isn't actually differentiating.
            const auto a1 = FODServer::BitmapGenerator::generateRunwayBitmap("A1");
            const auto f6 = FODServer::BitmapGenerator::generateRunwayBitmap("F6");
            Assert::AreEqual(a1.size(), f6.size());

            const int cmp = std::memcmp(a1.data(), f6.data(), a1.size());
            Assert::AreNotEqual(0, cmp,
                L"Bitmaps for different zones must differ in pixel data.");
        }
    };
}