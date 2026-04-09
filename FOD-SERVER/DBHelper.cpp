#include "DBHelper.h"
#include "FODHeader.h"
#include <chrono>
#include <ctime>
#include <sstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <array>

namespace FODServer
{
    //convert std::string to mutable SQLCHAR buffer
    static std::vector<SQLCHAR> toSqlBuffer(const std::string& str)
    {
        std::vector<SQLCHAR> buf(str.begin(), str.end());
        buf.push_back('\0');
        return buf;
    }

    //convert HazardType enum to string for NVARCHAR column
    static std::string hazardTypeToString(HazardType ht)
    {
        std::string name = "UNKNOWN";
        switch (ht)
        {
        case FOD_HAZARD_TYPE_DEBRIS: name = "DEBRIS";  break;
        case FOD_HAZARD_TYPE_LIQUID: name = "LIQUID";  break;
        case FOD_HAZARD_TYPE_ANIMAL: name = "ANIMAL";  break;
        case FOD_HAZARD_TYPE_OTHER:  name = "OTHER";   break;
        default:                     /* no action required */ break;
        }
        return name;
    }

    // Open connection
    bool DBHelper::openConnection(const std::string& connStr)
    {
        bool success = false;
        SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv);

        if (ret == SQL_SUCCESS)
        {
            ret = SQLSetEnvAttr(hEnv, SQL_ATTR_ODBC_VERSION,
                reinterpret_cast<SQLPOINTER>(SQL_OV_ODBC3), 0);
            if (ret == SQL_SUCCESS)
            {
                ret = SQLAllocHandle(SQL_HANDLE_DBC, hEnv, &hDbc);
                if (ret == SQL_SUCCESS)
                {
                    std::vector<SQLCHAR> connBuffer(connStr.begin(), connStr.end());
                    connBuffer.push_back('\0');

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
                    else
                    {
                        // print the real ODBC error
                        SQLCHAR sqlState[6] = {};
                        SQLCHAR message[512] = {};
                        SQLINTEGER nativeErr = 0;
                        SQLSMALLINT msgLen = 0;

                        SQLGetDiagRecA(SQL_HANDLE_DBC, hDbc, 1,
                            sqlState, &nativeErr, message, sizeof(message), &msgLen);

                        std::cerr << "ODBC SQLState: " << sqlState << "\n"
                            << "ODBC Error:    " << message << "\n"
                            << "Native Error:  " << nativeErr << "\n";
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

    //save a FOD record to the database
    bool DBHelper::saveFOD(const FODHeader& record, const FODDescription& desc)
    {
        bool success = false;

        if (hDbc != nullptr)
        {
            SQLHSTMT hStmt = SQL_NULL_HSTMT;
            SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);

            if (ret == SQL_SUCCESS)
            {
                const std::string sqlStr =
                    "INSERT INTO FODRecords "
                    "(PacketTypeId, HazardType, LocationZone, SeverityLevel, "
                    "OfficerName, Timestamp, description) "
                    "VALUES (?, ?, ?, ?, ?, ?, ?)";

                std::vector<SQLCHAR> sql = toSqlBuffer(sqlStr);
                ret = SQLPrepareA(hStmt, sql.data(), SQL_NTS);

                if (SQL_SUCCEEDED(ret))
                {
                    //Param 1 PacketTypeId (INT)
                    SQLINTEGER pktId = static_cast<SQLINTEGER>(record.packetTypeId);
                    ret = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER,
                        0, 0, &pktId, 0, nullptr);

                    //Param 2 HazardType (NVARCHAR) — declared in narrowest scope
                    if (SQL_SUCCEEDED(ret))
                    {
                        std::vector<SQLCHAR> hazardBuf = toSqlBuffer(
                            hazardTypeToString(record.hazardType));
                        ret = SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                            100, 0, hazardBuf.data(), 0, nullptr);

                        //Param 3 LocationZone (NVARCHAR)
                        if (SQL_SUCCEEDED(ret))
                        {
                            std::vector<SQLCHAR> zoneBuf = toSqlBuffer(record.locationZone);
                            ret = SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR,
                                SQL_VARCHAR, 100, 0, zoneBuf.data(), 0, nullptr);

                            //Param 4 SeverityLevel (INT)
                            SQLINTEGER severity = static_cast<SQLINTEGER>(record.severityLevel);
                            if (SQL_SUCCEEDED(ret))
                            {
                                ret = SQLBindParameter(hStmt, 4, SQL_PARAM_INPUT, SQL_C_SLONG,
                                    SQL_INTEGER, 0, 0, &severity, 0, nullptr);
                            }

                            //Param 5 OfficerName (NVARCHAR)
                            if (SQL_SUCCEEDED(ret))
                            {
                                std::vector<SQLCHAR> officerBuf = toSqlBuffer(record.officerName);
                                ret = SQLBindParameter(hStmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR,
                                    SQL_VARCHAR, 50, 0, officerBuf.data(), 0, nullptr);

                                //Param 6 Timestamp (DATETIME)
                                if (SQL_SUCCEEDED(ret))
                                {
                                    std::time_t t_c = std::chrono::system_clock::to_time_t(
                                        record.timestamp);
                                    std::tm timeStruct{};
                                    std::string timeStr = "1970-01-01 00:00:00";
                                    if (localtime_s(&timeStruct, &t_c) == 0)
                                    {
                                        std::array<char, 20> timeBuf = {};
                                        if (std::strftime(timeBuf.data(), timeBuf.size(),
                                            "%Y-%m-%d %H:%M:%S", &timeStruct) != 0)
                                        {
                                            timeStr = timeBuf.data();
                                        }
                                    }
                                    std::vector<SQLCHAR> tsBuf = toSqlBuffer(timeStr);
                                    ret = SQLBindParameter(hStmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR,
                                        SQL_VARCHAR, 50, 0, tsBuf.data(), 0, nullptr);

                                    //Param 7 description (NVARCHAR)
                                    if (SQL_SUCCEEDED(ret))
                                    {
                                        std::string descStr = (desc.description != nullptr)
                                            ? std::string(desc.description)
                                            : std::string("N/A");
                                        std::vector<SQLCHAR> descBuf = toSqlBuffer(descStr);
                                        ret = SQLBindParameter(hStmt, 7, SQL_PARAM_INPUT,
                                            SQL_C_CHAR, SQL_VARCHAR, 1000, 0,
                                            descBuf.data(), 0, nullptr);

                                        // Execute
                                        if (SQL_SUCCEEDED(ret))
                                        {
                                            ret = SQLExecute(hStmt);
                                            success = SQL_SUCCEEDED(ret) ? true : false;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                (void)SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
            }
        }

        return success;
    }
}