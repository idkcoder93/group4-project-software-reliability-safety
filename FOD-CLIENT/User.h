#pragma once
#include <string>
class User
{
	private:
		std::string username;
		std::string password;

	public:

		// authenticates user based on the credentials
		std::string authenticateUser(const std::string& inputUsername, const std::string& inputPassword);

};

