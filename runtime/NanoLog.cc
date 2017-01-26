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


#include <fcntl.h>
#include <iosfwd>
#include <locale>
#include <stdlib.h>
#include <unistd.h>

#include "snappy.h"

#include "Cycles.h"         /* Cycles::rdtsc() */
#include "Log.h"
#include "NanoLog.h"
#include "Config.h"

#include "TimeTrace.h"

// Define the static members of NanoLog here
__thread NanoLog::StagingBuffer* NanoLog::stagingBuffer = nullptr;
thread_local NanoLog::StagingBufferDestroyer NanoLog::sbc;
NanoLog NanoLog::nanoLogSingleton;

// NanoLog constructor
NanoLog::NanoLog()
    : threadBuffers()
    , nextBufferId()
    , bufferMutex()
    , compressionThread()
    , hasOutstandingOperation(false)
    , compressionThreadShouldExit(false)
    , syncRequested(false)
    , condMutex()
    , workAdded()
    , hintQueueEmptied()
    , outputFile("/tmp/compressedLog")
    , outputFd(-1)
    , aioCb()
    , compressingBuffer(nullptr)
    , outputDoubleBuffer(nullptr)
    , currentLogLevel(NOTICE)
    , cycleAtThreadStart(0)
    , cyclesAwake(0)
    , cyclesCompacting(0)
    , cyclesCompressing(0)
    , cyclesScanningAndCompressing(0)
    , cyclesAioAndFsync(0)
    , totalBytesRead(0)
    , totalBytesWritten(0)
    , padBytesWritten(0)
    , eventsProcessed(0)
    , numAioWritesCompleted(0)
    , maxPeekSizeEncountered(0)
    , totalNonZeroBytesPeeked(0)
    , numNonZeroPeeks(0)
{
    outputFd = open(outputFile, NanoLogConfig::FILE_PARAMS, 0666);
    if (outputFd < 0) {
        fprintf(stderr, "NanoLog could not open the default file location "
                "for the log file (\"%s\").\r\n Please check the permissions "
                "or use NanoLog::setLogFile(const char* filename) to "
                "specify a different log file.\r\n", "/tmp/compressedLog");
        std::exit(-1);
    }

    memset(&aioCb, 0, sizeof(aioCb));

    int err = posix_memalign(reinterpret_cast<void**>(&compressingBuffer),
                                        512, NanoLogConfig::OUTPUT_BUFFER_SIZE);
    if (err) {
        perror("The NanoLog system was not able to allocate enough memory "
                "to support its operations. Quitting...\r\n");
        std::exit(-1);
    }

    err = posix_memalign(reinterpret_cast<void**>(&outputDoubleBuffer),
                                        512, NanoLogConfig::OUTPUT_BUFFER_SIZE);
    if (err) {
        perror("The NanoLog system was not able to allocate enough memory "
                "to support its operations. Quitting...\r\n");
        std::exit(-1);
    }

    compressionThread = std::thread(&NanoLog::compressionThreadMain, this);
}

// NanoLog destructor
NanoLog::~NanoLog()
{
    sync();

    // Stop the compression thread
    {
        std::lock_guard<std::mutex> lock(nanoLogSingleton.condMutex);
        nanoLogSingleton.compressionThreadShouldExit = true;
        nanoLogSingleton.workAdded.notify_all();
    }

    if (nanoLogSingleton.compressionThread.joinable())
        nanoLogSingleton.compressionThread.join();

    // Free all the data structures
    if (compressingBuffer) {
        free(compressingBuffer);
        compressingBuffer = nullptr;
    }

    if (outputDoubleBuffer) {
        free(outputDoubleBuffer);
        outputDoubleBuffer = nullptr;
    }

    if (outputFd > 0)
        close(outputFd);

    outputFd = 0;
}

/**
 * User API: Print various statistics gathered by the NanoLog system to
 * stdout. This is primarily intended as a performance debugging aid and may
 * not be 100% consistent since it reads performance metrics without
 * synchronization.
 */
