
#include "PacketSerializer.h"
#include <chrono>
#include <cstring>
#include <ctime>

//private helpers 

static void appendInt(std::vector<char>& buf, int val)
{
    char bytes[4] = {};
    memcpy(bytes, &val, 4);
    for (int i = 0; i < 4; ++i) { buf.push_back(bytes[i]); }
}

static void appendString(std::vector<char>& buf, const std::string& s)
{
    appendInt(buf, static_cast<int>(s.size()));
    for (char c : s) { buf.push_back(c); }
}

static void appendTimeT(std::vector<char>& buf, const std::chrono::system_clock::time_point& tp)
{
    char bytes[8] = {};
    std::time_t t = std::chrono::system_clock::to_time_t(tp);
    memcpy(bytes, &t, sizeof(t));
    for (int i = 0; i < 8; ++i) { buf.push_back(bytes[i]); }
}

//public

int PacketSerializer::computeChecksum(const char* data, int length)
{
    int sum = 0;
    for (int i = 0; i < length; ++i)
    {
        sum += static_cast<unsigned char>(data[i]);
    }
    return sum;
}

FODHeader PacketSerializer::buildHeader(HazardType         hazard,
    const std::string& zone,
    int                severity,
    const std::string& officer,
    int                descLength)
{
    FODHeader h = {};
    h.packetTypeId = PACKET_TYPE_FOD_REPORT;
    h.hazardType = hazard;
    h.locationZone = zone;
    h.severityLevel = severity;
    h.officerName = officer;
    h.timestamp = std::chrono::system_clock::now();  //auto timestamp
    h.descLength = descLength;
    h.checkSum = 0;   //filled in by serializeHeader
    return h;
}

FODDescription PacketSerializer::buildDescription(const std::string& descText)
{
    FODDescription d = {};
    d.packetTypeId = PACKET_TYPE_FOD_REPORT;

    const int len = static_cast<int>(descText.size());
    d.description = new char[len + 1];          //dynamic char*
    memcpy(d.description, descText.c_str(), static_cast<size_t>(len) + 1U);

    d.checksum = computeChecksum(d.description, len);   
    return d;
}

std::vector<char> PacketSerializer::serializeHeader(FODHeader& header)
{
    std::vector<char> buf;

    //serialize with checkSum = 0 first so we can compute it
    header.checkSum = 0;

    appendInt(buf, header.packetTypeId);
    appendInt(buf, static_cast<int>(header.hazardType));
    appendString(buf, header.locationZone);
    appendInt(buf, header.severityLevel);
    appendString(buf, header.officerName);
    appendTimeT(buf, header.timestamp);
    appendInt(buf, header.descLength);
    appendInt(buf, 0);  //placeholder for checksum

    //Compute checksum over the whole buffer 
    header.checkSum = computeChecksum(buf.data(), static_cast<int>(buf.size()));

    //write checksum into the last 4 bytes
    memcpy(&buf[buf.size() - 4], &header.checkSum, 4);

    return buf;
}

std::vector<char> PacketSerializer::serializeDescription(const FODDescription& desc)
{
    std::vector<char> buf;

    appendInt(buf, desc.packetTypeId);
    appendInt(buf, desc.checksum);

    if (desc.description != nullptr)
    {
        const int len = static_cast<int>(strlen(desc.description));
        appendInt(buf, len);
        for (int i = 0; i < len; ++i) { buf.push_back(desc.description[i]); }
    }
    else
    {
        appendInt(buf, 0);
    }

    return buf;
}

void PacketSerializer::freeDescription(FODDescription& desc)
{
    delete[] desc.description;
    desc.description = nullptr;
}