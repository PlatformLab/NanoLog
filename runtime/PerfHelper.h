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

/**
 * This file contains functions/variables needed to support the operations
 * benchmarked by Perf.cc
 */

#ifndef PERFHELPER_H
#define PERFHELPER_H

#include <stdint.h>

#include "stdio.h"

namespace PerfHelper {

void flushCache();
uint64_t plusOne(uint64_t x) __attribute__ ((noinline));

// Simple function that performs no computation
template<int addTo>
int
emptyFunction() {
    return addTo;
}

// Used to approximate the cost to dereference to a function array
static int (*functionArray[50])() {
    emptyFunction<0>,
    emptyFunction<1>,
    emptyFunction<2>,
    emptyFunction<3>,
    emptyFunction<4>,
    emptyFunction<5>,
    emptyFunction<6>,
    emptyFunction<7>,
    emptyFunction<8>,
    emptyFunction<9>,
    emptyFunction<10>,
    emptyFunction<11>,
    emptyFunction<12>,
    emptyFunction<13>,
    emptyFunction<14>,
    emptyFunction<15>,
    emptyFunction<16>,
    emptyFunction<17>,
    emptyFunction<18>,
    emptyFunction<19>,
    emptyFunction<20>,
    emptyFunction<21>,
    emptyFunction<22>,
    emptyFunction<23>,
    emptyFunction<24>,
    emptyFunction<25>,
    emptyFunction<26>,
    emptyFunction<27>,
    emptyFunction<28>,
    emptyFunction<29>,
    emptyFunction<30>,
    emptyFunction<31>,
    emptyFunction<32>,
    emptyFunction<33>,
    emptyFunction<34>,
    emptyFunction<35>,
    emptyFunction<36>,
    emptyFunction<37>,
    emptyFunction<38>,
    emptyFunction<39>,
    emptyFunction<40>,
    emptyFunction<41>,
    emptyFunction<42>,
    emptyFunction<43>,
    emptyFunction<44>,
    emptyFunction<45>,
    emptyFunction<46>,
    emptyFunction<47>,
    emptyFunction<48>,
    emptyFunction<49>
};


/*
 * This function just discards its argument. It's used to make it
 * appear that data is used,  so that the compiler won't optimize
 * away the code we're trying to measure.
 *
 * \param value
 *      Pointer to arbitrary value; it's discarded.
 */
void discard(void* value) {
    int x = *reinterpret_cast<int*>(value);
    if (x == 0x43924776) {
        printf("Value was 0x%x\n", x);
    }
}
} // PerfHelper

#endif  // PERFHELPER_H
