#include "AssertionMacroDebugPrint.hpp"

#include <cstdio>

void AssertionMacro::CheckFailure(char const* Expression, char const* File, int Line, char const* Function)
{
    char Message[1024];
    int const Length = _snprintf(Message, sizeof(Message),
        "Assertion failed: %s [File:%s] [Line: %i] [Function: %s]\r\n",
        Expression, File, Line, Function);

    // _snprintf does not null-terminate on truncation
    if (Length < 0 || static_cast<size_t>(Length) >= sizeof(Message))
    {
        Message[sizeof(Message) - 1] = '\0';
    }

    fputs(Message, stderr);
    fflush(stderr);
}
