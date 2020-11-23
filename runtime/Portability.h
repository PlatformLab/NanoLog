#ifndef NANOLOG_PORTABILITY_H
#define NANOLOG_PORTABILITY_H

#ifdef _MSC_VER
#define NANOLOG_ALWAYS_INLINE __forceinline
#elif defined(__GNUC__)
#define NANOLOG_ALWAYS_INLINE inline __attribute__((__always_inline__))
#else
#define NANOLOG_ALWAYS_INLINE inline
#endif

#ifdef _MSC_VER
#define NANOLOG_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
#define NANOLOG_NOINLINE __attribute__((__noinline__))
#else
#define NANOLOG_NOINLINE
#endif

#ifdef _MSC_VER
#define NANOLOG_PACK_PUSH __pragma(pack(push, 1))
#define NANOLOG_PACK_POP __pragma(pack(pop))
#elif defined(__GNUC__)
#define NANOLOG_PACK_PUSH _Pragma("pack(push, 1)")
#define NANOLOG_PACK_POP _Pragma("pack(pop)")
#else
#define NANOLOG_PACK_PUSH
#define NANOLOG_PACK_POP
#endif

#if _MSC_VER

#ifdef _USE_ATTRIBUTES_FOR_SAL
#undef _USE_ATTRIBUTES_FOR_SAL
#endif

#define _USE_ATTRIBUTES_FOR_SAL 1
#include <sal.h>

#define NANOLOG_PRINTF_FORMAT _Printf_format_string_
#define NANOLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check)

#elif defined(__GNUC__)
#define NANOLOG_PRINTF_FORMAT
#define NANOLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check) \
  __attribute__((__format__(__printf__, string_index, first_to_check)))
#else
#define NANOLOG_PRINTF_FORMAT
#define NANOLOG_PRINTF_FORMAT_ATTR(string_index, first_to_check)
#endif

#endif /* NANOLOG_PORTABILITY_H */