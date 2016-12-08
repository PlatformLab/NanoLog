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
  * This file demonstrates the usage of the FastLogger API through the
  * implementation of simple benchmarking application that reports the
  * average latency and throughput of the FastLogger system.
  */

#include "Cycles.h"

// Required to use the FastLogger system
#include "FastLogger.h"

int main(int argc, char** argv) {
    // Number of messages to log repeatedly and take the average latency
    const uint64_t RECORDS = 100000000;

    uint64_t start, stop;
    double time;

    // Optional optimization: pre-allocates thread-local data structures
    // needed by FastLogger. This should be invoked once per new
    // thread that will use the FastLogger system.
    PerfUtils::FastLogger::preallocate();

    start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < RECORDS; ++i)
        FAST_LOG("Simple log message with no parameters");
    stop = PerfUtils::Cycles::rdtsc();

    time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("The total time spent invoking FAST_LOG with no parameters %lu "
            "times took %0.2lf seconds (%0.2lf ns/message average)\r\n",
            RECORDS, time, (time/RECORDS)*1e9);

    start = PerfUtils::Cycles::rdtsc();
    // Flush all pending log messages to disk
    PerfUtils::FastLogger::sync();
    stop = PerfUtils::Cycles::rdtsc();

    time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Flushing the log statements to disk took an additional %0.2lf secs\r\n",
            time);

    // Prints various statistics gathered by the FastLogger system to stdout
    PerfUtils::FastLogger::printStats();
}

