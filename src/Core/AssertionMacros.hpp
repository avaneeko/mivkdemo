#ifndef _ASSERTION_MACROS_HPP_
#define _ASSERTION_MACROS_HPP_

#include "AssertionMacroDebugPrint.hpp"

#ifndef _MACRO_DEBUG_BREAK
    #ifdef _MSC_VER
    #define _MACRO_DEBUG_BREAK __debugbreak();
    #else
    #define _MACRO_DEBUG_BREAK __builtin_trap();
    #endif
#endif

#ifndef CHECK_IMPL
#define CHECK_IMPL(expr) \
    do \
    { \
        if (not (expr)) [[unlikely]] \
        { \
            AssertionMacro::CheckFailure(#expr, __FILE__, __LINE__, __func__); \
            _MACRO_DEBUG_BREAK \
        } \
    } while (0)
#endif

#ifndef check
    #ifdef NDEBUG
    #define check(expr) ((void)0)
    #else
    #define check(expr) CHECK_IMPL(expr)
    #endif
#endif

#ifndef debug_break
#define debug_break(expr) _MACRO_DEBUG_BREAK
#endif

#endif