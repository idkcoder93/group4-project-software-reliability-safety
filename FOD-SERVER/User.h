#pragma once
#include "DBHelper.h"
#include <string>

namespace FODServer
{
	class User
	{
	private:
		std::string username;
		std::string password;

	public:

		// Getter
		const std::string& getUsername() const { return username; }

		// Setter
		void setUsername(const std::string& newUsername) { username = newUsername; }

		void setPassword(const std::string& newPassword) { password = newPassword; }

		// authenticates user based on the credentials
		bool authenticateUser(const std::string& inputUsername, const std::string& inputPassword, DBHelper& db);

		// authenicate client
		User parseClientLogin(const std::string& firstPacket);
	};
}
