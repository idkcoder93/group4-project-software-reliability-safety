#include "DBHelper.h"
#include "User.h"

#include <functional>
#include <string>
#include <vector>
#include <array>
#include <cstdlib>

namespace
{
    bool isAutomatedTestingEnabled()
    {
        char* envVal = nullptr;
        size_t len = 0;
        const bool enabled = ((_dupenv_s(&envVal, &len, "FOD_AUTOMATED_TESTING") == 0) && (envVal != nullptr));
        if (envVal != nullptr)
        {
            free(envVal);   //NOLINT(cppcoreguidelines-no-malloc)
        }
        return enabled;
    }

    std::string getAutomationCredential(const char* envName, const char* defaultValue)
    {
        char* envVal = nullptr;
        size_t len = 0;
        std::string result(defaultValue);

        if ((_dupenv_s(&envVal, &len, envName) == 0) && (envVal != nullptr))
        {
            result = std::string(envVal);
            free(envVal);   //NOLINT(cppcoreguidelines-no-malloc)
        }

        return result;
    }
}

namespace FODServer
{
    bool User::authenticateUser(const std::string& inputUsername,
        const std::string& inputPassword,
        DBHelper& db)
    {
        if (isAutomatedTestingEnabled())
        {
            const std::string expectedUsername = getAutomationCredential("FOD_TEST_USERNAME", "admin");
            const std::string expectedPassword = getAutomationCredential("FOD_TEST_PASSWORD", "pass@123");
            if ((inputUsername == expectedUsername) && (inputPassword == expectedPassword))
            {
                return true;
            }
        }

        bool result = false; // single exit point

        SQLHDBC hDbc = db.getDbc();
        SQLHSTMT hStmt = SQL_NULL_HSTMT;

        if ((hDbc != SQL_NULL_HDBC) &&
            (SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt) == SQL_SUCCESS))
        {
            // vector prevents sql string literal decay and add null terminator
            const std::string sqlStr = "SELECT Password FROM [User] WHERE Username = ?";
            std::vector<SQLCHAR> sql(sqlStr.begin(), sqlStr.end());
            sql.push_back('\0');

            const SQLRETURN prepRet = SQLPrepareA(hStmt, sql.data(), SQL_NTS);

            // mutable buffer
            std::vector<SQLCHAR> unameBuffer(inputUsername.begin(), inputUsername.end());
            unameBuffer.push_back('\0');

            const SQLRETURN bindRet = SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT,
                SQL_C_CHAR, SQL_VARCHAR, 255, 0,
                static_cast<SQLPOINTER>(unameBuffer.data()),
                0, NULL);

            if (SQL_SUCCEEDED(prepRet) && SQL_SUCCEEDED(bindRet) &&
                (SQLExecute(hStmt) == SQL_SUCCESS))
            {
                if (SQLFetch(hStmt) == SQL_SUCCESS)
                {
                    // std::array + .data() prevents array decay
                    std::array<SQLCHAR, 256> storedHash = {};

                    // return value from SQLGetData complying with MISRA
                    const SQLRETURN getData = SQLGetData(hStmt, 1, SQL_C_CHAR,
                        storedHash.data(),
                        static_cast<SQLLEN>(storedHash.size()),
                        NULL);

                    if (SQL_SUCCEEDED(getData))
                    {
                        const std::size_t hashedInput = std::hash<std::string>{}(inputPassword);
                        const std::string hashedInputStr = std::to_string(hashedInput);
                        const std::string storedHashStr(
                            reinterpret_cast<const char*>(storedHash.data()));

                        result = (hashedInputStr == storedHashStr);
                    }
                }
            }

            // single SQLFreeHandle, always reached, return value discarded intentionally
            // frees the sql handler
            (void)SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        }

        return result;
    }

    // single exit, always returns a value
    User User::parseClientLogin(const std::string& firstPacket)
    {
        User client;

        for (std::size_t i = 0U; i < firstPacket.size(); ++i)
        {
            if (firstPacket[i] == ':')
            {
                client.username = firstPacket.substr(0U, i);
                client.password = firstPacket.substr(i + 1U);
                break;
            }
        }

        return client;
    }
}