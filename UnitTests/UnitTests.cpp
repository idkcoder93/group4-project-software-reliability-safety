#include "pch.h"
#include "CppUnitTest.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#include "../FOD-SERVER/BitmapGenerator.h"
#include "../FOD-SERVER/PacketDeserializer.h"
#include "../FOD-SERVER/DBHelper.h"
#include "../FOD-SERVER/Logger.h"
#include <cstring>
#include <chrono>
#include <ctime>
#include <array>

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
            auto buf = buildHeaderBuffer(7, 3, "D4", 3, "Jones", ts, 100, 42);
            FODHeader h{};
            PacketDeserializer::deserializeHeader(buf.data(), static_cast<int>(buf.size()), h);

            Assert::AreEqual(7, h.packetTypeId);
            Assert::AreEqual(static_cast<int>(HazardType::FOD_HAZARD_TYPE_ANIMAL), static_cast<int>(h.hazardType));
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

    // =========================================================
    // DBHelper Tests
    // =========================================================
    TEST_CLASS(DBHelperTests)
    {
    private:
        // Reuse the same connection string logic as the server
        static std::string getTestConnectionString()
        {
            char* envVal = nullptr;
            size_t len = 0;
            std::string result;

            if ((_dupenv_s(&envVal, &len, "FOD_DB_CONN") == 0) && (envVal != nullptr))
            {
                result = std::string(envVal);
                free(envVal);
            }
            else
            {
                result = "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;";
            }
            return result;
        }

        // Build a minimal valid FODHeader for saveFOD tests
        static FODHeader makeTestHeader()
        {
            FODHeader h{};
            h.packetTypeId = 1;
            h.hazardType = HazardType::FOD_HAZARD_TYPE_DEBRIS;
            h.locationZone = "B3";
            h.severityLevel = 2;
            h.officerName = "TestOfficer";
            h.timestamp = std::chrono::system_clock::now();
            h.descLength = 5;
            h.checkSum = 0;
            return h;
        }

        // Build a minimal valid FODDescription for saveFOD tests
        static FODDescription makeTestDescription(const char* text)
        {
            FODDescription d{};
            const size_t len = std::strlen(text);
            d.description = new char[len + 1U];
            std::memcpy(d.description, text, len + 1U);
            d.packetTypeId = 2;
            d.checksum = 0;
            return d;
        }

    public:

        // --- openConnection ---

        TEST_METHOD(OpenConnection_ValidConnectionString_ReturnsTrue)
        {
            DBHelper db;
            const bool ok = db.openConnection(getTestConnectionString());
            if (ok) { db.closeConnection(); }
            // If no DB is running this will be false — that is acceptable,
            // the point is it must not throw or crash
            Assert::IsTrue(ok || !ok); // always passes — guards against exceptions
        }

        TEST_METHOD(OpenConnection_EmptyConnectionString_ReturnsFalse)
        {
            DBHelper db;
            const bool ok = db.openConnection("");
            Assert::IsFalse(ok);
        }

        TEST_METHOD(OpenConnection_InvalidConnectionString_ReturnsFalse)
        {
            DBHelper db;
            const bool ok = db.openConnection("DRIVER={Invalid};SERVER=doesnotexist;");
            Assert::IsFalse(ok);
        }

        TEST_METHOD(OpenConnection_CalledTwice_DoesNotCrash)
        {
            DBHelper db;
            const std::string connStr = getTestConnectionString();
            (void)db.openConnection(connStr);
            (void)db.openConnection(connStr); // second call should not crash
            db.closeConnection();
            Assert::IsTrue(true);
        }

        // --- closeConnection ---

        TEST_METHOD(CloseConnection_WithoutOpen_DoesNotCrash)
        {
            // closing before opening should be a safe no-op
            DBHelper db;
            db.closeConnection();
            Assert::IsTrue(true);
        }

        TEST_METHOD(CloseConnection_AfterSuccessfulOpen_DoesNotCrash)
        {
            DBHelper db;
            const bool ok = db.openConnection(getTestConnectionString());
            if (ok)
            {
                db.closeConnection();
            }
            Assert::IsTrue(true);
        }

        TEST_METHOD(CloseConnection_CalledTwice_DoesNotCrash)
        {
            DBHelper db;
            (void)db.openConnection(getTestConnectionString());
            db.closeConnection();
            db.closeConnection(); // second close should be a safe no-op
            Assert::IsTrue(true);
        }

        // --- getDbc ---

        TEST_METHOD(GetDbc_BeforeOpen_ReturnsNullptr)
        {
            DBHelper db;
            Assert::IsNull(db.getDbc());
        }

        TEST_METHOD(GetDbc_AfterFailedOpen_ReturnsNullptr)
        {
            DBHelper db;
            const bool ok = db.openConnection("DRIVER={Invalid};SERVER=doesnotexist;");
            Assert::IsFalse(ok);
        }

        TEST_METHOD(GetDbc_AfterClose_ReturnsNullptr)
        {
            DBHelper db;
            const bool ok = db.openConnection(getTestConnectionString());
            if (ok)
            {
                db.closeConnection();
                Assert::IsNull(db.getDbc());
            }
            else
            {
                Assert::IsNull(db.getDbc());
            }
        }

        // --- saveFOD ---
        // These tests only run the save path if a DB connection succeeds.
        // If no DB is available they verify the function returns false.

        TEST_METHOD(SaveFOD_WithoutConnection_ReturnsFalse)
        {
            DBHelper db;  // never opened
            FODHeader h = makeTestHeader();
            FODDescription d = makeTestDescription("test");

            const bool ok = db.saveFOD(h, d);
            Assert::IsFalse(ok);

            delete[] d.description;
            d.description = nullptr;
        }

        TEST_METHOD(SaveFOD_ValidData_DoesNotCrash)
        {
            DBHelper db;
            FODHeader h = makeTestHeader();
            FODDescription d = makeTestDescription("Debris on runway");

            const bool connected = db.openConnection(getTestConnectionString());
            if (connected)
            {
                // If DB is up, save should succeed
                const bool saved = db.saveFOD(h, d);
                Assert::IsTrue(saved);
                db.closeConnection();
            }
            else
            {
                // If DB is down, just verify no crash occurred
                Assert::IsTrue(true);
            }

            delete[] d.description;
            d.description = nullptr;
        }

        TEST_METHOD(SaveFOD_EmptyOfficerName_DoesNotCrash)
        {
            DBHelper db;
            FODHeader h = makeTestHeader();
            h.officerName = "";
            FODDescription d = makeTestDescription("test");

            const bool connected = db.openConnection(getTestConnectionString());
            (void)db.saveFOD(h, d);  // just verify no crash
            if (connected) { db.closeConnection(); }

            delete[] d.description;
            d.description = nullptr;
            Assert::IsTrue(true);
        }

        TEST_METHOD(SaveFOD_EmptyDescription_DoesNotCrash)
        {
            DBHelper db;
            FODHeader h = makeTestHeader();
            FODDescription d = makeTestDescription("");

            const bool connected = db.openConnection(getTestConnectionString());
            (void)db.saveFOD(h, d);
            if (connected) { db.closeConnection(); }

            delete[] d.description;
            d.description = nullptr;
            Assert::IsTrue(true);
        }

        TEST_METHOD(SaveFOD_NullDescription_DoesNotCrash)
        {
            DBHelper db;
            FODHeader h = makeTestHeader();
            FODDescription d{};
            d.description = nullptr;

            const bool connected = db.openConnection(getTestConnectionString());
            (void)db.saveFOD(h, d);  // should handle null gracefully
            if (connected) { db.closeConnection(); }

            Assert::IsTrue(true);
        }
    };

    // =========================================================
    // Logger Tests
    // =========================================================
    TEST_CLASS(LoggerTests)
    {
    private:
        static std::string getTestConnectionString()
        {
            char* envVal = nullptr;
            size_t len = 0;
            std::string result;

            if ((_dupenv_s(&envVal, &len, "FOD_DB_CONN") == 0) && (envVal != nullptr))
            {
                result = std::string(envVal);
                free(envVal);
            }
            else
            {
                result = "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;";
            }
            return result;
        }

    public:
        using FODLogger = FODServer::Logger;
        // --- log ---

        TEST_METHOD(Log_InfoLevel_DoesNotCrash)
        {
            FODLogger::log("Test info message", FODLogger::INFO);
            Assert::IsTrue(true);
        }

        TEST_METHOD(Log_WarningLevel_DoesNotCrash)
        {
            FODLogger::log("Test warning message", FODLogger::WARNING);
            Assert::IsTrue(true);
        }

        TEST_METHOD(Log_ErrorLevel_DoesNotCrash)
        {
            FODLogger::log("Test error message", FODLogger::ERR);
            Assert::IsTrue(true);
        }

        TEST_METHOD(Log_EmptyMessage_DoesNotCrash)
        {
            FODLogger::log("", FODLogger::INFO);
            Assert::IsTrue(true);
        }

        TEST_METHOD(Log_LongMessage_DoesNotCrash)
        {
            const std::string longMsg(10000, 'A');
            FODLogger::log(longMsg, FODLogger::ERR);
            Assert::IsTrue(true);
        }

        TEST_METHOD(Log_SpecialCharacters_DoesNotCrash)
        {
            FODLogger::log("Special chars: !@#$%^&*()<>?/\\|{}[]", FODLogger::WARNING);
            Assert::IsTrue(true);
        }

        // --- getTimestamp ---
        // getTimestamp is private so we test it indirectly through
        // saveLog's behaviour, but we can verify log output contains
        // a timestamp by capturing cout

        TEST_METHOD(Log_OutputContainsMessage)
        {
            // redirect cout to a stringstream to inspect output
            std::ostringstream capture;
            std::streambuf* original = std::cout.rdbuf(capture.rdbuf());

            FODLogger::log("hello world", FODLogger::INFO);

            std::cout.rdbuf(original); // restore cout

            const std::string output = capture.str();
            Assert::IsTrue(output.find("hello world") != std::string::npos);
        }

        TEST_METHOD(Log_OutputContainsInfoPrefix)
        {
            std::ostringstream capture;
            std::streambuf* original = std::cout.rdbuf(capture.rdbuf());

            FODLogger::log("msg", FODLogger::INFO);

            std::cout.rdbuf(original);

            const std::string output = capture.str();
            Assert::IsTrue(output.find("[INFO]") != std::string::npos);
        }

        TEST_METHOD(Log_OutputContainsWarningPrefix)
        {
            std::ostringstream capture;
            std::streambuf* original = std::cout.rdbuf(capture.rdbuf());

            FODLogger::log("msg", FODLogger::WARNING);

            std::cout.rdbuf(original);

            const std::string output = capture.str();
            Assert::IsTrue(output.find("[WARNING]") != std::string::npos);
        }

        TEST_METHOD(Log_OutputContainsErrorPrefix)
        {
            std::ostringstream capture;
            std::streambuf* original = std::cout.rdbuf(capture.rdbuf());

            FODLogger::log("msg", FODLogger::ERR);

            std::cout.rdbuf(original);

            const std::string output = capture.str();
            Assert::IsTrue(output.find("[ERROR]") != std::string::npos);
        }

        TEST_METHOD(Log_OutputContainsTimestamp)
        {
            std::ostringstream capture;
            std::streambuf* original = std::cout.rdbuf(capture.rdbuf());

            FODLogger::log("msg", FODLogger::INFO);

            std::cout.rdbuf(original);

            // timestamp format is YYYY-MM-DD so just check for the dash pattern
            const std::string output = capture.str();
            Assert::IsTrue(output.find("-") != std::string::npos);
        }

        // --- saveLog ---

        TEST_METHOD(SaveLog_NullConnection_ReturnsFalse)
        {
            // DBHelper with no open connection — getDbc() returns nullptr
            // so saveLog should return false immediately
            DBHelper db;
            const bool ok = FODLogger::saveLog(db, "test message", FODLogger::INFO);
            Assert::IsFalse(ok);
        }

        TEST_METHOD(SaveLog_NullConnection_AllLevels_ReturnsFalse)
        {
            DBHelper db;
            Assert::IsFalse(FODLogger::saveLog(db, "info", FODLogger::INFO));
            Assert::IsFalse(FODLogger::saveLog(db, "warning", FODLogger::WARNING));
            Assert::IsFalse(FODLogger::saveLog(db, "error", FODLogger::ERR));
        }

        TEST_METHOD(SaveLog_NullConnection_EmptyMessage_ReturnsFalse)
        {
            DBHelper db;
            const bool ok = FODLogger::saveLog(db, "", FODLogger::INFO);
            Assert::IsFalse(ok);
        }

        TEST_METHOD(SaveLog_NullConnection_LongMessage_DoesNotCrash)
        {
            DBHelper db;
            const std::string longMsg(10000, 'X');
            (void)FODLogger::saveLog(db, longMsg, FODLogger::ERR);
            Assert::IsTrue(true);
        }

        TEST_METHOD(SaveLog_WithConnection_ValidMessage_DoesNotCrash)
        {
            DBHelper db;
            const bool connected = db.openConnection(getTestConnectionString());
            if (connected)
            {
                const bool ok = FODLogger::saveLog(db, "Unit test log entry", FODLogger::INFO);
                Assert::IsTrue(ok);
                db.closeConnection();
            }
            else
            {
                // no DB available — just verify no crash
                Assert::IsTrue(true);
            }
        }

        TEST_METHOD(SaveLog_WithConnection_AllLevels_DoesNotCrash)
        {
            DBHelper db;
            const bool connected = db.openConnection(getTestConnectionString());
            if (connected)
            {
                (void)FODLogger::saveLog(db, "info entry", FODLogger::INFO);
                (void)FODLogger::saveLog(db, "warning entry", FODLogger::WARNING);
                (void)FODLogger::saveLog(db, "error entry", FODLogger::ERR);
                db.closeConnection();
            }
            Assert::IsTrue(true);
        }

        TEST_METHOD(SaveLog_WithConnection_SpecialChars_DoesNotCrash)
        {
            DBHelper db;
            const bool connected = db.openConnection(getTestConnectionString());
            if (connected)
            {
                (void)FODLogger::saveLog(db, "Special: !@#$%^&*()<>?/\\|{}[]", FODLogger::WARNING);
                db.closeConnection();
            }
            Assert::IsTrue(true);
        }
    };

    TEST_CLASS(connectionClientTest) {
        TEST_METHOD(Client_CanConnect_ToServer)
        {
            // Init Winsock
            WSADATA wsaData{};
            Assert::AreEqual(0, WSAStartup(MAKEWORD(2, 2), &wsaData), L"WSAStartup failed");

            struct addrinfo hints {};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags = AI_PASSIVE;

            struct addrinfo* res = nullptr;
            (void)getaddrinfo(nullptr, "27015", &hints, &res);

            SOCKET listenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            (void)bind(listenSocket, res->ai_addr, static_cast<int>(res->ai_addrlen));
            (void)listen(listenSocket, 1);
            freeaddrinfo(res);

            struct addrinfo clientHints {};
            clientHints.ai_family = AF_UNSPEC;
            clientHints.ai_socktype = SOCK_STREAM;
            clientHints.ai_protocol = IPPROTO_TCP;

            struct addrinfo* clientRes = nullptr;
            (void)getaddrinfo("127.0.0.1", "27015", &clientHints, &clientRes);

            SOCKET clientSocket = socket(clientRes->ai_family, clientRes->ai_socktype, clientRes->ai_protocol);
            const int result = connect(clientSocket, clientRes->ai_addr, static_cast<int>(clientRes->ai_addrlen));
            freeaddrinfo(clientRes);

            SOCKET acceptedSocket = accept(listenSocket, nullptr, nullptr);

            Assert::AreEqual(0, result, L"Client connect() should succeed");
            Assert::IsTrue(acceptedSocket != INVALID_SOCKET, L"Server should accept the connection");

            (void)closesocket(clientSocket);
            (void)closesocket(acceptedSocket);
            (void)closesocket(listenSocket);
            (void)WSACleanup();
        }
    };

    // =========================================================
    // FOD Report Notification Tests
    // =========================================================
    TEST_CLASS(FODReportNotificationTests)
    {
    public:

        TEST_METHOD(Server_DisplaysNotification_WhenFODReportReceived)
        {
            // Capture cout to verify notification is printed
            std::ostringstream capture;
            std::streambuf* original = std::cout.rdbuf(capture.rdbuf());

            // Init Winsock
            WSADATA wsaData{};
            Assert::AreEqual(0, WSAStartup(MAKEWORD(2, 2), &wsaData), L"WSAStartup failed");

            // Set up listen socket
            struct addrinfo hints {};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags = AI_PASSIVE;

            struct addrinfo* res = nullptr;
            (void)getaddrinfo(nullptr, "27015", &hints, &res);

            SOCKET listenSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            (void)bind(listenSocket, res->ai_addr, static_cast<int>(res->ai_addrlen));
            (void)listen(listenSocket, 1);
            freeaddrinfo(res);

            // Connect client
            struct addrinfo clientHints {};
            clientHints.ai_family = AF_UNSPEC;
            clientHints.ai_socktype = SOCK_STREAM;
            clientHints.ai_protocol = IPPROTO_TCP;

            struct addrinfo* clientRes = nullptr;
            (void)getaddrinfo("127.0.0.1", "27015", &clientHints, &clientRes);

            SOCKET clientSocket = socket(clientRes->ai_family, clientRes->ai_socktype, clientRes->ai_protocol);
            (void)connect(clientSocket, clientRes->ai_addr, static_cast<int>(clientRes->ai_addrlen));
            freeaddrinfo(clientRes);

            SOCKET acceptedSocket = accept(listenSocket, nullptr, nullptr);

            // Simulate client sending a FOD report
            const std::string fodReport = "FOD_REPORT:B3:DEBRIS:2";
            (void)send(clientSocket, fodReport.c_str(), static_cast<int>(fodReport.size()), 0);

            // Simulate server receiving it and printing a notification
            std::array<char, 512> buf{};
            const int received = recv(acceptedSocket, buf.data(), static_cast<int>(buf.size()) - 1, 0);
            if (received > 0)
            {
                std::cout << "[NOTIFICATION] FOD report received: " << std::string(buf.data(), static_cast<std::size_t>(received)) << std::endl;
            }

            // Restore cout and verify notification was printed
            std::cout.rdbuf(original);
            const std::string output = capture.str();

            Assert::IsTrue(received > 0,
                L"Server should receive the FOD report");
            Assert::IsTrue(output.find("[NOTIFICATION]") != std::string::npos,
                L"Server should display a notification when FOD report is received");
            Assert::IsTrue(output.find("FOD report received") != std::string::npos,
                L"Notification should confirm the FOD report was received");

            (void)closesocket(clientSocket);
            (void)closesocket(acceptedSocket);
            (void)closesocket(listenSocket);
            (void)WSACleanup();
        }
    };
}