void
NanoLog::printStats()
{
   // Leaks abstraction, but basically flush so we get all the time
    uint64_t start = PerfUtils::Cycles::rdtsc();
    fdatasync(nanoLogSingleton.outputFd);
    uint64_t stop = PerfUtils::Cycles::rdtsc();
    nanoLogSingleton.cyclesAioAndFsync += (stop - start);

    double outputTime =
            PerfUtils::Cycles::toSeconds(nanoLogSingleton.cyclesAioAndFsync);
    double compactTime =
            PerfUtils::Cycles::toSeconds(nanoLogSingleton.cyclesCompacting);
    double compressTime =
            PerfUtils::Cycles::toSeconds(nanoLogSingleton.cyclesCompressing);
    double workTime = outputTime + compactTime + compressTime;

    double totalBytesWrittenDouble = static_cast<double>(
                                            nanoLogSingleton.totalBytesWritten);
    double totalBytesReadDouble = static_cast<double>(
                                            nanoLogSingleton.totalBytesRead);
    double padBytesWrittenDouble = static_cast<double>(
                                            nanoLogSingleton.padBytesWritten);

    uint64_t eventsProcessed = nanoLogSingleton.eventsProcessed;

    // If compaction is disabled, we can't get a count of how many events
    // we've processed from the background thread (since it just memcpys the
    // bytes from the StagingBuffers), thus we have to guestimate based on
    // the benchmark configurations
    if (BENCHMARK_DISABLE_COMPACTION)
        eventsProcessed = ITERATIONS*BENCHMARK_THREADS;

    double numEventsProcessedDouble = static_cast<double>(eventsProcessed);

    printf("\r\nWrote %lu events (%0.2lf MB) in %0.3lf seconds "
            "(%0.6lf seconds spent processing, %0.6lf seconds compressing)\r\n",
            eventsProcessed,
            totalBytesWrittenDouble/1.0e6,
            workTime,
            compactTime,
            compressTime);

    printf("There were %u file flushes and the final sync time was %lf sec\r\n",
            nanoLogSingleton.numAioWritesCompleted,
            PerfUtils::Cycles::toSeconds(stop - start));

    double secondsAwake =
                    PerfUtils::Cycles::toSeconds(nanoLogSingleton.cyclesAwake);
    double secondsThreadHasBeenAlive = PerfUtils::Cycles::toSeconds(
                        PerfUtils::Cycles::rdtsc() - nanoLogSingleton.cycleAtThreadStart);
    printf("Compression Thread was active for %0.3lf out of %0.3lf seconds "
            "(%0.2lf %%)\r\n",
            secondsAwake,
            secondsThreadHasBeenAlive,
            100.0*secondsAwake/secondsThreadHasBeenAlive);

    printf("The maximum peek size was %lu (avg: %lu)\r\n",
                nanoLogSingleton.maxPeekSizeEncountered,
                nanoLogSingleton.totalNonZeroBytesPeeked
                                /std::max(nanoLogSingleton.numNonZeroPeeks, 1UL));

    printf("On average, that's\r\n");
    printf("\t%0.2lf MB/s or %0.2lf ns/byte w/ processing\r\n",
                (totalBytesWrittenDouble/1.0e6)/(workTime),
                (workTime*1.0e9)/totalBytesWrittenDouble);

    // Since we sleep at 1µs intervals and check for completion at wake up,
    // it's possible the IO finished before we woke-up, thus enlarging the time.
    printf("\t%0.2lf MB/s or %0.2lf ns/byte disk throughput (min)\r\n",
                (totalBytesWrittenDouble/1.0e6)/outputTime,
                (outputTime*1.0e9)/totalBytesWrittenDouble);

    printf("\t%0.2lf MB per flush with %0.1lf bytes/event\r\n",
            (totalBytesWrittenDouble/1.0e6)/nanoLogSingleton.numAioWritesCompleted,
            totalBytesWrittenDouble*1.0/numEventsProcessedDouble);

    printf("\t%0.2lf ns/event in total\r\n"
            "\t%0.2lf ns/event processing\r\n"
            "\t%0.2lf ns/event compressing with snappy\r\n",
            (workTime)*1.0e9/numEventsProcessedDouble,
            compactTime*1.0e9/numEventsProcessedDouble,
            compressTime*1.0e9/numEventsProcessedDouble);

    printf("The compression ratio was %0.2lf-%0.2lfx "
            "(%lu bytes in, %lu bytes out, %lu pad bytes)\n",
                    1.0*totalBytesReadDouble/(totalBytesWrittenDouble
                                                    + padBytesWrittenDouble),
                    1.0*totalBytesReadDouble/totalBytesWrittenDouble,
                    nanoLogSingleton.totalBytesRead,
                    nanoLogSingleton.totalBytesWritten,
                    nanoLogSingleton.padBytesWritten);
}

