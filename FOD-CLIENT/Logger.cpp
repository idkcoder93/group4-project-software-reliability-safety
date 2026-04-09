
#include "Logger.h"
#include <chrono>
#include <ctime>
#include <sstream>

//helpers

std::string Logger::currentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    struct tm tmBuf = {};
    localtime_s(&tmBuf, &t);         
    char buf[32] = {};
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmBuf);
    return std::string(buf);
}

const char* Logger::packetTypeStr(PacketType pt)
{
    switch (pt)
    {
    case PacketType::AUTH:            return "AUTH";
    case PacketType::FOD_HEADER:      return "FOD_HEADER";
    case PacketType::FOD_DESCRIPTION: return "FOD_DESCRIPTION";
    case PacketType::RESPONSE:        return "RESPONSE";
    case PacketType::INFO:            return "INFO";
    default:                          return "UNKNOWN";
    }
}

const char* Logger::directionStr(TransferDirection dir)
{
    return (dir == TransferDirection::SENT) ? "SENT" : "RECEIVED";
}

//public

Logger::Logger(const std::string& filename, const std::string& sid)
    : sessionId(sid)
{
    logFile.open(filename, std::ios::app);
    if (logFile.is_open())
    {
        logFile << "=== Session [" << sessionId << "] started at "
            << currentTimestamp() << " ===" << std::endl;
        logFile.flush();
    }
}

Logger::~Logger()
{
    if (logFile.is_open())
    {
        logFile << "=== Session [" << sessionId << "] ended at "
            << currentTimestamp() << " ===" << std::endl;
        logFile.close();
    }
}

bool Logger::isOpen() const
{
    return logFile.is_open();
}

void Logger::log(PacketType packetType,
    TransferDirection direction,
    const std::string& details)
{
    if (!logFile.is_open()) { return; }

    logFile << "[" << currentTimestamp() << "] "
        << "[" << packetTypeStr(packetType) << "] "
        << "[" << directionStr(direction) << "] "
        << "[Session:" << sessionId << "] "
        << details << std::endl;
    logFile.flush();
}

void Logger::logInfo(const std::string& message)
{
    if (!logFile.is_open()) { return; }

    logFile << "[" << currentTimestamp() << "] "
        << "[INFO] "
        << "[Session:" << sessionId << "] "
        << message << std::endl;
    logFile.flush();
}