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

#ifndef LOGCOMPRESSOR_H
#define LOGCOMPRESSOR_H

#include <aio.h>                /* POSIX AIO */
#include <condition_variable>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <xmmintrin.h>

namespace PerfUtils {
/**
 * LogCompressor manages a thread that will pull items off the thread local
 * StagingBuffers in the FastLogger System and compress them into an output
 * file.
 */
class LogCompressor {
public:
    void sync();
    void exit();

    void printStats();

    LogCompressor(const char *logFile="/tmp/compressedLog");
    ~LogCompressor();

private:
    void threadMain();
    void waitForAIO();
    
public:
    // Toggles whether the compressed log file will be outputted via POSIX AIO
    // or via regular, blocking file writes (for debugging)
    static const bool USE_AIO = true;

    // Controls in what mode the file will be opened in
    static const int FILE_PARAMS = O_APPEND|O_RDWR|O_CREAT|O_NOATIME|O_DSYNC|O_DIRECT;

    // Size of the buffer used to store compressed log messages before being
    // flushed to a file.
    static const uint32_t BUFFER_SIZE = 1<<26;

    // File handle for the output file; should only be opened once at the
    // construction of the LogCompressor
    int outputFd;

    // POSIX AIO structure used to communicate async IO requests
    struct aiocb aioCb;

    // Indicates there's an operation in aioCb that should be waited on
    bool hasOustandingOperation;

    // Background thread that polls the various staging buffers, compresses
    // the staged log messages, and outputs it to a file.
    std::thread workerThread;

    // Flag signaling that the LogCompressor thread should stop running
    bool shouldExit;

    // Mutex to protect the condition variables
    std::mutex mutex;

    // Signal for when the LogCompress thread should wake up. 
    std::condition_variable workAdded;

    // Signaled when the LogCompressor makes a complete pass through all the
    // thread staging buffers and finds no log messages to output.
    std::condition_variable hintQueueEmptied;
    
    // Level trigger for the LogCompressor to make a complete pass through
    // all the staging buffers before pausing
    bool syncRequested;

    // Used to stage the compressed log messages before passing it on to the
    // POSIX AIO library.
    char *outputBuffer;

    // Double buffer for outputBuffer that is used to hold compressed log
    // messages while POSIX AIO outputs it to a file.
    char *posixBuffer;

    // Metric: Number of times an AIO write was completed.
    uint32_t numAioWritesCompleted;

    // Metric: Amount of time spent scanning the buffers for work and
    // compressing events found.
    uint64_t cyclesScanningAndCompressing;

    // Metric: Amount of time spent on fsync() and writes. Note that if posix
    // AIO is used, the only the amount of time it takes to submit the job is
    // recorded.
    uint64_t cyclesAioAndFsync;

    // Metric: Amount of time spent compressing the dynamic log data
    uint64_t cyclesCompressing;

    // Metric: Number of pad bytes written to round the file to the nearest 512B
    uint64_t padBytesWritten;

    // Metric: Number of bytes read in from the staging buffers
    uint64_t totalBytesRead;

    // Metric: Number of bytes written to the output file (includes padding)
    uint64_t totalBytesWritten;

    // Metric: Number of events compressed and outputted.
    uint64_t eventsProcessed;
};
}; // namespace PerfUtils

#endif /* LOGCOMPRESSOR_H */

