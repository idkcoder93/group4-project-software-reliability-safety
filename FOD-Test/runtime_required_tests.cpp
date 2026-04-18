#include "pch.h"
#include "gtest/gtest.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>

#include <array>
#include <fstream>
#include <string>
#include <vector>

#include "../FOD-CLIENT/PacketSerializer.cpp"

#pragma comment(lib, "Ws2_32.lib")

namespace
{
    class WsaSession
    {
    public:
        WsaSession()
        {
            const int result = WSAStartup(MAKEWORD(2, 2), &data_);
            ok_ = (result == 0);
        }

        ~WsaSession()
        {
            if (ok_)
            {
                (void)WSACleanup();
            }
        }

        bool ok() const { return ok_; }

    private:
        WSADATA data_{};
        bool ok_{ false };
    };

    bool IsProcessRunning(const wchar_t* processName)
    {
        bool running = false;

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (Process32FirstW(snapshot, &entry) != FALSE)
        {
            do
            {
                if (_wcsicmp(entry.szExeFile, processName) == 0)
                {
                    running = true;
                    break;
                }
            } while (Process32NextW(snapshot, &entry) != FALSE);
        }

        (void)CloseHandle(snapshot);
        return running;
    }

    bool SendAll(SOCKET socketHandle, const char* data, int length)
    {
        int total = 0;
        while (total < length)
        {
            const int sent = send(socketHandle, &data[total], length - total, 0);
            if (sent == SOCKET_ERROR)
            {
                return false;
            }
            total += sent;
        }
        return true;
    }

    bool RecvAll(SOCKET socketHandle, char* data, int length)
    {
        int total = 0;
        while (total < length)
        {
            const int received = recv(socketHandle, &data[total], length - total, 0);
            if (received <= 0)
            {
                return false;
            }
            total += received;
        }
        return true;
    }

    bool SendInt(SOCKET socketHandle, int value)
    {
        return SendAll(socketHandle, reinterpret_cast<const char*>(&value), 4);
    }

    bool RecvInt(SOCKET socketHandle, int& value)
    {
        return RecvAll(socketHandle, reinterpret_cast<char*>(&value), 4);
    }

    std::wstring GetModuleDirectory()
    {
        std::wstring result;
        wchar_t buffer[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
        if (length != 0U)
        {
            result.assign(buffer, buffer + length);
            const std::wstring::size_type pos = result.find_last_of(L"\\/");
            if (pos != std::wstring::npos)
            {
                result.erase(pos);
            }
        }
        return result;
    }

    std::wstring BuildExecutablePath(const wchar_t* fileName)
    {
        std::wstring path = GetModuleDirectory();
        if (!path.empty())
        {
            path += L"\\";
            path += fileName;
        }
        return path;
    }

    std::wstring BuildFilePath(const wchar_t* fileName)
    {
        return BuildExecutablePath(fileName);
    }

    bool ConnectToLiveServer(SOCKET& socketHandle);
    bool AuthenticateLiveServer(SOCKET socketHandle, const std::string& username, const std::string& password, char& authResponse);
    void ShutdownAndCloseSocket(SOCKET& socketHandle);

    bool LaunchProcess(const wchar_t* fileName, PROCESS_INFORMATION& processInfo)
    {
        const std::wstring exePath = BuildExecutablePath(fileName);
        const std::wstring workingDirectory = GetModuleDirectory();
        if (exePath.empty() || workingDirectory.empty())
        {
            return false;
        }

        std::wstring commandLine = L"\"" + exePath + L"\"";
        std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
        commandLineBuffer.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);

        ZeroMemory(&processInfo, sizeof(processInfo));
        const BOOL created = CreateProcessW(
            nullptr,
            commandLineBuffer.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NEW_CONSOLE,
            nullptr,
            workingDirectory.c_str(),
            &startupInfo,
            &processInfo);

        return (created != FALSE);
    }

    bool WaitForServerReady()
    {
        WsaSession wsa;
        if (!wsa.ok())
        {
            return false;
        }

        SOCKET probe = INVALID_SOCKET;
        if (!ConnectToLiveServer(probe))
        {
            return false;
        }

        char authResponse = 0x00;
        const bool authenticated = AuthenticateLiveServer(probe, "admin", "pass@123", authResponse);
        ShutdownAndCloseSocket(probe);
        return authenticated && (authResponse == 0x01);
    }

