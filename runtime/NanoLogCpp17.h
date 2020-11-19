/* Copyright (c) 2018 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef NANOLOG_CPP17_H
#define NANOLOG_CPP17_H

#include <cstdint>
#include <cstring>

#include <algorithm>
#include <iostream>
#include <utility>

#include "Common.h"
#include "Cycles.h"
#include "Packer.h"
#include "Portability.h"
#include "NanoLog.h"

/***
 * This file contains all the C++17 constexpr/templated magic that makes
 * the non-preprocessor version of NanoLog work.
 *
 * In essence, it provides 3 types of functions
 *      (1) constexpr functions to analyze the static format string and
 *          produce lookup data structures at compile-time
 *      (2) size/store functions to ascertain the size of the raw arguments
 *          and store them into a char* buffer without compression
 *      (3) compress functions to take the raw arguments from the buffers
 *          and produce a more compact encoding that's compatible with the
 *          NanoLog decompressor.
 */
namespace NanoLogInternal {

/**
 * Checks whether a character is with the terminal set of format specifier
 * characters according to the printf specification:
 * http://www.cplusplus.com/reference/cstdio/printf/
 *
 * \param c
 *      character to check
 * \return
 *      true if the character is in the set, indicating the end of the specifier
 */
constexpr inline bool
isTerminal(char c)
{
    return c == 'd' || c == 'i'
                || c == 'u' || c == 'o'
                || c == 'x' || c == 'X'
                || c == 'f' || c == 'F'
                || c == 'e' || c == 'E'
                || c == 'g' || c == 'G'
                || c == 'a' || c == 'A'
                || c == 'c' || c == 'p'
                || c == '%' || c == 's'
                || c == 'n';
}

/**
 * Checks whether a character is in the set of characters that specifies
 * a flag according to the printf specification:
 * http://www.cplusplus.com/reference/cstdio/printf/
 *
 * \param c
 *      character to check
 * \return
 *      true if the character is in the set
 */
constexpr inline bool
isFlag(char c)
{
    return c == '-' || c == '+' || c == ' ' || c == '#' || c == '0';
}

/**
 * Checks whether a character is in the set of characters that specifies
 * a length field according to the printf specification:
 * http://www.cplusplus.com/reference/cstdio/printf/
 *
 * \param c
 *      character to check
 * \return
 *      true if the character is in the set
 */
constexpr inline bool
isLength(char c)
{
    return c == 'h' || c == 'l' || c == 'j'
            || c == 'z' ||  c == 't' || c == 'L';
}

/**
 * Checks whether a character is a digit (0-9) or not.
 *
 * \param c
 *      character to check
 * \return
 *      true if the character is a digit
 */
constexpr inline bool
isDigit(char c) {
    return (c >= '0' && c <= '9');
}

/**
 * Analyzes a static printf style format string and extracts type information
 * about the p-th parameter that would be used in a corresponding NANO_LOG()
 * invocation.
 *
 * \tparam N
 *      Length of the static format string (automatically deduced)
 * \param fmt
 *      Format string to parse
 * \param paramNum
 *      p-th parameter to return type information for (starts from zero)
 * \return
 *      Returns an ParamType enum describing the type of the parameter
 */
template<int N>
constexpr inline ParamType
getParamInfo(const char (&fmt)[N],
             int paramNum=0)
{
    int pos = 0;
    while (pos < N - 1) {

        // The code below searches for something that looks like a printf
        // specifier (i.e. something that follows the format of
        // %<flags><width>.<precision><length><terminal>). We only care
        // about precision and type, so everything else is ignored.
        if (fmt[pos] != '%') {
            ++pos;
            continue;
        } else {
            // Note: gcc++ 5,6,7,8 seems to hang whenever one uses the construct
            // "if (...) {... continue; }" without an else in constexpr
            // functions. Hence, we have the code here wrapped in an else {...}
            // I reported this bug to the developers here
            // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=86767
            ++pos;

            // Two %'s in a row => Comment
            if (fmt[pos] == '%') {
                ++pos;
                continue;
            } else {

                // Consume flags
                while (NanoLogInternal::isFlag(fmt[pos]))
                    ++pos;

                // Consume width
                if (fmt[pos] == '*') {
                    if (paramNum == 0)
                        return ParamType::DYNAMIC_WIDTH;

                    --paramNum;
                    ++pos;
                } else {
                    while (NanoLogInternal::isDigit(fmt[pos]))
                        ++pos;
                }

                // Consume precision
                bool hasDynamicPrecision = false;
                int precision = -1;
                if (fmt[pos] == '.') {
                    ++pos;  // consume '.'

                    if (fmt[pos] == '*') {
                        if (paramNum == 0)
                            return ParamType::DYNAMIC_PRECISION;

                        hasDynamicPrecision = true;
                        --paramNum;
                        ++pos;
                    } else {
                        precision = 0;
                        while (NanoLogInternal::isDigit(fmt[pos])) {
                            precision = 10*precision + (fmt[pos] - '0');
                            ++pos;
                        }
                    }
                }

                // consume length
                while (isLength(fmt[pos]))
                    ++pos;

                // Consume terminal
                if (!NanoLogInternal::isTerminal(fmt[pos])) {
                    throw std::invalid_argument(
                            "Unrecognized format specifier after %");
                }

                // Fail on %n specifiers (i.e. store position to address) since
                // we cannot know the position without formatting.
                if (fmt[pos] == 'n') {
                    throw std::invalid_argument(
                            "%n specifiers are not support in NanoLog!");
                }

                if (paramNum != 0) {
                    --paramNum;
                    ++pos;
                    continue;
                } else {
                    if (fmt[pos] != 's')
                        return ParamType::NON_STRING;

                    if (hasDynamicPrecision)
                        return ParamType::STRING_WITH_DYNAMIC_PRECISION;

                    if (precision == -1)
                        return ParamType::STRING_WITH_NO_PRECISION;
                    else
                        return ParamType(precision);
                }
            }
        }
    }

    return ParamType::INVALID;
}


/**
 * Helper to analyzeFormatString. This level of indirection is needed to
 * unpack the index_sequence generated in analyzeFormatString and
 * use the sequence as indices for calling getParamInfo.
 *
 * \tparam N
 *      Length of the format string (automatically deduced)
 * \tparam Indices
 *      An index sequence from [0, N) where N is the number of parameters in
 *      the format string (automatically deduced)
 *
 * \param fmt
 *      printf format string to analyze
 *
 * \return
 *      An std::array describing the types at each index (zero based).
 */
template<int N, std::size_t... Indices>
constexpr std::array<ParamType, sizeof...(Indices)>
analyzeFormatStringHelper(const char (&fmt)[N], std::index_sequence<Indices...>)
{
    return {{ getParamInfo(fmt, Indices)... }};
}


/**
 * Computes a ParamType array describing the parameters that would be used
 * with the provided printf style format string. The indices of the array
 * correspond with the parameter position in the variable args portion of
 * the invocation.
 *
 * \template NParams
 *      The number of additional format parameters that follow the format
 *      string in a printf-like function. For example printf("%*.*d", 9, 8, 7)
 *      would have NParams = 3
 * \template N
 *      length of the printf style format string (automatically deduced)
 *
 * \param fmt
 *      Format string to generate the array for
 *
 * \return
 *      An std::array where the n-th index indicates that the
 *      n-th format parameter is a "%s" or not.
 */
template<int NParams, size_t N>
constexpr std::array<ParamType, NParams>
analyzeFormatString(const char (&fmt)[N])
{
    return analyzeFormatStringHelper(fmt, std::make_index_sequence<NParams>{});
}

/**
 * Counts the number of parameters that need to be passed in for a particular
 * printf style format string.
 *
 * One subtle point is that we are counting parameters, not specifiers, so a
 * specifier of "%*.*d" will actually count as 3 since the two '*" will result
 * in a parameter being passed in each.
 *
 * \tparam N
 *      length of the printf style format string (automatically deduced)
 *
 * \param fmt
 *      printf style format string to analyze
 *
 * @return
 */
template<int N>
constexpr inline int
countFmtParams(const char (&fmt)[N])
{
    int count = 0;
    while (getParamInfo(fmt, count) != ParamType::INVALID)
        ++count;
    return count;
}

/**
 * Counts the number of nibbles that would be needed to represent all
 * the non-string and dynamic width/precision specifiers for a given
 * printf style format string in the NanoLog system.
 *
 * \tparam N
 *      length of the printf style format string (automatically deduced)
 * \param fmt
 *      printf style format string to analyze
 *
 * \return
 *      Number of non-string specifiers in the format string
 */
template<size_t N>
constexpr int
getNumNibblesNeeded(const char (&fmt)[N])
{
    int numNibbles = 0;
    for (int i = 0; i < countFmtParams(fmt); ++i) {
        ParamType t = getParamInfo(fmt, i);
        if (t == NON_STRING || t == DYNAMIC_PRECISION || t == DYNAMIC_WIDTH)
            ++numNibbles;
    }

    return numNibbles;
}

/**
 * Stores a single printf argument into a buffer and bumps the buffer pointer.
 *
 * Non-string types are stored (full-width) and string types are stored
 * with a uint32_t header describing the string length in bytes followed
 * by the string itself with no NULL terminator.
 *
 * Note: This is the non-string specialization of the function
 * (hence the std::enable_if below), so it contains extra
 * parameters that are unused.
 *
 * \tparam T
 *      Type to store (automatically deduced)
 *
 * \param[in/out] storage
 *      Buffer to store the argument into
 * \param arg
 *      Argument to store
 * \param paramType
 *      Type information deduced from the format string about this
 *      argument (unused here)
 * \param stringSize
 *      Stores the byte length of the argument, if it is a string (unused here)
 */
template<typename T>
inline
typename std::enable_if<!std::is_same<T, const wchar_t*>::value
                        && !std::is_same<T, const char*>::value
                        && !std::is_same<T, wchar_t*>::value
                        && !std::is_same<T, char*>::value
                        , void>::type
store_argument(char **storage,
               T arg,
               ParamType paramType,
               size_t stringSize)
{
    std::memcpy(*storage, &arg, sizeof(T));
    *storage += sizeof(T);

    #ifdef ENABLE_DEBUG_PRINTING
        printf("\tRBasic  [%p]= ", dest);
        std::cout << *dest << "\r\n";
    #endif
}

// string specialization of the above
template<typename T>
inline
typename std::enable_if<std::is_same<T, const wchar_t*>::value
                        || std::is_same<T, const char*>::value
                        || std::is_same<T, wchar_t*>::value
                        || std::is_same<T, char*>::value
                        , void>::type
store_argument(char **storage,
               T arg,
               const ParamType paramType,
               const size_t stringSize)
{
    // If the printf style format string's specifier says the arg is not
    // a string, we save it as a pointer instead
    if (paramType <= ParamType::NON_STRING) {
        store_argument<const void*>(storage, static_cast<const void*>(arg),
                                    paramType, stringSize);
        return;
    }

    // Since we've already paid the cost to find the string length earlier,
    // might as well save it in the stream so that the compression function
    // can later avoid another strlen/wsclen invocation.
    if(stringSize > std::numeric_limits<uint32_t>::max())
    {
        throw std::invalid_argument("Strings larger than std::numeric_limits<uint32_t>::max() are unsupported");
    }
    auto size = static_cast<uint32_t>(stringSize);
    std::memcpy(*storage, &size, sizeof(uint32_t));
    *storage += sizeof(uint32_t);

#ifdef ENABLE_DEBUG_PRINTING
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
#pragma GCC diagnostic ignored "-Wformat"
        if (sizeof(typename std::remove_pointer<T>::type) == 1) {
            printf("\tRString[%p-%u]= %s\r\n", *buffer, size, arg);
        } else {
            printf("\tRWString[%p-%u]= %ls\r\n", *buffer, size, arg);
        }
#pragma GCC diagnostic pop
#endif

    memcpy(*storage, arg, stringSize);
    *storage += stringSize;
    return;
}

/**
 * Given a variable number of arguments to a NANO_LOG (i.e. printf-like)
 * statement, recursively unpack the arguments, store them to a buffer, and
 * bump the buffer pointer.
 *
 * \tparam argNum
 *      Internal counter indicating which parameter we're storing
 *      (aka the recursion depth).
 * \tparam N
 *      Size of the isArgString array (automatically deduced)
 * \tparam M
 *      Size of the stringSizes array (automatically deduced)
 * \tparam T1
 *      Type of the Head of the remaining variable number of arguments (deduced)
 * \tparam Ts
 *      Type of the Rest of the remaining variable number of arguments (deduced)
 *
 * \param paramTypes
 *      Type information deduced from the printf format string about the
 *      n-th argument to be processed.
 * \param[in/out] stringBytes
 *      Stores the byte length of the n-th argument, if it is a string
 *      (if not, it is undefined).
 * \param[in/out] storage
 *      Buffer to store the arguments to
 * \param head
 *      Head of the remaining number of variable arguments
 * \param rest
 *      Rest of the remaining variable number of arguments
 */
template<int argNum = 0, unsigned long N, int M, typename T1, typename... Ts>
inline void
store_arguments(const std::array<ParamType, N>& paramTypes,
                size_t (&stringBytes)[M],
                char **storage,
                T1 head,
                Ts... rest)
{
    // Peel off one argument to store, and then recursively process rest
    store_argument(storage, head, paramTypes[argNum], stringBytes[argNum]);
    store_arguments<argNum + 1>(paramTypes, stringBytes, storage, rest...);
}

/**
 * Specialization of store_arguments that processes no arguments, i.e. this
 * is the end of the head/rest recursion. See above for full documentation.
 */
template<int argNum = 0, unsigned long N, int M>
inline void
store_arguments(const std::array<ParamType, N>&,
                size_t (&stringSizes)[M],
                char **)
{
    // No arguments, do nothing.
}

/**
 * Special templated function that takes in an argument T and attempts to
 * convert it to a uint64_t. If the type T is incompatible, than a value
 * of 0 is returned.
 *
 * This function is primarily to hack around
 *
 * \tparam T
 *      Type of the input parameter (automatically deduced)
 *
 * \param t
 *      Parameter to try to convert to a uint64_t
 *
 * \return
 *      t as a uint64_t if it's convertible, otherwise a 0.
 */
template<typename T>
inline
typename std::enable_if<std::is_convertible<T, uint64_t>::value
                        && !std::is_floating_point<T>::value
                        , uint64_t>::type
as_uint64_t(T t) {
    return t;
}

template<typename T>
inline
typename std::enable_if<!std::is_convertible<T, uint64_t>::value
                        || std::is_floating_point<T>::value
                        , uint64_t>::type
as_uint64_t(T t) {
    return 0;
}


/**
 * For a single non-string, non-void pointer argument, return the number
 * of bytes needed to represent the full-width type without compression.
 *
 * \tparam T
 *      Actual type of the argument (automatically deduced)
 *
 * \param fmtType
 *      Type of the argument according to the original printf-like format
 *      string (needed to disambiguate 'const char*' types from being
 *      '%p' or '%s' and for precision info)
 * \param[in/out] previousPrecision
 *      Store the last 'precision' format specifier type encountered
 *      (as dictated by the fmtType)
 * \param stringSize
 *      Byte length of the current argument, if it is a string, else, undefined
 * \param arg
 *      Argument to compute the size for
 *
 * \return
 *      Size of the full-width argument without compression
 */
template<typename T>
inline
typename std::enable_if<!std::is_same<T, const wchar_t*>::value
                        && !std::is_same<T, const char*>::value
                        && !std::is_same<T, wchar_t*>::value
                        && !std::is_same<T, char*>::value
                        && !std::is_same<T, const void*>::value
                        && !std::is_same<T, void*>::value
                        , size_t>::type
getArgSize(const ParamType fmtType,
           uint64_t &previousPrecision,
           size_t &stringSize,
           T arg)
{
    if (fmtType == ParamType::DYNAMIC_PRECISION)
        previousPrecision = as_uint64_t(arg);

    return sizeof(T);
}

/**
 * "void *" specialization for getArgSize. (See documentation above).
 */
inline size_t
getArgSize(const ParamType,
           uint64_t &previousPrecision,
           size_t &stringSize,
           const void*)
{
    return sizeof(void*);
}

/**
 * String specialization for getArgSize. Returns the number of bytes needed
 * to represent a string (with consideration for any 'precision' specifiers
 * in the original format string and) without a NULL terminator and with a
 * uint32_t length.
 *
 * \param fmtType
 *      Type of the argument according to the original printf-like format
 *      string (needed to disambiguate 'const char*' types from being
 *      '%p' or '%s' and for precision info)
 * \param previousPrecision
 *      Store the last 'precision' format specifier type encountered
 *      (as dictated by the fmtType)
 * \param stringBytes
 *      Byte length of the current argument, if it is a string, else, undefined
 * \param str
 *      String to compute the length for
 * \return
 *      Length of the string str with a uint32_t length and no NULL terminator
 */
inline size_t
getArgSize(const ParamType fmtType,
           uint64_t &previousPrecision,
           size_t &stringBytes,
           const char* str)
{
    if (fmtType <= ParamType::NON_STRING)
        return sizeof(void*);

    stringBytes = strlen(str);
    uint32_t fmtLength = static_cast<uint32_t>(fmtType);

    // Strings with static length specifiers (ex %.10s), have non-negative
    // ParamTypes equal to the static length. Thus, we use that value to
    // truncate the string as necessary.
    if (fmtType >= ParamType::STRING && stringBytes > fmtLength)
        stringBytes = fmtLength;

    // If the string had a dynamic precision specified (i.e. %.*s), use
    // the previous parameter as the precision and truncate as necessary.
    else if (fmtType == ParamType::STRING_WITH_DYNAMIC_PRECISION &&
                stringBytes > previousPrecision)
        stringBytes = previousPrecision;

    return stringBytes + sizeof(uint32_t);
}

/**
 * Wide-character string specialization of the above.
 */
inline size_t
getArgSize(const ParamType fmtType,
            uint64_t &previousPrecision,
            size_t &stringBytes,
            const wchar_t* wstr)
{
    if (fmtType <= ParamType::NON_STRING)
        return sizeof(void*);

    stringBytes = wcslen(wstr);
    uint32_t fmtLength = static_cast<uint32_t>(fmtType);

    // Strings with static length specifiers (ex %.10s), have non-negative
    // ParamTypes equal to the static length. Thus, we use that value to
    // truncate the string as necessary.
    if (fmtType >= ParamType::STRING && stringBytes > fmtLength)
        stringBytes = fmtLength;

    // If the string had a dynamic precision specified (i.e. %.*s), use
    // the previous parameter as the precision and truncate as necessary.
    else if (fmtType == ParamType::STRING_WITH_DYNAMIC_PRECISION &&
             stringBytes > previousPrecision)
        stringBytes = previousPrecision;

    stringBytes *= sizeof(wchar_t);
    return stringBytes + sizeof(uint32_t);
}

/**
 * Given a variable number of printf arguments and type information deduced
 * from the original format string, compute the amount of space needed to
 * store all the arguments without compression.
 *
 * For the most part, all non-string arguments will be calculated as full
 * width and the all string arguments will have a 32-bit length descriptor
 * and no NULL terminator.
 *
 * \tparam argNum
 *      Internal counter for which n-th argument we're processing, aka
 *      the recursion depth.
 * \tparam N
 *      Length of argFmtTypes array (automatically deduced)
 * \tparam M
 *      Length of the stringSizes array (automatically deduced)
 * \tparam T1
 *      Type of the head of the arguments (automatically deduced)
 * \tparam Ts
 *      Types of the tail of the argument pack (automatically deduced)
 *
 * \param argFmtTypes
 *      Types of the arguments according to the original printf-like format
 *      string.
 * \param previousPrecision
 *      Internal parameter that stores the last dynamic 'precision' format
 *      argument encountered (as dictated by argFmtTypes).
 * \param[out] stringSizes
 *      Stores the lengths of string arguments without a NULL terminator
 *      and with a 32-bit length descriptor
 * \param head
 *      First of the argument pack
 * \param rest
 *      Rest of the argument pack
 * \return
 *      Total number of bytes needed to represent all arguments with no
 *      compression in the NanoLog system.
 */
template<int argNum = 0, unsigned long N, int M, typename T1, typename... Ts>
inline size_t
getArgSizes(const std::array<ParamType, N>& argFmtTypes,
            uint64_t &previousPrecision,
            size_t (&stringSizes)[M],
            T1 head, Ts... rest)
{
    return getArgSize(argFmtTypes[argNum], previousPrecision,
                                                    stringSizes[argNum], head)
           + getArgSizes<argNum + 1>(argFmtTypes, previousPrecision,
                                                    stringSizes, rest...);
}

/**
 * Specialization for getArgSizes when there are no arguments, i.e. it is
 * the end of the recursion. (See above for documentation)
 */
template<int argNum = 0, unsigned long N, int M>
inline size_t
getArgSizes(const std::array<ParamType, N>&, uint64_t &, size_t (&)[M])
{
    return 0;
}

/**
 * Takes a single argument and compresses into a format that's compatible with
 * the NanoLog Decompressor.
 *
 * \tparam T
 *      Type of the argument to compress
 *
 * \param[in/out] nibbles
 *      Preallocated location for nibbles (used for non-string type compression)
 * \param[in/out] nibbleCnt
 *      Number of nibbles used so far
 * \param paramType
 *      Type of the argument according to the original printf-like format string
 * \param stringsOnly
 *      Indicates that the compression function should store strings only
 *      False means that it will store non-string types only
 * \param[in/out] in
 *      Input buffer to read the arguments back from
 * \param[in/out out
 *      Output buffer to write the compressed results to
 */
template<typename T>
inline void
compressSingle(BufferUtils::TwoNibbles* nibbles,
                int *nibbleCnt,
                const ParamType paramType,
                bool stringsOnly,
                char **in,
                char **out)
{
    if (paramType > ParamType::NON_STRING) {
        uint32_t stringBytes;
        std::memcpy(&stringBytes, *in, sizeof(uint32_t));
        *in += sizeof(uint32_t);

        // Skipping strings
        if (!stringsOnly) {
            *in += stringBytes;
            return;
        }

#ifdef ENABLE_DEBUG_PRINTING
        printf("\tCString [%p->%p-%u]\r\n", *in, *out, stringBytes);
#endif

        memcpy(*out, *in, stringBytes);
        *in += stringBytes;
        *out += stringBytes;

        // Switch to null terminated strings in the compressed output to
        // save space. The length was explicitly encoded previously in the
        // uncompressed format to allow the two-pass compression function
        // to quickly skip strings in the stringsOnly=false pass.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpointer-arith"
        constexpr uint32_t characterWidth = [](){
            if constexpr(std::is_same_v<std::decay_t<std::remove_pointer_t<T>>, void>)
            {
                return sizeof(void *);
            }
            else
            {
                return sizeof(typename std::remove_pointer<T>::type);
            }
        }();
#pragma GCC diagnostic pop

        bzero(*out, characterWidth);
        *out += characterWidth;
        return;
    }

    // Don't store basic types if we're just processing strings
    if (stringsOnly) {
        *in += sizeof(T);
        return;
    }

    T argument;
    std::memcpy(&argument, *in, sizeof(T));

#ifdef ENABLE_DEBUG_PRINTING
    printf("\tCBasic  [%p->%p]= ", *in, *out);
    std::cout << argument << "\r\n";
#endif
    if (*nibbleCnt & 0x1)
        nibbles[*nibbleCnt/2].second = 0xf & BufferUtils::pack(out, argument);
    else
        nibbles[*nibbleCnt/2].first = 0xf & BufferUtils::pack(out, argument);

    ++(*nibbleCnt);
    *in += sizeof(T);
}

/**
 * Trickiness: There is an extra level of indirection (which will be compiled
 * out, but) required between compress_internal and compressHelper due to C++
 * disallowing partial template function specialization. More specifically, we
 * cannot have a single level of calls (i.e. compress<T, Ts..> and
 * compress<Ts..> because for a case of compress<int>, the compiler can't tell
 * whether it's suppose to call the first one with arguments <int, {}>
 * or <int> with the second.
 *
 * https://stackoverflow.com/questions/23443511/how-to-match-empty-arguments-pack-in-variadic-template
 */
template<typename... Ts>
NANOLOG_ALWAYS_INLINE
void compress_internal(BufferUtils::TwoNibbles*, int,
                       const bool*, bool, int, char **, char **);

/**
 * Recursively peels off an argument from an argument pack and compresses
 * into a format that's compatible withthe NanoLog Decompressor.
 *
 * \tparam T1
 *      Type of the head argument (i.e the current one to process)
 * \tparam Ts
 *      Type of the rest of the pack arguments
 *
 * \param[in/out] nibbles
 *      Preallocated location for nibbles (used for non-string type compression)
 * \param[in/out] nibbleCnt
 *      Number of nibbles used so far
 * \param paramType
 *      Type of the argument according to the original printf-like format string
 * \param stringsOnly
 *      Indicates that the compression function should store strings only
 *      False means that it will store non-string types only
 * \param argNum
 *      The argument number we're processing (i.e. the recursion depth)
 * \param[in/out] in
 *      Input buffer to read the arguments back from
 * \param[in/out out
 *      Output buffer to write the compressed results to
 */
template<typename T1, typename... Ts>
NANOLOG_ALWAYS_INLINE
void compressHelper(BufferUtils::TwoNibbles *nibbles,
                    int nibbleCnt,
                    const ParamType *paramTypes,
                    bool stringsOnly,
                    int argNum,
                    char **in,
                    char **out)
{
    // Peel off the first argument, and recursively process the rest
    compressSingle<T1>(nibbles, &nibbleCnt, paramTypes[argNum], stringsOnly,
                       in, out);
    compress_internal<Ts...>(nibbles, nibbleCnt, paramTypes, stringsOnly,
                                argNum + 1, in, out);
}


template<typename... Ts>
NANOLOG_ALWAYS_INLINE 
void compress_internal(BufferUtils::TwoNibbles *nibbles, int nibbleCnt,
                       const ParamType *isArgString, bool stringsOnly, int argNum,
                       char **in, char **out)
{
    compressHelper<Ts...>(nibbles, nibbleCnt, isArgString, stringsOnly,
                                argNum, in, out);
}

template<>
NANOLOG_ALWAYS_INLINE 
void compress_internal(BufferUtils::TwoNibbles *nibbles, int nibbleCnt,
                       const ParamType *isArgString, bool stringsOnly, int argNum,
                       char **in, char **out)
{
    // This is a catch for compress when the template arguments are empty,
    // in which case we do nothing. This is needed since the head/tail pack
    // expansion used above will always end with Ts = {}
}


/**
 * Consumes the raw log format parameters in the input buffer and compresses
 * them to the output buffer. Both the pointers will be modified to reflect
 * the consumption/output. The template parameters are used to specify how to
 * interpret the input buffer bytes.
 *
 * \tparam Ts
 *      Varadic template that specifies the order and types of arguments
 *      encoded in the input buffer.
 * \param numNibbles
 *      Number of nibbles required for the arguments according to the types
 *      specified in the original printf format string
 * \param paramTypes
 *      Type information deduced from the printf format string about the
 *      n-th argument to be processed.
 * \param[in/out] in
 *      Input buffer to read the arguments back from
 * \param[in/out out
 *      Output buffer to write the compressed results to
 */
template<typename... Ts>
inline void
compress(int numNibbles, const ParamType *paramTypes, char **input, char **output) {
    char *in = *input;
    char *out = *output;

    // Compress the arguments into a format that looks something like:
    // <Nibbles>
    // <Non-String types>
    // <string types with null-terminator>
    auto *nibbles = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += (numNibbles + 1)/2;

#ifdef ENABLE_DEBUG_PRINTING
    printf("\tisArgString [%p] = ", isArgString);
    for (size_t i = 0; i < sizeof...(Ts); ++i) {
        printf("%d ", isArgString[i]);
    }
    printf("\r\n");
#endif

    // This method of passing in stack-copies of the input/output pointers
    // seems to allow the compiler to generate much more optimized code.
    // The alternative of passing **input/**output directly into
    // compress_internal generate 4x more instructions on g++ 4.9.2, slowing
    // down the operation. My suspicion is that the compiler can more
    // aggressively optimize the compress_internal functions when it KNOWS
    // it has exclusive access to the indirection pointers.
    compress_internal<Ts...>(nibbles, 0, paramTypes, false, 0,  &in, &out);
    in = *input;

    // We make two passes through the arguments, once processing only the
    // non-string types and a second processing only strings. This produces
    // an encoding that keeps all the nibbles closely packed together and
    // is compatible with the legacy pre-processor based NanoLog system.
    compress_internal<Ts...>(nibbles, 0, paramTypes, true, 0,  &in, &out);
    *input = in;
    *output = out;
}

/**
 * Logs a log message in the NanoLog system given all the static and dynamic
 * information associated with the log message. This function is meant to work
 * in conjunction with the #define-d NANO_LOG() and expects the caller to
 * maintain a permanent mapping of logId to static information once it's
 * assigned by this function.
 *
 * \tparam N
 *      length of the format string (automatically deduced)
 * \tparam M
 *      length of the paramTypes array (automatically deduced)
 * \tparam Ts
 *      Types of the arguments passed in for the log (automatically deduced)
 *
 * \param logId[in/out]
 *      LogId that should be permanently associated with the static information.
 *      An input value of -1 indicates that NanoLog should persist the static
 *      log information and assign a new, globally unique identifier.
 * \param filename
 *      Name of the file containing the log invocation
 * \param linenum
 *      Line number within filename of the log invocation.
 * \param severity
 *      LogLevel severity of the log invocation
 * \param format
 *      Static printf format string associated with the log invocation
 * \param numNibbles
 *      Number of nibbles needed to store all the arguments (derived from
 *      the format string).
 * \param paramTypes
 *      An array indicating the type of the n-th format parameter associated
 *      with the format string to be processed.
 *      *** THIS VARIABLE MUST HAVE A STATIC LIFETIME AS PTRS WILL BE SAVED ***
 * \param args
 *      Argument pack for all the arguments for the log invocation
 */
template<long unsigned int N, int M, typename... Ts>
inline void
log(int &logId,
    const char *filename,
    const int linenum,
    const LogLevel severity,
    const char (&format)[M],
    const int numNibbles,
    const std::array<ParamType, N>& paramTypes,
    Ts... args)
{
    using namespace NanoLogInternal::Log;
    assert(N == static_cast<uint32_t>(sizeof...(Ts)));

    if (logId == UNASSIGNED_LOGID) {
        const ParamType *array = paramTypes.data();
        StaticLogInfo info(&compress<Ts...>,
                        filename,
                        linenum,
                        severity,
                        format,
                        sizeof...(Ts),
                        numNibbles,
                        array);

        RuntimeLogger::registerInvocationSite(info, logId);
    }

    uint64_t previousPrecision = -1;
    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    size_t stringSizes[N + 1] = {}; //HACK: Zero length arrays are not allowed
    size_t allocSize = getArgSizes(paramTypes, previousPrecision,
                            stringSizes, args...) + sizeof(UncompressedEntry);

    char *writePos = NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize);
    auto originalWritePos = writePos;

