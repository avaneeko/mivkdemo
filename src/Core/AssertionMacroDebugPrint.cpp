#include "AssertionMacroDebugPrint.hpp"

#include <cstdio>

void AssertionMacro::CheckFailure(char const* Expression, char const* File, int Line, char const* Function)
{
    // UNDONE: Implement this nicer.
    fprintf(stderr, "CheckFailure %s %s:%i %s\r\n", Expression, File, Line, Function);
}