    std::string ReadTextFile(const std::wstring& filePath)
    {
        const std::string narrowPath(filePath.begin(), filePath.end());
        std::ifstream input(narrowPath.c_str(), std::ios::in | std::ios::binary);
        if (!input.is_open())
        {
            return std::string();
        }

        std::string contents;
        input.seekg(0, std::ios::end);
        const std::streampos endPos = input.tellg();
        if (endPos <= 0)
        {
            return std::string();
        }

        contents.resize(static_cast<std::size_t>(endPos));
        input.seekg(0, std::ios::beg);
        (void)input.read(&contents[0], static_cast<std::streamsize>(contents.size()));
        return contents;
    }

    bool SendLengthPrefixedBytes(SOCKET socketHandle, const std::vector<char>& bytes)
    {
        const int length = static_cast<int>(bytes.size());
        return SendInt(socketHandle, length) && SendAll(socketHandle, bytes.data(), length);
    }

    bool SendLengthPrefixedString(SOCKET socketHandle, const std::string& text)
    {
        const int length = static_cast<int>(text.size());
        return SendInt(socketHandle, length) && SendAll(socketHandle, text.data(), length);
    }

    bool ConnectToLiveServer(SOCKET& socketHandle)
    {
        socketHandle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketHandle == INVALID_SOCKET)
        {
            return false;
        }

