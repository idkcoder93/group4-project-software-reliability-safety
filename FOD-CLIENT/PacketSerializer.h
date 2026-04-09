#pragma once

// Serializes FODHeader and FODDescription structs into a byte stream
// suitable for transmission over TCP.  Mirrors PacketDeserializer on the server side.

#include "FODHeader.h"
#include "FODDescription.h"
#include <vector>
#include <string>

//Packet Type IDs
static const int PACKET_TYPE_AUTH = 0x02;
static const int PACKET_TYPE_FOD_REPORT = 0x03;
static const int PACKET_TYPE_RESPONSE = 0x04;
static const int PACKET_TYPE_BITMAP = 0x05;  //Runway zone bitmap transfer
static const int PACKET_TYPE_HEARTBEAT = 0x06;  //Heartbeat

class PacketSerializer
{
public:
    static int computeChecksum(const char* data, int length);

    static FODHeader buildHeader(HazardType    hazard,
        const std::string& zone,
        int           severity,
        const std::string& officer,
        int           descLength);

    //MISRA deviation: new[] for desc.description required
    static FODDescription buildDescription(const std::string& descText);

    static std::vector<char> serializeHeader(FODHeader& header);
    static std::vector<char> serializeDescription(const FODDescription& desc);

    //MISRA deviation: delete[] required for dynamic char*
    static void freeDescription(FODDescription& desc);
};