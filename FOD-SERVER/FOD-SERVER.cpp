// FOD SERVER

#define WIN32_LEAN_AND_MEAN
#include "User.h"
#include "DBHelper.h"

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_PORT "27015"
#define DEFAULT_BUFLEN 512
#define DEFAULT_CONN_STR "DRIVER={ODBC Driver 18 for SQL Server};SERVER=localhost;DATABASE=FODDatabase;Trusted_Connection=Yes;TrustServerCertificate=Yes;"

int main()
{
    DBHelper db;
    User user;

    if (!db.openConnection(DEFAULT_CONN_STR)) {
        std::cout << "Failed to connect to DB." << std::endl;
        return 1;
    }

    std::string inputUsername, inputPassword;
    std::cout << "Enter username: ";
    std::cin >> inputUsername;
    std::cout << "Enter password: ";
    std::cin >> inputPassword;

    bool loggedIn = user.authenticateUser(inputUsername, inputPassword, db);

    if (!loggedIn) {
        std::cout << "Authentication failed. Exiting." << std::endl;
        return 1;
    }

    std::cout << "Login successful!" << std::endl;
    db.closeConnection();

    WSADATA wsaData;
    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;
    struct addrinfo* result = NULL;
    struct addrinfo hints;
    int iResult;

    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;            // IPv4
    hints.ai_socktype = SOCK_STREAM;      // TCP
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;          // For bind

    // Resolve the local address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    // Create a listening socket
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    // Bind the socket
    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    // Start listening
    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Server listening on port %s...\n", DEFAULT_PORT);

    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) {
        printf("accept failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    printf("Client connected!\n");

    // Receive data
    do {
        iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
        if (iResult > 0) {
            printf("Bytes received: %d\n", iResult);
            printf("Message: %.*s\n", iResult, recvbuf);

            // Echo the message back to client
            int iSendResult = send(ClientSocket, recvbuf, iResult, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send failed: %d\n", WSAGetLastError());
                break;
            }
        }
        else if (iResult == 0) {
            printf("Connection closing...\n");
            break;
        }
        else {
            printf("recv failed: %d\n", WSAGetLastError());
            break;
        }
    } while (iResult > 0);

    // cleanup
    closesocket(ClientSocket);
    closesocket(ListenSocket);
    WSACleanup();

    return 0;
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