#pragma once

#if defined(_MSC_VER)
#   define SM_CC_MSVC 1
#elif defined(__clang__)
#   define SM_CC_CLANG 1
#elif defined(__GNUC__)
#   define SM_CC_GCC 1
#else
#   error "Unsupported compiler"
#endif

#if SM_CC_MSVC
#   define SM_ENSURE(...) __assume(__VA_ARGS__)
#   define SM_UNREACHABLE() __assume(0)
#elif SM_CC_CLANG || SM_CC_GCC
#   define SM_ENSURE(...) __builtin_assume(__VA_ARGS__)
#   define SM_UNREACHABLE() __builtin_unreachable()
#else
#   error "Unsupported compiler"
#endif
