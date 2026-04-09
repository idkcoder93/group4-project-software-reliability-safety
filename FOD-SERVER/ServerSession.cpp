// ServerSession.cpp — Sprint 2 Update (MISRA 2008 Compliant)

#include "ServerSession.h"
#include "ServerStateMachine.h"
#include "BitmapGenerator.h"
#include "PacketDeserializer.h"
#include "Logger.h"
#include "User.h"
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <cstring>

namespace FODServer
{
    //packet type constants
    static constexpr int PACKET_TYPE_FOD_REPORT = 0x03;
    static constexpr int PACKET_TYPE_RESPONSE = 0x04;
    static constexpr int PACKET_TYPE_BITMAP = 0x05;
    static constexpr int PACKET_TYPE_HEARTBEAT = 0x06;

    //heartbeat / timeout constants (seconds)
    static constexpr int HEARTBEAT_TIMEOUT_SEC = 90;

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

    static bool recvLengthPrefixed(SOCKET s, std::vector<char>& out, int maxBytes = 65536)
    {
        bool success = false;
        int len = 0;
        if (recvInt(s, len) && (len > 0) && (len <= maxBytes))
        {
            out.resize(static_cast<size_t>(len));
            success = recvAll(s, out.data(), len);
        }
        return success;
    }

    static bool sendResponse(SOCKET s, const std::string& msg)
    {
        const int len = static_cast<int>(msg.size());
        bool success = sendAll(s, reinterpret_cast<const char*>(&len), 4);
        if (success)
        {
            success = sendAll(s, msg.c_str(), len);
        }
        return success;
    }

    static const char* hazardTypeStr(HazardType ht)
    {
        const char* name = "UNKNOWN";
        switch (ht)
        {
        case FOD_HAZARD_TYPE_DEBRIS: name = "DEBRIS";  break;
        case FOD_HAZARD_TYPE_LIQUID: name = "LIQUID";  break;
        case FOD_HAZARD_TYPE_ANIMAL: name = "ANIMAL";  break;
        case FOD_HAZARD_TYPE_OTHER:  name = "OTHER";   break;
        default:                     /* no action required */ break;
        }
        return name;
    }

    // send bitmap using dedicated bitmap packet
    static bool sendBitmap(SOCKET s, const std::vector<char>& bmpData)
    {
        const int pktType = PACKET_TYPE_BITMAP;
        bool success = sendAll(s, reinterpret_cast<const char*>(&pktType), 4);
        if (success)
        {
            const int bmpSize = static_cast<int>(bmpData.size());
            success = sendAll(s, reinterpret_cast<const char*>(&bmpSize), 4);
            if (success)
            {
                success = sendAll(s, bmpData.data(), bmpSize);
            }
        }
        return success;
    }

    //check if data is available on socket within timeout
    static int waitForData(SOCKET s, int timeoutSec)
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);

        struct timeval tv = {};
        tv.tv_sec = timeoutSec;
        tv.tv_usec = 0;

        return select(0, &readSet, nullptr, nullptr, &tv);
    }

    //send a heartbeat acknowledgement back to client
    static bool sendHeartbeatAck(SOCKET s)
    {
        const int pktType = PACKET_TYPE_HEARTBEAT;
        const int len = 4;
        bool success = sendAll(s, reinterpret_cast<const char*>(&len), 4);
        if (success)
        {
            success = sendAll(s, reinterpret_cast<const char*>(&pktType), 4);
        }
        return success;
    }

    //display a prominent tower operator alert
    static void displayTowerAlert(const FODHeader& header, const char* description)
    {
        std::cout << std::endl;
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cout << "!!          TOWER ALERT: NEW FOD REPORT       !!" << std::endl;
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cout << "  Officer  : " << header.officerName << std::endl;
        std::cout << "  Zone     : " << header.locationZone << std::endl;
        std::cout << "  Hazard   : " << hazardTypeStr(header.hazardType) << std::endl;
        std::cout << "  Severity : " << header.severityLevel << std::endl;
        std::cout << "  Desc     : " << description << std::endl;
        std::cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cout << std::endl;
    }

    //Determine runway state based on severity
    static ServerState assessRunwayState(int severity)
    {
        ServerState result = ServerState::HAZARD;
        if (severity <= 0)
        {
            result = ServerState::CLEARED;
        }
        return result;
    }
}

