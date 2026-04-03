#pragma once


#include <string>
#include <fstream>

//packet type recorded per entry
enum class PacketType
{
    AUTH,
    FOD_HEADER,
    FOD_DESCRIPTION,
    RESPONSE,
    INFO
};

//transfer direction recorded per entry
enum class TransferDirection
{
    SENT,
    RECEIVED
};

class Logger
{
private:
    std::ofstream  logFile;
    std::string    sessionId;

    static std::string currentTimestamp();
    static const char* packetTypeStr(PacketType pt);
    static const char* directionStr(TransferDirection dir);

public:
    //tags every entry with sessionId
    Logger(const std::string& filename, const std::string& sessionId);
    ~Logger();

    bool isOpen() const;

    //Log a packet transfer event 
    void log(PacketType packetType,
        TransferDirection direction,
        const std::string& details);

    //Log a plain informational message
    void logInfo(const std::string& message);
};