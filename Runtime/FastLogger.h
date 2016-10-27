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
    class StagingBuffer;

    FastLogger();
    static void sync();
    static void exit();

PROTECTED:
    static __thread StagingBuffer* stagingBuffer;
    static PerfUtils::LogCompressor* compressor;
    static std::vector<StagingBuffer*> threadBuffers;
    static std::mutex bufferMutex;

public:
    /**
     * Reserve space for a RecordEntry within a thread-local staging buffer.
     * One shall invoke finishAlloc() when the RecordEntry's arguments are
     * valid and before the next invocation to reserveAlloc().
     * 
     * \param argBytes  - Number of bytes needed to store the arguments of the
     *                    dynamic arguments
     *
     * \return          - A RecordEntry to use or NULL if not enough space.
     */
    static inline BufferUtils::RecordEntry*
    reserveAlloc(int argBytes) {
        if (stagingBuffer == NULL) {
            std::lock_guard<std::mutex> guard(bufferMutex);

            if (stagingBuffer == NULL) {
                stagingBuffer = new StagingBuffer();
                threadBuffers.push_back(stagingBuffer);
            }

            if (compressor == NULL)
                compressor = new LogCompressor();
        }

        return stagingBuffer->reserveAlloc(argBytes);
    }

    /**
     * Complementary to reserveAlloc, makes the bytes reserved visible to be
     * read by the output head.
     *
     * \param re - RecordEntry that was returned in the reserveAlloc. This is
     *             only used for debugging purposes.
     */
    static inline void
    finishAlloc(BufferUtils::RecordEntry *re)
    {
        stagingBuffer->finishAlloc(re);
    }

    /**
     * Implements a cache friendly circular FIFO queue that is used to hold
     * the dynamic information of a FastLogger log statement (producer) as
     * it waits for compression and output via the LogCompressor
     * thread (consumer).
     *
     * The data in the StagingBuffer is roughly structured as
     * ****************
     * * Record Entry *
     * *   Metadata   *
     * ****************
     * * Uncompressed *
     * *   Arguments  *
     * ****************
     * * Record Entry *
     * ....
     *
     * Note that Record Entries with their arguments cannot span a wrap
     * around in the buffer, thus upon reserveAlloc, the entirety of the
     * Entry must be allocated at once.
     */
    class StagingBuffer {
    PRIVATE:
        // Backing store used to implement the circular queue
        char storage[BUFFER_SIZE];

        // Position within storage[] where the next log message can be placed
        char *recordHead;

        // Hints as to how much contiguous space one can allocate without
        // rolling over the log head or stalling behind the consumer
        int hintContiguousRecordSpace;

        // Amount of contiguous-space reserved in the buffer via reserveAlloc()
        int reservedSpace;

        // Metric: number of reserveAlloc() failures
        uint64_t allocFailures;

        // An extra cache-line to separate the variables that are primarily
        // updated/read by the producer (above) from the ones by the
        // consumer(below)
        static const uint32_t BYTES_PER_CACHE_LINE = 64;
        char cacheLine[BYTES_PER_CACHE_LINE];

        // Position within the storage buffer where the consumer can start
        // consuming events from. This value is updated/read only be the
        // consumer.
        char *readHead;

        // The consumer's cached version of the recordHead; it exists to
        // prevent extraneous cache-coherence messages between the consumer
        // and the producer as its only updated when the consumer runs out
        // of space.
        char *cachedRecordHead;

    public:
        // Determines the byte size of the staging buffer
        static const uint32_t BUFFER_SIZE = BufferUtils::BUFFER_SIZE;

        /**
         * Attempt to reserve some contiguous space for a new record entry
         * and its corresponding arguments in the staging buffer.
         *
         * Note that the user should invoke finishAlloc() to make this entry
         * visible to the consumer and the user shall not invoke reserveAlloc()
         * again until they have finishAlloc()ed.
         *
         * \return  - pointer to valid space; NULL if not enough space.
         */
        inline BufferUtils::RecordEntry*
        reserveAlloc(int argBytes) {
            // User should not be able to alloc twice in a row
            assert(reservedSpace == 0);

            // There's a subtle point here, all the checks for remaining
            // space are strictly < or >, not <= or => because if we allow
            // the record and print positions to overlap, we can't tell
            // if the buffer either completely full or completely empty.
            // Doing this check here ensures that == means completely empty.

            int reqSpace = sizeof(BufferUtils::RecordEntry) + argBytes;
            if (hintContiguousRecordSpace > reqSpace) {
                reservedSpace = reqSpace;
                return reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);
            }

            // Hint failed, have to read the readHead
            char *readPos = readHead;
            const char *endOfBuffer = storage + BUFFER_SIZE;

            if (recordHead >= readPos) {
                hintContiguousRecordSpace = endOfBuffer - recordHead;
                if (reqSpace < hintContiguousRecordSpace) {
                    reservedSpace = reqSpace;
                    return
                        reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);
                }

                // Is there enough space if we wrap around?
                if (readPos - storage > reqSpace) {
                    // Clear the last bit of the buffer so that consumer knows
                    // it's invalid and that it should wrap around
                    int bytesToClear = hintContiguousRecordSpace;
                    if (hintContiguousRecordSpace >
                            sizeof(BufferUtils::RecordEntry)) {
                        bytesToClear = sizeof(BufferUtils::RecordEntry);
                    }
                    bzero(recordHead, bytesToClear);

                    // Complete wrap around
                    hintContiguousRecordSpace = readPos - storage;
                    reservedSpace = reqSpace;
                    recordHead = storage;
                    return
                        reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);
                }

                ++allocFailures;
                return NULL;
            } else {
                hintContiguousRecordSpace = readPos - recordHead;
                if (hintContiguousRecordSpace > reqSpace) {
                    reservedSpace = reqSpace;
                    return
                        reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);
                }

                ++allocFailures;
                return NULL;
            }
        }

        /**
         * Complementary to reserveAlloc() that makes the space reserved
         * visible to the consumer.
         * 
         * \param re - RecordEntry that was return in reserveAlloc(), this is
         *              only used for assertion checks.
         */
        inline void
        finishAlloc(BufferUtils::RecordEntry* re) {
            assert(reservedSpace > 0);
            assert(hintContiguousRecordSpace >= reservedSpace);
            assert(recordHead == reinterpret_cast<char*>(re));
            assert(re->entrySize == reservedSpace);

            recordHead += reservedSpace;
            hintContiguousRecordSpace -= reservedSpace;
            reservedSpace = 0;
        }

        /**
         * Attempt to peek at the next RecordEntry within the staging buffer.
         * The consumer should invoke consumeNext() to advance peek to the
         * next entry.
         *
         * \param allowRecursion  - Internal parameter to limit recursive calls
         *
         * \return      The next recordEntry, null if there isn't one.
         */
        BufferUtils::RecordEntry*
        peek(bool allowRecursion=true)
        {
            static const char *endOfBuffer = storage + BUFFER_SIZE;
            BufferUtils::RecordEntry *re = NULL;

            if (cachedRecordHead >= readHead) {
                int readableBytes = cachedRecordHead - readHead;
                if (readableBytes >= sizeof(BufferUtils::RecordEntry)) {

                    re = reinterpret_cast<BufferUtils::RecordEntry*>(readHead);
                    if (readableBytes < re->entrySize || re->fmtId == 0)
                        return nullptr;

                    return re;
                }

                // Will updating the recordHead help?
                if (cachedRecordHead == recordHead)
                    return NULL;

                // Since we updated the recordHead, we don't know whether
                // it's still ahead of the readHead, so we perform a recursive
                // call.
                cachedRecordHead = recordHead;
                if (!allowRecursion)
                    return NULL;

                return peek(false);
            } else {
                int readableBytes = endOfBuffer - readHead;
                if (readableBytes >= sizeof(BufferUtils::RecordEntry)) {
                    re = reinterpret_cast<BufferUtils::RecordEntry*>(readHead);
                    if (re->fmtId > 0) {
                        // Having a record entry spanning a roll-over should
                        // not be possible.
                        assert (re->entrySize <= readableBytes);
                        return re;
                    }
                }

                // If any of the cases above fail, then a roll-over is needed
                readHead = storage;
                return peek(true);
            }
        }

        /**
         * Consumes the next RecordEntry and advances the internal pointers
         * for peek().
         */
        void
        consumeNext() {
            BufferUtils::RecordEntry* re = peek();
            if (re)
                readHead += re->entrySize;
        }

        /**
         * Return the number of failed allocations due to out of space
         * 
         * \return - number of failed allocations
         */
        uint64_t getNumberOfAllocFailures() {
            return allocFailures;
        }

        StagingBuffer()
            : storage()
            , recordHead(storage)
            , hintContiguousRecordSpace(BUFFER_SIZE)
            , reservedSpace(0)
            , cacheLine()
            , readHead(storage)
            , cachedRecordHead(storage)
            , allocFailures(0)
        {
            bzero(storage, BUFFER_SIZE);
        }
    };

    // Allow access to the thread buffers and mutex protecting them.
    friend LogCompressor;

};  // FastLogger
}; // namespace PerfUtils

#endif /* FASTLOGGER_H */

