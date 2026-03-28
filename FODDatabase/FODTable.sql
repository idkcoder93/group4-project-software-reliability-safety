CREATE TABLE FODRecords (
    Id INT IDENTITY(1,1) PRIMARY KEY,
    PacketTypeId INT NOT NULL,
    HazardType NVARCHAR(100),
    LocationZone NVARCHAR(100),
    SeverityLevel INT,
    OfficerName NVARCHAR(50),
    Timestamp DATETIME,
    description NVARCHAR(1000),
);