/**
 * User API: Print the configuration parameters the NanoLog is currently
 * using. This is primarily for keeping track of configurations during
 * benchmarking
 */
void
NanoLog::printConfig() {
    printf("\r\n==== NanoLog Configuration ====\r\n");
    printf("StagingBuffer size: %0.3lf KB\r\n",
                                    NanoLogConfig::STAGING_BUFFER_SIZE/1000.0);
    printf("Output Buffer size: %0.3lf MB\r\n",
                                    NanoLogConfig::OUTPUT_BUFFER_SIZE/1000000.0);
    printf("Release Threshold : %0.3lf KB\r\n",
                                    NanoLogConfig::RELEASE_THRESHOLD/1000.0);
    printf("Idle Poll Interval: %u µs\r\n",
                                    NanoLogConfig::POLL_INTERVAL_NO_WORK_US);
    printf("IO Poll Interval  : %u µs\r\n",
                                    NanoLogConfig::POLL_INTERVAL_DURING_IO_US);
    printf("Output            : %s\r\n", nanoLogSingleton.outputFile);

    printf("Compaction        : ");
    if (BENCHMARK_DISABLE_COMPACTION)
        printf("disabled\r\n");
    else
        printf("enabled\r\n");

    printf("\r\n==== Time Trace Log ====\r\n");
    PerfUtils::TimeTrace::print();
}

/**
 * User API: Preallocate the thread-local data structures needed by the
 * NanoLog system for the current thread. Although optional, it is
 * recommended to invoke this function in every thread that will use the
 * NanoLog system before the first log message.
 */
void
NanoLog::preallocate()
{
    nanoLogSingleton.ensureStagingBufferAllocated();
    // I wonder if it'll be a good idea to update minFreeSpace as well since
    // the user is already willing to invoke this up front cost.
}

/**
 * Internal helper function to wait for AIO completion.
 */
void
NanoLog::waitForAIO() {
    if (hasOutstandingOperation) {
        if (aio_error(&aioCb) == EINPROGRESS) {
            const struct aiocb * const aiocb_list[] = { &aioCb };
            int err = aio_suspend(aiocb_list, 1, NULL);

            if (err != 0)
                perror("LogCompressor's Posix AIO suspend operation failed");
        }

        int err = aio_error(&aioCb);
        ssize_t ret = aio_return(&aioCb);

        if (err != 0) {
            fprintf(stderr, "LogCompressor's POSIX AIO failed with %d: %s\r\n",
                    err, strerror(err));
        } else if (ret < 0) {
            perror("LogCompressor's Posix AIO Write operation failed");
        }
        ++numAioWritesCompleted;
        hasOutstandingOperation = false;
    }
}

/**
 * Main compression thread that handles scanning through the StagingBuffers,
 * compressing log entries, and outputting a compressed log file.
 */
