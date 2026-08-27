#pragma once

#if defined(_MSC_VER)
#define JDK_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define JDK_RESTRICT __restrict__
#else
#define JDK_RESTRICT
#endif