#include "utils.h"
#include <iostream>
#include <conio.h>
#include <cstdlib>

/*
    MISRA _getch is not allowed for condition of a loop, so we read the first character before 
    entering the loop and then read subsequent characters at the end of the loop.This way we can
    still use _getch without violating MISRA rules.

*/ 
namespace FODServer
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

    std::string getPassword()
    {
        if (isAutomatedTestingEnabled())
        {
            return getAutomationCredential("FOD_TEST_PASSWORD", "pass@123");
        }

        std::string password;
        char ch{ 0 };

        // Read the first character before entering the loop
        ch = _getch();

        while (ch != '\r')  // Enter key
        {
            if (ch == '\b')  // Backspace
            {
                if (!password.empty())
                {
                    password.pop_back();
                    std::cout << "\b \b";  // erase last character
                }
            }
            else
            {
                password.push_back(ch);
                std::cout << '*';  // mask input
            }

            // Read the next character at the end of the loop
            ch = _getch();
        }

        std::cout << std::endl;
        return password;
    }
}