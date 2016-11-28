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