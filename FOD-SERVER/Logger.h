#pragma once
#include "DBHelper.h"
#include <string>

namespace FODServer
{

    class Logger {
    public:
        enum Level { INFO, WARNING, ERR };

        // Logs a message with timestamp and level
        static void log(const std::string& message, Level level = INFO);

        // saves log info to db
        static bool saveLog(DBHelper& db, const std::string& message, Level level = INFO);

    private:
        // Returns current timestamp as string: YYYY-MM-DD HH:MM:SS
        static std::string getTimestamp();
    };
}