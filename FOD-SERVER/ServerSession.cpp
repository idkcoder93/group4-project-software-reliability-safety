
#include "ServerSession.h"
#include "PacketDeserializer.h"
#include "Logger.h"
#include "User.h"
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

using namespace FODServer;

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

static bool recvLengthPrefixed(SOCKET s, std::vector<char>& out, int maxBytes = 65536)
{
    int len = 0;
    if (!recvInt(s, len)) { return false; }
    if (len <= 0 || len > maxBytes) { return false; }
    out.resize(static_cast<size_t>(len));
    return recvAll(s, out.data(), len);
}

static bool sendResponse(SOCKET s, const std::string& msg)
{
    const int len = static_cast<int>(msg.size());
    if (!sendAll(s, reinterpret_cast<const char*>(&len), 4)) { return false; }
    return sendAll(s, msg.c_str(), len);
}

static const char* hazardTypeStr(FODServer::HazardType ht)
{
    switch (ht)
    {
    case FOD_HAZARD_TYPE_DEBRIS: return "DEBRIS";
    case FOD_HAZARD_TYPE_LIQUID: return "LIQUID";
    case FOD_HAZARD_TYPE_ANIMAL: return "ANIMAL";
    case FOD_HAZARD_TYPE_OTHER:  return "OTHER";
    default:                     return "UNKNOWN";
    }
}

//entry point

int runServerSession(SOCKET clientSock, FODServer::DBHelper& db)
{

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
    const std::string clientPassword = authStr.substr(colonPos + 1);

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


    //FOD receive loop                      

    bool continueLoop = true;
    while (continueLoop)
    {
        std::vector<char> headerBuf;
        if (!recvLengthPrefixed(clientSock, headerBuf))
        {
            Logger::log("Client disconnected.", Logger::INFO);
            (void)Logger::saveLog(db, "Client disconnected", Logger::INFO);
            continueLoop = false;
            continue;
        }

        FODHeader header = {};
        if (!PacketDeserializer::deserializeHeader(
            headerBuf.data(), static_cast<int>(headerBuf.size()), header))
        {
            Logger::log("Malformed FOD header.", Logger::ERR);
            continueLoop = false;
            continue;
        }

        const std::string headerLog =
            "FOD_HEADER RECEIVED Zone=" + header.locationZone +
            " Severity=" + std::to_string(header.severityLevel) +
            " Officer=" + header.officerName;
        Logger::log(headerLog, Logger::INFO);
        (void)Logger::saveLog(db, headerLog, Logger::INFO);

        std::vector<char> descBuf;
        if (!recvLengthPrefixed(clientSock, descBuf))
        {
            Logger::log("Failed to receive FOD description.", Logger::ERR);
            continueLoop = false;
            continue;
        }

        FODDescription desc = {};
        if (!PacketDeserializer::deserializeDescription(
            descBuf.data(), static_cast<int>(descBuf.size()), desc))
        {
            Logger::log("Malformed FOD description.", Logger::ERR);
            continueLoop = false;
            continue;
        }

        if (!PacketDeserializer::verifyDescriptionChecksum(desc))
        {
            Logger::log("Checksum FAILED - possible corruption.", Logger::WARNING);
            (void)Logger::saveLog(db, "Checksum FAILED", Logger::WARNING);
        }
        else
        {
            const std::string descLog =
                std::string("FOD_DESCRIPTION RECEIVED Desc=") + desc.description;
            Logger::log(descLog, Logger::INFO);
            (void)Logger::saveLog(db, descLog, Logger::INFO);
        }

        std::cout << "\n=== NEW FOD REPORT ===" << std::endl;
        std::cout << "  Officer  : " << header.officerName << std::endl;
        std::cout << "  Zone     : " << header.locationZone << std::endl;
        std::cout << "  Hazard   : " << hazardTypeStr(header.hazardType) << std::endl;
        std::cout << "  Severity : " << header.severityLevel << std::endl;
        std::cout << "  Desc     : " << desc.description << std::endl;
        std::cout << "======================" << std::endl;

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

        PacketDeserializer::freeDescription(desc);

        const std::string confirmation =
            "FOD received. Zone: " + header.locationZone +
            " | Status: INSPECTION | Officer: " + header.officerName;

        if (!sendResponse(clientSock, confirmation))
        {
            Logger::log("Failed to send confirmation.", Logger::ERR);
            continueLoop = false;
        }
        else
        {
            Logger::log("RESPONSE SENT: " + confirmation, Logger::INFO);
            (void)Logger::saveLog(db, "RESPONSE SENT: " + confirmation, Logger::INFO);
        }
    }

    return 0;
}