        DWORD timeoutMs = 5000;
        (void)setsockopt(socketHandle, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(27015);
        serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        const int connectResult = connect(socketHandle, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
        if (connectResult == SOCKET_ERROR)
        {
            (void)closesocket(socketHandle);
            socketHandle = INVALID_SOCKET;
            return false;
        }

        return true;
    }

    bool AuthenticateLiveServer(SOCKET socketHandle, const std::string& username, const std::string& password, char& authResponse)
    {
        const std::string payload = username + ":" + password;
        const int payloadLength = static_cast<int>(payload.size());
        if (!SendInt(socketHandle, payloadLength) || !SendAll(socketHandle, payload.data(), payloadLength))
        {
            return false;
        }

        authResponse = 0x00;
        return (recv(socketHandle, &authResponse, 1, 0) == 1);
    }

    bool SendHeartbeatAndReceiveAck(SOCKET socketHandle)
    {
        const int heartbeatLen = 4;
        const int heartbeatType = 0x06;
        if (!SendInt(socketHandle, heartbeatLen) || !SendInt(socketHandle, heartbeatType))
        {
            return false;
        }

        int ackLen = 0;
        int ackType = 0;
        return RecvInt(socketHandle, ackLen) && (ackLen == 4) && RecvInt(socketHandle, ackType) && (ackType == 0x06);
    }

    bool SendMalformedReportPacket(SOCKET socketHandle)
    {
        const int truncatedPacketLength = 4;
        const int packetType = 0x03;
        return SendInt(socketHandle, truncatedPacketLength) && SendInt(socketHandle, packetType);
    }

    bool SubmitLiveFodReport(SOCKET socketHandle,
        HazardType hazard,
        const std::string& zone,
        int severity,
        const std::string& officer,
        const std::string& description,
        std::string& response,
        int& bitmapPacketType,
        std::vector<char>& bitmapBytes)
    {
        FODDescription desc = PacketSerializer::buildDescription(description);
        FODHeader header = PacketSerializer::buildHeader(hazard, zone, severity, officer,
            static_cast<int>(description.size()));

        const std::vector<char> headerBytes = PacketSerializer::serializeHeader(header);
        const std::vector<char> descriptionBytes = PacketSerializer::serializeDescription(desc);
        PacketSerializer::freeDescription(desc);

        if (!SendLengthPrefixedBytes(socketHandle, headerBytes) ||
            !SendLengthPrefixedBytes(socketHandle, descriptionBytes))
        {
            return false;
        }

        int responseLength = 0;
        if (!RecvInt(socketHandle, responseLength) || (responseLength <= 0) || (responseLength > 4096))
        {
            return false;
        }

        response.assign(static_cast<std::size_t>(responseLength), '\0');
        if (!RecvAll(socketHandle, &response[0], responseLength))
        {
            return false;
        }

        int bitmapSize = 0;
        if (!RecvInt(socketHandle, bitmapPacketType) || (bitmapPacketType != 0x05) ||
            !RecvInt(socketHandle, bitmapSize) || (bitmapSize <= 54))
        {
            return false;
        }

        bitmapBytes.assign(static_cast<std::size_t>(bitmapSize), '\0');
        if (!RecvAll(socketHandle, bitmapBytes.data(), bitmapSize))
        {
            return false;
        }

        return true;
    }

    void ShutdownAndCloseSocket(SOCKET& socketHandle)
    {
        if (socketHandle != INVALID_SOCKET)
        {
            (void)shutdown(socketHandle, SD_BOTH);
            (void)closesocket(socketHandle);
            socketHandle = INVALID_SOCKET;
        }
    }

    bool IsBitmapHeaderValid(const std::vector<char>& bitmapBytes)
    {
        return (bitmapBytes.size() > 54U) && (bitmapBytes[0] == 'B') && (bitmapBytes[1] == 'M');
    }

    bool ReadClientLogContains(const std::string& marker)
    {
        const std::wstring logPath = BuildFilePath(L"client_log.txt");
        const std::string contents = ReadTextFile(logPath);
        return (contents.find(marker) != std::string::npos);
    }

    class LiveAppHarness
    {
    public:
        ~LiveAppHarness()
        {
            Stop();
        }

        void EnsureServerRunning()
        {
            ConfigureAutomationEnvironment();
            CleanupFinishedProcess(serverProcess_, serverStarted_);

            if (!IsProcessRunning(L"FOD-SERVER.exe"))
            {
                ASSERT_TRUE(LaunchProcess(L"FOD-SERVER.exe", serverProcess_));
                serverStarted_ = true;
                ASSERT_TRUE(WaitForServerReady());
            }
        }

        void EnsureClientRunning()
        {
            ConfigureAutomationEnvironment();
            EnsureServerRunning();
            CleanupFinishedProcess(clientProcess_, clientStarted_);

            if (!IsProcessRunning(L"FOD-CLIENT.exe"))
            {
                ASSERT_TRUE(LaunchProcess(L"FOD-CLIENT.exe", clientProcess_));
                clientStarted_ = true;
            }
        }

        void StopClient()
        {
            if (clientProcess_.hProcess != nullptr)
            {
                (void)TerminateProcess(clientProcess_.hProcess, 0);
                (void)CloseHandle(clientProcess_.hThread);
                (void)CloseHandle(clientProcess_.hProcess);
                clientProcess_.hProcess = nullptr;
                clientProcess_.hThread = nullptr;
            }

            clientStarted_ = false;
        }

    private:
        static void ConfigureAutomationEnvironment()
        {
            (void)SetEnvironmentVariableW(L"FOD_AUTOMATED_TESTING", L"1");
            (void)SetEnvironmentVariableW(L"FOD_TEST_USERNAME", L"admin");
            (void)SetEnvironmentVariableW(L"FOD_TEST_PASSWORD", L"pass@123");
        }

        static void CleanupFinishedProcess(PROCESS_INFORMATION& processInfo, bool& started)
        {
            if ((processInfo.hProcess != nullptr) && (WaitForSingleObject(processInfo.hProcess, 0) == WAIT_OBJECT_0))
            {
                (void)CloseHandle(processInfo.hThread);
                (void)CloseHandle(processInfo.hProcess);
                processInfo.hThread = nullptr;
                processInfo.hProcess = nullptr;
                started = false;
            }
        }

        void Stop()
        {
            if ((clientProcess_.hProcess != nullptr) || (serverProcess_.hProcess != nullptr))
            {
                (void)Sleep(10000);
            }

            if (clientProcess_.hProcess != nullptr)
            {
                (void)TerminateProcess(clientProcess_.hProcess, 0);
                (void)CloseHandle(clientProcess_.hThread);
                (void)CloseHandle(clientProcess_.hProcess);
                clientProcess_.hProcess = nullptr;
                clientProcess_.hThread = nullptr;
            }

            if (serverProcess_.hProcess != nullptr)
            {
                (void)TerminateProcess(serverProcess_.hProcess, 0);
                (void)CloseHandle(serverProcess_.hThread);
                (void)CloseHandle(serverProcess_.hProcess);
                serverProcess_.hProcess = nullptr;
                serverProcess_.hThread = nullptr;
            }
        }

        PROCESS_INFORMATION serverProcess_{};
        PROCESS_INFORMATION clientProcess_{};
        bool serverStarted_{ false };
        bool clientStarted_{ false };
    };

    LiveAppHarness& Harness()
    {
        static LiveAppHarness harness;
        return harness;
    }
}

TEST(FODRuntimeRequiredTests, ServerProcessMustBeRunning)
{
    Harness().EnsureServerRunning();
    EXPECT_TRUE(IsProcessRunning(L"FOD-SERVER.exe"));
}

TEST(FODRuntimeRequiredTests, ClientProcessMustBeRunning)
{
    Harness().EnsureClientRunning();
    EXPECT_TRUE(IsProcessRunning(L"FOD-CLIENT.exe"));
}

TEST(FODRuntimeRequiredTests, LiveAppDemoWindowsAreVisible)
{
    Harness().EnsureClientRunning();
    EXPECT_TRUE(IsProcessRunning(L"FOD-SERVER.exe"));
    EXPECT_TRUE(IsProcessRunning(L"FOD-CLIENT.exe"));
    (void)Sleep(10000);
    Harness().StopClient();
    (void)Sleep(1000);
}

TEST(FODRuntimeRequiredTests, RunningServerAcceptsAuthenticatedClientOnDefaultPort)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "pass@123", authResponse));
    EXPECT_EQ(authResponse, 0x01);

    ShutdownAndCloseSocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerAcceptsSecondAuthenticatedClientAfterDisconnect)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET firstSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(firstSocket));
    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(firstSocket, "admin", "pass@123", authResponse));
    EXPECT_EQ(authResponse, 0x01);
    ShutdownAndCloseSocket(firstSocket);

    SOCKET secondSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(secondSocket));
    authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(secondSocket, "admin", "pass@123", authResponse));
    EXPECT_EQ(authResponse, 0x01);
    ShutdownAndCloseSocket(secondSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerReturnsClearedStatusForNoDebrisReport)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "pass@123", authResponse));
    ASSERT_EQ(authResponse, 0x01);

    std::string response;
    int bitmapPacketType = 0;
    std::vector<char> bitmapBytes;
    ASSERT_TRUE(SubmitLiveFodReport(clientSocket,
        FOD_HAZARD_TYPE_DEBRIS,
        "A1",
        0,
        "Tower Demo",
        "No debris found during inspection.",
        response,
        bitmapPacketType,
        bitmapBytes));

    EXPECT_NE(response.find("Runway Status: CLEARED"), std::string::npos);
    EXPECT_EQ(bitmapPacketType, 0x05);
    EXPECT_TRUE(IsBitmapHeaderValid(bitmapBytes));

    ShutdownAndCloseSocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerSendsBitmapForHazardReport)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "pass@123", authResponse));
    ASSERT_EQ(authResponse, 0x01);

    std::string response;
    int bitmapPacketType = 0;
    std::vector<char> bitmapBytes;
    ASSERT_TRUE(SubmitLiveFodReport(clientSocket,
        FOD_HAZARD_TYPE_DEBRIS,
        "B2",
        4,
        "Tower Demo",
        "Debris reported near the runway edge.",
        response,
        bitmapPacketType,
        bitmapBytes));

    EXPECT_NE(response.find("Runway Status: HAZARD"), std::string::npos);
    EXPECT_EQ(bitmapPacketType, 0x05);
    EXPECT_GT(bitmapBytes.size(), 1024U * 1024U);
    EXPECT_TRUE(IsBitmapHeaderValid(bitmapBytes));

    ShutdownAndCloseSocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, ClientLogContainsAuthenticatedSessionEntriesAfterAutomatedRun)
{
    Harness().StopClient();
    const std::wstring logPath = BuildFilePath(L"client_log.txt");
    (void)DeleteFileW(logPath.c_str());

    Harness().EnsureClientRunning();
    (void)Sleep(2000);
    Harness().StopClient();

    const std::string contents = ReadTextFile(logPath);
    ASSERT_FALSE(contents.empty());
    EXPECT_NE(contents.find("Session:SESSION_1"), std::string::npos);
    EXPECT_NE(contents.find("[AUTH] [SENT]"), std::string::npos);
    EXPECT_NE(contents.find("[AUTH] [RECEIVED] [Session:SESSION_1] ACCEPTED"), std::string::npos);
}

