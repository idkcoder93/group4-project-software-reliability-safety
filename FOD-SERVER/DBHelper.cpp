#define _CRT_SECURE_NO_WARNINGS

#include "DBHelper.h"
#include "FODHeader.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <iostream>


// Open connection
bool DBHelper::openConnection(const std::string& connStr) {
    // Allocate environment handle
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv) != SQL_SUCCESS) return false;
    if (SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0) != SQL_SUCCESS) return false;

    // Allocate connection handle
    if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc) != SQL_SUCCESS) return false;

    // Connect
    SQLRETURN ret = SQLDriverConnectA(
        hDbc,
        nullptr,
        (SQLCHAR*)connStr.c_str(),
        SQL_NTS,
        nullptr,
        0,
        nullptr,
        SQL_DRIVER_NOPROMPT
    );

    return SQL_SUCCEEDED(ret);
}

// Close connection
void DBHelper::closeConnection() {
    if (hDbc) {
        SQLDisconnect(hDbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc);
        hDbc = nullptr;
    }
    if (hEnv) {
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv);
        hEnv = nullptr;
    }
}

// adding FOD record to database
bool DBHelper::saveFOD(const FODHeader& record, const FODDescription& desc) {
    if (!hDbc) return false;

    SQLHSTMT hStmt;
    if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt) != SQL_SUCCESS) return false;

    // Convert timestamp
    std::time_t t_c = std::chrono::system_clock::to_time_t(record.timestamp);
    char timeStr[20]; // YYYY-MM-DD HH:MM:SS
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", std::localtime(&t_c));

    // Build SQL
    std::stringstream ss;
    ss << "INSERT INTO FODRecords "
        << "(PacketTypeId,HazardType,LocationZone,SeverityLevel,OfficerName,Timestamp,DescLength,CheckSum) VALUES ("
        << record.packetTypeId << ", '"
        << record.hazardType << "', '"
        << record.locationZone << "', "
        << record.severityLevel << ", '"
        << record.officerName << "', '"
        << timeStr << "', "
        << desc.description << ");";

    std::string sql = ss.str();

    SQLRETURN ret = SQLExecDirectA(hStmt, (SQLCHAR*)sql.c_str(), SQL_NTS);

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

    return SQL_SUCCEEDED(ret);
}