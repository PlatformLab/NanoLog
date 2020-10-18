/* Copyright (c) 2016-2020 Stanford University
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
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <stdlib.h>
#include <unistd.h>

#include "Cycles.h"         /* Cycles::rdtsc() */
#include "RuntimeLogger.h"
#include "Config.h"
#include "Util.h"

namespace NanoLogInternal {

// Define the static members of RuntimeLogger here
__thread RuntimeLogger::StagingBuffer *RuntimeLogger::stagingBuffer = nullptr;
thread_local RuntimeLogger::StagingBufferDestroyer RuntimeLogger::sbc;
RuntimeLogger RuntimeLogger::nanoLogSingleton;

// RuntimeLogger constructor
RuntimeLogger::RuntimeLogger()
        : threadBuffers()
        , nextBufferId()
        , bufferMutex()
        , compressionThread()
        , hasOutstandingOperation(false)
        , compressionThreadShouldExit(false)
        , syncStatus(SYNC_COMPLETED)
        , condMutex()
        , workAdded()
        , hintSyncCompleted()
        , outputFd(-1)
        , aioCb()
        , compressingBuffer(nullptr)
        , outputDoubleBuffer(nullptr)
        , currentLogLevel(NOTICE)
        , cycleAtThreadStart(0)
        , cyclesAtLastAIOStart(0)
        , cyclesActive(0)
        , cyclesCompressing(0)
        , stagingBufferPeekDist()
        , cyclesScanningAndCompressing(0)
        , cyclesDiskIO_upperBound(0)
        , totalBytesRead(0)
        , totalBytesWritten(0)
        , padBytesWritten(0)
        , logsProcessed(0)
        , numAioWritesCompleted(0)
        , coreId(-1)
        , registrationMutex()
        , invocationSites()
        , nextInvocationIndexToBePersisted(0)
{
    for (size_t i = 0; i < Util::arraySize(stagingBufferPeekDist); ++i)
        stagingBufferPeekDist[i] = 0;

    const char *filename = NanoLogConfig::DEFAULT_LOG_FILE;
    outputFd = open(filename, NanoLogConfig::FILE_PARAMS, 0666);
    if (outputFd < 0) {
        fprintf(stderr, "NanoLog could not open the default file location "
                "for the log file (\"%s\").\r\n Please check the permissions "
                "or use NanoLog::setLogFile(const char* filename) to "
                "specify a different log file.\r\n", filename);
        std::exit(-1);
    }

    memset(&aioCb, 0, sizeof(aioCb));

    int err = posix_memalign(reinterpret_cast<void **>(&compressingBuffer),
                             512, NanoLogConfig::OUTPUT_BUFFER_SIZE);
    if (err) {
        perror("The NanoLog system was not able to allocate enough memory "
                       "to support its operations. Quitting...\r\n");
        std::exit(-1);
    }

    err = posix_memalign(reinterpret_cast<void **>(&outputDoubleBuffer),
                         512, NanoLogConfig::OUTPUT_BUFFER_SIZE);
    if (err) {
        perror("The NanoLog system was not able to allocate enough memory "
                       "to support its operations. Quitting...\r\n");
        std::exit(-1);
    }

#ifndef BENCHMARK_DISCARD_ENTRIES_AT_STAGINGBUFFER
    compressionThread = std::thread(&RuntimeLogger::compressionThreadMain, this);
#endif
}

// RuntimeLogger destructor
RuntimeLogger::~RuntimeLogger() {
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

// Documentation in NanoLog.h
std::string
RuntimeLogger::getStats() {
    std::ostringstream out;
    char buffer[1024];
    // Leaks abstraction, but basically flush so we get all the time
    uint64_t start = PerfUtils::Cycles::rdtsc();
    fdatasync(nanoLogSingleton.outputFd);
    uint64_t stop = PerfUtils::Cycles::rdtsc();
    nanoLogSingleton.cyclesDiskIO_upperBound += (stop - start);

    double outputTime =
            PerfUtils::Cycles::toSeconds(nanoLogSingleton.cyclesDiskIO_upperBound);
    double compressTime =
            PerfUtils::Cycles::toSeconds(nanoLogSingleton.cyclesCompressing);
    double workTime = outputTime + compressTime;

    double totalBytesWrittenDouble = static_cast<double>(
            nanoLogSingleton.totalBytesWritten);
    double totalBytesReadDouble = static_cast<double>(
            nanoLogSingleton.totalBytesRead);
    double padBytesWrittenDouble = static_cast<double>(
            nanoLogSingleton.padBytesWritten);
    double numEventsProcessedDouble = static_cast<double>(
            nanoLogSingleton.logsProcessed);

    snprintf(buffer, 1024,
               "\r\nWrote %lu events (%0.2lf MB) in %0.3lf seconds "
                   "(%0.3lf seconds spent compressing)\r\n",
               nanoLogSingleton.logsProcessed,
               totalBytesWrittenDouble / 1.0e6,
               workTime,
               compressTime);
    out << buffer;

    snprintf(buffer, 1024,
           "There were %u file flushes and the final sync time was %lf sec\r\n",
           nanoLogSingleton.numAioWritesCompleted,
           PerfUtils::Cycles::toSeconds(stop - start));
    out << buffer;

    double secondsAwake =
            PerfUtils::Cycles::toSeconds(nanoLogSingleton.cyclesActive);
    double secondsThreadHasBeenAlive = PerfUtils::Cycles::toSeconds(
            PerfUtils::Cycles::rdtsc() - nanoLogSingleton.cycleAtThreadStart);
    snprintf(buffer, 1024,
               "Compression Thread was active for %0.3lf out of %0.3lf seconds "
                   "(%0.2lf %%)\r\n",
               secondsAwake,
               secondsThreadHasBeenAlive,
               100.0 * secondsAwake / secondsThreadHasBeenAlive);
    out << buffer;

    snprintf(buffer, 1024,
                "On average, that's\r\n\t%0.2lf MB/s or "
                    "%0.2lf ns/byte w/ processing\r\n",
               (totalBytesWrittenDouble / 1.0e6) / (workTime),
               (workTime * 1.0e9) / totalBytesWrittenDouble);
    out << buffer;

    // Since we sleep at 1µs intervals and check for completion at wake up,
    // it's possible the IO finished before we woke-up, thus enlarging the time.
    snprintf(buffer, 1024,
                "\t%0.2lf MB/s or %0.2lf ns/byte disk throughput (min)\r\n",
                (totalBytesWrittenDouble / 1.0e6) / outputTime,
                (outputTime * 1.0e9) / totalBytesWrittenDouble);
    out << buffer;

    snprintf(buffer, 1024,
                "\t%0.2lf MB per flush with %0.1lf bytes/event\r\n",
                (totalBytesWrittenDouble / 1.0e6) /
                                         nanoLogSingleton.numAioWritesCompleted,
                totalBytesWrittenDouble * 1.0 / numEventsProcessedDouble);
    out << buffer;

    snprintf(buffer, 1024,
                "\t%0.2lf ns/event in total\r\n"
                   "\t%0.2lf ns/event compressing\r\n",
                (workTime) * 1.0e9 / numEventsProcessedDouble,
                compressTime * 1.0e9 / numEventsProcessedDouble);
    out << buffer;

    snprintf(buffer, 1024, "The compression ratio was %0.2lf-%0.2lfx "
                   "(%lu bytes in, %lu bytes out, %lu pad bytes)\n",
           1.0 * totalBytesReadDouble / (totalBytesWrittenDouble
                                         + padBytesWrittenDouble),
           1.0 * totalBytesReadDouble / totalBytesWrittenDouble,
           nanoLogSingleton.totalBytesRead,
           nanoLogSingleton.totalBytesWritten,
           nanoLogSingleton.padBytesWritten);
    out << buffer;

    return out.str();
}

/**
 * Returns a string detailing the distribution of how long vs. how many times
 * the log producers had to wait for free space and how big vs. how many times
 * the consumer (background thread) read.
 *
 * Note: The distribution stats for the producer must be enabled via
 * -DRECORD_PRODUCER_STATS during compilation, otherwise only the consumer
 * stats will be printed.
 */
std::string
RuntimeLogger::getHistograms()
{
    std::ostringstream out;
    char buffer[1024];

    snprintf(buffer, 1024, "Distribution of StagingBuffer.peek() sizes\r\n");
    out << buffer;
    size_t numIntervals =
            Util::arraySize(nanoLogSingleton.stagingBufferPeekDist);
    for (size_t i = 0; i < numIntervals; ++i) {
        snprintf(buffer, 1024
                , "\t%02lu - %02lu%%: %lu\r\n"
                , i*100/numIntervals
                , (i+1)*100/numIntervals
                , nanoLogSingleton.stagingBufferPeekDist[i]);
        out << buffer;
    }

    {
        std::unique_lock<std::mutex> lock(nanoLogSingleton.bufferMutex);
        for (size_t i = 0; i < nanoLogSingleton.threadBuffers.size(); ++i) {
            StagingBuffer *sb = nanoLogSingleton.threadBuffers.at(i);
            if (sb) {
                snprintf(buffer, 1024, "Thread %u:\r\n", sb->getId());
                out << buffer;

                snprintf(buffer, 1024,
                                 "\tAllocations   : %lu\r\n"
                                 "\tTimes Blocked : %u\r\n",
                         sb->numAllocations,
                         sb->numTimesProducerBlocked);
                out << buffer;

#ifdef RECORD_PRODUCER_STATS
                uint64_t averageBlockNs = PerfUtils::Cycles::toNanoseconds(
                        sb->cyclesProducerBlocked)/sb->numTimesProducerBlocked;
                snprintf(buffer, 1024,
                                 "\tAvgBlock (ns) : %lu\r\n"
                                 "\tBlock Dist\r\n",
                         averageBlockNs);
                for (size_t i = 0; i < Util::arraySize(
                        sb->cyclesProducerBlockedDist); ++i)
                {
                    snprintf(buffer, 1024
                            , "\t\t%4lu - %4lu ns: %u\r\n"
                            , i*10
                            , (i+1)*10
                            , sb->cyclesProducerBlockedDist[i]);
                    out << buffer;
                }
#endif
            }
        }
    }


#ifndef RECORD_PRODUCER_STATS
    out << "Note: Detailed Producer stats were compiled out. Enable "
            "via -DRECORD_PRODUCER_STATS";
#endif

    return out.str();
}

// See documentation in NanoLog.h
void
RuntimeLogger::preallocate() {
    nanoLogSingleton.ensureStagingBufferAllocated();
    // I wonder if it'll be a good idea to update minFreeSpace as well since
    // the user is already willing to invoke this up front cost.
}

/**
* Internal helper function to wait for AIO completion.
*/
void
RuntimeLogger::waitForAIO() {
    if (hasOutstandingOperation) {
        if (aio_error(&aioCb) == EINPROGRESS) {
            const struct aiocb *const aiocb_list[] = {&aioCb};
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

        if (syncStatus == WAITING_ON_AIO) {
            syncStatus = SYNC_COMPLETED;
            hintSyncCompleted.notify_one();
        }
    }
}

/**
* Main compression thread that handles scanning through the StagingBuffers,
* compressing log entries, and outputting a compressed log file.
*/
void
RuntimeLogger::compressionThreadMain() {
    // Index of the last StagingBuffer checked for uncompressed log messages
    size_t lastStagingBufferChecked = 0;

    // Marks when the thread wakes up. This value should be used to calculate
    // the number of cyclesActive right before blocking/sleeping and then updated
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

    // Keeps a shadow mapping of the log identifiers to static information
    // to allow the logging threads to register in parallel with compression
    // lookup
    std::vector<StaticLogInfo> shadowStaticInfo;

    // Each iteration of this loop scans for uncompressed log messages in the
    // thread buffers, compresses as much as possible, and outputs it to a file.
    // The loop will run so long as it's not shutdown or there's outstanding I/O
    while (!compressionThreadShouldExit || encoder.getEncodedBytes() > 0
                                        || hasOutstandingOperation)
    {
        coreId = sched_getcpu();

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

            // Output new dictionary entries, if necessary
            if (nextInvocationIndexToBePersisted < invocationSites.size())
            {
                std::unique_lock<std::mutex> lock (registrationMutex);
                encoder.encodeNewDictionaryEntries(
                                               nextInvocationIndexToBePersisted,
                                               invocationSites);

                // update our shadow copy
                for (uint64_t i = shadowStaticInfo.size();
                                    i < nextInvocationIndexToBePersisted; ++i)
                {
                    shadowStaticInfo.push_back(invocationSites.at(i));
                }
            }

            // Scan through the threadBuffers looking for log messages to
            // compress while the output buffer is not full.
            while (!outputBufferFull && !threadBuffers.empty())
            {
                uint64_t peekBytes = 0;
                StagingBuffer *sb = threadBuffers[i];
                char *peekPosition = sb->peek(&peekBytes);

                // If there's work, unlock to perform it
                if (peekBytes > 0) {
                    uint64_t start = PerfUtils::Cycles::rdtsc();
                    lock.unlock();

                    // Record metrics on the peek size
                    size_t sizeOfDist = Util::arraySize(stagingBufferPeekDist);
                    size_t distIndex = (sizeOfDist*peekBytes)/
                                            NanoLogConfig::STAGING_BUFFER_SIZE;
                    ++(stagingBufferPeekDist[distIndex]);


                    // Encode the data in RELEASE_THRESHOLD chunks
                    uint32_t remaining = downCast<uint32_t>(peekBytes);
                    while (remaining > 0) {
                        long bytesToEncode = std::min(
                                NanoLogConfig::RELEASE_THRESHOLD,
                                remaining);
#ifdef PREPROCESSOR_NANOLOG
                        long bytesRead = encoder.encodeLogMsgs(
                                peekPosition + (peekBytes - remaining),
                                bytesToEncode,
                                sb->getId(),
                                wrapAround,
                                &logsProcessed);
#else
                        long bytesRead = encoder.encodeLogMsgs(
                                peekPosition + (peekBytes - remaining),
                                bytesToEncode,
                                sb->getId(),
                                wrapAround,
                                shadowStaticInfo,
                                &logsProcessed);
#endif


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
                    cyclesCompressing += PerfUtils::Cycles::rdtsc() - start;
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
            if (syncStatus == SYNC_REQUESTED) {
                syncStatus = PERFORMING_SECOND_PASS;
                continue;
            }

            if (syncStatus == PERFORMING_SECOND_PASS) {
                syncStatus = (hasOutstandingOperation) ? WAITING_ON_AIO
                                                      : SYNC_COMPLETED;
            }

            if (syncStatus == SYNC_COMPLETED) {
                hintSyncCompleted.notify_one();
            }

            cyclesActive += PerfUtils::Cycles::rdtsc() - cyclesAwakeStart;
            workAdded.wait_for(lock, std::chrono::microseconds(
                    NanoLogConfig::POLL_INTERVAL_NO_WORK_US));

            cyclesAwakeStart = PerfUtils::Cycles::rdtsc();
        }

        if (hasOutstandingOperation) {
            if (aio_error(&aioCb) == EINPROGRESS) {
                const struct aiocb *const aiocb_list[] = {&aioCb};
                if (outputBufferFull) {
                    // If the output buffer is full and we're not done,
                    // wait for completion
                    cyclesActive += PerfUtils::Cycles::rdtsc() - cyclesAwakeStart;
                    int err = aio_suspend(aiocb_list, 1, NULL);
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
                        cyclesActive += PerfUtils::Cycles::rdtsc() -
                                       cyclesAwakeStart;
                        workAdded.wait_for(lock, std::chrono::microseconds(
                                NanoLogConfig::POLL_INTERVAL_DURING_IO_US));
                        cyclesAwakeStart = PerfUtils::Cycles::rdtsc();
                    }

                    if (aio_error(&aioCb) == EINPROGRESS)
                        continue;
                }
            }

            // Finishing up the IO
            int err = aio_error(&aioCb);
            ssize_t ret = aio_return(&aioCb);

            if (err != 0) {
                fprintf(stderr, "LogCompressor's POSIX AIO failed"
                        " with %d: %s\r\n", err, strerror(err));
            } else if (ret < 0) {
                perror("LogCompressor's Posix AIO Write failed");
            }
            ++numAioWritesCompleted;
            hasOutstandingOperation = false;
            cyclesDiskIO_upperBound += (start - cyclesAtLastAIOStart);

            // We've completed an AIO, check if we need to notify
            if (syncStatus == WAITING_ON_AIO) {
                std::unique_lock<std::mutex> lock(nanoLogSingleton.condMutex);
                if (syncStatus == WAITING_ON_AIO) {
                    syncStatus = SYNC_COMPLETED;
                    hintSyncCompleted.notify_one();
                }
            }
        }

        // If we reach this point in the code, it means that all AIO operations
        // have completed and the double buffer is now free. We'll check if
        // we need to start a new AIO.
        ssize_t bytesToWrite = encoder.getEncodedBytes();
        if (bytesToWrite == 0)
            continue;

        // Pad the output if necessary
        if (NanoLogConfig::FILE_PARAMS & O_DIRECT) {
            ssize_t bytesOver = bytesToWrite % 512;

            if (bytesOver != 0) {
                memset(compressingBuffer, 0, 512 - bytesOver);
                bytesToWrite = bytesToWrite + 512 - bytesOver;
                padBytesWritten += (512 - bytesOver);
            }
        }

        aioCb.aio_fildes = outputFd;
        aioCb.aio_buf = compressingBuffer;
        aioCb.aio_nbytes = bytesToWrite;
        totalBytesWritten += bytesToWrite;

        cyclesAtLastAIOStart = PerfUtils::Cycles::rdtsc();
        if (aio_write(&aioCb) == -1)
            fprintf(stderr, "Error at aio_write(): %s\n", strerror(errno));

        hasOutstandingOperation = true;

        // Swap buffers
        encoder.swapBuffer(outputDoubleBuffer,
                           NanoLogConfig::OUTPUT_BUFFER_SIZE);
        std::swap(outputDoubleBuffer, compressingBuffer);
        outputBufferFull = false;
    }

    cycleAtThreadStart = 0;
    cyclesActive += PerfUtils::Cycles::rdtsc() - cyclesAwakeStart;
}

// Documentation in NanoLog.h
void
RuntimeLogger::setLogFile_internal(const char *filename) {
    // Check if it exists and is readable/writeable
    if (access(filename, F_OK) == 0 && access(filename, R_OK | W_OK) != 0) {
        std::string err = "Unable to read/write from new log file: ";
        err.append(filename);
        throw std::ios_base::failure(err);
    }

    // Try to open the file
    int newFd = open(filename, NanoLogConfig::FILE_PARAMS, 0666);
    if (newFd < 0) {
        std::string err = "Unable to open file new log file: '";
        err.append(filename);
        err.append("': ");
        err.append(strerror(errno));
        throw std::ios_base::failure(err);
    }

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
    nextInvocationIndexToBePersisted = 0; // Reset the dictionary
    compressionThreadShouldExit = false;
#ifndef BENCHMARK_DISCARD_ENTRIES_AT_STAGINGBUFFER
    compressionThread = std::thread(&RuntimeLogger::compressionThreadMain, this);
#endif
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
RuntimeLogger::setLogFile(const char *filename) {
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
RuntimeLogger::setLogLevel(LogLevel logLevel) {
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
RuntimeLogger::sync() {
#ifdef BENCHMARK_DISCARD_ENTRIES_AT_STAGINGBUFFER
    return;
#endif

    std::unique_lock<std::mutex> lock(nanoLogSingleton.condMutex);
    nanoLogSingleton.syncStatus = SYNC_REQUESTED;
    nanoLogSingleton.workAdded.notify_all();
    nanoLogSingleton.hintSyncCompleted.wait(lock);
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
char *
RuntimeLogger::StagingBuffer::reserveSpaceInternal(size_t nbytes, bool blocking) {
    const char *endOfBuffer = storage + NanoLogConfig::STAGING_BUFFER_SIZE;

#ifdef RECORD_PRODUCER_STATS
    uint64_t start = PerfUtils::Cycles::rdtsc();
#endif

    // There's a subtle point here, all the checks for remaining
    // space are strictly < or >, not <= or => because if we allow
    // the record and print positions to overlap, we can't tell
    // if the buffer either completely full or completely empty.
    // Doing this check here ensures that == means completely empty.
    while (minFreeSpace <= nbytes) {
        // Since consumerPos can be updated in a different thread, we
        // save a consistent copy of it here to do calculations on
        char *cachedConsumerPos = consumerPos;

        if (cachedConsumerPos <= producerPos) {
            minFreeSpace = endOfBuffer - producerPos;

            if (minFreeSpace > nbytes)
                break;

            // Not enough space at the end of the buffer; wrap around
            endOfRecordedSpace = producerPos;

            // Prevent the roll over if it overlaps the two positions because
            // that would imply the buffer is completely empty when it's not.
            if (cachedConsumerPos != storage) {
                // prevents producerPos from updating before endOfRecordedSpace
                Fence::sfence();
                producerPos = storage;
                minFreeSpace = cachedConsumerPos - producerPos;
            }
        } else {
            minFreeSpace = cachedConsumerPos - producerPos;
        }

#ifdef BENCHMARK_DISCARD_ENTRIES_AT_STAGINGBUFFER
        // If we are discarding entries anwyay, just reset space to the head
        producerPos = storage;
        minFreeSpace = endOfBuffer - storage;
#endif

        // Needed to prevent infinite loops in tests
        if (!blocking && minFreeSpace <= nbytes)
            return nullptr;
    }

#ifdef RECORD_PRODUCER_STATS
    uint64_t cyclesBlocked = PerfUtils::Cycles::rdtsc() - start;
    cyclesProducerBlocked += cyclesBlocked;

    size_t maxIndex = Util::arraySize(cyclesProducerBlockedDist) - 1;
    size_t index = std::min(cyclesBlocked/cyclesIn10Ns, maxIndex);
    ++(cyclesProducerBlockedDist[index]);
#endif

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
char *
RuntimeLogger::StagingBuffer::peek(uint64_t *bytesAvailable) {
    // Save a consistent copy of producerPos
    char *cachedProducerPos = producerPos;

    if (cachedProducerPos < consumerPos) {
        Fence::lfence(); // Prevent reading new producerPos but old endOf...
        *bytesAvailable = endOfRecordedSpace - consumerPos;

        if (*bytesAvailable > 0)
            return consumerPos;

        // Roll over
        consumerPos = storage;
    }

    *bytesAvailable = cachedProducerPos - consumerPos;
    return consumerPos;
}

}; // namespace NanoLog Internal