TEST(FODRuntimeRequiredTests, RunningServerRespondsToInvalidAuthPacket)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(clientSocket, INVALID_SOCKET);

    DWORD timeoutMs = 2000;
    (void)setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(27015);
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    ASSERT_NE(connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)), SOCKET_ERROR);

    const std::string payload = "runtime-test-user:runtime-test-password";
    const int payloadLength = static_cast<int>(payload.size());
    ASSERT_TRUE(SendAll(clientSocket, reinterpret_cast<const char*>(&payloadLength), 4));
    ASSERT_TRUE(SendAll(clientSocket, payload.data(), payloadLength));

    char authResponse = 0x01;
    const int received = recv(clientSocket, &authResponse, 1, 0);
    ASSERT_EQ(received, 1);
    EXPECT_EQ(authResponse, 0x00);

    (void)shutdown(clientSocket, SD_BOTH);
    (void)closesocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerRejectsInvalidCredentials)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x01;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "wrong-password", authResponse));
    EXPECT_EQ(authResponse, 0x00);

    ShutdownAndCloseSocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerAcknowledgesHeartbeatPacket)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "pass@123", authResponse));
    ASSERT_EQ(authResponse, 0x01);

    ASSERT_TRUE(SendHeartbeatAndReceiveAck(clientSocket));

    ShutdownAndCloseSocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerRejectsMalformedReportPacketAfterAuth)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "pass@123", authResponse));
    ASSERT_EQ(authResponse, 0x01);

    ASSERT_TRUE(SendMalformedReportPacket(clientSocket));

    char closeProbe = 0;
    const int received = recv(clientSocket, &closeProbe, 1, 0);
    EXPECT_LE(received, 0);

    ShutdownAndCloseSocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerProcessesMultipleReportsInSingleSession)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "pass@123", authResponse));
    ASSERT_EQ(authResponse, 0x01);

    std::string responseOne;
    int bitmapPacketTypeOne = 0;
    std::vector<char> bitmapOne;
    ASSERT_TRUE(SubmitLiveFodReport(clientSocket,
        FOD_HAZARD_TYPE_DEBRIS,
        "A1",
        0,
        "Tower Demo",
        "Inspection completed, no debris present.",
        responseOne,
        bitmapPacketTypeOne,
        bitmapOne));

    std::string responseTwo;
    int bitmapPacketTypeTwo = 0;
    std::vector<char> bitmapTwo;
    ASSERT_TRUE(SubmitLiveFodReport(clientSocket,
        FOD_HAZARD_TYPE_LIQUID,
        "C4",
        4,
        "Tower Demo",
        "Fuel spill reported near taxiway.",
        responseTwo,
        bitmapPacketTypeTwo,
        bitmapTwo));

    EXPECT_NE(responseOne.find("Runway Status: CLEARED"), std::string::npos);
    EXPECT_NE(responseTwo.find("Runway Status: HAZARD"), std::string::npos);
    EXPECT_GT(bitmapOne.size(), 1024U * 1024U);
    EXPECT_GT(bitmapTwo.size(), 1024U * 1024U);
    EXPECT_TRUE(IsBitmapHeaderValid(bitmapOne));
    EXPECT_TRUE(IsBitmapHeaderValid(bitmapTwo));

    ShutdownAndCloseSocket(clientSocket);
}

