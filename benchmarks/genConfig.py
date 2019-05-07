#! /usr/bin/python

import sys, getopt

def printHelp():
  print \
"""
Generates a configuration file to be used in benchmarking NanoLog
(./BenchmarkConfig.h). Note that invoking this file will also
overwrite the contents of ../runtime/Config.h to refer to the
variables in ./BenchmarkConfig.h.

Usage:
        python genConfig.py options

Options:
    --disableOutput                 Drops the log messages by piping the
                                    results to /dev/null
    --disableCompaction             Entries will be memcpy'ed directly from
                                    the StagingBuffer to the OutputBuffer.
                                    The output produced will not be useable
                                    by the NanoLog Decompressor
    --discardEntriesAtStagingBuffer Drops messages after they've been placed
                                    into the staging buffer by record()
    --stagingBufferExp <exp>        Specifies the size of the StagingBuffer
                                    as 2^exp bytes (default 20)
    --outputBufferExp <exp>         Specifies the size of the OutputBuffer
                                    as 2^exp bytes (default 26)
    --releaseThresholdExp <exp>     Specifies the release threshold as 2^exp
                                    bytes (default 19)
    --pollInterval <us>             Amount of time (in us) that the NanoLog
                                    should wake from sleep to check for work
                                    (default 1)

    --threads <num>                 Number of producer threads (default 1).
                                    Each thread will run iterations of benchOp

    --iterations <num>              Number of iterations to run benchOp (below)
                                    (default 100000000)

    --benchOp <Nano Log Code>       C++ code to run <iteration> times
                                    (default "NANO_LOG("Simple log message with 0 parameters");)
                                    Note: variable int 'i' is accessible here

    --useSnappy                     Enable the logic to compress the NanoLog
                                    output with snappy https://github.com/google/snappy

Examples:

  python genConfig.py               Generates a default configuration

  python genConfig.py --benchOp "NANO_LOG(\"Test\");" --iterations=100
"""



clientConfigTemplate = """
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
 * This file declares a bunch of constants that are used as configuration in
 * the various parts of the NanoLog system. It is abstracted in this fashion
 * for ease of benchmarking and code simplicity (i.e. the production code
 * should have hard-coded values). These values can be changed manually, but
 * they are typically generated with a python script.
 */
#ifndef BENCHMARKCONFIG_H
#define BENCHMARKCONFIG_H

#include <stdint.h>

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

// Special hacked parameters to disable compaction
static constexpr const char BENCHMARK_OUTPUT_FILE[] = "%s";
static const bool BENCHMARK_DISABLE_COMPACTION      = %s;

// See documentation in NANO_LOG.h
static const uint32_t BENCHMARK_STAGING_BUFFER_SIZE = 1<<%d;
static const uint32_t BENCHMARK_OUTPUT_BUFFER_SIZE  = 1<<%d;
static const uint32_t BENCHMARK_RELEASE_THRESHOLD   = 1<<%d;

static const uint32_t BENCHMARK_POLL_INTERVAL_NO_WORK_US   = %d;
static const uint32_t BENCHMARK_POLL_INTERVAL_DURING_IO_US = %d;

static const uint32_t BENCHMARK_THREADS                    = %d;

// Number of iterations to run the benchmark
static const uint64_t ITERATIONS = %d;

// Function (or functions) to use for the NanoLog benchmark
#define BENCH_OPS %s;

const char BENCH_OPS_AS_A_STR[] = "%s";

// Additional #defines here
%s

#endif /* BENCHMARKCONFIG_H */
"""

