#include "DBHelper.h"
#include "User.h"


#include <fstream>
#include <sstream>
#include <string>


bool User::authenticateUser(const std::string& inputUsername, const std::string& inputPassword, DBHelper& db)
{
    SQLHDBC hDbc = db.getDbc();
    if (!hDbc) return false;

    SQLHSTMT hStmt;
    if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt) != SQL_SUCCESS) return false;

    // Parameterized query is safer, but for now using string concatenation
    std::string sql = "SELECT COUNT(*) FROM [User] WHERE Username = '" + inputUsername +
        "' AND Password = '" + inputPassword + "'";

    if (SQLExecDirectA(hStmt, (SQLCHAR*)sql.c_str(), SQL_NTS) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    // Fetch result
    SQLINTEGER count = 0;
    if (SQLFetch(hStmt) == SQL_SUCCESS) {
        SQLGetData(hStmt, 1, SQL_C_SLONG, &count, 0, NULL);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

    return count > 0; // return true if user exists
}

// client sends username & password delimiter separating them
User User::parseClientLogin(const std::string& firstPacket) {
    for (size_t i = 0; i < firstPacket.size(); ++i) {
        if (firstPacket[i] == ':') {
			User client;
            username = firstPacket.substr(0, i);
            password = firstPacket.substr(i + 1);
			client.username = username;
			client.password = password;
            return client;
        }
	}
}
