// FOD SERVER - MISRA 2008 Compliant Version

#define WIN32_LEAN_AND_MEAN
#include "User.h"
#include "DBHelper.h"
#include "Logger.h"
#include "utils.h"
#include "ServerSession.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <iostream>
#include <string>
#include <functional>
#include <cstdlib>

#pragma comment(lib, "Ws2_32.lib")
/*
namespace FODServer
{
    //Constants for server configuration
    constexpr auto DEFAULT_PORT = "27015";
    constexpr std::size_t DEFAULT_BUFLEN = 512;
    constexpr auto DEFAULT_CONN_STR =
        "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;";
}

*/

// ------------------------------------------------------------------
// DATABASE CONNECTION SETUP
// By default this connects to localhost. If your SQL Server uses a 
// named instance, set an environment variable on your machine:
//
// use this command in an admin command prompt, replacing YOUR-PC\INSTANCE with your server name:
// setx FOD_DB_CONN "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;"
//
// Then restart Visual Studio so it picks up the new variable.
// ------------------------------------------------------------------
namespace FODServer
{
    //constants for server configuration
    constexpr auto DEFAULT_PORT = "27015";
    constexpr std::size_t DEFAULT_BUFLEN = 512;

    static std::string getConnectionString()
    {
        char* envVal = nullptr;
        size_t len = 0;
        std::string result;

        if ((_dupenv_s(&envVal, &len, "FOD_DB_CONN") == 0) && (envVal != nullptr))
        {
            result = std::string(envVal);
            //MISRA deviation free() required by _dupenv_s contract  no alternative
            free(envVal);   //NOLINT(cppcoreguidelines-no-malloc)
        }
        else
        {
            result = "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;";
        }

        return result;
    }
    constexpr int HANDSHAKE_TIMEOUT_SEC = 120;
}

int main()
{
    using namespace FODServer;

    // single point of exit for main
    int returnCode = 0;

    DBHelper db;
    User user;

    if (!db.openConnection(/*DEFAULT_CONN_STR*/  getConnectionString()))
    {
        Logger::log("Failed to connect to DB.", Logger::ERR);
        returnCode = 1;
    }

    if (returnCode == 0)
    {
        std::string inputUsername{};
        std::string inputPassword{};

        if (isAutomatedTestingEnabled())
        {
            inputUsername = getAutomationCredential("FOD_TEST_USERNAME", "admin");
            inputPassword = getAutomationCredential("FOD_TEST_PASSWORD", "pass@123");
            Logger::log("Automated test login enabled.", Logger::INFO);
        }
        else
        {
            std::cout << "Enter username: ";
            std::cin >> inputUsername;
            std::cout << "Enter password: ";
            inputPassword = getPassword();
        }

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

    if (returnCode == 0)
    {
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0)
        {
            Logger::log("WSAStartup failed: " + std::to_string(iResult), Logger::ERR);
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
            Logger::log("getaddrinfo failed: " + std::to_string(iResult), Logger::ERR);
            (void)WSACleanup();
            returnCode = 1;
        }
    }

    if ((returnCode == 0) && (result != nullptr))
    {
        ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (ListenSocket == INVALID_SOCKET)
        {
            Logger::log("socket failed: " + std::to_string(WSAGetLastError()), Logger::ERR);
            freeaddrinfo(result);
            result = nullptr;
            (void)WSACleanup();
            returnCode = 1;
        }
    }

    if ((returnCode == 0) && (result != nullptr))
    {
        iResult = bind(ListenSocket, result->ai_addr, static_cast<int>(result->ai_addrlen));
        if (iResult == SOCKET_ERROR)
        {
            Logger::log("bind failed: " + std::to_string(WSAGetLastError()), Logger::ERR);
            freeaddrinfo(result);
            result = nullptr;
            (void)closesocket(ListenSocket);
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
            Logger::log("listen failed: " + std::to_string(WSAGetLastError()), Logger::ERR);
            (void)closesocket(ListenSocket);
            (void)WSACleanup();
            returnCode = 1;
        }
    }

    if (returnCode == 0)
    {
        Logger::log("Server listening on port " + std::string(DEFAULT_PORT), Logger::INFO);

        const bool automatedTesting = isAutomatedTestingEnabled();
        bool keepListening = true;
        while ((returnCode == 0) && keepListening)
        {
            //apply 30-second timeout on accept via select()
            fd_set acceptSet;
            FD_ZERO(&acceptSet);
            FD_SET(ListenSocket, &acceptSet);

            struct timeval timeout = {};
            timeout.tv_sec = HANDSHAKE_TIMEOUT_SEC;
            timeout.tv_usec = 0;

            const int selectResult = select(0, &acceptSet, nullptr, nullptr, &timeout);
            if (selectResult <= 0)
            {
                Logger::log("Connection timeout — no client connected within "
                    + std::to_string(HANDSHAKE_TIMEOUT_SEC) + " seconds.", Logger::ERR);
                if (!automatedTesting)
                {
                    (void)closesocket(ListenSocket);
                    (void)WSACleanup();
                    returnCode = 1;
                    keepListening = false;
                }
                else
                {
                    (void)Sleep(250);
                }
            }
            else
            {
                ClientSocket = accept(ListenSocket, nullptr, nullptr);
                if (ClientSocket == INVALID_SOCKET)
                {
                    Logger::log("accept failed: " + std::to_string(WSAGetLastError()), Logger::ERR);
                    if (!automatedTesting)
                    {
                        (void)closesocket(ListenSocket);
                        (void)WSACleanup();
                        returnCode = 1;
                        keepListening = false;
                    }
                }
                else
                {
                    //run the authenticated FOD session
                    (void)runServerSession(ClientSocket, db);
                    (void)closesocket(ClientSocket);
                    ClientSocket = INVALID_SOCKET;

                    if (!automatedTesting)
                    {
                        keepListening = false;
                    }
                }
            }
        }
    }

    (void)closesocket(ClientSocket);
    (void)closesocket(ListenSocket);
    (void)WSACleanup();
    db.closeConnection();

    return returnCode;
}