// FOD SERVER - MISRA 2008 Compliant Version

#define WIN32_LEAN_AND_MEAN
#include "User.h"
#include "DBHelper.h"
#include "Logger.h"
#include "utils.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

namespace FODServer
{
	// Constants for server configuration
    constexpr auto DEFAULT_PORT = "27015";
    constexpr std::size_t DEFAULT_BUFLEN = 512;
    constexpr auto DEFAULT_CONN_STR =
        "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;";
}


int main()
{
    using namespace FODServer;

    // single point of exit for main
    int returnCode = 0;

    DBHelper db;
    User user;

    if (!db.openConnection(DEFAULT_CONN_STR))
    {
        Logger::log("Failed to connect to DB.", Logger::ERR);
        returnCode = 1;
    }

    if (returnCode == 0)
    {
        std::string inputUsername{};
        std::string inputPassword{};
        std::cout << "Enter username: ";
        std::cin >> inputUsername;
        std::cout << "Enter password: ";
        inputPassword = getPassword();

        const bool loggedIn = user.authenticateUser(inputUsername, inputPassword, db);
        if (!loggedIn)
        {
            Logger::log("Authentication failed.", Logger::ERR);
            returnCode = 1;
        }
        else
        {
            Logger::log("Login successful.", Logger::INFO);
        }
    }

    WSADATA wsaData{};
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    struct addrinfo* result = nullptr;
    struct addrinfo hints {};
    int iResult = 0;

    std::array<char, DEFAULT_BUFLEN> recvbuf{};
    const int recvbuflen = static_cast<int>(recvbuf.size());

    if (returnCode == 0)
    {
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0)
        {
            Logger::log("WSAStartup failed with error: " + std::to_string(iResult), Logger::ERR);
            returnCode = 1;
        }
    }

    if (returnCode == 0)
    {
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
        hints.ai_flags = AI_PASSIVE;

        iResult = getaddrinfo(nullptr, DEFAULT_PORT, &hints, &result);
        if (iResult != 0)
        {
            Logger::log("getaddrinfo failed with error: " + std::to_string(iResult), Logger::ERR);
            (void)WSACleanup();   // return handled in helper
            returnCode = 1;
        }
    }

    // check for return code and result abide by MISRA guidelines
    if ((returnCode == 0) && (result != nullptr))
    {
        ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (ListenSocket == INVALID_SOCKET)
        {
            Logger::log("socket failed with error: " + std::to_string(WSAGetLastError()), Logger::ERR);
            freeaddrinfo(result);
            result = nullptr;
            (void)WSACleanup();
            returnCode = 1;
        }
    }

    // same null guard before bind dereferences result
    if ((returnCode == 0) && (result != nullptr))
    {
        iResult = bind(ListenSocket, result->ai_addr, static_cast<int>(result->ai_addrlen));
        if (iResult == SOCKET_ERROR)
        {
            Logger::log("bind failed with error: " + std::to_string(WSAGetLastError()), Logger::ERR);
            freeaddrinfo(result);
            result = nullptr;
            (void)closesocket(ListenSocket);  // handled in helper
            (void)WSACleanup();
            returnCode = 1;
        }
    }

    if (result != nullptr)
    {
        freeaddrinfo(result);
        result = nullptr;
    }

    if (returnCode == 0)
    {
        iResult = listen(ListenSocket, SOMAXCONN);
        if (iResult == SOCKET_ERROR)
        {
            Logger::log("listen failed with error: " + std::to_string(WSAGetLastError()), Logger::ERR);
            (void)closesocket(ListenSocket);
            (void)WSACleanup();
            returnCode = 1;
        }
    }

    if (returnCode == 0)
    {
        Logger::log("Server listening on port " + std::string(DEFAULT_PORT), Logger::INFO);

        ClientSocket = accept(ListenSocket, nullptr, nullptr);
        if (ClientSocket == INVALID_SOCKET)
        {
            Logger::log("accept failed with error: " + std::to_string(WSAGetLastError()), Logger::ERR);
            (void)closesocket(ListenSocket);
            (void)WSACleanup();
            returnCode = 1;
        }
    }

    if (returnCode == 0)
    {
        bool continueLoop = true;
        while (continueLoop)
        {
            iResult = recv(ClientSocket, recvbuf.data(), recvbuflen, 0);
            if (iResult > 0)
            {
                const std::string msg(recvbuf.data(), static_cast<std::size_t>(iResult));
				std::cout << "Received message: " << msg << std::endl;
                Logger::log("Message received", Logger::INFO);

                // capture and check return value of saveLog
                const bool saved = Logger::saveLog(db, "FOD was received", Logger::INFO);
                if (!saved)
                {
                    Logger::log("saveLog failed.", Logger::ERR);
                }

                const int iSendResult = send(ClientSocket, recvbuf.data(), iResult, 0);
                if (iSendResult == SOCKET_ERROR)
                {
                    Logger::log("send failed: " + std::to_string(WSAGetLastError()), Logger::ERR);
                    continueLoop = false;
                }
            }
            else if (iResult == 0)
            {
                Logger::log("Connection closing", Logger::INFO);
                continueLoop = false;
            }
            else
            {
                Logger::log("recv failed: " + std::to_string(WSAGetLastError()), Logger::ERR);
                continueLoop = false;
            }
        }
    }

    // Cleanup — all return values handled through helpers
    (void)closesocket(ClientSocket);
    (void)closesocket(ListenSocket);
    (void)WSACleanup();

    db.closeConnection();

    return returnCode;
}

//this test code to make sure that the database connection and FOD record saving works correctly. You can run this code in a separate test project to verify that the DBHelper and FOD classes are functioning as expected before integrating them into the server application.

//#include "FOD.h"
//#include "DBHelper.h"
//#include <chrono>
//#include <iostream>
//
//int main() {
//    DBHelper db;
//    std::string connStr =
//        "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;";
//
//    if (!db.openConnection(connStr)) {
//        std::cout << "Failed to connect!" << std::endl;
//        return 1;
//    }
//
//    FOD record(1, HazardType::FOD_HAZARD_TYPE_UNKNOWN, "Runway 1", 2, "Officer Smith",
//        std::chrono::system_clock::now(), 100, 12345);
//
//    if (db.saveFOD(record)) {
//        std::cout << "FOD record saved successfully!" << std::endl;
//    }
//    else {
//        std::cout << "Failed to save FOD record!" << std::endl;
//    }
//
//    db.closeConnection();
//    return 0;
//}