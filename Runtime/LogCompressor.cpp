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

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <aio.h>

#include "BufferStuffer.h"
#include "BufferUtils.h"
#include "Cycles.h"
#include "FastLogger.h"
#include "LogCompressor.h"

namespace PerfUtils {

/**
 * Construct a LogCompressor that would spawn a thread to scan the
 * staging buffers and output to the logFile.
 *
 * \param logFile - log file to output compressed logs to.
 */
LogCompressor::LogCompressor(const char *logFile)
        : outputFd(0)
        , aioCb()
        , hasOustandingOperation(false)
        , workerThread()
        , run(true)
        , mutex()
        , workAdded()
        , hintQueueEmptied()
        , syncRequested(false)
        , outputBuffer(NULL)
        , posixBuffer(NULL)
        , numBuffersProcessed(0)
        , cyclesSearchingForWork(0)
        , cyclesAioAndFsync(0)
        , cyclesCompressing(0)
        , padBytesWritten(0)
        , totalBytesRead(0)
        , totalBytesWritten(0)
        , eventsProcessed(0)
    {
        outputFd = open(logFile, FILE_PARAMS);
        memset(&aioCb, 0, sizeof(aioCb));

        int err = posix_memalign(reinterpret_cast<void**>(&outputBuffer),
                                                            512, BUFFER_SIZE);
        if (err) {
            perror("Memory alignment for LogCompressor's output buffer failed");
            std::exit(-1);
        }

        err = posix_memalign(reinterpret_cast<void**>(&posixBuffer),
                                                            512, BUFFER_SIZE);
        if (err) {
            perror("Memory alignment for LogCompressor's output buffer failed");
            std::exit(-1);
        }

        workerThread = std::thread(
                            &PerfUtils::LogCompressor::threadMain, this);
    }

/**
 * Log Compressor Destructor
 */
LogCompressor::~LogCompressor() {
    if (outputFd > 0)
        close(outputFd);

    outputFd = 0;
}

/**
 * Internal helper function to wait for any in-progress POSIX AIO operations
 */
void
LogCompressor::waitForAIO() {
    if (hasOustandingOperation) {
        // burn time waiting for io (could aio_suspend)
        // and could signal back when ready via a register.
        while (aio_error(&aioCb) == EINPROGRESS);
        int err = aio_error(&aioCb);
        int ret = aio_return(&aioCb);

        if (err != 0) {
            printf("LogCompressor's POSIX AIO failed with %d: %s\r\n",
                    err, strerror(err));
        } else if (ret < 0) {
            perror("LogCompressor's Posix AIO Write operation failed");
        }
        ++numAioWritesCompleted;
        hasOustandingOperation = false;
    }
}

/**
 * Main loop for the background log compressor thread.
 */
void
LogCompressor::threadMain() {
    FastLogger::StagingBuffer *buff = NULL;
    uint32_t lastBufferIndex = 0;
    uint32_t currentBufferIndex = 0;

    uint64_t lastTimestamp = 0;
    uint32_t lastFmtId = 0;

    while(run) {
        bool foundWork = false;
        //TODO(syang0) right now the thread just spins; make it sleep

        // Step 1: Find a buffer with valid data inside
        char *out = outputBuffer;
        char *endOfOutputBuffer = outputBuffer + BUFFER_SIZE;
        do {
            uint64_t start = Cycles::rdtsc();
            foundWork = false;

            BufferUtils::RecordEntry *re = nullptr;
            buff = nullptr;

            {
                std::unique_lock<std::mutex> lock(FastLogger::bufferMutex);
                do {
                    if (FastLogger::threadBuffers.empty())
                        break;

                    currentBufferIndex =  (currentBufferIndex + 1) %
                                            FastLogger::threadBuffers.size();

                    buff = FastLogger::threadBuffers[currentBufferIndex];

                    // Could be that the thread was deallocated
                    if (buff == nullptr)
                        continue;

                    re = buff->peek();
                    if (re == nullptr) {
                        buff = nullptr;
                        continue;
                    }

                    foundWork = true;
                } while (!foundWork && currentBufferIndex != lastBufferIndex);
            }
            cyclesSearchingForWork += (Cycles::rdtsc() - start);

            if (!foundWork || buff == nullptr)
                break;

            // Step 2: Start processing the data we found
            start = Cycles::rdtsc();
            if (!BufferUtils::insertCheckpoint(&out, endOfOutputBuffer))
                break;

            while (re != nullptr) {
                // Check that we have enough space
                if (re->entrySize + re->argMetaBytes > endOfOutputBuffer - out)
                    break;

                // TODO(syang0) Instead of explicitly passing in lastTS/FmtId,
                // consider having compressMetadata requiring an opaque data
                // structure (or having it stored statically in the function
                // if thread safety is not an issue).
                BufferUtils::compressMetadata(re, &out, lastTimestamp, lastFmtId);
                compressFnArray[re->fmtId](re, &out);

                lastFmtId = re->fmtId;
                lastTimestamp = re->timestamp;

                eventsProcessed++;
                totalBytesRead += re->entrySize;

                buff->consumeNext();
                re = buff->peek();
            }
            ++numBuffersProcessed;
            cyclesCompressing += Cycles::rdtsc() - start;

        } while(foundWork && lastBufferIndex != currentBufferIndex);

        // Step 3: Start outputting the compressed data!
        uint64_t start = Cycles::rdtsc();

        // Determine how many pad bytes we will need if O_DIRECT is used
        uint32_t bytesToWrite = out - outputBuffer;
        if (FILE_PARAMS & O_DIRECT) {
            uint32_t bytesOver = bytesToWrite%512;

            if (bytesOver != 0) {
                bytesToWrite = bytesToWrite + 512 - bytesOver;
                padBytesWritten += (512 - bytesOver);
            }
        }

        if (bytesToWrite) {
            if (USE_AIO) {
                waitForAIO();
                aioCb.aio_fildes = outputFd;
                aioCb.aio_buf = outputBuffer;
                aioCb.aio_nbytes = bytesToWrite;

                if (aio_write(&aioCb) == -1)
                    printf("Error at aio_write(): %s\n", strerror(errno));

                hasOustandingOperation = true;
                totalBytesWritten += bytesToWrite;

                // Swap buffers
                char *tmp = outputBuffer;
                outputBuffer = posixBuffer;
                posixBuffer = tmp;
                endOfOutputBuffer = outputBuffer + BUFFER_SIZE;

            } else {
                if (bytesToWrite != write(outputFd, outputBuffer, bytesToWrite))
                    perror("Error dumping log");
            }

            // TODO(syang0) Currently, the cyclesAioAndFsync metric is
            // incorrect if we use POSIX AIO since it only measures the
            // time to submit the work and (if applicable) the amount of
            // time spent waiting for a previous incomplete AIO to finish.
            // We could get a better time metric if we spawned a thread to
            // do synchronous IO on our behalf.
            cyclesAioAndFsync += (Cycles::rdtsc() - start);
        }

        if (!foundWork) {
            //TODO(syang0) Currently this thread never sleeps. At some point
            // I think it ought to.
            std::unique_lock<std::mutex> lock(mutex);
            if (syncRequested)
                syncRequested = false;
            else
                hintQueueEmptied.notify_all();
        }
    }

    if (hasOustandingOperation) {
        uint64_t start = Cycles::rdtsc();
        waitForAIO();
        cyclesAioAndFsync += (Cycles::rdtsc() - start);
    }

    // Output the stats after this thread exits
    printf("\r\nLogger Compressor Thread Exiting, printing stats\r\n");
    printStats();
}

/**
 * Print out various statistics related to the LogCompressor to stdout.
 */
void LogCompressor::printStats() {
    // Leaks abstraction, but basically flush so we get all the time
    uint64_t start = Cycles::rdtsc();
    fdatasync(outputFd);
    uint64_t stop = Cycles::rdtsc();
    cyclesAioAndFsync += (stop - start);

    double outputTime = Cycles::toSeconds(cyclesAioAndFsync);
//    double lookingForWork = Cycles::toSeconds(cyclesSearchingForWork);
    double compressTime = Cycles::toSeconds(cyclesCompressing);
    double workTime = outputTime + compressTime;

    uint64_t logMisses = 0;
    {
        std::unique_lock<std::mutex> lock(FastLogger::bufferMutex);
        for (FastLogger::StagingBuffer *buff : FastLogger::threadBuffers) {
            if (buff != nullptr)
                logMisses += buff->getNumberOfAllocFailures();
        }
    }

    printf("Wrote %lu events (%0.2lf MB) in %0.3lf seconds "
            "(%0.3lf seconds spent compressing)\r\n",
            eventsProcessed,
            totalBytesWritten/1.0e6,
            outputTime,
            compressTime);

    printf("There were %u buffers processed, %u file flushes, and "
            "%lu events were  missed due to lack of space (%0.2lf%%)\r\n",
                numBuffersProcessed, numAioWritesCompleted, logMisses,
                100.0*logMisses/(eventsProcessed + logMisses));
    printf("Final fsync time was %lf sec\r\n",
                Cycles::toSeconds(stop - start));

    printf("On average, that's\r\n"
            "\t%0.2lf MB/s or %0.2lf ns/byte w/ processing\r\n"
            "\t%0.2lf MB/s or %0.2lf ns/byte raw output\r\n"
            "\t%0.2lf MB per flush with %0.1lf bytes/event\r\n",
            (totalBytesWritten/1.0e6)/(workTime),
            (workTime*1.0e9)/totalBytesWritten,
            (totalBytesWritten/1.0e6)/outputTime,
            (outputTime)*1.0e9/totalBytesWritten,
            (totalBytesWritten/1.0e6)/numBuffersProcessed,
            totalBytesWritten*1.0/eventsProcessed);

    printf("\t%0.2lf ns/event in total\r\n"
            "\t%0.2lf ns/event compressing\r\n",
            (outputTime + compressTime)*1.0e9/eventsProcessed,
            compressTime*1.0e9/eventsProcessed);

    printf("The compression ratio was %0.2lf-%0.2lfx "
            "(%lu bytes in, %lu bytes out, %lu pad bytes)\n",
                    1.0*totalBytesRead/(totalBytesWritten + padBytesWritten),
                    1.0*totalBytesRead/totalBytesWritten,
                    totalBytesRead,
                    totalBytesWritten,
                    padBytesWritten);
}

/**
 * Blocks until the LogCompressor is unable to find anymore work in its pass
 * through the thread local staging buffers. Note that since access to the
 * buffers is not synchronized, it's possible that some log messages enqueued
 * after this invocation will be missed.
 */
void
LogCompressor::sync()
{
    std::unique_lock<std::mutex> lock(mutex);
    syncRequested = true;
    workAdded.notify_all();
    hintQueueEmptied.wait(lock);
}

/**
 * Stops the log compressor thread as soon as possible. Note that this will
 * not ensure that all log messages are persisted before the exit. If the
 * behavior is desired, one must invoke stop all logging, invoke sync() and
 * then exit().
 */
void
LogCompressor::exit()
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        run = false;
        workAdded.notify_all();
    }
    workerThread.join();
}
} // Namespace

