#include "pch.h"
#include "gtest/gtest.h"

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>

#include <array>
#include <string>

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
}

TEST(FODRuntimeRequiredTests, ServerProcessMustBeRunning)
{
    EXPECT_TRUE(IsProcessRunning(L"FOD-SERVER.exe"));
}

TEST(FODRuntimeRequiredTests, ClientProcessMustBeRunning)
{
    EXPECT_TRUE(IsProcessRunning(L"FOD-CLIENT.exe"));
}

TEST(FODRuntimeRequiredTests, RunningServerAcceptsConnectionOnDefaultPort)
{
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
