#pragma once
#include <string>

namespace FODServer
{
    struct FODDescription
    {
        int packetTypeId;
        char* description; // dynamic alloc. requirement
        int checksum;
    };
}