void
NanoLog::compressionThreadMain()
{
    PerfUtils::TimeTrace::record("Compression Thread Started (warmup)");
    PerfUtils::TimeTrace::record("Real Compression thread started");

    // Index of the last StagingBuffer checked for uncompressed log messages
    size_t lastStagingBufferChecked = 0;

    // Marks when the thread wakes up. This value should be used to calculate
    // the number of cyclesAwake right before blocking/sleeping and then updated
    // to the latest rdtsc() when the thread re-awakens.
    uint64_t cyclesAwakeStart = PerfUtils::Cycles::rdtsc();
    cycleAtThreadStart = cyclesAwakeStart;

    // Manages the state associated with compressing log messages
    Log::Encoder encoder(compressingBuffer, NanoLogConfig::OUTPUT_BUFFER_SIZE);

    // Indicates whether a compression operation failed or not due
    // to insufficient space in the outputBuffer
    bool outputBufferFull = false;

    // Indicates that in scanning the StagingBuffers, we have passed the
    // zero-th index, but have not yet encoded that in he compressed output
    bool wrapAround = false;

#ifdef USE_SNAPPY

    char *snappyOutputBuffer = nullptr;
    size_t bufferSize = snappy::MaxCompressedLength(
                                            NanoLogConfig::OUTPUT_BUFFER_SIZE);
    snappyOutputBuffer = static_cast<char*>(malloc(bufferSize));
    if (!snappyOutputBuffer) {
        printf("Could not allocate snappy buffer\r\n");
        exit(1);
    }
#endif


    // Each iteration of this loop scans for uncompressed log messages in the
    // thread buffers, compresses as much as possible, and outputs it to a file.
    while (!compressionThreadShouldExit) {
        // Indicates how many bytes we have consumed from the StagingBuffers
        // in a single iteration of the while above. A value of 0 means we
        // were unable to consume anymore data any of the stagingBuffers
        // (either due to empty stagingBuffers or a full output encoder)
        uint64_t bytesConsumedThisIteration = 0;

        uint64_t start = PerfUtils::Cycles::rdtsc();
        // Step 1: Find buffers with entries and compress them
        {
            std::unique_lock<std::mutex> lock(bufferMutex);
            size_t i = lastStagingBufferChecked;

            // Scan through the threadBuffers looking for log messages to
            // compress while the output buffer is not full.
//            if (!threadBuffers.empty())
//                PerfUtils::TimeTrace::record("Searching for work ");

            while (!compressionThreadShouldExit
                        && !outputBufferFull
                        && !threadBuffers.empty()) {
                uint64_t peekBytes = 0;
                StagingBuffer *sb = threadBuffers[i];
                char *peekPosition = sb->peek(&peekBytes);

                // If there's work, unlock to perform it
                if (peekBytes > 0) {
                    uint64_t start = PerfUtils::Cycles::rdtsc();
                    lock.unlock();

                    if (maxPeekSizeEncountered < peekBytes)
                        maxPeekSizeEncountered = peekBytes;

                    totalNonZeroBytesPeeked += peekBytes;
                    numNonZeroPeeks++;

//                    PerfUtils::TimeTrace::record("Compression thread found work of size %u", (uint32_t)readableBytes);
                    // Encode the data in RELEASE_THRESHOLD chunks
                    uint32_t remaining = downCast<uint32_t>(peekBytes);
                    while (remaining > 0) {
                        long bytesToEncode = std::min(
                                            NanoLogConfig::RELEASE_THRESHOLD,
                                                                remaining);
                        long bytesRead = encoder.encodeLogMsgs(
                                        peekPosition + (peekBytes - remaining),
                                        bytesToEncode,
                                        sb->getId(),
                                        wrapAround,
                                        &eventsProcessed);

                        if (bytesRead == 0) {
                            lastStagingBufferChecked = i;
                            outputBufferFull = true;
                            break;
                        }

                        wrapAround = false;
                        remaining -= downCast<uint32_t>(bytesRead);
                        sb->consume(bytesRead);
                        totalBytesRead += bytesRead;
                        bytesConsumedThisIteration += bytesRead;
                    }
                    cyclesCompacting += PerfUtils::Cycles::rdtsc() - start;
//                    PerfUtils::TimeTrace::record("Compression complete");
                    lock.lock();
                } else {
                    // If there's no work, check if we're supposed to delete
                    // the stagingBuffer
                    if (sb->checkCanDelete()) {
                        delete sb;

                        threadBuffers.erase(threadBuffers.begin() + i);
                        if (threadBuffers.empty()) {
                            lastStagingBufferChecked = i = 0;
                            wrapAround = true;
                            break;
                        }

                        // Back up the indexes so that we ensure we wont skip
                        // a buffer in our pass (and it's okay to redo one)
                        if (lastStagingBufferChecked >= i &&
                                lastStagingBufferChecked > 0) {
                            --lastStagingBufferChecked;
                        }
                        --i;
                    }
                }

                i = (i + 1) % threadBuffers.size();

                if (i == 0)
                    wrapAround = true;

                // Completed a full pass through the buffers
                if (i == lastStagingBufferChecked)
                    break;
            }

            cyclesScanningAndCompressing += PerfUtils::Cycles::rdtsc() - start;
        }

        // If there's no data to output, go to sleep.
        if (encoder.getEncodedBytes() == 0) {
            std::unique_lock<std::mutex> lock(condMutex);

            // If a sync was requested, we should make at least 1 more
            // pass to make sure we got everything up to the sync point.
            if (syncRequested) {
                syncRequested = false;
                continue;
            }

            cyclesAwake += PerfUtils::Cycles::rdtsc() - cyclesAwakeStart;

            hintQueueEmptied.notify_one();
            workAdded.wait_for(lock, std::chrono::microseconds(
                                    NanoLogConfig::POLL_INTERVAL_NO_WORK_US));

            cyclesAwakeStart = PerfUtils::Cycles::rdtsc();
            continue;
        }

        if (hasOutstandingOperation) {
            if (aio_error(&aioCb) == EINPROGRESS) {
                const struct aiocb * const aiocb_list[] = { &aioCb };
                if (outputBufferFull) {
                    PerfUtils::TimeTrace::record("Going to sleep due to full buffer");

                    // If the output buffer is full and we're not done,
                    // wait for completion
                    cyclesAwake += PerfUtils::Cycles::rdtsc() -cyclesAwakeStart;
                    int err = aio_suspend(aiocb_list, 1, NULL);
                    PerfUtils::TimeTrace::record("Wakeup from full sleep");
                    cyclesAwakeStart = PerfUtils::Cycles::rdtsc();
                    if (err != 0)
                        perror("LogCompressor's Posix AIO "
                                "suspend operation failed");
                } else {
                    // If there's no new data, go to sleep.
                    if (bytesConsumedThisIteration == 0 &&
                            NanoLogConfig::POLL_INTERVAL_DURING_IO_US > 0)
                    {
                        std::unique_lock<std::mutex> lock(condMutex);
                        cyclesAwake += PerfUtils::Cycles::rdtsc() -
                                                            cyclesAwakeStart;
                        workAdded.wait_for(lock, std::chrono::microseconds(
                                    NanoLogConfig::POLL_INTERVAL_DURING_IO_US));
                        cyclesAwakeStart = PerfUtils::Cycles::rdtsc();
                    }

                    if (aio_error(&aioCb) == EINPROGRESS) {
                        cyclesAioAndFsync += (PerfUtils::Cycles::rdtsc()-start);
                        continue;
                    }
                }
            }

            // Finishing up the IO
            int err = aio_error(&aioCb);
            ssize_t ret = aio_return(&aioCb);
            PerfUtils::TimeTrace::record("IO Complete");

            if (err != 0) {
                fprintf(stderr, "LogCompressor's POSIX AIO failed"
                        " with %d: %s\r\n", err, strerror(err));
            } else if (ret < 0) {
                perror("LogCompressor's Posix AIO Write failed");
            }
            ++numAioWritesCompleted;
            hasOutstandingOperation = false;
        }

        // At this point, compact-ed items exist in the buffer and the double
        // buffer used for IO is now free. Pad the output (if necessary) and
        // output.
        ssize_t bytesToWrite = encoder.getEncodedBytes();
        if (NanoLogConfig::FILE_PARAMS & O_DIRECT) {
            ssize_t bytesOver = bytesToWrite%512;

            if (bytesOver != 0) {
                memset(compressingBuffer, 0, 512 - bytesOver);
                bytesToWrite = bytesToWrite + 512 - bytesOver;
                padBytesWritten += (512 - bytesOver);
            }
        }

#ifdef USE_SNAPPY
        size_t outSize;
        uint64_t compressionTimeStart = PerfUtils::Cycles::rdtsc();
        snappy::RawCompress(compressingBuffer, bytesToWrite,
                                    snappyOutputBuffer, &outSize);

        // These two lines fake the compression
//        outSize = bytesToWrite;
//        memcpy(snappyOutputBuffer, compressingBuffer, bytesToWrite);

        uint64_t compressionTimeStop = PerfUtils::Cycles::rdtsc();
        cyclesCompressing += compressionTimeStop - compressionTimeStart;
        aioCb.aio_fildes = outputFd;
        aioCb.aio_buf = snappyOutputBuffer;
        aioCb.aio_nbytes = outSize;
        totalBytesWritten += outSize;
#else
        aioCb.aio_fildes = outputFd;
        aioCb.aio_buf = compressingBuffer;
        aioCb.aio_nbytes = bytesToWrite;
        totalBytesWritten += bytesToWrite;
#endif

        PerfUtils::TimeTrace::record("Issuing IO of size %u bytes", int(bytesToWrite));
        if (aio_write(&aioCb) == -1)
            fprintf(stderr, "Error at aio_write(): %s\n", strerror(errno));

        hasOutstandingOperation = true;

        // Swap buffers
        encoder.swapBuffer(outputDoubleBuffer,
                                            NanoLogConfig::OUTPUT_BUFFER_SIZE);
        std::swap(outputDoubleBuffer, compressingBuffer);
        outputBufferFull = false;

        // TODO(syang0) Currently, the cyclesAioAndFsync metric is
        // incorrect if we use POSIX AIO since it only measures the
        // time to submit the work and (if applicable) the amount of
        // time spent waiting for a previous incomplete AIO to finish.
        // We could get a better time metric if we spawned a thread to
        // do synchronous IO on our behalf.
        cyclesAioAndFsync += (PerfUtils::Cycles::rdtsc() - start);
    }

    if (hasOutstandingOperation) {
        uint64_t start = PerfUtils::Cycles::rdtsc();
        // Wait for any outstanding AIO to finish
        while (aio_error(&aioCb) == EINPROGRESS);
        int err = aio_error(&aioCb);
        ssize_t ret = aio_return(&aioCb);
        PerfUtils::TimeTrace::record("IO Complete");

        if (err != 0) {
            fprintf(stderr, "LogCompressor's POSIX AIO failed with %d: %s\r\n",
                    err, strerror(err));
        } else if (ret < 0) {
            perror("LogCompressor's Posix AIO Write operation failed");
        }
        ++numAioWritesCompleted;
        hasOutstandingOperation = false;
        cyclesAioAndFsync += (PerfUtils::Cycles::rdtsc() - start);
    }

    cycleAtThreadStart = 0;
    cyclesAwake += PerfUtils::Cycles::rdtsc() - cyclesAwakeStart;

#ifdef USE_SNAPPY
    free(snappyOutputBuffer);
#endif

    PerfUtils::TimeTrace::record("Compression thread exiting");
}

