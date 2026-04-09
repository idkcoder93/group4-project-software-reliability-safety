#include "pch.h"
#include "gtest/gtest.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <string>
#include <array>

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
}

TEST(FODClientDynamicTests, ClientLikeSocketFlowCanConnectSendAndReceive)
{
    WsaSession wsa;
    ASSERT_TRUE(wsa.ok());

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(listenSocket, INVALID_SOCKET);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = 0;

    ASSERT_NE(bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)), SOCKET_ERROR);
    ASSERT_NE(listen(listenSocket, 1), SOCKET_ERROR);

    sockaddr_in boundAddr{};
    int boundLen = sizeof(boundAddr);
    ASSERT_NE(getsockname(listenSocket, reinterpret_cast<sockaddr*>(&boundAddr), &boundLen), SOCKET_ERROR);

    const unsigned short assignedPort = ntohs(boundAddr.sin_port);
    ASSERT_NE(assignedPort, 0);

    std::string receivedByServer;
    std::thread serverThread([&]() {
        SOCKET accepted = accept(listenSocket, nullptr, nullptr);
        if (accepted == INVALID_SOCKET)
        {
            return;
        }

        std::array<char, 512> buffer{};
        const int recvResult = recv(accepted, buffer.data(), static_cast<int>(buffer.size()), 0);
        if (recvResult > 0)
        {
            receivedByServer.assign(buffer.data(), static_cast<std::size_t>(recvResult));
            (void)send(accepted, buffer.data(), recvResult, 0);
        }

        (void)shutdown(accepted, SD_BOTH);
        (void)closesocket(accepted);
    });

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(clientSocket, INVALID_SOCKET);

    sockaddr_in connectAddr{};
    connectAddr.sin_family = AF_INET;
    connectAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    connectAddr.sin_port = htons(assignedPort);

    ASSERT_NE(connect(clientSocket, reinterpret_cast<sockaddr*>(&connectAddr), sizeof(connectAddr)), SOCKET_ERROR);

    const std::string payload = "dynamic-client-message";
    const int sent = send(clientSocket, payload.data(), static_cast<int>(payload.size()), 0);
    ASSERT_EQ(sent, static_cast<int>(payload.size()));

    std::array<char, 512> recvBuffer{};
    const int recvResult = recv(clientSocket, recvBuffer.data(), static_cast<int>(recvBuffer.size()), 0);
    ASSERT_GT(recvResult, 0);

    const std::string echoed(recvBuffer.data(), static_cast<std::size_t>(recvResult));
    EXPECT_EQ(echoed, payload);

    (void)shutdown(clientSocket, SD_BOTH);
    (void)closesocket(clientSocket);
    (void)closesocket(listenSocket);

    serverThread.join();
    EXPECT_EQ(receivedByServer, payload);
}
