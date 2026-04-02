#include "utils.h"
#include <iostream>
#include <conio.h>

/*
    MISRA _getch is not allowed for condition of a loop, so we read the first character before 
    entering the loop and then read subsequent characters at the end of the loop.This way we can
    still use _getch without violating MISRA rules.

*/ 
namespace FODServer
{
    std::string getPassword()
    {
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