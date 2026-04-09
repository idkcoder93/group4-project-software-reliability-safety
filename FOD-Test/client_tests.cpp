#include "pch.h"
#include "gtest/gtest.h"

#include "../FOD-CLIENT/FODHeader.h"
#include "../FOD-CLIENT/FODDescription.h"

#include <chrono>

TEST(FODClientTests, HazardTypeValuesAreStable)
{
    EXPECT_EQ(FOD_HAZARD_TYPE_UNKNOWN, 0);
    EXPECT_EQ(FOD_HAZARD_TYPE_DEBRIS, 1);
    EXPECT_EQ(FOD_HAZARD_TYPE_LIQUID, 2);
    EXPECT_EQ(FOD_HAZARD_TYPE_ANIMAL, 3);
    EXPECT_EQ(FOD_HAZARD_TYPE_OTHER, 4);
}

TEST(FODClientTests, HazardTypeValuesAreContiguous)
{
    EXPECT_EQ(FOD_HAZARD_TYPE_DEBRIS, FOD_HAZARD_TYPE_UNKNOWN + 1);
    EXPECT_EQ(FOD_HAZARD_TYPE_LIQUID, FOD_HAZARD_TYPE_DEBRIS + 1);
    EXPECT_EQ(FOD_HAZARD_TYPE_ANIMAL, FOD_HAZARD_TYPE_LIQUID + 1);
    EXPECT_EQ(FOD_HAZARD_TYPE_OTHER, FOD_HAZARD_TYPE_ANIMAL + 1);
}

TEST(FODClientTests, FODHeaderDefaultInitializationIsSafe)
{
    FODHeader header{};

    EXPECT_EQ(header.packetTypeId, 0);
    EXPECT_EQ(header.hazardType, FOD_HAZARD_TYPE_UNKNOWN);
    EXPECT_TRUE(header.locationZone.empty());
    EXPECT_EQ(header.severityLevel, 0);
    EXPECT_TRUE(header.officerName.empty());
    EXPECT_EQ(header.descLength, 0);
    EXPECT_EQ(header.checkSum, 0);
}

TEST(FODClientTests, FODHeaderStoresAssignedValues)
{
    FODHeader header{};
    header.packetTypeId = 10;
    header.hazardType = FOD_HAZARD_TYPE_DEBRIS;
    header.locationZone = "Runway-A";
    header.severityLevel = 2;
    header.officerName = "Officer Green";
    header.descLength = 18;
    header.checkSum = 12345;

    EXPECT_EQ(header.packetTypeId, 10);
    EXPECT_EQ(header.hazardType, FOD_HAZARD_TYPE_DEBRIS);
    EXPECT_EQ(header.locationZone, "Runway-A");
    EXPECT_EQ(header.severityLevel, 2);
    EXPECT_EQ(header.officerName, "Officer Green");
    EXPECT_EQ(header.descLength, 18);
    EXPECT_EQ(header.checkSum, 12345);
}

TEST(FODClientTests, FODHeaderTimestampStoresAssignedTime)
{
    FODHeader header{};
    const auto now = std::chrono::system_clock::now();
    header.timestamp = now;

    EXPECT_EQ(header.timestamp, now);
}

TEST(FODClientTests, FODDescriptionDefaultInitializationIsSafe)
{
    FODDescription description{};

    EXPECT_EQ(description.packetTypeId, 0);
    EXPECT_EQ(description.description, nullptr);
    EXPECT_EQ(description.checksum, 0);
}

TEST(FODClientTests, FODHeaderCopyRetainsAllFields)
{
    FODHeader original{};
    original.packetTypeId = 42;
    original.hazardType = FOD_HAZARD_TYPE_OTHER;
    original.locationZone = "Taxiway-C";
    original.severityLevel = 5;
    original.officerName = "Officer Lane";
    original.descLength = 77;
    original.checkSum = 9001;

    FODHeader copy = original;

    EXPECT_EQ(copy.packetTypeId, original.packetTypeId);
    EXPECT_EQ(copy.hazardType, original.hazardType);
    EXPECT_EQ(copy.locationZone, original.locationZone);
    EXPECT_EQ(copy.severityLevel, original.severityLevel);
    EXPECT_EQ(copy.officerName, original.officerName);
    EXPECT_EQ(copy.descLength, original.descLength);
    EXPECT_EQ(copy.checkSum, original.checkSum);
}
