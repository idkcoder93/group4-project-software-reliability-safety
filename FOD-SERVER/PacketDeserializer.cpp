
#include "PacketDeserializer.h"
#include "FODHeader.h"
#include "FODDescription.h"
#include <cstring>
#include <string>
#include <chrono>
#include <ctime>

using namespace FODServer;

//private helpers

int PacketDeserializer::computeChecksum(const char* data, int length)
{
    int sum = 0;
    for (int i = 0; i < length; ++i)
    {
        sum += static_cast<unsigned char>(data[i]);
    }
    return sum;
}

static bool readInt(const char* data, int len, int& offset, int& out)
{
    if (offset + 4 > len) { return false; }
    memcpy(&out, data + offset, 4);
    offset += 4;
    return true;
}

static bool readString(const char* data, int len, int& offset, std::string& out)
{
    int strLen = 0;
    if (!readInt(data, len, offset, strLen)) { return false; }
    if (strLen < 0 || offset + strLen > len) { return false; }
    out.assign(data + offset, static_cast<size_t>(strLen));
    offset += strLen;
    return true;
}

//public

bool PacketDeserializer::deserializeHeader(const char* data, int len, FODHeader& h)
{
    int offset = 0;
    int tmp = 0;

    if (!readInt(data, len, offset, h.packetTypeId)) { return false; }
    if (!readInt(data, len, offset, tmp)) { return false; }
    h.hazardType = static_cast<HazardType>(tmp);

    if (!readString(data, len, offset, h.locationZone)) { return false; }
    if (!readInt(data, len, offset, h.severityLevel)) { return false; }
    if (!readString(data, len, offset, h.officerName)) { return false; }

    if (offset + 8 > len) { return false; }
    std::time_t t = 0;
    memcpy(&t, data + offset, sizeof(t));
    offset += 8;
    h.timestamp = std::chrono::system_clock::from_time_t(t);

    if (!readInt(data, len, offset, h.descLength)) { return false; }
    if (!readInt(data, len, offset, h.checkSum)) { return false; }

    return true;
}

bool PacketDeserializer::deserializeDescription(const char* data, int len, FODDescription& d)
{
    int offset = 0;
    int descLen = 0;

    if (!readInt(data, len, offset, d.packetTypeId)) { return false; }
    if (!readInt(data, len, offset, d.checksum)) { return false; }
    if (!readInt(data, len, offset, descLen)) { return false; }

    if (descLen < 0 || offset + descLen > len) { return false; }

    d.description = new char[static_cast<size_t>(descLen) + 1U];
    memcpy(d.description, data + offset, static_cast<size_t>(descLen));
    d.description[descLen] = '\0';

    return true;
}

bool PacketDeserializer::verifyDescriptionChecksum(const FODDescription& desc)
{
    if (desc.description == nullptr) { return false; }
    const int len = static_cast<int>(strlen(desc.description));
    const int computed = computeChecksum(desc.description, len);
    return (computed == desc.checksum);
}

void PacketDeserializer::freeDescription(FODDescription& desc)
{
    delete[] desc.description;
    desc.description = nullptr;
}