//entry point 

int runServerSession(SOCKET clientSock, FODServer::DBHelper& db)
{
    using namespace FODServer;

    ServerStateMachine sm;

    //Client authentication handshake

    std::vector<char> authBuf;
    if (!recvLengthPrefixed(clientSock, authBuf, 256))
    {
        Logger::log("Failed to receive auth packet.", Logger::ERR);
        return 1;
    }

    const std::string authStr(authBuf.data(), authBuf.size());
    const size_t colonPos = authStr.find(':');
    if (colonPos == std::string::npos)
    {
        Logger::log("Malformed auth packet.", Logger::ERR);
        const char reject = 0x00;
        (void)send(clientSock, &reject, 1, 0);
        return 1;
    }

    const std::string clientUsername = authStr.substr(0, colonPos);
    const std::string clientPassword = authStr.substr(colonPos + 1U);

    Logger::log("Auth attempt from user=" + clientUsername, Logger::INFO);
    (void)Logger::saveLog(db, "Auth attempt from user=" + clientUsername, Logger::INFO);

    User u;
    const bool authenticated = u.authenticateUser(clientUsername, clientPassword, db);

    if (!authenticated)
    {
        Logger::log("Auth REJECTED for user=" + clientUsername, Logger::ERR);
        (void)Logger::saveLog(db, "Auth REJECTED for user=" + clientUsername, Logger::ERR);
        const char reject = 0x00;
        (void)send(clientSock, &reject, 1, 0);
        return 1;
    }

    const char accept = 0x01;
    (void)send(clientSock, &accept, 1, 0);
    Logger::log("Auth ACCEPTED for user=" + clientUsername, Logger::INFO);
    (void)Logger::saveLog(db, "Auth ACCEPTED for user=" + clientUsername, Logger::INFO);

    (void)sm.transition(ServerState::CONNECTED);

    //FOD receive loop with heartbeat monitoring

    bool continueLoop = true;
    while (continueLoop)
    {
        const int selectResult = waitForData(clientSock, HEARTBEAT_TIMEOUT_SEC);

        if (selectResult < 0)
        {
            Logger::log("Socket error during select().", Logger::ERR);
            (void)Logger::saveLog(db, "Socket error during select()", Logger::ERR);
            (void)sm.transition(ServerState::DISCONNECTED);
            continueLoop = false;
            continue;
        }

        if (selectResult == 0)
        {
            Logger::log("Heartbeat timeout - client connection lost.", Logger::WARNING);
            (void)Logger::saveLog(db, "Heartbeat timeout - connection lost", Logger::WARNING);
            (void)sm.transition(ServerState::DISCONNECTED);
            continueLoop = false;
            continue;
        }

        std::vector<char> packetBuf;
        if (!recvLengthPrefixed(clientSock, packetBuf))
        {
            Logger::log("Client disconnected.", Logger::INFO);
            (void)Logger::saveLog(db, "Client disconnected", Logger::INFO);
            (void)sm.transition(ServerState::DISCONNECTED);
            continueLoop = false;
            continue;
        }

        if (packetBuf.size() < 4U)
        {
            Logger::log("Packet too short to contain type ID.", Logger::ERR);
            continueLoop = false;
            continue;
        }

        int packetType = 0;
        (void)memcpy(&packetType, packetBuf.data(), 4);

        //handle heartbeat packet
        if (packetType == PACKET_TYPE_HEARTBEAT)
        {
            Logger::log("Heartbeat received from client.", Logger::INFO);
            if (!sendHeartbeatAck(clientSock))
            {
                Logger::log("Failed to send heartbeat ACK.", Logger::ERR);
                (void)sm.transition(ServerState::DISCONNECTED);
                continueLoop = false;
            }
            continue;
        }

        //Process FOD report

        FODHeader header = {};
        if (!PacketDeserializer::deserializeHeader(
            packetBuf.data(), static_cast<int>(packetBuf.size()), header))
        {
            Logger::log("Malformed FOD header.", Logger::ERR);
            continueLoop = false;
            continue;
        }

        // verify header checksum
        if (!PacketDeserializer::verifyHeaderChecksum(
            packetBuf.data(), static_cast<int>(packetBuf.size()), header))
        {
            Logger::log("Header checksum FAILED - possible data corruption.", Logger::WARNING);
            (void)Logger::saveLog(db, "Header checksum FAILED", Logger::WARNING);
        }
        else
        {
            Logger::log("Header checksum verified.", Logger::INFO);
        }

        const std::string headerLog =
            "FOD_HEADER RECEIVED Zone=" + header.locationZone +
            " Severity=" + std::to_string(header.severityLevel) +
            " Officer=" + header.officerName;
        Logger::log(headerLog, Logger::INFO);
        (void)Logger::saveLog(db, headerLog, Logger::INFO);

        (void)sm.transition(ServerState::INSPECTION);

        //receive description
        std::vector<char> descBuf;
        if (!recvLengthPrefixed(clientSock, descBuf))
        {
            Logger::log("Failed to receive FOD description.", Logger::ERR);
            (void)sm.transition(ServerState::DISCONNECTED);
            continueLoop = false;
            continue;
        }

        FODDescription desc = {};
        if (!PacketDeserializer::deserializeDescription(
            descBuf.data(), static_cast<int>(descBuf.size()), desc))
        {
            Logger::log("Malformed FOD description.", Logger::ERR);
            (void)sm.transition(ServerState::DISCONNECTED);
            continueLoop = false;
            continue;
        }

        //verify description checksum
        if (!PacketDeserializer::verifyDescriptionChecksum(desc))
        {
            Logger::log("Description checksum FAILED - possible corruption.", Logger::WARNING);
            (void)Logger::saveLog(db, "Description checksum FAILED", Logger::WARNING);
        }
        else
        {
            const std::string descLog =
                std::string("FOD_DESCRIPTION RECEIVED Desc=") + desc.description;
            Logger::log(descLog, Logger::INFO);
            (void)Logger::saveLog(db, descLog, Logger::INFO);
        }

        //display prominent tower operator alert
        const char* descPtr = (desc.description != nullptr) ? desc.description : "N/A";
        displayTowerAlert(header, descPtr);

        // Save to database
        if (db.saveFOD(header, desc))
        {
            Logger::log("FOD saved to DB zone=" + header.locationZone, Logger::INFO);
            (void)Logger::saveLog(db, "FOD saved zone=" + header.locationZone, Logger::INFO);
        }
        else
        {
            Logger::log("DB save FAILED zone=" + header.locationZone, Logger::ERR);
            (void)Logger::saveLog(db, "DB save FAILED zone=" + header.locationZone, Logger::ERR);
        }

        // US-09: Determine next state based on severity
        const ServerState nextState = assessRunwayState(header.severityLevel);
        (void)sm.transition(nextState);

        //build confirmation response including runway state
        const std::string confirmation =
            "FOD received. Zone: " + header.locationZone + " | Runway Status: " + sm.currentStateName() +" | Officer: " + header.officerName;

        if (!sendResponse(clientSock, confirmation))
        {
            Logger::log("Failed to send confirmation.", Logger::ERR);
            (void)sm.transition(ServerState::DISCONNECTED);
            continueLoop = false;
        }
        else
        {
            Logger::log("RESPONSE SENT: " + confirmation, Logger::INFO);
            (void)Logger::saveLog(db, "RESPONSE SENT: " + confirmation, Logger::INFO);
        }

        //generate and send runway zone bitmap
        if (continueLoop)
        {
            Logger::log("Generating runway bitmap for zone=" + header.locationZone, Logger::INFO);
            const std::vector<char> bmpData =
                BitmapGenerator::generateRunwayBitmap(header.locationZone);
            Logger::log("Bitmap size: " + std::to_string(bmpData.size()) + " bytes",
                Logger::INFO);

            if (!sendBitmap(clientSock, bmpData))
            {
                Logger::log("Failed to send bitmap.", Logger::ERR);
                (void)Logger::saveLog(db, "Bitmap send FAILED", Logger::ERR);
                (void)sm.transition(ServerState::DISCONNECTED);
                continueLoop = false;
            }
            else
            {
                Logger::log("Bitmap sent successfully for zone=" +
                    header.locationZone, Logger::INFO);
                (void)Logger::saveLog(db, "Bitmap sent zone=" +
                    header.locationZone, Logger::INFO);
            }
        }

        PacketDeserializer::freeDescription(desc);
    }

    return 0;
}