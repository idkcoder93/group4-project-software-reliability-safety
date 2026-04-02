#include "User.h"

#include <fstream>
#include <sstream>
#include <string>

std::string FOD::User::authenticateUser(const std::string& inputUsername, const std::string& inputPassword)
{
	return inputUsername + ":" + inputPassword;
}