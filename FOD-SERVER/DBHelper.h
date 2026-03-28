#pragma once
#include "FODHeader.h"
#include "FODDescription.h"
#include <windows.h>
#include <sqlext.h>
#include <string>
#include <iostream>

class DBHelper {
private:
    SQLHENV hEnv = nullptr;
    SQLHDBC hDbc = nullptr;

public:

    // Open connection
    bool openConnection(const std::string& connStr);

    // Close connection
    void closeConnection();

    // Save a FOD object
    bool saveFOD(const FODHeader& record, const FODDescription& desc);

    // Getter for connection handle (optional)
    SQLHDBC getDbc() const { return hDbc; }
};

