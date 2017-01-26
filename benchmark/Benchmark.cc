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

void runBenchmark(int id, pthread_barrier_t *barrier)
{
    // Number of messages to log repeatedly and take the average latency
    uint64_t start, stop;
    double time;

    // PerfUtils::Util::pinThreadToCore(1);

    // Optional optimization: pre-allocates thread-local data structures
    // needed by NanoLog. This should be invoked once per new
    // thread that will use the NanoLog system.
    PerfUtils::TimeTrace::record("Thread[%d]: Preallocation", id);
    PerfUtils::TimeTrace::record("Thread[%d]: Preallocation", id);
    NanoLog::preallocate();
    PerfUtils::TimeTrace::record("Thread[%d]: Preallocation Done", id);

    PerfUtils::TimeTrace::record("Thread[%d]: Waiting for barrier...", id);
    pthread_barrier_wait(barrier);

    PerfUtils::TimeTrace::record("Thread[%d]: Starting benchmark", id);
    start = PerfUtils::Cycles::rdtsc();

    for (int i = 0; i < ITERATIONS; ++i) {
        BENCH_OPS
    }
    stop = PerfUtils::Cycles::rdtsc();
    PerfUtils::TimeTrace::record("Thread[%d]: Benchmark Done", id);

    time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Thread[%d]: The total time spent invoking BENCH_OP %lu "
            "times took %0.2lf seconds (%0.2lf ns/op average)\r\n",
            id, ITERATIONS, time, (time/ITERATIONS)*1e9);

        // Break abstraction to bring metrics on cycles blocked.
    uint32_t nBlocks = NanoLog::stagingBuffer->numTimesProducerBlocked;
    double timeBlocked = 1e9*PerfUtils::Cycles::toSeconds(
                NanoLog::stagingBuffer->cyclesProducerBlocked);
    printf("Thread[%d]: Time producer was stuck for = %0.2e ns (avg: %0.2e ns cnt: %u)\r\n",
            id,
            timeBlocked,
            timeBlocked/nBlocks,
            nBlocks);
}

int main(int argc, char** argv) {
    // Optional: Set the output location for the NanoLog system. By default
    // the log will be output to /tmp/compressedLog
    // NanoLog::setLogFile("/tmp/logFile");
    NanoLog::setLogFile(BENCHMARK_OUPTUT_FILE);

    printf("BENCH_OP = '" STRINGIFY(BENCH_OPS) "'\r\n");

    pthread_barrier_t barrier;
    if (pthread_barrier_init(&barrier, NULL, BENCHMARK_THREADS)) {
        printf("pthread error\r\n");
    }

    std::vector<std::thread> threads;
    threads.reserve(BENCHMARK_THREADS);
    for (int i = 1; i < BENCHMARK_THREADS; ++i)
        threads.emplace_back(runBenchmark, i, &barrier);

    uint64_t start = PerfUtils::Cycles::rdtsc();
    runBenchmark(0, &barrier);

    for (int i = 0; i < threads.size(); ++i)
        if (threads[i].joinable())
            threads.at(i).join();

    uint64_t syncStart = PerfUtils::Cycles::rdtsc();
    // Flush all pending log messages to disk
    NanoLog::sync();
    uint64_t stop = PerfUtils::Cycles::rdtsc();

    double time = PerfUtils::Cycles::toSeconds(stop - syncStart);
    printf("Flushing the log statements to disk took an additional %0.2lf secs\r\n",
            time);

    uint64_t totalEvents = NanoLog::nanoLogSingleton.eventsProcessed;
    double totalTime = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Took %0.2lf seconds to log %lu operations\r\nThroughput: %0.2lf op/s (%0.2lf Mop/s)\r\n",
                totalTime, totalEvents,
                totalEvents/totalTime,
                totalEvents/(totalTime*1e6));

    // Prints various statistics gathered by the NanoLog system to stdout
    NanoLog::printStats();
    NanoLog::printConfig();

    // Again print out all the parameters on one line so that aggregation's a bit easier
    const char *compaction = (BENCHMARK_DISABLE_COMPACTION) ? "false" : "true";
    printf("# Throughput(Mop/s)  Operations Time Threads Compaction OutputFile BenchOp\r\n");
    printf("%0.2lf %lu %0.6lf %d %10s %10s %10s\r\n", totalEvents/(totalTime*1e6), totalEvents, totalTime, BENCHMARK_THREADS, compaction, BENCHMARK_OUPTUT_FILE, STRINGIFY(BENCH_OPS));

    // This is useful for when output is disabled and our metrics from the consumer aren't correct
    totalEvents = NanoLog::stagingBuffer->numAllocations*BENCHMARK_THREADS;
    printf("# Same as the above, but guestimated from the producer side\r\n");
    printf("%10.2lf %10lu %10.6lf %10d %10s %10s %10s\r\n", totalEvents/(totalTime*1e6), totalEvents, totalTime, BENCHMARK_THREADS, compaction, BENCHMARK_OUPTUT_FILE, STRINGIFY(BENCH_OPS));
}

