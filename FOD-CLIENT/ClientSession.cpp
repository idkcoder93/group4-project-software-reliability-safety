#include "ClientSession.h"
#include "ClientStateMachine.h"
#include "PacketSerializer.h"
#include "User.h"
#include <iostream>
#include <string>
#include <cstring>
#include <vector>

using namespace FOD;  

//internal helpers

static bool sendAll(SOCKET s, const char* data, int len)
{
    int total = 0;
    while (total < len)
    {
        const int sent = send(s, data + total, len - total, 0);
        if (sent == SOCKET_ERROR) { return false; }
        total += sent;
    }
    return true;
}

static bool recvAll(SOCKET s, char* buf, int len)
{
    int total = 0;
    while (total < len)
    {
        const int got = recv(s, buf + total, len - total, 0);
        if (got <= 0) { return false; }
        total += got;
    }
    return true;
}

static bool recvInt(SOCKET s, int& out)
{
    char buf[4] = {};
    if (!recvAll(s, buf, 4)) { return false; }
    memcpy(&out, buf, 4);
    return true;
}

static bool sendLengthPrefixed(SOCKET s, const std::vector<char>& data)
{
    int len = static_cast<int>(data.size());
    if (!sendAll(s, reinterpret_cast<const char*>(&len), 4)) { return false; }
    return sendAll(s, data.data(), len);
}

static HazardType promptHazardType()
{
    std::cout << "  Hazard type (0=Unknown 1=Debris 2=Liquid 3=Animal 4=Other): ";
    int choice = 0;
    std::cin >> choice;
    if (choice < 0 || choice > 4) { choice = 0; }
    std::cin.ignore();  
    return static_cast<HazardType>(choice);
}

// entry point
int runClientSession(SOCKET sock, Logger& logger)
{
    ClientStateMachine sm;

    //Auth handshake
    sm.transition(ClientState::CONNECTING);

    std::string username, password;
    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Password: ";
    std::cin >> password;

    sm.transition(ClientState::AUTHENTICATING);

    User u;
    const std::string authPayload = u.authenticateUser(username, password);
    const int authLen = static_cast<int>(authPayload.size());

    if (!sendAll(sock, reinterpret_cast<const char*>(&authLen), 4) ||
        !sendAll(sock, authPayload.c_str(), authLen))
    {
        printf("[ERROR] Failed to send auth packet.\n");
        sm.transition(ClientState::DISCONNECTED);
        return 1;
    }
    logger.log(PacketType::AUTH, TransferDirection::SENT, "user=" + username);

    char authResponse = 0;
    if (recv(sock, &authResponse, 1, 0) <= 0 || authResponse != 0x01)
    {
        printf("[AUTH] Rejected by server.\n");
        logger.log(PacketType::AUTH, TransferDirection::RECEIVED, "REJECTED");
        sm.transition(ClientState::DISCONNECTED);
        return 1;
    }

    printf("[AUTH] Accepted.\n");
    logger.log(PacketType::AUTH, TransferDirection::RECEIVED, "ACCEPTED");
    sm.transition(ClientState::CONNECTED);

    //FOD reporting loop
    bool active = true;
    while (active && sm.canReport())
    {
        printf("\n[%s] 1=Submit Report  2=Exit\nChoice: ", sm.currentStateName());
        int choice = 0;
        std::cin >> choice;

        if (choice == 2)
        {
            logger.logInfo("Client disconnecting");
            sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }

        if (choice != 1) { continue; }

        sm.transition(ClientState::REPORTING);

        std::string zone, officer, descText;
        int severity = 0;
        std::cin.ignore();

        const HazardType hazard = promptHazardType();

        std::cout << "  Location zone: ";
        std::getline(std::cin, zone);

        std::cout << "  Severity (1-5): ";
        std::cin >> severity;
        if (severity < 1 || severity > 5) { severity = 1; }
        std::cin.ignore();

        std::cout << "  Officer name: ";
        std::getline(std::cin, officer);

        std::cout << "  Description: ";
        std::getline(std::cin, descText);

        FODDescription desc = PacketSerializer::buildDescription(descText);
        FODHeader      header = PacketSerializer::buildHeader(
            hazard, zone, severity, officer,
            static_cast<int>(strlen(desc.description)));

        std::vector<char> hBytes = PacketSerializer::serializeHeader(header);
        std::vector<char> dBytes = PacketSerializer::serializeDescription(desc);

        if (!sendLengthPrefixed(sock, hBytes))
        {
            printf("[ERROR] Failed to send header.\n");
            PacketSerializer::freeDescription(desc);
            sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }
        logger.log(PacketType::FOD_HEADER, TransferDirection::SENT,
            "Zone=" + zone + " Sev=" + std::to_string(severity) +
            " Checksum=" + std::to_string(header.checkSum));

        if (!sendLengthPrefixed(sock, dBytes))
        {
            printf("[ERROR] Failed to send description.\n");
            PacketSerializer::freeDescription(desc);
            sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }
        logger.log(PacketType::FOD_DESCRIPTION, TransferDirection::SENT,
            "Desc=" + descText + " Checksum=" + std::to_string(desc.checksum));

        PacketSerializer::freeDescription(desc);
        sm.transition(ClientState::WAITING_RESPONSE);

        int respLen = 0;
        if (!recvInt(sock, respLen) || respLen <= 0 || respLen > 1024)
        {
            printf("[ERROR] Bad response from server.\n");
            sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }

        std::string response(static_cast<size_t>(respLen), '\0');
        if (!recvAll(sock, &response[0], respLen))
        {
            printf("[ERROR] Failed to receive response.\n");
            sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }

        printf("\n[SERVER] %s\n", response.c_str());
        logger.log(PacketType::RESPONSE, TransferDirection::RECEIVED, response);
        sm.transition(ClientState::CONNECTED);
    }

    return 0;
}