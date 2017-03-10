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
#include <cstring>

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
 * library must encode the type in some other way (The NanoLog's
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
    if (val < (1UL << 8)) {
            numBytes = 1;
    } else if (val < (1UL << 16)) {
        numBytes = 2;
    } else if (val < (1UL << 24)) {
        numBytes = 3;
    } else if (val < (1UL << 32)) {
        numBytes = 4;
    } else if (val < (1UL << 40)) {
        numBytes = 5;
    } else if (val < (1UL << 48)) {
        numBytes = 6;
    } else if (val < (1UL << 56)) {
        numBytes = 7;
    } else {
        numBytes = 8;
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
        return pack<uint16_t>(buffer, static_cast<uint16_t>(val));
    else
        return 8 + pack<uint16_t>(buffer, static_cast<uint16_t>(-val));
}

inline int
pack(char **buffer, int32_t val)
{
    if (val >= 0 || val <= int32_t(-(1<<24)))
        return pack<uint32_t>(buffer, static_cast<uint32_t>(val));
    else
        return 8 + pack<uint32_t>(buffer, static_cast<uint32_t>(-val));
}

inline int
pack(char **buffer, int64_t val)
{
    if (val >= 0 || val <= int64_t(-(1LL<<56)))
        return pack<uint64_t>(buffer, static_cast<uint64_t>(val));
    else
        return 8 + pack<uint64_t>(buffer, static_cast<uint64_t>(-val));
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
pack(char **in, T* pointer) {
    return pack<uint64_t>(in, reinterpret_cast<uint64_t>(pointer));
}

/**
 * Floating point specialization for the pack template that will copy the value
 * without compression.
 *
 * \param[in/out] buffer
 *      char array to copy the float into and bump
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
 * Below are various unpack functions that will take in a data array pointer
 * and the special pack code, return the value originally pack()-ed and bump
 * the pointer to "consume" the pack()-ed value.
 *
 * \param in
 *      data array pointer to read the data back from and increment.
 * \param packResult
 *      special 4-bit code returned from pack()
 *
 * \return
 *      original full-width value before compression
 */

template<typename T>
inline typename std::enable_if<!std::is_floating_point<T>::value &&
                                !std::is_pointer<T>::value, T>::type
unpack(const char **in, uint8_t packResult)
{
    T packed = 0;

    if (packResult == 0)
        return packed;

    if (packResult <= 8) {
        memcpy(&packed, (*in), packResult);
        (*in) += packResult;
       return packed;
    }

    int bytes = (0x0f & (packResult - 8));
    memcpy(&packed, (*in), bytes);
    (*in) += bytes;

    return -packed;
}
template<typename T>
inline typename std::enable_if<std::is_pointer<T>::value, T>::type
unpack(const char **in, uint8_t packNibble) {
    return (T*)(unpack<uint64_t>(in, packNibble));
}

template<typename T>
inline typename std::enable_if<std::is_floating_point<T>::value, T>::type
unpack(const char **in, uint8_t packNibble) {
    if (packNibble == 0)
        return 0.0;

    T result;
    uint8_t nib = (0x0f & packNibble);
    std::memcpy(&result, (*in), nib);
    (*in) += (0x0f & nib);
    
    return result;
}
} /* BufferUtils */

#endif /* PACKER_H */

