/* Copyright (c) 2012 Stanford University
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

#ifndef PERFUTILS_ATOMIC_H
#define PERFUTILS_ATOMIC_H

#include <stdint.h>

namespace PerfUtils {
/**
 * The following classes allows us to increment pointers in units of the
 * referent object size for Atomics that are pointers, and in units of
 * 1 for integer types.
 */
template<typename T>
struct AtomicStride {
    /// How much to add to the value when it is "incremented".
    static const unsigned int unitSize = 1;
};
template<typename T>
struct AtomicStride<T*> {
    /// How much to add to the value when it is "incremented".
    static const unsigned int unitSize = sizeof(T);
};

/**
 * This class implements atomic operations that are safe for inter-thread
 * synchronization.  It supports values of any type, as long as they are
 * either 4 or 8 bytes in length.  Note: this class does not deal with
 * instruction reordering issues; it only guarantees the atomicity of
 * operations.  Proper synchronization also requires the use of facilities
 * such as those provided by the \c Fence class.
 *
 * As of 6/2011 this class is significantly faster than the C++ atomic_int
 * class, because the C++ facilities incorporate expensive fence operations,
 * which often are not necessary.
 */
template<typename ValueType>
class Atomic {
    static_assert(sizeof(ValueType) == 4 ||
                  sizeof(ValueType) == 8,
                  "Atomic only works on 4- and 8-byte values!");
  public:
    /**
     * Construct an Atomic.
     *
     * \param value
     *      Initial value.
     */
    explicit Atomic(const ValueType value = 0) : value(value) { }

    /**
     * Atomically increment the value by a given amount.
     *
     * \param increment
     *      How much to add to the value. If the value is a pointer type,
     *      the actual increment is this value multiplied by the size of
     *      the reference objects. I.e. this produces the same effect as
     *      the C++ statement "value += increment;".
     */
    void add(int64_t increment)
    {
        if (sizeof(value) == 8) {
            __asm__ __volatile__("lock; addq %1,%0" : "=m" (value) :
                    "r" (increment*AtomicStride<ValueType>::unitSize));
        } else {
            __asm__ __volatile__("lock; addl %1,%0" : "=m" (value) :
                    "r" (static_cast<int>(increment)*
                         AtomicStride<ValueType>::unitSize));
        }
    }

    /**
     * Atomically compare the value with a test value and, if they match,
     * replace the value with a new value.
     *
     * \param test
     *      Replace the value only if its current value equals this.
     * \param newValue
     *      This value will replace the current value.
     * \result
     *      The previous value.
     */
    ValueType compareExchange(ValueType test, ValueType newValue)
    {
        if (sizeof(value) == 8) {
            __asm__ __volatile__("lock; cmpxchgq %0,%1" : "=r" (newValue),
                    "=m" (value), "=a" (test) : "0" (newValue), "2" (test));
        } else {
            __asm__ __volatile__("lock; cmpxchgl %0,%1" : "=r" (newValue),
                    "=m" (value), "=a" (test) : "0" (newValue), "2" (test));
        }
        return test;
    }

    /**
     * Atomically replace the value while returning its old value.
     *
     * \param newValue
     *      This value will replace the current value.
     * \result
     *      The previous value.
     */
    ValueType exchange(ValueType newValue)
    {
        if (sizeof(value) == 8) {
            __asm__ __volatile__("xchgq %0,%1" : "=r" (newValue), "=m" (value) :
                    "0" (newValue));
        } else {
            __asm__ __volatile__("xchgl %0,%1" : "=r" (newValue), "=m" (value) :
                    "0" (newValue));
        }
        return newValue;
    }

    /**
     * Atomically increment the current value (for pointer types the
     * increment is the size of the referent type; otherwise the increment
     * is 1).
     */
    void inc()
    {
        if (AtomicStride<ValueType>::unitSize == 1) {
            if (sizeof(value) == 8) {
                __asm__ __volatile__("lock; incq %0" : "=m" (value));
            } else {
                __asm__ __volatile__("lock; incl %0" : "=m" (value));
            }
        } else {
            add(1);
        }
    }

    /**
     * Return the current value.
     */
    ValueType load()
    {
        return value;
    }

    /**
     * Assign to an Atomic.
     *
     * \param newValue
     *      This value will replace the current value.
     * \return
     *      The new value.
     */
    Atomic<ValueType>& operator=(ValueType newValue)
    {
        store(newValue);
        return *this;
    }

    /**
     * Return the current value.
     */
    operator ValueType()
    {
        return load();
    }

    /**
     * Atomically increment the current value (for pointer types the
     * increment is the size of the referent type; otherwise the increment
     * is 1).
     */
    Atomic<ValueType>& operator++()
    {
        inc();
        return *this;
    }
    Atomic<ValueType> operator++(int)              // NOLINT
    {
        inc();
        return *this;
    }

    /**
     * Atomically decrement the current value (for pointer types the
     * decrement is the size of the referent type; otherwise the decrement
     * is 1).
     */
    Atomic<ValueType>& operator--()
    {
        add(-1);
        return *this;
    }
    Atomic<ValueType> operator--(int)              // NOLINT
    {
        add(-1);
        return *this;
    }

    /**
     * Set the value.
     *
     * \param newValue
     *      This value will replace the current value.
     */
    void store(ValueType newValue)
    {
        value = newValue;
    }

  protected:
    // The value on which the atomic operations operate.
    volatile ValueType value;
};

} // end RAMCloud

#endif  // PERFUTILS_ATOMIC_H
