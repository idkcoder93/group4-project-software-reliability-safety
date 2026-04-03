#include "Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <vector>

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
            timeStruct = std::tm{};
        }

        std::ostringstream oss;
        oss << std::put_time(&timeStruct, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    // vector for no decay to pointer (raw array data)
    static std::vector<SQLCHAR> toSqlBuffer(const std::string& str)
    {
        std::vector<SQLCHAR> buf(str.begin(), str.end());
        buf.push_back('\0');
        return buf;
    }

    bool Logger::saveLog(DBHelper& db, const std::string& message, Level level)
    {
        bool success = false; // signal exist point

        SQLHDBC hDbc = db.getDbc();
        if (hDbc != nullptr)
        {
            SQLHSTMT hStmt = SQL_NULL_HSTMT;
            SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt);
            if (ret == SQL_SUCCESS)
            {
                const std::string sqlStr =
                    "INSERT INTO LogTable (message, timestamp, level) VALUES (?, ?, ?)";
                std::vector<SQLCHAR> sql = toSqlBuffer(sqlStr);

                ret = SQLPrepareA(hStmt, sql.data(), SQL_NTS);
                if (SQL_SUCCEEDED(ret))
                {
                    std::vector<SQLCHAR> msgBuf = toSqlBuffer(message);
                    ret = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                        1024, 0,
                        static_cast<SQLPOINTER>(msgBuf.data()),
                        0, nullptr);

                    if (SQL_SUCCEEDED(ret))
                    {
                        // mutable buffer for timestamp sending raw data no pointer
                        std::vector<SQLCHAR> tsBuf = toSqlBuffer(getTimestamp());
                        ret = SQLBindParameter(hStmt, 2, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                            50, 0,
                            static_cast<SQLPOINTER>(tsBuf.data()),
                            0, nullptr);

                        if (SQL_SUCCEEDED(ret))
                        {
                            std::string levelStr;
                            switch (level)
                            {
                                case INFO:    levelStr = "INFO";    break;
                                case WARNING: levelStr = "WARNING"; break;
                                case ERR:     levelStr = "ERROR";   break;
                            }
                            std::vector<SQLCHAR> levelBuf = toSqlBuffer(levelStr);
                            ret = SQLBindParameter(hStmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                                20, 0,
                                static_cast<SQLPOINTER>(levelBuf.data()),
                                0, nullptr);

                            if (SQL_SUCCEEDED(ret))
                            {
                                ret = SQLExecute(hStmt);
                                success = SQL_SUCCEEDED(ret) ? true : false;
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