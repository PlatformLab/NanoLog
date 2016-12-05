/* Copyright (c) 2016 Stanford University
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

#include <cstddef>
#include <fstream>          // std::ifstream

#include <stdint.h>

#ifndef PACKER_H
#define PACKER_H

/**
 * This file contains a collection of pack/unpack functions that are used
 * by the LogCompressor/Decompressor to find the smallest representation
 * possible for various primitive types and pack/unpack them into a char
 * array. The current implementation of the pack/unpack functions requires
 * the passing of a special code that identifies how the primitive was
 * packed.
 *
 * The special code that is returned is a nibble (4-bits sets) that specify
 * how many bytes are to be used for interpreting an argument. Nibbles are
 * omitted for string arguments and require the presence of a null character to
 * determine size. The values of the nibbles are as follows:
 *      (a) From 0 to sizeof(T) -> That many bytes were used to represent
 *          the integer
 *      (b) From 8 to 8 + sizeof(T) - 1 -> That many - 9 bytes were used
 *          to represent a negated integer. Note that it's not worth having
 *          a representation of a negated 8byte number because compaction-wise
 *          it saves no additional space.
 *
 * Note however, that the special code only indicates how many bytes and
 * whether a negation occurred for the packed primitive. It does not encode
 * a type and it's also impossible for the C++ type system to infer the
 * type from the ifstream the unpack() function takes. Thus the user of this
 * library must encode the type in some other way (The FastLogger's
 * LogDecompressor achieves this by encoding the type in generated source code).
 *
 * IMPORTANT NOTE: These compression schemes only work on little-endian machines
 */

namespace BufferUtils {


/**
 * Packs two 4-bit nibbles into one byte. This is used to pack the special
 * codes returned by pack() in the compressed log.
 */
struct TwoNibbles {
    uint8_t first:4;
    uint8_t second:4;
} __attribute__((packed));


/**
 * Given an unsigned integer and a char array, find the fewest number of
 * bytes needed to represent the integer, copy that many bytes into the
 * char array, and bump the char array pointer.
 *
 * \param[in/out] buffer
 *      char array pointer used to store the compressed value and bump
 * \param val
 *      Unsigned integer to pack into the buffer
 *
 * \return
 *      Special 4-bit value indicating how the primitive was packed
 */
template<typename T>
inline typename std::enable_if<std::is_integral<T>::value &&
                                !std::is_signed<T>::value, int>::type
pack(char **buffer, T val) {
    // Binary search for the smallest container. It is also worth noting that
    // with -O3, the compiler will strip out extraneous if-statements based on T
    // For example, if T is uint16_t, it would only leave the t < 1U<<8 check

    //TODO(syang0) Is this too costly vs. a simple for loop?
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

    // Although we store the entire value here, we take advantage of the fact
    // that x86-64 is little-endian (storing the least significant bits first)
    // and lop off the rest by only partially incrementing the buffer pointer
    *reinterpret_cast<T*>(*buffer) = val;
    *buffer += numBytes;

    return numBytes;
}

/**
 * Below are a series of pack functions that take in a signed integer,
 * test to see if the value will be smaller if negated, and then invoke
 * the unsigned version of the pack() function above.
 *
 * \param[in/out] buffer
 *      char array to copy the value into and bump
 * \param val
 *      Unsigned integer to pack into the buffer
 *
 * \return
 *      Special 4-bit value indicating how the primitive was packed
 */
inline int
pack(char **buffer, int8_t val)
{
    **buffer = val;
    (*buffer)++;
    return 1;
}

inline int
pack(char **buffer, int16_t val)
{
    if (val >= 0 || val <= int16_t(-(1<<8)))
        return pack(buffer, static_cast<uint16_t>(val));
    else
        return 8 + pack(buffer, static_cast<uint16_t>(-val));
}

inline int
pack(char **buffer, int32_t val)
{
    if (val >= 0 || val <= int32_t(-(1<<24)))
        return pack(buffer, static_cast<uint32_t>(val));
    else
        return 8 + pack(buffer, static_cast<uint32_t>(-val));
}

inline int
pack(char **buffer, int64_t val)
{
    if (val >= 0 || val <= int64_t(-(1LL<<56)))
        return pack(buffer, static_cast<uint64_t>(val));
    else
        return 8 + pack(buffer, static_cast<uint64_t>(-val));
}

/**
 * Pointer specialization for the pack template that will copy the value
 * without compression.
 *
 * \param[in/out] buffer
 *      char array to copy the integer into and bump
 * \param val
 *      Unsigned integer to pack into the buffer
 *
 * \return - Special 4-bit value indicating how the primitive was packed
 */
template<typename T>
inline int
pack(char **buffer, T* pointer) {
    // TODO(syang0) Implement and benchmark an alterative where we take
    // differences in pointer values and pack that.
    return pack<uint64_t>(buffer, reinterpret_cast<uint64_t>(pointer));
}

/**
 * Floating point specialization for the pack template that will copy the value
 * without compression.
 *
 * \param buffer
 *      char array to copy the integer into and bump
 * \param val
 *      Unsigned integer to pack into the buffer
 *
 * \return - Special 4-bit value indicating how the primitive was packed
 */
template<typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, int>::type
pack(char **buffer, T val) {
    *((T*)*buffer) = val;
    *buffer += sizeof(T);
    return sizeof(T);
}

/**
 * Below are various unpack functions that will take an input stream and
 * the special code and return the original value.
 *
 * \param in
 *      std::ifstream to read the data from
 * \param packResult
 *      special 4-bit code returned from pack()
 *
 * \return
 *      original full-width value before compression
 */

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
template<typename T>
inline T*
unpackPointer(std::ifstream &in, uint8_t packResult) {
    return (T*)(unpack<uint64_t>(in, packResult));
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

