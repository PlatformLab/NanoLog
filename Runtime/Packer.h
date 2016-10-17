/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */


/* Includes for types */
#include <cstddef>
#include <stdint.h>

#include <string.h>     /* strlen + memcpy*/
#include <fstream>          // std::ifstream

#include "Cycles.h"

#ifndef PACKER_H
#define PACKER_H

namespace BufferUtils {

/**
 * Below is a collection of pack functions that will attempt to pack a primitive
 * type into an array and return a special code that is to be used to unpack
 * the argument.
 *
 * For documentation sake,
 * Argument sizes is a set of nibbles (4-bits sets) that specify how many
 * bytes are to be used for interpreting an argument. Nibbles are omitted
 * for string arguments and require the presence of a null character to
 * determine size. The values of the nibbles are as follows;
 *      (a) From 0 to sizeof(T) -> That many bytes were used to represent
 *          the integral
 *      (b) From 8 to 8 + sizeof(T) - 1 -> That many - 9 bytes were used
 *          to represent a negated integral. Note that it's not worth having
 *          a representation of a negated 8byte number because compaction-wise
 *          it saves us no additional space.
 *
 */

// For a given unsigned integral ,this function will return the
// smallest number of bytes that can be used to represent the integral.
template<typename T>
inline typename std::enable_if<std::is_integral<T>::value &&
                                !std::is_signed<T>::value, int>::type
pack(char **ptr, T val) {
    // Binary search for the smallest container. It is also worth noting that
    // with -O3, the compiler will strip out extraneous if-statements based on T
    // For example, if T is uint16_t, it would only leave the t < 1U<<8 check
    int numBytes;
    if (val < (1ULL << 32))
    {
        if (val < (1U << 24))
        {
            if (val < (1U << 16))
            {
                if (val < (1U << 8))
                    numBytes = 1;
                else
                    numBytes = 2;
            }
            else numBytes = 3;
        }
        else numBytes = 4;
    }
    else
    {
        if (val < (1ULL << 56))
        {
            if (val < (1ULL << 48))
            {
                if (val < (1ULL << 40))
                    numBytes = 5;
                else
                    numBytes = 6;
            }
            else numBytes = 7;
        }
        else numBytes = 8;
    }

    *reinterpret_cast<T*>(*ptr) = val;
    *ptr += numBytes;

    return numBytes;
}

inline int
pack(char **ptr, int8_t val)
{
    **ptr = val;
    (*ptr)++;
    return 1;
}

inline int
pack(char **ptr, int16_t val)
{
    if (val >= 0 || val <= int16_t(-(1<<8)))
        return pack(ptr, static_cast<uint16_t>(val));
    else
        return 8 + pack(ptr, static_cast<uint16_t>(-val));
}

inline int
pack(char **ptr, int32_t val)
{
    if (val >= 0 || val <= int32_t(-(1<<24)))
        return pack(ptr, static_cast<uint32_t>(val));
    else
        return 8 + pack(ptr, static_cast<uint32_t>(-val));
}

inline int
pack(char **ptr, int64_t val)
{
    if (val >= 0 || val <= int64_t(-(1LL<<56)))
        return pack(ptr, static_cast<uint64_t>(val));
    else
        return 8 + pack(ptr, static_cast<uint64_t>(-val));
}

template<typename T>
inline typename std::enable_if<!std::is_floating_point<T>::value, T>::type
unpack(char **ptr, uint8_t packResult)
{
    T res = 0;
    if (packResult == 0)
        return res;

    if (packResult <= 8) {
        res = *reinterpret_cast<T*>(*ptr);
        uint64_t mask = (packResult == 8) ? -1 : (1ULL<<(8*packResult)) - 1;
        res &= mask;
        *ptr += packResult;
        return res;
    }

    int bytes = packResult - 8;
    res = *reinterpret_cast<T*>(*ptr);
    res &= ((1LL<<(8*bytes)) - 1);
    *ptr += bytes;
    return -res;
}

template<typename T>
inline typename std::enable_if<!std::is_floating_point<T>::value, T>::type
unpack(std::ifstream &in, uint8_t packResult)
{
    T packed = 0;

    if (packResult == 0)
        return packed;

    if (packResult <= 8) {
       in.read(reinterpret_cast<char*>(&packed), packResult);
       return packed;
    }

    int bytes = packResult - 8;
    in.read(reinterpret_cast<char*>(&packed), bytes);
    return -packed;
}

// Pointer specialization
template<typename T>
inline int
pack(char **ptr, T* pointer) {
    return pack<uint64_t>(ptr, reinterpret_cast<uint64_t>(pointer));
}

template<typename T>
inline T*
unpackPointer(char **ptr, uint8_t packResult) {
    return (T*)(unpack<uint64_t>(ptr, packResult));
}

template<typename T>
inline T*
unpackPointer(std::ifstream &in, uint8_t packResult) {
    return (T*)(unpack<uint64_t>(in, packResult));
}


// Floating point specialization
template<typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, int>::type
pack(char **ptr, T val) {
    *((T*)*ptr) = val;
    *ptr += sizeof(T);
    return sizeof(T);
}

template<typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, T>::type
unpack(char **ptr, uint8_t packResult) {
    if (packResult == 0)
        return 0.0;

    T res = *reinterpret_cast<T*>(*ptr);
    *ptr += packResult;
    return res;
}

template<typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, T>::type
unpack(std::ifstream &in, uint8_t packResult) {
    if (packResult == 0)
        return 0.0;

    T res = 0.0;
    in.read(reinterpret_cast<char*>(&res), packResult);
    
    return res;
}

} /* BufferUtils */


#endif /* PACKER_H */

