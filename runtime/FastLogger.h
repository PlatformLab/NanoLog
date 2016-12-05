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

#ifndef FASTLOGGER_H
#define FASTLOGGER_H

#include <mutex>
#include <vector>

#include <assert.h>

#include "Common.h"
#include "Cycles.h"
#include "BufferUtils.h"
#include "LogCompressor.h"

// These header files are needed for the in-lined runtime code. They are
// included here so that the user of the FastLogger system only has to
// include one file.
#include <string.h>         /* strlen + memcpy*/

namespace PerfUtils {

/**
 * The FastLogger class manages the StagingBuffers for each thread that
 * utilizes the FastLogger mechanism and serves as the main interface for
 * the injected Runtime code as it provides access to the thread local
 * StagingBuffer.
 */
class FastLogger {
public:
    /**
     * Implements a circular FIFO byte queue that is used to hold the dynamic
     * information of a FastLogger log statement (producer) as it waits
     * for compression and output via the LogCompressor thread (consumer).
     */
    class StagingBuffer {
    public:
        // Determines the byte size of the staging buffer. It is fairly large
        // to ensure that in the best case scenario of 8x compression, we will
        // end up with ~8MB of data which is optimal for amortizing disk seeks.
        static const uint32_t BUFFER_SIZE = 1<<26;

        /**
         * Attempt to reserve contiguous space for the producer without
         * making it visible to the consumer. The user should invoke
         * finishReservation() to make this entry visible to the consumer
         * and shall not invoke this function again until they have
         * finishAlloc-ed().
         *
         * This function will block behind the consumer if there's
         * not enough space.
         *
         * \param nbytes
         *      Number of bytes to allocate
         *
         * \return
         *      Pointer to at least nbytes of contiguous space
         */
        inline char*
        reserveProducerSpace(size_t nbytes) {
            // Fast in-line path
            if (nbytes < minFreeSpace)
                return producerPos;

            // Slow allocation
            return reserveSpaceInternal(nbytes);
        }

        /**
         * Complement to reserveProducerSpace that makes nbytes from the
         * producer space visible to the consumer.
         */
        inline void
        finishReservation(size_t nbytes) {
            assert(nbytes < minFreeSpace);
            assert(producerPos + nbytes < storage + BUFFER_SIZE);
            // Don't pass the read head with finish
            assert(producerPos >= consumerPos
                            || producerPos + nbytes < consumerPos);

            minFreeSpace -= nbytes;
            producerPos += nbytes;
        }

        char* peek(uint64_t *bytesAvailable);

        /**
         * Consumes the next RecordEntry and advances the internal pointers
         * for peek().
         */
        inline void
        consume(uint64_t nbytes) {
            consumerPos += nbytes;
        }

        StagingBuffer()
            : producerPos(storage)
            , endOfRecordedSpace(storage + BUFFER_SIZE)
            , minFreeSpace(BUFFER_SIZE)
            , cacheLineSpacer()
            , consumerPos(storage)
            , storage()
        {
        }

        ~StagingBuffer() {
            // Flush out all log messages
            uint64_t remainingData;
            while (remainingData > 0) {
                FastLogger::sync();
                peek(&remainingData);
            }

            // Mark for deletion in the vector
            {
                std::lock_guard<std::mutex> guard(FastLogger::bufferMutex);

                // Erase ourselves from the thread buffer pool.
                for (size_t i = 0; i < FastLogger::threadBuffers.size(); ++i) {
                    if (FastLogger::threadBuffers[i] == this) {
                        FastLogger::threadBuffers[i] = nullptr;
                    }
                }
            }
        }

    //TODO(syang0) PRIVATE
    PRIVATE:
        char* reserveSpaceInternal(size_t nbytes, bool blocking=true);

        // Position within storage[] where the producer may place new data
        char *producerPos;

        // Marks the end of valid data for the consumer. Set by the producer
        // on a roll-over
        char *endOfRecordedSpace;

        // Lower bound on the number of bytes the producer can allocate
        // without rolling over the producerPos or stalling behind the consumer
        uint64_t minFreeSpace;

        // An extra cache-line to separate the variables that are primarily
        // updated/read by the producer (above) from the ones by the
        // consumer(below)
        static const uint32_t BYTES_PER_CACHE_LINE = 64;
        char cacheLineSpacer[BYTES_PER_CACHE_LINE];

        // Position within the storage buffer where the consumer can start
        // consuming bytes from. This value is only updated by the consumer.
        char *consumerPos;

        // Backing store used to implement the circular queue
        char storage[BUFFER_SIZE];
    };


    FastLogger();
    static void sync();
    static void exit();

    static inline void
    initialize() {
        if (stagingBuffer == NULL) {
            std::lock_guard<std::mutex> guard(bufferMutex);

            if (stagingBuffer == NULL) {
                stagingBuffer = new StagingBuffer();
                threadBuffers.push_back(stagingBuffer);
            }

            if (compressor == NULL)
                compressor = new LogCompressor();
        }
    }

    /**
     * Allocate space for the producer, but do not make it visible to
     * the consumer. The user should invoke finishAlloc() to make the space
     * visible and this function should not be invoked again until finishAlloc()
     * is invoked.
     *
     * Note this will block of the buffer is full.
     *
     * \param nbytes
     *      Amount of contiguous space to allocate
     *
     * \return
     *      Pointer to the space
     */
    static inline char*
    reserveAlloc(size_t nbytes)
    {
        initialize();
        return stagingBuffer->reserveProducerSpace(nbytes);
    }

    /**
     * Complement to reserveAlloc, makes the bytes reserved visible to be
     * read by the output head.
     *
     * \param nbytes
     *      Number of bytes to make visible
     */
    static inline void
    finishAlloc(size_t nbytes)
    {
        stagingBuffer->finishReservation(nbytes);
    }

    //TODO(syang0) PROTECTED
PROTECTED:
    // Stores uncompressed log statements as they await output; one per thread.
    static __thread StagingBuffer* stagingBuffer;

    // Tracks the stagingBuffers allocated from thread creation. Null entries
    // indicate that the owning thread has destructed.
    static std::vector<StagingBuffer*> threadBuffers;

    // Singleton that iterates through the threadBuffers and outputs their
    // log messages to a file in the background.
    static PerfUtils::LogCompressor* compressor;

    // Protects modification to the threadBuffers vector and log compressor
    static std::mutex bufferMutex;

    // Allow access to the thread buffers and mutex protecting them.
    friend LogCompressor;
};  // FastLogger
}; // namespace PerfUtils

#endif /* FASTLOGGER_H */

