#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace FODServer
{
    void Logger::log(const std::string& message, Level level)
    {
        std::string prefix;
        switch (level)
        {
        case INFO:
            prefix = "[INFO] ";
            break;
        case WARNING:
            prefix = "[WARNING] ";
            break;
        case ERR:
            prefix = "[ERROR] ";
            break;
        }

        std::cout << getTimestamp() << " " << prefix << message << std::endl;
    }

    std::string Logger::getTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        std::time_t time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm timeStruct{};
        if (localtime_s(&timeStruct, &time_t_now) != 0)
        {
            // fallback if localtime_s fails
            timeStruct = std::tm{};
        }

        std::ostringstream oss;
        oss << std::put_time(&timeStruct, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    bool Logger::saveLog(DBHelper& db, const std::string& message, Level level)
    {
        bool success = false; // single exit point

        SQLHDBC hDbc = db.getDbc();
        if (hDbc != nullptr)
        {
            SQLHSTMT hStmt = SQL_NULL_HSTMT;
            SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
            if (ret == SQL_SUCCESS)
            {
                const char sqlText[] = "INSERT INTO LogTable (message, timestamp, level) VALUES (?, ?, ?)";
                ret = SQLPrepareA(hStmt, const_cast<SQLCHAR*>(reinterpret_cast<const SQLCHAR*>(sqlText)), SQL_NTS);
                if (SQL_SUCCEEDED(ret))
                {
                    // bind message
                    ret = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 1024, 0,
                        const_cast<SQLPOINTER>(static_cast<const void*>(message.c_str())), 0, nullptr);

                    if (SQL_SUCCEEDED(ret))
                    {
                        // bind timestamp
                        const std::string ts = getTimestamp();
                        ret = SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 50, 0,
                            const_cast<SQLPOINTER>(static_cast<const void*>(ts.c_str())), 0, nullptr);

                        if (SQL_SUCCEEDED(ret))
                        {
                            // convert log level as string
                            std::string levelStr;
                            switch (level)
                            {
                            case INFO:
                                levelStr = "INFO";
                                break;
                            case WARNING:
                                levelStr = "WARNING";
                                break;
                            case ERR:
                                levelStr = "ERROR";
                                break;
                            }

                            ret = SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 20, 0,
                                const_cast<SQLPOINTER>(static_cast<const void*>(levelStr.c_str())), 0, nullptr);

                            if (SQL_SUCCEEDED(ret))
                            {
                                ret = SQLExecute(hStmt);
                                success = SQL_SUCCEEDED(ret);
                            }
                        }
                    }
                }

                ret = SQLFreeHandle(SQL_HANDLE_STMT, hStmt); // capture return value
                (void)ret;
            }
        }

        return success;
    }
}