/* Copyright (c) 2019 Stanford University
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
 * This benchmark attempts to measure the variation in thread performance over
 * time. It does this by taking many time points back-to-back using either
 * rdtsc or high_resolution_clock and reports the inverse cdf of the time
 * between operations.
 */
#include <cstdio>
#include <cstring>

#include <algorithm>
#include <chrono>
#include <vector>
#include <string>
#include <stdexcept>

#include "Cycles.h"

/**
 * Produces a GNUPlot graphable reverse CDF graph to stdout given a vector of
 * rdtsc time deltas a conversion factor for tsc to wall time seconds.
 * This is primarily used by NanoLog to visualize extreme tail latency behavior.
 *
 * \param timeDeltas
 *      vector of rdtsc time differences
 *
 * \param cyclesPerSecond
 *      conversion factor of tsc counts to seconds
 */
void runRdtscRCDF(std::vector<uint64_t> timeDeltas, double cyclesPerSecond)
{
    printf("#\tSorting Times\r\n");
    std::sort(timeDeltas.begin(), timeDeltas.end());
    printf("#\tDone! Outputting rcdf\r\n");
    printf("#   Latency     Percentage of Operations\r\n");

    uint64_t sum = 0;
    double boundary = 1.0e-10; // 1 decimal points into nanoseconds
    uint64_t bound = PerfUtils::Cycles::fromSeconds(boundary, cyclesPerSecond);
    double size = double(timeDeltas.size());

    printf("%8.2lf    %11.10lf\r\n",
          1e9*PerfUtils::Cycles::toSeconds(timeDeltas.front(), cyclesPerSecond),
          1.0);

    uint64_t lastPrinted = timeDeltas.front();
    for(uint64_t i = 1; i < timeDeltas.size(); ++i) {
        sum += timeDeltas[i];
        if (timeDeltas[i] - lastPrinted <= bound)
            continue;

        printf("%8.2lf    %11.10lf\r\n"
                , 1e9*PerfUtils::Cycles::toSeconds(lastPrinted, cyclesPerSecond)
                , 1.0 - double(i)/size);
        lastPrinted = timeDeltas[i];
    }

    printf("%8.2lf    %11.10lf\r\n",
           1e9*PerfUtils::Cycles::toSeconds(timeDeltas.back(), cyclesPerSecond),
           1/size);

    printf("\r\n# The mean was %0.2lf ns for rdtsc\r\n",
            1e9*PerfUtils::Cycles::toSeconds(
                                       sum/timeDeltas.size(), cyclesPerSecond));
}

/**
 * Produces a GNUPlot graphable reverse CDF graph to stdout given a vector of
 * time deltas. This is primarily used by NanoLog to visualize extreme
 * tail latency behavior.
 *
 * \param timeDeltas
 *      vector of time deltas between measurements
 */
void runRCDF(std::vector<int> timeDeltas)
{
    printf("#\tSorting Times\r\n");
    std::sort(timeDeltas.begin(), timeDeltas.end());
    printf("#\tDone! Outputting rcdf\r\n");
    printf("#   Latency     Percentage of Operations\r\n");

    int64_t sum = 0;
    double size = double(timeDeltas.size());

    printf("%8d    %11.10lf\r\n", timeDeltas.front(), 1.0);

    int lastPrinted = timeDeltas.front();
    for(uint64_t i = 1; i < timeDeltas.size(); ++i) {
        sum += timeDeltas[i];
        if (timeDeltas[i] - lastPrinted <= 0)
            continue;

        printf("%8d    %11.10lf\r\n", lastPrinted, 1.0 - double(i)/size);
        lastPrinted = timeDeltas[i];
    }

    printf("%8d    %11.10lf\r\n", timeDeltas.back(), 1/size);
    printf("\r\n# The mean was %0.2lf ns for high_resolution_clock\r\n",
                                                double(sum)/timeDeltas.size());
}

/**
 * Prints a description of the application and its usage information to stdout.
 *
 * \param exec
 *          Name of the executable
 */
void printHelp(const char* exec)
{
    printf("Collects time points back-to-back with either PerfUtils' "
           "rdtsc operation\r\n"
           "or C++'s high_resolution_clock, and then "
           "outputs the inverse-cdf of the operations.\r\n");
    printf("\r\nUsage:\r\n");
    printf("\t%s (rdtsc|high_resolution_clock) <num_samples>\r\n\r\n", exec);
}


int main(int argc, char** argv) {
    if (argc < 3) {
        printHelp(argv[0]);
        exit(1);
    }

    // Parse Arguments
    const char *command = argv[1];
    long numTimes = 1000;
    try {
        numTimes = std::stoll(argv[2]);
    } catch (const std::invalid_argument& e) {
        printf("Invalid num_samples, please enter a number: %s\r\n", argv[2]);
        exit(-1);
    } catch (const std::out_of_range& e) {
        printf("num_samples is too large: %s\r\n", argv[2]);
        exit(-1);
    }

    if (numTimes < 0) {
        printf("num_samples must be positive: %s\r\n", argv[2]);
        exit(-1);
    }

    // Run Experiment
    if (strcmp(command, "high_resolution_clock") == 0) {
        std::vector<std::chrono::high_resolution_clock::time_point> time_points;
        time_points.resize(numTimes);

        printf("# Starting Data Gathering Phase for high_resolution_clock\r\n");

        // Warmup
        int warmup = std::min(1000L, numTimes);
        for (int i = 0; i < warmup; ++i)
            time_points[i] = std::chrono::high_resolution_clock::now();

        // Time Collection
        for (size_t i = 0; i < numTimes; ++i) {
            time_points[i] = std::chrono::high_resolution_clock::now();
        }

        std::chrono::high_resolution_clock::time_point lastTime =
                std::chrono::high_resolution_clock::now();

        // Take Differences
        std::vector<int> timeDeltasNs;
        timeDeltasNs.resize(numTimes);
        for (size_t i = 0; i < (numTimes - 1); ++i)
            timeDeltasNs[i] =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                            time_points[i + 1] - time_points[i]).count();
        timeDeltasNs[numTimes - 1] =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                        lastTime - time_points[numTimes - 1]).count();

        runRCDF(timeDeltasNs);
    } else if (strcmp(command, "rdtsc") == 0) {
        std::vector<uint64_t> time_points(numTimes);
        time_points.resize(numTimes);

        printf("# Starting Data Gathering Phase for rdtsc\r\n");

        // Warmup
        int warmup = std::min(1000L, numTimes);
        for (int i = 0; i < warmup; ++i)
            time_points[i] = PerfUtils::Cycles::rdtsc();

        // Gather Times
        for (size_t i = 0; i < numTimes; ++i) {
            time_points[i] = PerfUtils::Cycles::rdtsc();
        }

        uint64_t lastTime = PerfUtils::Cycles::rdtsc();

        // Take Differences
        for (size_t i = 0; i < (numTimes - 1); ++i)
            time_points[i] = time_points[i + 1] - time_points[i];
        time_points[numTimes - 1] = lastTime - time_points[numTimes - 1];

        // Aggregate!
        runRdtscRCDF(time_points, PerfUtils::Cycles::getCyclesPerSec());
    } else {
        printHelp(argv[0]);
    }
}

