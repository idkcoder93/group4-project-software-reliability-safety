#pragma once
#include <string>

//namespace for the whole project to comply with the guidelines and avoid name clashes with other libraries
namespace FOD {

    class User
    {
    private:
        std::string username;
        std::string password;

    public:
        //authenticates user based on the credentials
        std::string authenticateUser(const std::string& inputUsername, const std::string& inputPassword);
    };

}