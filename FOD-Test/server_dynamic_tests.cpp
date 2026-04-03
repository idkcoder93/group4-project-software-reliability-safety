#include "pch.h"
#include "gtest/gtest.h"

#include "../FOD-SERVER/User.h"

#include <random>
#include <string>

#pragma comment(lib, "odbc32.lib")

TEST(FODServerDynamicTests, ParseClientLoginHandlesManyGeneratedCredentials)
{
    FODServer::User parser{};

    std::mt19937 rng(12345U);
    std::uniform_int_distribution<int> lenDist(3, 12);
    std::uniform_int_distribution<int> charDist(0, 25);

    for (int i = 0; i < 200; ++i)
    {
        std::string username;
        std::string password;

        const int userLen = lenDist(rng);
        const int passLen = lenDist(rng);

        for (int u = 0; u < userLen; ++u)
        {
            username.push_back(static_cast<char>('a' + charDist(rng)));
        }

        for (int p = 0; p < passLen; ++p)
        {
            password.push_back(static_cast<char>('a' + charDist(rng)));
        }

        const std::string packet = username + ":" + password;
        FODServer::User parsed = parser.parseClientLogin(packet);

        EXPECT_EQ(parsed.getUsername(), username);
    }
}

TEST(FODServerDynamicTests, ParseClientLoginReturnsEmptyUsernameForGeneratedPacketsWithoutDelimiter)
{
    FODServer::User parser{};

    std::mt19937 rng(67890U);
    std::uniform_int_distribution<int> lenDist(1, 20);
    std::uniform_int_distribution<int> charDist(0, 25);

    for (int i = 0; i < 200; ++i)
    {
        std::string packet;
        const int len = lenDist(rng);

        for (int j = 0; j < len; ++j)
        {
            packet.push_back(static_cast<char>('a' + charDist(rng)));
        }

        FODServer::User parsed = parser.parseClientLogin(packet);
        EXPECT_TRUE(parsed.getUsername().empty());
    }
}