    UncompressedEntry *ue = new(writePos) UncompressedEntry();
    writePos += sizeof(UncompressedEntry);

    store_arguments(paramTypes, stringSizes, &writePos, args...);

    ue->fmtId = logId;
    ue->timestamp = timestamp;
    ue->entrySize = downCast<uint32_t>(allocSize);

#ifdef ENABLE_DEBUG_PRINTING
    printf("\r\nRecording %d:'%s' of size %u\r\n",
                        logId, info.formatString, ue->entrySize);
#endif

    assert(allocSize == downCast<uint32_t>((writePos - originalWritePos)));
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}

/**
 * No-Op function that triggers the GNU preprocessor's format checker for
 * printf format strings and argument parameters.
 *
 * \param format
 *      printf format string
 * \param ...
 *      format parameters
 */
static void
NANOLOG_PRINTF_FORMAT_ATTR(1, 2)
checkFormat(NANOLOG_PRINTF_FORMAT const char *, ...) {}


/**
 * NANO_LOG macro used for logging.
 *
 * \param severity
 *      The LogLevel of the log invocation (must be constant)
 * \param format
 *      printf-like format string (must be literal)
 * \param ...UNASSIGNED_LOGID
 *      Log arguments associated with the printf-like string.
 */
#define NANO_LOG(severity, format, ...) do { \
    constexpr int numNibbles = NanoLogInternal::getNumNibblesNeeded(format); \
    constexpr int nParams = NanoLogInternal::countFmtParams(format); \
    \
    /*** Very Important*** These must be 'static' so that we can save pointers
     * to these variables and have them persist beyond the invocation.
     * The static logId is used to forever associate this local scope (tied
     * to an expansion of #NANO_LOG) with an id and the paramTypes array is
     * used by the compression function, which is invoked in another thread
     * at a much later time. */ \
    static constexpr std::array<NanoLogInternal::ParamType, nParams> paramTypes = \
                                NanoLogInternal::analyzeFormatString<nParams>(format); \
    static int logId = NanoLogInternal::UNASSIGNED_LOGID; \
    \
    if (NanoLog::severity > NanoLog::getLogLevel()) \
        break; \
    \
    /* Triggers the GNU printf checker by passing it into a no-op function.
     * Trick: This call is surrounded by an if false so that the VA_ARGS don't
     * evaluate for cases like '++i'.*/ \
    if (false) { NanoLogInternal::checkFormat(format, ##__VA_ARGS__); } /*NOLINT(cppcoreguidelines-pro-type-vararg, hicpp-vararg)*/\
    \
    NanoLogInternal::log(logId, __FILE__, __LINE__, NanoLog::severity, format, \
                            numNibbles, paramTypes, ##__VA_ARGS__); \
} while(0)
} /* Namespace NanoLogInternal */

#endif //NANOLOG_CPP17_H
