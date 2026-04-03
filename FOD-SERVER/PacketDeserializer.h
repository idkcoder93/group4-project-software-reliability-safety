#pragma once
#include "FODHeader.h"
#include "FODDescription.h"

class PacketDeserializer
{
public:
    static bool deserializeHeader(const char* data, int len, FODServer::FODHeader& outHeader);
    static bool deserializeDescription(const char* data, int len, FODServer::FODDescription& outDesc);
    static bool verifyDescriptionChecksum(const FODServer::FODDescription& desc);
    static void freeDescription(FODServer::FODDescription& desc);

private:
    static int computeChecksum(const char* data, int length);
};