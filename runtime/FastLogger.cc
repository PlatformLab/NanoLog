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

#include "FastLogger.h"

namespace PerfUtils {

// Define the static members of FastLogger here
LogCompressor* FastLogger::compressor = NULL;
__thread FastLogger::StagingBuffer* FastLogger::stagingBuffer = NULL;

std::vector<FastLogger::StagingBuffer*> FastLogger::threadBuffers;
std::mutex FastLogger::bufferMutex;

// Slow path
char*
FastLogger::StagingBuffer::reserveSpaceInternal(size_t nbytes, bool blocking)
{
    const char *endOfBuffer = storage + BUFFER_SIZE;

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
            //TODO(syang0) I think a lock is needed here in case of reordering
            endOfRecordedSpace = producerPos;
            producerPos = storage;
        }

        minFreeSpace = cachedReadPos - producerPos;

        // Needed to prevent infinite loops in tests
        if (!blocking && minFreeSpace <= nbytes)
            return nullptr;
    }

    return producerPos;
}

//TODO(syang0) There's a problem here, we're going to have to peek
// twice to get everything in case of a wrap around.
/**
 * Peek at the data available for consumption within the stagingBuffer.
 * The consumer should also invoke consume() to release space back
 * to the producer. This can and should be done piece-wise where a
 * large peek can be consume()-ed in smaller pieces to prevent blocking
 * the producer.
 *
 * \param[out] bytesAvailable
 *      Number of bytes consumable
 *
 * \return
 *      Pointer to the consumable space
 */
char*
FastLogger::StagingBuffer::peek(uint64_t* bytesAvailable)
{
    // Save a consistent copy of recordHead
    char *cachedRecordHead = producerPos;

    if (cachedRecordHead < consumerPos) {
        //TODO(syang0) LocK? See reserveSpaceInternal
        *bytesAvailable = endOfRecordedSpace - consumerPos;

        if (*bytesAvailable > 0)
            return consumerPos;

        // Roll over
        consumerPos = storage;
    }

    *bytesAvailable = cachedRecordHead - consumerPos;
    return consumerPos;
}


/**
 * Stops the LogCompressor thread.
 *
 * Note that this does not guarantee that all the pending log messages
 * will be persisted to disk before exit. Thus one must invoke sync()
 * before exit() for durability.
 *
 * Also note that this is just a shim for benchmarking. In a real system,
 * this should never be invoked.
 */
void
FastLogger::exit()
{
    PerfUtils::LogCompressor *cmp = NULL;
    {
        std::lock_guard<std::mutex> guard(bufferMutex);
        cmp = compressor;
    }

    if (cmp)
        compressor->exit();
}

/**
 * Blocks until the FastLogger system is able to persist the pending log
 * messages that occurred before this invocation to disk. Note that this
 * operation has similar behavior to a "non-quiescent checkpoint" in a
 * database which means log messages occurring after this point this
 * invocation may also be persisted in a multi-threaded system.
 */
void
FastLogger::sync()
{
    // The compressor object should never be destructed/moved, so it should
    // be safe to use the lock to only grab the compressor pointer
    PerfUtils::LogCompressor *cmp = NULL;
    {
        std::lock_guard<std::mutex> guard(bufferMutex);
        cmp = compressor;
    }

    if (cmp)
        compressor->sync();
}
} // PerfUtils