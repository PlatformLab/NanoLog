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

 /**
  * This file demonstrates the usage of the NanoLog API through the
  * implementation of simple benchmarking application that reports the
  * average latency and throughput of the NanoLog system.
  */

#include <algorithm>
#include <thread>
#include <vector>
#include <xmmintrin.h>

#include <pthread.h>

// For benchmarking
#include "BenchmarkConfig.h"

#include "Cycles.h"

// Required to use the NanoLog system
#include "NanoLog.h"
static uint64_t cntr = 0;

// This function takes somewhere between 9-10 on average.
void function(uint64_t cycles) {
    cntr = cycles*2%100 + PerfUtils::Cycles::rdtsc();
}

void printRCDF(std::vector<uint64_t> nums)
{
    std::sort(nums.begin(), nums.end());

    uint64_t sum = 0;
    double boundary = 1.0e-11; // 2 decimal points into nanoseconds
    uint64_t bound = PerfUtils::Cycles::fromSeconds(boundary);
    double size = double(nums.size());

    printf("%8.2lf    %11.10lf\r\n",
            1e9*PerfUtils::Cycles::toSeconds(nums.front()),
            1.0);

    uint64_t lastPrintedIndex = 0;
    uint64_t lastPrinted = nums.front();
    for(uint64_t i = 1; i < nums.size(); ++i) {
        sum += nums[i];
        if (nums[i] - lastPrinted > bound) {
            // Before
            printf("%8.2lf    %11.10lf\r\n"
                    "%8.2lf    %11.10lf\r\n"
                    , 1e9*PerfUtils::Cycles::toSeconds(lastPrinted)
                    , 1.0 - double(lastPrintedIndex)/size
                    , 1e9*PerfUtils::Cycles::toSeconds(lastPrinted)
                    , 1.0 - double(i)/size);
            lastPrinted = nums[i];
            lastPrintedIndex = i;
        }
    }

    printf("%8.2lf    %11.10lf\r\n",
            1e9*PerfUtils::Cycles::toSeconds(nums.back()),
            1/size);

    printf("# Percentiles\r\n");
    printf("# 10th    : %lf \r\n", 1.0e9*PerfUtils::Cycles::toSeconds(nums[nums.size()/10]));
    printf("# 50th    : %lf \r\n", 1.0e9*PerfUtils::Cycles::toSeconds(nums[nums.size()/2]));
    printf("# 90th    : %lf \r\n", 1.0e9*PerfUtils::Cycles::toSeconds(nums[nums.size() - nums.size()/10]));
    printf("# 99th    : %lf \r\n", 1.0e9*PerfUtils::Cycles::toSeconds(nums[nums.size() - nums.size()/100]));
    printf("# 99.9th  : %lf \r\n", 1.0e9*PerfUtils::Cycles::toSeconds(nums[nums.size() - nums.size()/1000]));
    printf("# 99.99th : %lf \r\n", 1.0e9*PerfUtils::Cycles::toSeconds(nums[nums.size() - nums.size()/10000]));
}

void delayNanos(uint64_t nanos) {
    uint64_t stop = PerfUtils::Cycles::rdtsc() + PerfUtils::Cycles::fromNanoseconds(nanos);
    while (PerfUtils::Cycles::rdtsc() < stop) {}
}

int main(int argc, char** argv) {
    // Optional: Set the output location for the NanoLog system. By default
    // the log will be output to /tmp/compressedLog
    // NanoLog::setLogFile("/tmp/logFile");
    NanoLog::setLogFile(BENCHMARK_OUPTUT_FILE);

    // NanoLog will typically output a 3-30 byte message per log message
    // On a 200MB/s drive, that's like 100ns
    uint64_t interLogDelayNs = 100;

    uint64_t start, stop, accumulator = 0;
    uint64_t rdtscCycles;
    std::vector<uint64_t> timeDeltas;
    timeDeltas.reserve(ITERATIONS);

    start = PerfUtils::Cycles::rdtsc();
    NanoLog::preallocate();
    stop = PerfUtils::Cycles::rdtsc();
    uint64_t preallocateCycles = stop - start;

    // Measure the cost of an rdtsc() call
    start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < 1000000; ++i)
        accumulator += PerfUtils::Cycles::rdtsc();
    stop = PerfUtils::Cycles::rdtsc();
    rdtscCycles = (stop - start)/1000000;

    // Do actual measurement.
    for (int i = 0; i < ITERATIONS; ++i) {
        start = PerfUtils::Cycles::rdtsc();
        BENCH_OPS;
        stop = PerfUtils::Cycles::rdtsc();
        timeDeltas.push_back(stop - start - rdtscCycles);
        delayNanos(interLogDelayNs);
    }

    uint64_t syncStart = PerfUtils::Cycles::rdtsc();
    // Flush all pending log messages to disk
    NanoLog::sync();
    stop = PerfUtils::Cycles::rdtsc();

    double time = PerfUtils::Cycles::toSeconds(stop - syncStart);


    printf("# NanoLog Unloaded Latency test\r\n");
    printf("#\r\n");
    printf("# Number Of Operations: %lu\r\n", ITERATIONS);
    printf("# Log Message         : \"%s\"\r\n", STRINGIFY(BENCH_OPS));
    printf("# Cost of rdtsc       : %0.2lf ns\r\n", PerfUtils::Cycles::toSeconds(rdtscCycles)*1.0e9);
    printf("# Preallocate cost    : %0.2lf ns\r\n", PerfUtils::Cycles::toSeconds(preallocateCycles)*1.0e9);
    printf("# Inter-log delay     : %lu ns\r\n", interLogDelayNs);
    const char *compaction = (BENCHMARK_DISABLE_COMPACTION) ? "false" : "true";
    printf("# Compaction          : %s\r\n", compaction);
    printf("# Output File         : %s\r\n", BENCHMARK_OUPTUT_FILE);
    printf("# Flushing the log statements to disk took an additional %0.2lf secs\r\n",
            time);

    printRCDF(timeDeltas);
}

