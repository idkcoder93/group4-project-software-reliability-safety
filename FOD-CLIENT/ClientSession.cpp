//ClientSession

#include "ClientSession.h"
#include "ClientStateMachine.h"
#include "PacketSerializer.h"
#include "User.h"
#include <iostream>
#include <string>
#include <cstring>
#include <vector>
#include <array>
#include <thread>
#include <atomic>
#include <fstream>

namespace FODClient
{
    //heartbeat interval in seconds
    static constexpr int HEARTBEAT_INTERVAL_SEC = 60;

    //helpers

    static bool sendAll(SOCKET s, const char* data, int len)
    {
        bool success = true;
        int total = 0;
        while ((total < len) && success)
        {
            const int sent = send(s, &data[total], len - total, 0);
            if (sent == SOCKET_ERROR) { success = false; }
            else { total += sent; }
        }
        return success;
    }

    static bool recvAll(SOCKET s, char* buf, int len)
    {
        bool success = true;
        int total = 0;
        while ((total < len) && success)
        {
            const int got = recv(s, &buf[total], len - total, 0);
            if (got <= 0) { success = false; }
            else { total += got; }
        }
        return success;
    }

    static bool recvInt(SOCKET s, int& out)
    {
        std::array<char, 4> buf = {};
        bool success = recvAll(s, buf.data(), 4);
        if (success)
        {
            (void)memcpy(&out, buf.data(), 4);
        }
        return success;
    }

    static bool sendLengthPrefixed(SOCKET s, const std::vector<char>& data)
    {
        int len = static_cast<int>(data.size());
        bool success = sendAll(s, reinterpret_cast<const char*>(&len), 4);
        if (success)
        {
            success = sendAll(s, data.data(), len);
        }
        return success;
    }

    //prompt for hazard type with clear labels
    static HazardType promptHazardType()
    {
        std::cout << "\n  Select hazard type:" << std::endl;
        std::cout << "    0 = Unknown" << std::endl;
        std::cout << "    1 = Debris (tools, parts, fragments)" << std::endl;
        std::cout << "    2 = Liquid (fuel spill, hydraulic leak)" << std::endl;
        std::cout << "    3 = Animal (wildlife on runway)" << std::endl;
        std::cout << "    4 = Other" << std::endl;
        std::cout << "  Choice: ";
        int choice = 0;
        std::cin >> choice;
        if ((choice < 0) || (choice > 4)) { choice = 0; }
        (void)std::cin.ignore();
        return static_cast<HazardType>(choice);
    }

