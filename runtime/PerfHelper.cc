/* Copyright (c) 2016-2017 Stanford University
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

#include <cstdarg>
#include <stdint.h>

/**
 * This file is the complement to PerfHelper.h, which contain various
 * functions/variables needed to support the operations benchmarked by
 * Perf.cc
 */

namespace PerfHelper {

/// Flush the CPU data cache by reading and writing 100MB of new data.
void
flushCache()
{
    int hundredMegs = 100 * 1024 * 1024;
    volatile char* block = new char[hundredMegs];
    for (int i = 0; i < hundredMegs; i++)
        block[i] = 1;
    delete[] block;
}

/// Used in functionCall().
uint64_t
plusOne(uint64_t x)
{
    return x + 1;
}

// See documentation in PerfHelper.h
int sum4(int a, int b, int c, int d) {
    return a + b + c + d;
}

// See documentation in PerfHelper.h
int va_argSum(int count, ...)
{
    int result = 0;
    va_list args;
    va_start(args, count);
    for (int i = 0; i < count; ++i) {
        result += va_arg(args, int);
    }
    va_end(args);
    return result;
}

} // PerfHelper namespace
