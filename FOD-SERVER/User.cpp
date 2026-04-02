#include "DBHelper.h"
#include "User.h"

#include <functional>
#include <fstream>
#include <sstream>
#include <string>

bool FODServer::User::authenticateUser(const std::string& inputUsername,
    const std::string& inputPassword,
    DBHelper& db)
{
    SQLHDBC hDbc = db.getDbc();
    if (!hDbc) {
        return false;
    }

    SQLHSTMT hStmt;
    if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc, &hStmt) != SQL_SUCCESS) {
        return false;
    }

    // type definition for preparing SQL 
    SQLCHAR sql [] = "SELECT Password FROM [User] WHERE Username = ?";

    SQLPrepareA(hStmt, sql, SQL_NTS);
    SQLBindParameter(hStmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
        255, 0, (SQLPOINTER)inputUsername.c_str(), 0, NULL);

    if (SQLExecute(hStmt) != SQL_SUCCESS) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
        return false;
    }

    char storedHash[256] = { 0 };

    if (SQLFetch(hStmt) == SQL_SUCCESS) {
        SQLGetData(hStmt, 1, SQL_C_CHAR, storedHash, sizeof(storedHash), NULL);
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt);

        // hash input password
        std::hash<std::string> hasher;
        size_t hashedInput = hasher(inputPassword);

        // convert to string for comparison
        std::string hashedInputStr = std::to_string(hashedInput);

        return hashedInputStr == storedHash;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hStmt);
    return false;
}

// client sends username & password delimiter separating them
FODServer::User FODServer::User::parseClientLogin(const std::string& firstPacket) {
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

