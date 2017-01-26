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
  * This file demonstrates the usage of the FastLogger API through the
  * implementation of simple benchmarking application that reports the
  * average latency and throughput of the FastLogger system.
  */
#include <algorithm>
#include <thread>
#include <vector>
#include <xmmintrin.h>

#include <pthread.h>

// For benchmarking
#include "BenchmarkConfig.h"

#include "Cycles.h"

// Required to use the FastLogger system
#include "FastLogger.h"
static uint64_t cntr = 0;

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


uint64_t interference() {
    uint64_t scratchSize = 1<<26;
    char *scratch = static_cast<char*>(malloc(scratchSize));
    if (!scratch) {
        printf("Could not allocate size of %d bytes\r\n", scratchSize);
        exit(-1);
    }

    uint64_t sum = 1;
    for (int i = 0; i < scratchSize; i += 100)
        sum += scratch[i];

    free(scratch);
    return sum;
}

void runBenchmark(int id, pthread_barrier_t *barrier)
{
    // Number of messages to log repeatedly and take the average latency
    uint64_t start, stop;
    double time;

    std::vector<uint64_t> times;
    times.reserve(ITERATIONS);



//    PerfUtils::Util::pinThreadToCore(id);

    // Optional optimization: pre-allocates thread-local data structures
    // needed by FastLogger. This should be invoked once per new
    // thread that will use the FastLogger system.
    PerfUtils::TimeTrace::record("Thread[%d]: Preallocation", id);
    PerfUtils::TimeTrace::record("Thread[%d]: Preallocation", id);
    PerfUtils::FastLogger::preallocate();
    PerfUtils::TimeTrace::record("Thread[%d]: Preallocation Done", id);

    PerfUtils::TimeTrace::record("Thread[%d]: Waiting for barrier...", id);
    pthread_barrier_wait(barrier);

    PerfUtils::TimeTrace::record("Thread[%d]: Starting benchmark", id);

    uint64_t interferenceSum = 0;
    start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t start, stop;

        interferenceSum += interference();
        start = PerfUtils::Cycles::rdtsc();
        BENCH_OPS
        stop = PerfUtils::Cycles::rdtsc();
        times.push_back(stop - start);
    }

    stop = PerfUtils::Cycles::rdtsc();
    PerfUtils::TimeTrace::record("Thread[%d]: Benchmark Done", id);

    time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Thread[%d]: Ran %d iterations which took %0.2lf seconds\r\n",
            id, ITERATIONS, time);

    printf("Random number on the house: %lu\r\n", interferenceSum);

    printRCDF(times);


        // Break abstraction to bring metrics on cycles blocked.
    uint32_t nBlocks = PerfUtils::FastLogger::stagingBuffer->numTimesProducerBlocked;
    double timeBlocked = 1e9*PerfUtils::Cycles::toSeconds(
                PerfUtils::FastLogger::stagingBuffer->cyclesProducerBlocked);
    printf("Thread[%d]: Time producer was stuck for = %0.2e ns (avg: %0.2e ns cnt: %lu)\r\n",
            id,
            timeBlocked,
            timeBlocked/nBlocks,
            nBlocks);
}

int main(int argc, char** argv) {
    // Optional: Set the output location for the FastLogger system. By default
    // the log will be output to /tmp/compressedLog
    PerfUtils::FastLogger::setLogFile("/tmp/logFile");

    printf("BENCH_OP = '" STRINGIFY(BENCH_OPS) "'\r\n");

    pthread_barrier_t barrier;
    if (pthread_barrier_init(&barrier, NULL, BENCHMARK_THREADS)) {
        printf("pthread error\r\n");
    }

    std::vector<std::thread> threads;
    threads.reserve(BENCHMARK_THREADS);
    for (int i = 1; i < BENCHMARK_THREADS; ++i)
        threads.emplace_back(runBenchmark, i, &barrier);

    runBenchmark(0, &barrier);

    for (int i = 0; i < threads.size(); ++i)
        if (threads[i].joinable())
            threads.at(i).join();

    uint64_t start = PerfUtils::Cycles::rdtsc();
    // Flush all pending log messages to disk
    PerfUtils::FastLogger::sync();
    uint64_t stop = PerfUtils::Cycles::rdtsc();

    uint64_t time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Flushing the log statements to disk took an additional %0.2lf secs\r\n",
            time);

    // Prints various statistics gathered by the FastLogger system to stdout
}

