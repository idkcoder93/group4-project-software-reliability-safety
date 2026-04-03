// FOD CLIENT - MISRA 2008 Compliant Version
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include "Logger.h"
#include "ClientSession.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

namespace FODClientConfig {
    constexpr const char* SERVER_NAME = "127.0.0.1";
    constexpr const char* DEFAULT_PORT = "27015";
}

int main()
{
    int exitCode = 0;

    WSADATA wsaData{};
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = nullptr;
    struct addrinfo* ptr = nullptr;
    int iResult = 0;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        exitCode = iResult;
    }

    if (exitCode == 0)
    {
        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        iResult = getaddrinfo(FODClientConfig::SERVER_NAME,
            FODClientConfig::DEFAULT_PORT, &hints, &result);
        if (iResult != 0)
        {
            std::cerr << "getaddrinfo failed: " << iResult << std::endl;
            exitCode = 1;
        }
    }

    if (exitCode == 0)
    {
        for (ptr = result; ptr != nullptr; ptr = ptr->ai_next)
        {
            ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (ConnectSocket == INVALID_SOCKET) { continue; }

            iResult = connect(ConnectSocket, ptr->ai_addr,
                static_cast<int>(ptr->ai_addrlen));
            if (iResult != SOCKET_ERROR) { break; }

            (void)closesocket(ConnectSocket);
            ConnectSocket = INVALID_SOCKET;
        }

        freeaddrinfo(result);

        if (ConnectSocket == INVALID_SOCKET)
        {
            std::cerr << "Unable to connect to server!" << std::endl;
            exitCode = 1;
        }
    }

    // ---- Run the authenticated FOD session (US-02, US-03, US-04, US-06) ----
    if (exitCode == 0)
    {
        std::cout << "Connected to server!" << std::endl;
        Logger clientLogger("client_log.txt", "SESSION_1");
        (void)runClientSession(ConnectSocket, clientLogger);
    }

    if (ConnectSocket != INVALID_SOCKET)
    {
        (void)closesocket(ConnectSocket);
    }
    (void)WSACleanup();

    return exitCode;
}