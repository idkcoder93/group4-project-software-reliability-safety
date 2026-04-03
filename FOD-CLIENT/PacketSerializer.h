#pragma once

// Serializes FODHeader and FODDescription structs into a byte stream
// suitable for transmission over TCP.  Mirrors PacketDeserializer on the
// server side.

// Wire format for FODHeader:
//   [4] packetTypeId  (int)
//   [4] hazardType    (int, cast of enum)
//   [4] locationZone length  then [N] chars
//   [4] severityLevel (int)
//   [4] officerName length   then [N] chars
//   [8] timestamp     (time_t)
//   [4] descLength    (int)
//   [4] checkSum      (int)   <- computed last, over all preceding bytes
//
// Wire format for FODDescription:
//   [4] packetTypeId  (int)
//   [4] checksum      (int)   <- sum of description bytes
//   [4] description length   then [N] chars

#include "FODHeader.h"
#include "FODDescription.h"
#include <vector>
#include <string>

//Packet Type IDs
static const int PACKET_TYPE_AUTH = 0x02;
static const int PACKET_TYPE_FOD_REPORT = 0x03;
static const int PACKET_TYPE_RESPONSE = 0x04;

class PacketSerializer
{
public:
    //Simple additive byte checksum
    static int computeChecksum(const char* data, int length);

    //Build a FODHeader from collected user input
    static FODHeader buildHeader(HazardType    hazard,
        const std::string& zone,
        int           severity,
        const std::string& officer,
        int           descLength);

    //Build a FODDescription allocates description on the heap
    //Caller must call freeDescription() when done.
    static FODDescription buildDescription(const std::string& descText);

    //Serialize header to a byte buffer computes and embeds checksum
    static std::vector<char> serializeHeader(FODHeader& header);

    //Serialize description to a byte buffer
    static std::vector<char> serializeDescription(const FODDescription& desc);

    //Free the heap-allocated description pointer
    static void freeDescription(FODDescription& desc);
};