    //heartbeat sender runs in a background thread
    //pause when the main thread is actively exchanging packets with the server
    static void heartbeatSender(SOCKET sock, std::atomic<bool>& running, std::atomic<bool>& paused)
    {
        const int pktType = PACKET_TYPE_HEARTBEAT;

        while (running.load())
        {
            for (int i = 0; i < HEARTBEAT_INTERVAL_SEC; ++i)
            {
                if (!running.load()) { return; }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            if (!running.load()) { return; }
            if (paused.load()) { continue; }

            const int len = 4;
            if (!sendAll(sock, reinterpret_cast<const char*>(&len), 4) ||
                !sendAll(sock, reinterpret_cast<const char*>(&pktType), 4))
            {
                std::cout << "[HEARTBEAT] Failed to send - connection may be lost." << std::endl;
                running.store(false);
                return;
            }

            std::cout << "[HEARTBEAT] Sent." << std::endl;
        }
    }

    //receive bitmap from server and save to file
    static bool receiveBitmap(SOCKET sock, const std::string& zone, Logger& logger)
    {
        bool success = false;

        int pktType = 0;
        if (!recvInt(sock, pktType))
        {
            std::cout << "[ERROR] Failed to receive bitmap packet type." << std::endl;
        }
        else if (pktType != PACKET_TYPE_BITMAP)
        {
            std::cout << "[ERROR] Expected bitmap packet (0x05), got 0x"
                << std::hex << pktType << std::dec << "." << std::endl;
        }
        else
        {
            int bmpSize = 0;
            if (!recvInt(sock, bmpSize) || (bmpSize <= 0) || (bmpSize > (5 * 1024 * 1024)))
            {
                std::cout << "[ERROR] Invalid bitmap size: " << bmpSize << std::endl;
            }
            else
            {
                std::vector<char> bmpData(static_cast<size_t>(bmpSize));
                if (!recvAll(sock, bmpData.data(), bmpSize))
                {
                    std::cout << "[ERROR] Failed to receive bitmap data." << std::endl;
                }
                else
                {
                    const std::string filename = "runway_zone_" + zone + ".bmp";
                    std::ofstream outFile(filename, std::ios::binary);
                    if (!outFile.is_open())
                    {
                        std::cout << "[ERROR] Could not create bitmap file: "
                            << filename << std::endl;
                    }
                    else
                    {
                        (void)outFile.write(bmpData.data(), bmpSize);
                        outFile.close();

                        std::cout << "[BITMAP] Runway zone map saved: " << filename
                            << " (" << bmpSize << " bytes)" << std::endl;
                        logger.log(PacketType::RESPONSE, TransferDirection::RECEIVED,
                            "Bitmap received: " + filename + " (" +
                            std::to_string(bmpSize) + " bytes)");
                        success = true;
                    }
                }
            }
        }

        return success;
    }

    //parse and display runway status from server response
    static void displayRunwayStatus(const std::string& response)
    {
        std::cout << std::endl;
        std::cout << "  +-----------------------------------------+" << std::endl;
        std::cout << "  |         RUNWAY STATUS UPDATE             |" << std::endl;
        std::cout << "  +-----------------------------------------+" << std::endl;
        std::cout << "  | " << response << std::endl;
        std::cout << "  +-----------------------------------------+" << std::endl;

        const size_t statusPos = response.find("Runway Status: ");
        if (statusPos != std::string::npos)
        {
            const size_t startPos = statusPos + 15U;
            const size_t endPos = response.find(" |", startPos);
            const std::string status = (endPos != std::string::npos)
                ? response.substr(startPos, endPos - startPos)
                : response.substr(startPos);
            std::cout << "  >> Current Runway Status: " << status << " <<" << std::endl;
        }
        std::cout << std::endl;
    }
}

//entry point

int runClientSession(SOCKET sock, Logger& logger)
{
    using namespace FODClient;

    ClientStateMachine sm;

    //Auth handshake

    (void)sm.transition(ClientState::CONNECTING);

    std::string username;
    std::string password;
    std::cout << "Username: ";
    std::cin >> username;
    std::cout << "Password: ";
    std::cin >> password;

    (void)sm.transition(ClientState::AUTHENTICATING);

    FOD::User u;
    const std::string authPayload = u.authenticateUser(username, password);
    const int authLen = static_cast<int>(authPayload.size());

    if (!sendAll(sock, reinterpret_cast<const char*>(&authLen), 4) ||
        !sendAll(sock, authPayload.c_str(), authLen))
    {
        std::cout << "[ERROR] Failed to send auth packet." << std::endl;
        (void)sm.transition(ClientState::DISCONNECTED);
        return 1;
    }
    logger.log(PacketType::AUTH, TransferDirection::SENT, "user=" + username);

    char authResponse = 0;
    if ((recv(sock, &authResponse, 1, 0) <= 0) || (authResponse != 0x01))
    {
        std::cout << "[AUTH] Rejected by server." << std::endl;
        logger.log(PacketType::AUTH, TransferDirection::RECEIVED, "REJECTED");
        (void)sm.transition(ClientState::DISCONNECTED);
        return 1;
    }

    std::cout << "[AUTH] Accepted." << std::endl;
    logger.log(PacketType::AUTH, TransferDirection::RECEIVED, "ACCEPTED");
    (void)sm.transition(ClientState::CONNECTED);

    //start heartbeat sender thread
    std::atomic<bool> heartbeatRunning(true);
    std::atomic<bool> heartbeatPaused(false);
    std::thread heartbeatThread(heartbeatSender, sock,
        std::ref(heartbeatRunning),
        std::ref(heartbeatPaused));

    //FOD reporting loop

    bool active = true;
    while (active && sm.canReport())
    {
        std::cout << "\n[" << sm.currentStateName()
            << "] 1=Submit FOD Report  2=Exit\nChoice: ";
        int choice = 0;
        std::cin >> choice;

        if (choice == 2)
        {
            logger.logInfo("Client disconnecting");
            (void)sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }

        if (choice != 1) { continue; }

        //pause heartbeat immediately 
        //prevents bug where ACK landing in the receive buffer while the user is typing report details
        heartbeatPaused.store(true);

        (void)sm.transition(ClientState::REPORTING);

        //collect hazard report details from ground crew
        std::string zone;
        std::string officer;
        std::string descText;
        int severity = 0;
        (void)std::cin.ignore();

        const HazardType hazard = promptHazardType();

        std::cout << "  Location zone (e.g. A3, B1): ";
        (void)std::getline(std::cin, zone);

        std::cout << "  Severity (1-5, or 0 for no debris): ";
        std::cin >> severity;
        if ((severity < 0) || (severity > 5)) { severity = 1; }
        (void)std::cin.ignore();

        std::cout << "  Officer name: ";
        (void)std::getline(std::cin, officer);

        std::cout << "  Description: ";
        (void)std::getline(std::cin, descText);

        // Build and serialize packets
        //new/delete for desc.description is an intentional MISRA deviation
        //required by REQ-PKT-020 (dynamic char* allocation)
        FODDescription desc = PacketSerializer::buildDescription(descText);
        const int descLen = static_cast<int>(descText.size());
        FODHeader header = PacketSerializer::buildHeader(
            hazard, zone, severity, officer, descLen);

        std::vector<char> hBytes = PacketSerializer::serializeHeader(header);
        std::vector<char> dBytes = PacketSerializer::serializeDescription(desc);

        //send header
        if (!sendLengthPrefixed(sock, hBytes))
        {
            std::cout << "[ERROR] Failed to send header." << std::endl;
            PacketSerializer::freeDescription(desc);
            (void)sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }
        logger.log(PacketType::FOD_HEADER, TransferDirection::SENT,
            "Zone=" + zone + " Sev=" + std::to_string(severity) +
            " Checksum=" + std::to_string(header.checkSum));

        //send description
        if (!sendLengthPrefixed(sock, dBytes))
        {
            std::cout << "[ERROR] Failed to send description." << std::endl;
            PacketSerializer::freeDescription(desc);
            (void)sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }
        logger.log(PacketType::FOD_DESCRIPTION, TransferDirection::SENT,
            "Desc=" + descText + " Checksum=" + std::to_string(desc.checksum));

        PacketSerializer::freeDescription(desc);
        (void)sm.transition(ClientState::WAITING_RESPONSE);

        //Receive server confirmation

        int respLen = 0;
        if (!recvInt(sock, respLen) || (respLen <= 0) || (respLen > 1024))
        {
            std::cout << "[ERROR] Bad response from server." << std::endl;
            (void)sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }

        std::string response(static_cast<size_t>(respLen), '\0');
        if (!recvAll(sock, &response[0], respLen))
        {
            std::cout << "[ERROR] Failed to receive response." << std::endl;
            (void)sm.transition(ClientState::DISCONNECTED);
            active = false;
            continue;
        }

        //display runway status from server response
        displayRunwayStatus(response);
        logger.log(PacketType::RESPONSE, TransferDirection::RECEIVED, response);

        //receive runway zone bitmap
        (void)sm.transition(ClientState::RECEIVING_BITMAP);

        if (!receiveBitmap(sock, zone, logger))
        {
            std::cout << "[WARNING] Bitmap transfer failed, but report was submitted."
                << std::endl;
        }

        //resume heartbeat after report exchange is complete
        heartbeatPaused.store(false);

        (void)sm.transition(ClientState::CONNECTED);
    }

    //stop heartbeat thread
    heartbeatRunning.store(false);
    if (heartbeatThread.joinable())
    {
        heartbeatThread.join();
    }

    return 0;
}