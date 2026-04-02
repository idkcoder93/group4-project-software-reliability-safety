#include "DBHelper.h"
#include "FODHeader.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <vector>
#include <iostream>

namespace FODServer
{

    // Open connection
    bool DBHelper::openConnection(const std::string& connStr)
    {
        bool success = false; // single exit point MISRA
        SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);

        if (ret == SQL_SUCCESS)
        {
            ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION,
				reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0); // MISRA compliant type-casting in cpp
            if (ret == SQL_SUCCESS)
            {
                ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
                if (ret == SQL_SUCCESS)
                {
                    // a buffer for connStr with appr. type-casting
                    std::vector<SQLCHAR> connBuffer(connStr.begin(), connStr.end());
                    connBuffer.push_back('\0'); // null-terminate c-style for argument

                    ret = SQLDriverConnectA(
                        hDbc,
                        nullptr,
                        connBuffer.data(),
                        SQL_NTS,
                        nullptr,
                        0,
                        nullptr,
                        SQL_DRIVER_NOPROMPT);

                    if (SQL_SUCCEEDED(ret))
                    {
                        success = true;
                    }
                }
            }
        }

        return success;
    }

    // Close connection
    void DBHelper::closeConnection()
    {
        SQLRETURN ret;
        if (hDbc != nullptr)
        {
            ret = SQLDisconnect(hDbc); (void)ret;
            ret = SQLFreeHandle(SQL_HANDLE_DBC, hDbc); (void)ret;
            hDbc = nullptr;
        }
        if (hEnv != nullptr)
        {
            ret = SQLFreeHandle(SQL_HANDLE_ENV, hEnv); (void)ret;
            hEnv = nullptr;
        }
    }

    // Adding FOD record to database
    bool DBHelper::saveFOD(const FODHeader& record, const FODDescription& desc)
    {
        bool success = false; // single exit point

        if (hDbc != nullptr)
        {
            SQLHSTMT hStmt;
            if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt) == SQL_SUCCESS)
            {
                // Convert timestamp
                std::time_t t_c = std::chrono::system_clock::to_time_t(record.timestamp);
                std::tm timeStruct{};
                if (localtime_s(&timeStruct, &t_c) == 0) // safe version no define
                {
                    char timeStr[20] = { 0 }; // YYYY-MM-DD HH:MM:SS
                    if (std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeStruct) != 0)
                    {
                        // Build SQL safely
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

                        // Use mutable vector for SQL string
                        std::vector<SQLCHAR> sqlBuffer(sql.begin(), sql.end());
                        sqlBuffer.push_back('\0');

                        SQLRETURN ret = SQLExecDirectA(hStmt, sqlBuffer.data(), SQL_NTS);
                        if (SQL_SUCCEEDED(ret))
                        {
                            success = true;
                        }
                    }
                }

                SQLRETURN freeRet = SQLFreeHandle(SQL_HANDLE_STMT, hStmt); (void)freeRet;
            }
        }

        return success;
    }

}