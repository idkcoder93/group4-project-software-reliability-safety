#pragma once

//runClientSession() is the single entry-point

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include "Logger.h"

int runClientSession(SOCKET sock, Logger& logger);