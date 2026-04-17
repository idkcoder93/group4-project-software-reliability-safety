#pragma once
#include <string>

namespace FODServer
{
	std::string getPassword();
	bool isAutomatedTestingEnabled();
	std::string getAutomationCredential(const char* envName, const char* defaultValue);
}