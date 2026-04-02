#define WIN32_LEAN_AND_MEAN

/*
	MISRA Compliant - V2575 - use of namespace to avoid name clashes with other libraries and to comply with the guidelines
	MISRA Compliant - V2575 - use of constexpr for compile-time constants to improve code safety and readability
	MISRA Compliant - V2575 - use of std::array for fixed-size buffers to improve safety and prevent buffer overflows
	MISRA Compliant - V2575 - use of std::string for user input to improve safety and prevent buffer overflows
	MISRA Compliant - V2575 - use of a single exit point in the main function to improve code readability and maintainability
	MISRA Compliant - V2575 - use of error handling and logging to improve code robustness and maintainability

*/

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <array>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

namespace FODClientConfig {
    constexpr std::size_t DEFAULT_BUFLEN = 512U;
    constexpr const char* SERVER_NAME = "127.0.0.1";
    constexpr const char* DEFAULT_PORT = "27015";
}

int main()
{
    int exitCode = 0; // single exit point

    WSADATA wsaData;
    SOCKET ConnectSocket = INVALID_SOCKET;
    struct addrinfo* result = nullptr;
    struct addrinfo* ptr = nullptr;
    std::array<char, FODClientConfig::DEFAULT_BUFLEN> sendbuf{};
    std::array<char, FODClientConfig::DEFAULT_BUFLEN> recvbuf{};
    int iResult = 0;
    bool exitLoop = false;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        std::cerr << "WSAStartup failed: " << iResult << std::endl;
        exitCode = iResult;
    }

    if (exitCode == 0) {
        struct addrinfo hints {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        iResult = getaddrinfo(FODClientConfig::SERVER_NAME, FODClientConfig::DEFAULT_PORT, &hints, &result);
        if (iResult != 0) {
            std::cerr << "getaddrinfo failed: " << iResult << std::endl;
            exitCode = WSAGetLastError();
        }
    }

    if (exitCode == 0) {
        for (ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
            ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
            if (ConnectSocket == INVALID_SOCKET) {
                std::cerr << "socket failed: " << WSAGetLastError() << std::endl;
                continue;
            }

            iResult = connect(ConnectSocket, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen));
            if (iResult != SOCKET_ERROR) {
                break;
            }

            int closeResult = closesocket(ConnectSocket);
            (void)closeResult;
            ConnectSocket = INVALID_SOCKET;
        }

        freeaddrinfo(result);

        if (ConnectSocket == INVALID_SOCKET) {
            std::cerr << "Unable to connect to server!" << std::endl;
            exitCode = 1;
        }
    }

    if (exitCode == 0) {
        std::cout << "Connected to server!" << std::endl;

        while (!exitLoop) {
            std::cout << "\nEnter message (type 'exit' to quit): ";
            std::string userInput;
            if (!std::getline(std::cin, userInput)) {
                exitLoop = true;
                continue;
            }

            if (userInput == "exit") {
                std::cout << "Exiting..." << std::endl;
                exitLoop = true;
                continue;
            }

            // Send message
            iResult = send(ConnectSocket, userInput.data(), static_cast<int>(userInput.size()), 0);
            if (iResult == SOCKET_ERROR) {
                std::cerr << "send failed: " << WSAGetLastError() << std::endl;
                exitLoop = true;
                continue;
            }

            std::cout << "Bytes Sent: " << iResult << std::endl;

            // Receive response
            iResult = recv(ConnectSocket, recvbuf.data(), static_cast<int>(recvbuf.size()), 0);
            if (iResult > 0) {
                std::cout << "Server: " << std::string(recvbuf.data(), iResult) << std::endl;
            }
            else if (iResult == 0) {
                std::cout << "Connection closed by server" << std::endl;
                exitLoop = true;
            }
            else {
                std::cerr << "recv failed: " << WSAGetLastError() << std::endl;
                exitLoop = true;
            }
        }
    }

    if (ConnectSocket != INVALID_SOCKET) {
        int closeResult = closesocket(ConnectSocket);
        (void)closeResult;
    }

    int cleanupResult = WSACleanup();
    (void)cleanupResult;

    return exitCode;
}