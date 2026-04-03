#pragma once


#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include "DBHelper.h"

int runServerSession(SOCKET clientSock, FODServer::DBHelper& db);