
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
static constexpr const char* BENCHMARK_OUPTUT_FILE  = "/tmp/logFile";
static const bool BENCHMARK_DISABLE_COMPACTION      = false;

// See documentation in NANO_LOG.h
static const uint32_t BENCHMARK_STAGING_BUFFER_SIZE = 1<<20;
static const uint32_t BENCHMARK_OUTPUT_BUFFER_SIZE  = 1<<26;
static const uint32_t BENCHMARK_RELEASE_THRESHOLD   = 1<<19;

static const uint32_t BENCHMARK_POLL_INTERVAL_NO_WORK_US   = 1;
static const uint32_t BENCHMARK_POLL_INTERVAL_DURING_IO_US = 1;

static const uint32_t BENCHMARK_THREADS                    = 1;

// Number of iterations to run the benchmark
static const uint64_t ITERATIONS = 100000000;

// Function (or functions) to use for the NanoLog benchmark
#define BENCH_OPS NANO_LOG(NOTICE, "Simple log message with 0 parameters");;

// Additional #defines here


#endif /* BENCHMARKCONFIG_H */