TEST(FODRuntimeRequiredTests, RunningServerGeneratesDifferentBitmapsForDifferentZones)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = INVALID_SOCKET;
    ASSERT_TRUE(ConnectToLiveServer(clientSocket));

    char authResponse = 0x00;
    ASSERT_TRUE(AuthenticateLiveServer(clientSocket, "admin", "pass@123", authResponse));
    ASSERT_EQ(authResponse, 0x01);

    std::string responseA;
    int bitmapPacketTypeA = 0;
    std::vector<char> bitmapA;
    ASSERT_TRUE(SubmitLiveFodReport(clientSocket,
        FOD_HAZARD_TYPE_DEBRIS,
        "A1",
        4,
        "Tower Demo",
        "Debris at zone A1.",
        responseA,
        bitmapPacketTypeA,
        bitmapA));

    std::string responseB;
    int bitmapPacketTypeB = 0;
    std::vector<char> bitmapB;
    ASSERT_TRUE(SubmitLiveFodReport(clientSocket,
        FOD_HAZARD_TYPE_DEBRIS,
        "B2",
        4,
        "Tower Demo",
        "Debris at zone B2.",
        responseB,
        bitmapPacketTypeB,
        bitmapB));

    EXPECT_EQ(bitmapPacketTypeA, 0x05);
    EXPECT_EQ(bitmapPacketTypeB, 0x05);
    EXPECT_TRUE(IsBitmapHeaderValid(bitmapA));
    EXPECT_TRUE(IsBitmapHeaderValid(bitmapB));
    EXPECT_NE(bitmapA, bitmapB);

    ShutdownAndCloseSocket(clientSocket);
}
