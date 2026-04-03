#include "pch.h"
#include "gtest/gtest.h"

#include "../FOD-SERVER/FODHeader.h"
#include "../FOD-SERVER/FODDescription.h"
#include "../FOD-SERVER/User.h"
#include "../FOD-SERVER/User.cpp"

#pragma comment(lib, "odbc32.lib")

TEST(FODServerTests, HazardTypeValuesAreStable)
{
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_UNKNOWN, 0);
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_DEBRIS, 1);
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_LIQUID, 2);
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_ANIMAL, 3);
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_OTHER, 4);
}

TEST(FODServerTests, HazardTypeValuesAreContiguous)
{
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_DEBRIS, FODServer::FOD_HAZARD_TYPE_UNKNOWN + 1);
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_LIQUID, FODServer::FOD_HAZARD_TYPE_DEBRIS + 1);
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_ANIMAL, FODServer::FOD_HAZARD_TYPE_LIQUID + 1);
    EXPECT_EQ(FODServer::FOD_HAZARD_TYPE_OTHER, FODServer::FOD_HAZARD_TYPE_ANIMAL + 1);
}

TEST(FODServerTests, UserSetUsernameAndGetUsernameRoundTrip)
{
    FODServer::User user{};
    user.setUsername("airfield_admin");

    EXPECT_EQ(user.getUsername(), "airfield_admin");
}

TEST(FODServerTests, UserSetUsernameCanOverwriteExistingValue)
{
    FODServer::User user{};
    user.setUsername("first");
    user.setUsername("second");

    EXPECT_EQ(user.getUsername(), "second");
}

TEST(FODServerTests, ParseClientLoginParsesUsernameBeforeDelimiter)
{
    FODServer::User parser{};
    FODServer::User parsed = parser.parseClientLogin("testUser:testPassword");

    EXPECT_EQ(parsed.getUsername(), "testUser");
}

TEST(FODServerTests, ParseClientLoginWithoutDelimiterLeavesUsernameEmpty)
{
    FODServer::User parser{};
    FODServer::User parsed = parser.parseClientLogin("testUserOnly");

    EXPECT_TRUE(parsed.getUsername().empty());
}

TEST(FODServerTests, ParseClientLoginWithLeadingDelimiterProducesEmptyUsername)
{
    FODServer::User parser{};
    FODServer::User parsed = parser.parseClientLogin(":passwordOnly");

    EXPECT_TRUE(parsed.getUsername().empty());
}

TEST(FODServerTests, ParseClientLoginWithTrailingDelimiterParsesUsername)
{
    FODServer::User parser{};
    FODServer::User parsed = parser.parseClientLogin("usernameOnly:");

    EXPECT_EQ(parsed.getUsername(), "usernameOnly");
}

TEST(FODServerTests, ParseClientLoginUsesFirstDelimiterWhenMultipleExist)
{
    FODServer::User parser{};
    FODServer::User parsed = parser.parseClientLogin("user:pass:extra");

    EXPECT_EQ(parsed.getUsername(), "user");
}

TEST(FODServerTests, ParseClientLoginPreservesWhitespaceInUsername)
{
    FODServer::User parser{};
    FODServer::User parsed = parser.parseClientLogin(" user name :pass");

    EXPECT_EQ(parsed.getUsername(), " user name ");
}

TEST(FODServerTests, FODDescriptionDefaultInitializationIsSafe)
{
    FODServer::FODDescription description{};

    EXPECT_EQ(description.packetTypeId, 0);
    EXPECT_EQ(description.description, nullptr);
    EXPECT_EQ(description.checksum, 0);
}