/**
 * See setLogFile
 */
void
NanoLog::setLogFile_internal(const char* filename) {
    // Check if it exists and is readable/writeable
    if (access (filename, F_OK) == 0 && access (filename, R_OK | W_OK) != 0) {
        std::string err = "Unable to read/write from file: ";
        err.append(filename);
        throw std::ios_base::failure(err);
    }

    // Try to open the file
    int newFd = open(filename, NanoLogConfig::FILE_PARAMS, 0666);
    if (newFd < 0) {
        std::string err = "Unable to open file '";
        err.append(filename);
        err.append("': ");
        err.append(strerror(errno));
        throw std::ios_base::failure(err);
    }

    outputFile = filename;

    // Everything seems okay, stop the background thread and change files
    sync();

     // Stop the compression thread completely
    {
        std::lock_guard<std::mutex> lock(nanoLogSingleton.condMutex);
        compressionThreadShouldExit = true;
        workAdded.notify_all();
    }

    if (compressionThread.joinable())
        compressionThread.join();

    if (outputFd > 0)
        close(outputFd);
    outputFd = newFd;

    // Relaunch thread
    compressionThreadShouldExit = false;
    compressionThread = std::thread(&NanoLog::compressionThreadMain, this);
}

/**
 * Set where the NanoLog should output its compressed log. If a previous
 * log file was specified, NanoLog will attempt to sync() the remaining log
 * entries before swapping files. For best practices, the output file shall
 * be set before the first invocation to log by the main thread as this
 * function is *not* thread safe.
 *
 * By default, the NanoLog will output to /tmp/compressedLog
 *
 * \param filename
 *      File for NanoLog to output the compress log
 *
 * \throw is_base::failure
 *      if the file cannot be opened or crated
 */
