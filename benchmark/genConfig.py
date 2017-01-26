#! /usr/bin/python

import sys, getopt

template = """
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
static constexpr const char* BENCHMARK_OUPTUT_FILE  = "%s";
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

// Additional #defines here
%s

#endif /* BENCHMARKCONFIG_H */
"""

def printHelp():
  print \
"""
Generate a BenchmarkConfig.h file to be used with NanoLog
Usage:
        python genConfig.py options

Options:
    --disableOutput                 Drops the log messages by piping the
                                    results to /dev/null
    --disableCompaction             Entries will be memcpy'ed directly from
                                    the StagingBuffer to the OutputBuffer.
                                    The output produced will not be useable
                                    by the NanoLog Decompressor
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
      opts, args = getopt.getopt(argv,"hs:o:r:p:i:t:b:",["disableOutput", "disableCompaction", "stagingBufferExp=","outputBufferExp=", "releaseThresholdExp=", "pollInterval=", "threads=", "iterations=","benchOp=","useSnappy"])
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
        extraDefines = "#define USE_SNAPPY"

    with open('BenchmarkConfig.h', 'w') as oFile:
      oFile.write(template % (outputFile, disableCompaction, stagingBufferExp, outputBufferExp, releaseThreshExp, pollInterval, pollInterval, threads, iterations, benchOp, extraDefines))

if __name__ == "__main__":
   main(sys.argv[1:])
