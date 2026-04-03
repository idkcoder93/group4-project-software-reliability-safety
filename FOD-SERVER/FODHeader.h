#pragma once
#include "FODDescription.h"
#include <string>
#include <chrono>

namespace FODServer
{

    enum HazardType
    {
        FOD_HAZARD_TYPE_UNKNOWN = 0,
        FOD_HAZARD_TYPE_DEBRIS = 1,
        FOD_HAZARD_TYPE_LIQUID = 2,
        FOD_HAZARD_TYPE_ANIMAL = 3,
        FOD_HAZARD_TYPE_OTHER = 4
    };

    struct FODHeader {
        int packetTypeId;
        HazardType hazardType;
        std::string locationZone;
        int severityLevel;
        std::string officerName;
        std::chrono::system_clock::time_point timestamp;
        int descLength;
        int checkSum;
    };
}