void
NanoLog::setLogFile(const char* filename)
{
    nanoLogSingleton.setLogFile_internal(filename);
}

/**
 * Sets the minimum log level new NANO_LOG messages will have to meet before
 * they are saved. Anything lower will be dropped.
 *
 * \param logLevel
 *      LogLevel enum that specifies the minimum log level.
 */
void
NanoLog::setLogLevel(LogLevel logLevel)
{
    if (logLevel < 0)
        logLevel = static_cast<LogLevel>(0);
    else if (logLevel >= NUM_LOG_LEVELS)
        logLevel = static_cast<LogLevel>(NUM_LOG_LEVELS - 1);
    nanoLogSingleton.currentLogLevel = logLevel;
}

/**
 * Blocks until the NanoLog system is able to persist to disk the
 * pending log messages that occurred before this invocation. Note that this
 * operation has similar behavior to a "non-quiescent checkpoint" in a
 * database which means log messages occurring after this point this
 * invocation may also be persisted in a multi-threaded system.
 */
void
NanoLog::sync()
{
    std::unique_lock<std::mutex> lock(nanoLogSingleton.condMutex);
    nanoLogSingleton.syncRequested = true;
    nanoLogSingleton.workAdded.notify_all();
    nanoLogSingleton.hintQueueEmptied.wait(lock);
}

