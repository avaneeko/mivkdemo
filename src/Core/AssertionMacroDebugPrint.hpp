// This implementation only exists to avoid pulling the whole stdio.h header in AssertionMacros.hpp

#ifndef _ASSERTION_MACRO_DEBUG_PRINT_HPP_
#define _ASSERTION_MACRO_DEBUG_PRINT_HPP_

namespace AssertionMacro
{
    void CheckFailure(char const* Expression, char const* File, int Line, char const* Function);
};

#endif