libraryConfigTemplate = """
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
#ifndef CONFIG_H
#define CONFIG_H

#include <fcntl.h>
#include <cassert>
#include <cstdint>

/**
 * This file centralizes the Configuration Options that can be made to NanoLog.
 *
 * This particular instance of the file is generated by a **BENCHMARK CLIENT**
 * so feel free to overwrite this file with a "git checkout" if you encounter
 * a compilation error.
 */
#include "../benchmarks/BenchmarkConfig.h"


namespace NanoLogConfig {
    // Controls in what mode the compressed log file will be opened
    static const int FILE_PARAMS = O_APPEND|O_RDWR|O_CREAT|O_DSYNC;

    // Location of the initial log file
    static constexpr const char* DEFAULT_LOG_FILE = BENCHMARK_OUTPUT_FILE;

    // Determines the byte size of the per-thread StagingBuffer that decouples
    // the producer logging thread from the consumer background compression
    // thread. This value should be large enough to handle bursts of activity.
    static const uint32_t STAGING_BUFFER_SIZE = BENCHMARK_STAGING_BUFFER_SIZE;

    // Determines the size of the output buffer used to store compressed log
    // messages. It should be at least 8MB large to amortize disk seeks and
    // shall not be smaller than STAGING_BUFFER_SIZE.
    static const uint32_t OUTPUT_BUFFER_SIZE = BENCHMARK_OUTPUT_BUFFER_SIZE;

    // This invariant must be true so that we can output at least one full
    // StagingBuffer per output buffer.
    static_assert(STAGING_BUFFER_SIZE <= OUTPUT_BUFFER_SIZE,
        "OUTPUT_BUFFER_SIZE must be greater than or "
            "equal to the STAGING_BUFFER_SIZE");

    // The threshold at which the consumer should release space back to the
    // producer in the thread-local StagingBuffer. Due to the blocking nature
    // of the producer when it runs out of space, a low value will incur more
    // more blocking but at a shorter duration, whereas a high value will have
    // the opposite effect.
    static const uint32_t RELEASE_THRESHOLD = BENCHMARK_RELEASE_THRESHOLD;

    // How often should the background compression thread wake up to check
    // for more log messages in the StagingBuffers to compress and output.
    // Due to overheads in the kernel, this number will a lower bound and
    // the actual time spent sleeping may be significantly higher.
    static const uint32_t POLL_INTERVAL_NO_WORK_US =
                                    BENCHMARK_POLL_INTERVAL_NO_WORK_US;

    // How often should the background compression thread wake up and
    // check for more log messages when it's stalled waiting for an IO
    // to complete. Due to overheads in the kernel, this number will
    // be a lower bound and the actual time spent sleeping may be higher.
    static const uint32_t POLL_INTERVAL_DURING_IO_US =
                                    BENCHMARK_POLL_INTERVAL_DURING_IO_US;
}

#endif /* CONFIG_H */
"""

def main(argv):

    disableCompaction = "false"
    outputFile        = "/tmp/logFile"
    stagingBufferExp  = 20
    outputBufferExp   = 26
    releaseThreshExp  = 19
    pollInterval      = 1
    iterations        = 100000000
    benchOp           = "NANO_LOG(NOTICE, \"Simple log message with 0 parameters\");"
    threads           = 1
    extraDefines      = ""


    try:
      opts, args = getopt.getopt(argv,"hs:o:r:p:i:t:b:",["disableOutput", "disableCompaction", "discardEntriesAtStagingBuffer", "stagingBufferExp=","outputBufferExp=", "releaseThresholdExp=", "pollInterval=", "threads=", "iterations=","benchOp=","useSnappy"])
    except getopt.GetoptError:
      printHelp()
      sys.exit(2)
    for opt, arg in opts:
      if opt == '-h':
         printHelp()
         sys.exit()
      elif opt in ("--disableOutput"):
        outputFile = "/dev/null"
      elif opt in ("--disableCompaction"):
        disableCompaction = "true"
      elif opt in ("-s", "--stagingBufferExp"):
         stagingBufferExp = int(arg)
      elif opt in ("-o", "--outputBufferExp"):
         outputBufferExp = int(arg)
      elif opt in ("-r", "--releaseThresholdExp"):
         releaseThreshExp = int(arg)
      elif opt in ("-p", "--pollInterval"):
         pollInterval = int(arg)
      elif opt in ("-t", "--threads"):
        threads = int(arg)
      elif opt in ("-i", "--iterations"):
         iterations = int(arg)
      elif opt in ("-b", "--benchOp"):
         benchOp = str(arg)
      elif opt in ("--useSnappy"):
        extraDefines += "\r\n#define USE_SNAPPY"
      elif opt in ("--discardEntriesAtStagingBuffer"):
        extraDefines += "\r\n#define BENCHMARK_DISCARD_ENTRIES_AT_STAGINGBUFFER"

    with open('BenchmarkConfig.h', 'w') as oFile:
      benchOpStr = benchOp.replace('"', "'")
      oFile.write(clientConfigTemplate % (outputFile, disableCompaction, stagingBufferExp, outputBufferExp, releaseThreshExp, pollInterval, pollInterval, threads, iterations, benchOp, benchOpStr, extraDefines))

    with open('../runtime/Config.h', 'w') as oFile:
      oFile.write(libraryConfigTemplate)
      print """
***********
* WARNING *
***********
      """
      print "\"../runtime/Config.h\" has been modified to support " \
              "the benchmark.\r\nPlease checkout a fresh version when " \
              "building the library for other purposes with:\r\n" \
              "\t git checkout ../runtime/Config.h\r\n"

if __name__ == "__main__":
   main(sys.argv[1:])