/**
 * Attempt to reserve contiguous space for the producer without making it
 * visible to the consumer (See reserveProducerSpace).
 *
 * This is the slow path of reserveProducerSpace that checks for free space
 * within storage[] that involves touching variable shared with the compression
 * thread and thus causing potential cache-coherency delays.
 *
 * \param nbytes
 *      Number of contiguous bytes to reserve.
 *
 * \param blocking
 *      Test parameter that indicates that the function should
 *      return with a nullptr rather than block when there's
 *      not enough space.
 *
 * \return
 *      A pointer into storage[] that can be written to by the producer for
 *      at least nbytes.
 */
char*
NanoLog::StagingBuffer::reserveSpaceInternal(size_t nbytes, bool blocking)
{
    const char *endOfBuffer = storage + NanoLogConfig::STAGING_BUFFER_SIZE;
    uint64_t start = PerfUtils::Cycles::rdtsc();

    // There's a subtle point here, all the checks for remaining
    // space are strictly < or >, not <= or => because if we allow
    // the record and print positions to overlap, we can't tell
    // if the buffer either completely full or completely empty.
    // Doing this check here ensures that == means completely empty.
    while (minFreeSpace <= nbytes) {
        // Since readHead can be updated in a different thread, we
        // save a consistent copy of it here to do calculations on
        char *cachedReadPos = consumerPos;

        if (cachedReadPos <= producerPos) {
            minFreeSpace = endOfBuffer - producerPos;

            if (minFreeSpace > nbytes)
                return producerPos;

            // Not enough space at the end of the buffer; wrap around
            endOfRecordedSpace = producerPos;

            // Prevent the roll over if it overlaps the two positions because
            // that would imply the buffer is completely empty when it's not.
            if (cachedReadPos != storage) {
                // prevents producerPos from updating before endOfRecordedSpace
                Fence::sfence();
                producerPos = storage;
                minFreeSpace = cachedReadPos - producerPos;
            }
        } else {
            minFreeSpace = cachedReadPos - producerPos;
        }

        // Needed to prevent infinite loops in tests
        if (!blocking && minFreeSpace <= nbytes)
            return nullptr;
    }

    cyclesProducerBlocked += PerfUtils::Cycles::rdtsc() - start;
    ++numTimesProducerBlocked;

    return producerPos;
}

/**
 * Peek at the data available for consumption within the stagingBuffer.
 * The consumer should also invoke consume() to release space back
 * to the producer. This can and should be done piece-wise where a
 * large peek can be consume()-ed in smaller pieces to prevent blocking
 * the producer.
 *
 * \param[out] bytesAvailable
 *      Number of bytes consumable
 * \return
 *      Pointer to the consumable space
 */
char*
NanoLog::StagingBuffer::peek(uint64_t* bytesAvailable)
{
    // Save a consistent copy of recordHead
    char *cachedRecordHead = producerPos;

    if (cachedRecordHead < consumerPos) {
        Fence::lfence(); // Prevent reading new producerPos but old endOf...
        *bytesAvailable = endOfRecordedSpace - consumerPos;

        if (*bytesAvailable > 0)
            return consumerPos;

        // Roll over
        consumerPos = storage;
    }

    *bytesAvailable = cachedRecordHead - consumerPos;
    return consumerPos;
}