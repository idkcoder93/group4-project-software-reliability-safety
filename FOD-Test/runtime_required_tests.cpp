#include "pch.h"
#include "gtest/gtest.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>

#include <array>
#include <string>
#include <vector>

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

        SOCKET probe = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (probe == INVALID_SOCKET)
        {
            return false;
        }

        DWORD timeoutMs = 2000;
        (void)setsockopt(probe, SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(27015);
        serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        bool ready = false;
        for (int attempt = 0; attempt < 120; ++attempt)
        {
            const int connectResult = connect(probe, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
            if (connectResult == SOCKET_ERROR)
            {
                (void)Sleep(250);
                continue;
            }

            const std::string authPayload = std::string("admin:pass@123");
            const int authLen = static_cast<int>(authPayload.size());
            if (!SendAll(probe, reinterpret_cast<const char*>(&authLen), 4) ||
                !SendAll(probe, authPayload.data(), authLen))
            {
                break;
            }

            char authResponse = 0x00;
            const int received = recv(probe, &authResponse, 1, 0);
            if ((received == 1) && (authResponse == 0x01))
            {
                ready = true;
            }
            break;
        }

        (void)shutdown(probe, SD_BOTH);
        (void)closesocket(probe);
        return ready;
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

            if (!serverStarted_)
            {
                if (!IsProcessRunning(L"FOD-SERVER.exe"))
                {
                    ASSERT_TRUE(LaunchProcess(L"FOD-SERVER.exe", serverProcess_));
                    serverStarted_ = true;
                }
                ASSERT_TRUE(WaitForServerReady());
            }
        }

        void EnsureClientRunning()
        {
            ConfigureAutomationEnvironment();
            EnsureServerRunning();

            if (!clientStarted_)
            {
                if (!IsProcessRunning(L"FOD-CLIENT.exe"))
                {
                    ASSERT_TRUE(LaunchProcess(L"FOD-CLIENT.exe", clientProcess_));
                    clientStarted_ = true;
                }
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

        void Stop()
        {
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

TEST(FODRuntimeRequiredTests, RunningServerAcceptsConnectionOnDefaultPort)
{
    Harness().EnsureServerRunning();

    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(clientSocket, INVALID_SOCKET);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(27015);
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    const int connectResult = connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr));
    EXPECT_NE(connectResult, SOCKET_ERROR);

    (void)shutdown(clientSocket, SD_BOTH);
    (void)closesocket(clientSocket);
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
