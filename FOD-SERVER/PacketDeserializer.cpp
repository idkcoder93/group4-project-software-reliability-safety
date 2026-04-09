#include "PacketDeserializer.h"
#include <cstring>
#include <string>
#include <array>
#include <chrono>
#include <ctime>

namespace FODServer
{
    // private helpers

    int PacketDeserializer::computeChecksum(const char* data, int length)
    {
        int sum = 0;
        for (int i = 0; i < length; ++i)
        {
            sum += static_cast<int>(static_cast<unsigned char>(data[i]));
        }
        return sum;
    }

    static bool readInt(const char* data, int len, int& offset, int& out)
    {
        bool success = false;
        if ((offset + 4) <= len)
        {
            (void)memcpy(&out, &data[offset], 4);
            offset += 4;
            success = true;
        }
        return success;
    }

    static bool readString(const char* data, int len, int& offset, std::string& out)
    {
        bool success = false;
        int strLen = 0;
        if (readInt(data, len, offset, strLen))
        {
            if ((strLen >= 0) && ((offset + strLen) <= len))
            {
                out.assign(&data[offset], static_cast<size_t>(strLen));
                offset += strLen;
                success = true;
            }
        }
        return success;
    }

    // public

    bool PacketDeserializer::deserializeHeader(const char* data, int len, FODHeader& h)
    {
        bool success = true;
        int offset = 0;
        int tmp = 0;

        if (success) { success = readInt(data, len, offset, h.packetTypeId); }
        if (success) { success = readInt(data, len, offset, tmp); }
        if (success) { h.hazardType = static_cast<HazardType>(tmp); }
        if (success) { success = readString(data, len, offset, h.locationZone); }
        if (success) { success = readInt(data, len, offset, h.severityLevel); }
        if (success) { success = readString(data, len, offset, h.officerName); }

        if (success && ((offset + 8) <= len))
        {
            std::time_t t = 0;
            (void)memcpy(&t, &data[offset], sizeof(t));
            offset += 8;
            h.timestamp = std::chrono::system_clock::from_time_t(t);
        }
        else
        {
            success = false;
        }

        if (success) { success = readInt(data, len, offset, h.descLength); }
        if (success) { success = readInt(data, len, offset, h.checkSum); }

        return success;
    }

    bool PacketDeserializer::deserializeDescription(const char* data, int len, FODDescription& d)
    {
        bool success = false;
        int offset = 0;
        int descLen = 0;

        if (readInt(data, len, offset, d.packetTypeId) &&
            readInt(data, len, offset, d.checksum) &&
            readInt(data, len, offset, descLen))
        {
            if ((descLen >= 0) && ((offset + descLen) <= len))
            {
                // MISRA deviation: new[] required for dynamic char* (REQ-PKT-020)
                d.description = new char[static_cast<size_t>(descLen) + 1U];
                (void)memcpy(d.description, &data[offset], static_cast<size_t>(descLen));
                d.description[descLen] = '\0';
                success = true;
            }
        }

        return success;
    }

    // US-14: Verify header checksum
    bool PacketDeserializer::verifyHeaderChecksum(const char* rawData, int rawLen,
        const FODHeader& header)
    {
        bool valid = false;
        if (rawLen >= 8)
        {
            const int checksumDataLen = rawLen - 4;
            const int computed = computeChecksum(rawData, checksumDataLen);
            valid = (computed == header.checkSum);
        }
        return valid;
    }

    bool PacketDeserializer::verifyDescriptionChecksum(const FODDescription& desc)
    {
        bool valid = false;
        if (desc.description != nullptr)
        {
            //MISRA deviation strlen required to find dynamic char* length
            const int len = static_cast<int>(std::strlen(desc.description));
            const int computed = computeChecksum(desc.description, len);
            valid = (computed == desc.checksum);
        }
        return valid;
    }

    void PacketDeserializer::freeDescription(FODDescription& desc)
    {
        //MISRA deviation delete[] required for dynamic char* 
        delete[] desc.description;
        desc.description = nullptr;
    }
}