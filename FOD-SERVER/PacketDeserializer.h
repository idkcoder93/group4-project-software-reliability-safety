#pragma once

#include "FODHeader.h"
#include "FODDescription.h"

namespace FODServer
{
    class PacketDeserializer
    {
    public:
        static bool deserializeHeader(const char* data, int len, FODHeader& outHeader);
        static bool deserializeDescription(const char* data, int len, FODDescription& outDesc);

        //verify checksums on received packets
        static bool verifyHeaderChecksum(const char* rawData, int rawLen, const FODHeader& header);
        static bool verifyDescriptionChecksum(const FODDescription& desc);

        //MISRA deviation delete[] required for dynamic char*
        static void freeDescription(FODDescription& desc);

    private:
        static int computeChecksum(const char* data, int length);
    };
}