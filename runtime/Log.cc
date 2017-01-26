/* Copyright (c) 2017 Stanford University
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

#include <algorithm>

#include <bits/algorithmfwd.h>
#include <fcntl.h>
#include <vector>

#include "BenchmarkConfig.h"
#include "Log.h"
#include "GeneratedCode.h"

/**
  * Friendly names for each #LogLevel value.
  * Keep this in sync with the LogLevel enum in NanoLog.h.
  */
static const char* logLevelNames[] = {"(none)", "ERROR", "WARNING",
                                       "NOTICE", "DEBUG"};

/**
 * Encoder constructor. The construction of an Encoder should logically
 * correlate with the start of a new log file as it will embed unique metadata
 * information at the beginning of log file/buffer.
 *
 * \param buffer
 *      Buffer to encode log messages and metadata to
 * \param bufferSize
 *      The number of bytes usable within the buffer
 * \param skipCheckpoint
 *      Optional parameter to skip embedding metadata information at the
 *      beginning of the buffer. This parameter should never bet set except
 *      in unit tests.
 */
Log::Encoder::Encoder(char *buffer,
                                size_t bufferSize,
                                bool skipCheckpoint)
    : backing_buffer(buffer)
    , writePos(buffer)
    , endOfBuffer(buffer + bufferSize)
    , lastBufferIdEncoded(-1)
    , currentExtentSize(nullptr)
    , lastTimestamp(0)
{
    assert(buffer);

    // Start the buffer off with a checkpoint
    if (!skipCheckpoint) {
        assert(static_cast<uint64_t>(endOfBuffer - buffer)
                                                        >= sizeof(Checkpoint));
        insertCheckpoint(&writePos, endOfBuffer);
    }
}

/**
 * Interprets the uncompressed log messages (created by the compile-time
 * generated code) contained in the *from buffer and compresses them to
 * the internal buffer. The encoded data can later be retrieved via swapBuffer()
 *
 * \param from
 *      A buffer containing the uncompressed log message created by the
 *      compile-time generated code
 * \param nbytes
 *      Maximum number of bytes that can be extracted from the *from buffer
 * \param bufferId
 *      The runtime thread/StagingBuffer id to associate the logs with
 * \param newPass
 *      Indicates that this encoding correlates with starting a new pass
 *      through the runtime StagingBuffers. In other words, this should be true
 *      on the first invocation of this function after the runtime has checked
 *      and encoded all the StagingBuffers at least once.
 * \param[out] numEventsCompressed
 *      adds the number of log messages processed in this invocation
 *
 * \return
 *      The number of bytes read from *from. A value of 0 indicates there is
 *      insufficient space in the internal buffer to fit the compressed message.
 */
long
Log::Encoder::encodeLogMsgs(char *from,
                                    uint64_t nbytes,
                                    uint32_t bufferId,
                                    bool newPass,
                                    uint64_t *numEventsCompressed)
{
    if (lastBufferIdEncoded != bufferId || newPass)
        if(!encodeBufferExtentStart(bufferId, newPass))
            return 0;

    long remaining = nbytes;
    long numEventsProcessed = 0;
    char *bufferStart = writePos;

    if (BENCHMARK_DISABLE_COMPACTION) {
        long freeBytes = endOfBuffer - writePos;
        long writeSize = std::min(freeBytes, remaining);
        memcpy(writePos, from, writeSize);

        *currentExtentSize += downCast<uint32_t>(writePos - bufferStart);

        writePos += writeSize;
        return writeSize;
    }

    while (remaining > 0) {
        UncompressedEntry *entry
                = reinterpret_cast<UncompressedEntry*>(from);

        if (entry->entrySize > remaining)
            break;

        // Check for free space using the worst case assumption that
        // none of the arguments compressed and there are as many Nibbles
        // as there are data bytes.
        uint32_t maxCompressedSize = downCast<uint32_t>(2*entry->entrySize
                                + sizeof(Log::UncompressedEntry));
        if (maxCompressedSize > endOfBuffer - writePos)
            break;

        compressLogHeader(entry, &writePos, lastTimestamp);
        lastTimestamp = entry->timestamp;

        size_t argBytesWritten =
            GeneratedFunctions::compressFnArray[entry->fmtId](entry, writePos);
        writePos += argBytesWritten;

        remaining -= entry->entrySize;
        from += entry->entrySize;

        ++numEventsProcessed;
    }

    assert(currentExtentSize);
    *currentExtentSize += downCast<uint32_t>(writePos - bufferStart);

    if (numEventsCompressed)
        *numEventsCompressed += numEventsProcessed;

    return nbytes - remaining;
}

/**
 * Internal function that encodes a marker indicating that all log messages
 * after this point (but after the next marker) belong to a particular buffer.
 * This is only used in encodeLogMsgs, but is separated out to allow for easy
 * unit testing with its counterpart in Decoder.
 *
 * \param bufferId
 *      Buffer id to encode in the extent
 * \param newPass
 *      Indicates whether this buffer change also correlates with the start of
 *      a new pass through the runtime StagingBuffers by the caller
 * \return
 *      Whether the operation completed successfully (true) or failed due to
 *      lack of space in the internal buffer (false)
 */
bool
Log::Encoder::encodeBufferExtentStart(uint32_t bufferId, bool newPass)
{
    // For size check, assume the worst case of no compression on bufferId
    char *writePosStart = writePos;
    if (sizeof(BufferExtent) + sizeof(bufferId) >
            static_cast<size_t>(endOfBuffer - writePos))
        return false;

    BufferExtent *tc = reinterpret_cast<BufferExtent*>(writePos);
    writePos += sizeof(BufferExtent);

    tc->entryType = EntryType::BUFFER_EXTENT;
    tc->wrapAround = newPass;

    if (bufferId < (1<<4)) {
        tc->isShort = true;
        tc->threadIdOrPackNibble = 0x0F & bufferId;
    } else {
        tc->isShort = false;
        tc->threadIdOrPackNibble = 0x0F & BufferUtils::pack<uint32_t>(
                                                        &writePos, bufferId);
    }

    tc->length = downCast<uint32_t>(writePos - writePosStart);
    currentExtentSize = &(tc->length);
    lastBufferIdEncoded = bufferId;
    lastTimestamp = 0;

    return true;
}

/**
 * Retrieve the number of bytes encoded in the internal buffer
 *
 * \return
 *      Number of bytes encoded in the internal buffer
 */
size_t
Log::Encoder::getEncodedBytes() {
    return writePos - backing_buffer;
}

/**
 * Releases the internal buffer and replaces it with a different one.
 *
 * \param inBuffer
 *      the new buffer to swap in
 * \param inSize
 *      the amount of free space usable in the buffer
 * \param[out] outBuffer
 *      returns a pointer to the original buffer
 * \param[out] outLength
 *      returns the number of bytes of encoded data in the outBuffer
 */
void
Log::Encoder::swapBuffer(char *inBuffer, size_t inSize, char **outBuffer,
                                            size_t *outLength, size_t *outSize)
{
    char *ret = backing_buffer;
    size_t size = writePos - backing_buffer;
    size_t originalSize = endOfBuffer - backing_buffer;

    backing_buffer = inBuffer;
    writePos = inBuffer;
    endOfBuffer = inBuffer + inSize;
    lastBufferIdEncoded = -1;
    currentExtentSize = nullptr;
    lastTimestamp = 0;

    if (outBuffer)
        *outBuffer = ret;

    if (outLength)
        *outLength = size;

    if (outSize)
        *outSize = originalSize;
}

/**
 * Decoder constructor.
 *
 * Due to the large amount of memory needed to buffer log statements, the
 * decoder is intended to be constructed once and then re-used via open().
 */
Log::Decoder::Decoder()
    : filename(nullptr)
    , inputFd(nullptr)
    , checkpoint()
    , freeBuffers()
{}

/**
 * Opens a compressed log with contents created by Encoder.
 *
 * \param filename
 *      Compressed log file to open
 * \return
 *      True if success, false if the log file is not valid or cannot be opened
 */
bool
Log::Decoder::open(const char *filename) {
    inputFd = fopen(filename, "rb");
    if (!inputFd) {
        return false;
    }

    if(!readCheckpoint(checkpoint, inputFd)) {
        printf("Error: Could not read initial checkpoint, "
                "the compressed log may be corrupted.\r\n");
        fclose(inputFd);
        inputFd = 0;
        return false;
    }

    this->filename = filename;
    return true;
}
/**
 * Decoder destructor
 */
Log::Decoder::~Decoder() {
    if (inputFd)
        fclose(inputFd);

    filename = nullptr;
    inputFd = nullptr;

    for (BufferFragment *bf : freeBuffers)
        delete bf;

    freeBuffers.clear();
}

/**
 * Internal function to allocate a BufferFragment
 *
 * \return
 *      Allocated BufferFragment
 */
Log::Decoder::BufferFragment*
Log::Decoder::allocateBufferFragment()
{
    if (!freeBuffers.empty()) {
        BufferFragment *ret = freeBuffers.back();
        freeBuffers.pop_back();
        return ret;
    }

    return new BufferFragment();
}

/**
 * Internal function to store a BufferFragment on the free list
 *
 * \param bf
 *      BufferFragment to store
 */
void
Log::Decoder::freeBufferFragment(BufferFragment *bf)
{
    bf->reset();
    freeBuffers.push_back(bf);
}

// BufferFragment constructor
Log::Decoder::BufferFragment::BufferFragment()
    : storage()
    , validBytes(0)
    , runtimeId(-1)
    , readPos(nullptr)
    , endOfBuffer(nullptr)
    , hasMoreLogs(false)
    , nextLogId(-1)
    , nextLogTimestamp(0)
{
}

/**
 * Resets the state of the BufferFragment so that the data cannot be reused
 */
void
Log::Decoder::BufferFragment::reset()
{
    validBytes = 0;
    runtimeId = -1;
    readPos = nullptr;
    endOfBuffer = nullptr;
    hasMoreLogs = false;
}
/**
 * Read in the next buffer fragment from the compressed log. If an error occurs
 * the file descriptor will be in an undefined state.
 *
 * \param fd
 *      File stream to read it from
 * \param[out] wrapAround
 *      Indicates whether a wrap around was indicated in the log or not.
 *
 * \return
 *      indicates whether the operation succeeded (true) or failed due to
 *      a malformed log data.
 */
bool
Log::Decoder::BufferFragment::readBufferExtent(FILE *fd, bool *wrapAround) {
    validBytes = fread(storage, 1, sizeof(BufferExtent), fd);
    BufferExtent *be = reinterpret_cast<BufferExtent*>(storage);

    if (be->entryType != EntryType::BUFFER_EXTENT ||
            validBytes < sizeof(BufferExtent) ||
            be->length > sizeof(storage)) {
        reset();
        return false;
    }

    assert(be->length >= validBytes);
    uint64_t remaining = be->length - validBytes;
    validBytes += fread(storage + validBytes, 1, remaining, fd);

    if (validBytes != be->length) {
        reset();
        return false;
    }

    readPos = storage + sizeof(BufferExtent);
    endOfBuffer = storage + validBytes;

    if (be->isShort)
        runtimeId = be->threadIdOrPackNibble;
    else
        runtimeId = BufferUtils::unpack<uint32_t>(
                                            &readPos, be->threadIdOrPackNibble);

    if (wrapAround)
        *wrapAround = be->wrapAround;

    hasMoreLogs = decompressLogHeader(&readPos, 0, nextLogId, nextLogTimestamp);
    if (!hasMoreLogs)
        reset();

    return hasMoreLogs;
}

/**
 * Attempt to read back the next log statement contained in the BufferFragment,
 * output the original log message to outputFd, and if applicable, run an
 * aggregation function on the log message.
 *
 * The aggregation function should take in the same arguments as the original
 * invocation of NANO_LOG. For example, NANO_LOG("The number is %d." num) should
 * have the function signature of (const char *fmtString, int num).
 *
 * \param outputFd
 *      File descriptor to output the log messages to
 * \param[in/out] logMsgsPrinted
 *      The number of lines outputted to outputFd
 * \param lastTimestamp
 *      The timestamp of the last log message to be outputted (this is used
 *      to print time differences).
 * \param checkpoint
 *      The checkpoint containing rdtsc-to-time mapping this function should use
 * \param aggregationFilterId
 *      The logId to target running aggregationFn on
 * \param aggregationFn
 *      This is an aggregation function that can be passed to any log messages
 *      matching aggregationFilterId
 *
 * \return
 *      true indicates there are more log messages to decompress, false
 *      indicates that there is not (whether it be due to end-of-file or
 *      file corruption).
 */
bool
Log::Decoder::BufferFragment::decompressNextLogStatement(FILE *outputFd,
                                        uint64_t &logMsgsPrinted,
                                        uint64_t &lastTimestamp,
                                        const Checkpoint &checkpoint,
                                        long aggregationFilterId,
                                        void (*aggregationFn)(const char*, ...))
{
    if (readPos > endOfBuffer || !hasMoreLogs)
        return false;

    if (outputFd) {
        char timeString[32];

        // Convert to relative time
//        double timeDiff;
//        if (nextLogTimestamp >= lastTimestamp)
//            timeDiff = 1.0e9*PerfUtils::Cycles::toSeconds(
//                                    nextLogTimestamp - lastTimestamp,
//                                    checkpoint.cyclesPerSecond));
//        else
//            timeDiff = -1.0e9*PerfUtils::Cycles::toSeconds(
//                                    lastTimestamp - nextLogTimestamp,
//                                    checkpoint.cyclesPerSecond));
//        if (logMsgsPrinted == 0)
//            timeDiff = 0;
//
//        fprintf(outputFd, "%4ld) +%12.2lf ns ", logMsgsPrinted, timeDiff);

        // Convert to absolute time
        double secondsSinceCheckpoint = PerfUtils::Cycles::toSeconds(
               nextLogTimestamp - checkpoint.rdtsc, checkpoint.cyclesPerSecond);
        uint64_t wholeSeconds = static_cast<uint64_t>(secondsSinceCheckpoint);
        double nanos = 1.0e9*(secondsSinceCheckpoint
                                        - static_cast<double>(wholeSeconds));

        std::time_t absTime = wholeSeconds + checkpoint.unixTime;
        std::tm *tm = localtime(&absTime);
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", tm);

        // Output the context
        struct GeneratedFunctions::LogMetadata meta =
                                GeneratedFunctions::logId2Metadata[nextLogId];
        fprintf(outputFd,"%s.%09.0lf %s:%u %s[%u]: "
                                        , timeString
                                        , nanos
                                        , meta.fileName
                                        , meta.lineNumber
                                        , logLevelNames[meta.logLevel]
                                        , runtimeId);
    }

    void (*aggFn)(const char*, ...) = nullptr;
    if (aggregationFilterId == nextLogId)
        aggFn = aggregationFn;

    GeneratedFunctions::decompressAndPrintFnArray[nextLogId](&readPos,
                                                            outputFd, aggFn);

    lastTimestamp = nextLogTimestamp;
    logMsgsPrinted++;

    if (readPos >= endOfBuffer)
        return false;

    hasMoreLogs = decompressLogHeader(&readPos, nextLogTimestamp,
                                        nextLogId, nextLogTimestamp);
    return hasMoreLogs;
}

/**
 * Whether one can invoke decompressNextLogStatement or not
 */
bool
Log::Decoder::BufferFragment::hasNext() {
    return hasMoreLogs;
}

/**
 * Return the timestamp of the next log message that can be outputted. It is
 * the responsibility of the caller to ensure that the BufferFragment contains
 * a next log message (i.e. if decompressNextLogStatement returns true).
 */
uint64_t
Log::Decoder::BufferFragment::getNextLogTimestamp() const
{
    assert(readPos <= endOfBuffer && validBytes > 0);
    return nextLogTimestamp;
}

/**
 * Decompress the log file that was open()-ed and print the message out in
 * an arbitrary order (i.e. dependent on runtime implementation and not
 * necessarily in chronological order).
 *
 * \param outputFd
 *      The file descriptor to print the log messages to
 * \param logMsgsToPrint
 *      Limit the number of log messages to print (use -1 to print all)
 * \param logMsgsPrinted
 *      Actual number of log messages printed
 * \param callRCDF
 *      Store the rdtsc() difference between every invocation of the log
 *      function. Note values may be negative due to unorderedness
 * \param aggregationTargetId
 *      Target logId to run the aggregation function on
 * \param aggregationFn
 *      Aggregation function to run on targetId. The signature of the function
 *      should be the same as the original NANO_LOG invocation that created
 *      the log message. For example,
 *          NANO_LOG("number %d, string %s, float %f", num, str, flo)
 *      should have the signature
 *          aggregation(const char*, int, const char*, float)
 *
 * \return
 *      true indicates the operation succeeded without problems.
 *      false indicates that there was an error and an incomplete log was
 *      outputted to outputFd;
 */
bool
Log::Decoder::internalDecompressUnordered(FILE* outputFd,
                                        uint64_t logMsgsToPrint,
                                        uint64_t *logMsgsPrinted,
                                        std::vector<uint64_t> *callRCDF,
                                        uint32_t aggregationTargetId,
                                        void(*aggregationFn)(const char*,...))
{
    if (logMsgsPrinted)
        *logMsgsPrinted = 0;

    if (!filename || !inputFd)
       return false;

    if (callRCDF)
        callRCDF->reserve(100000000);

    // Hack for RCDF-ing multi-threads
    std::vector<uint64_t> lastTimestamps;

    bool good = true;
    uint64_t linesPrinted = 0;
    BufferFragment *bf = allocateBufferFragment();
    while(!feof(inputFd) && good && linesPrinted < logMsgsToPrint) {
        bool wrapAround = false;

        EntryType entry = peekEntryType(inputFd);
        switch (entry) {
            case EntryType::BUFFER_EXTENT:
            {
                if (!bf->readBufferExtent(inputFd, &wrapAround)){
                    printf("Internal Error: Corrupted BufferExtent\r\n");
                    break;
                }

                bool hasMore = true;
                while (hasMore && linesPrinted < logMsgsToPrint) {
                    // Hack for RCDF-ing multi-threads
                    while (lastTimestamps.size() <= bf->runtimeId)
                        lastTimestamps.push_back(0);

                    // Cheat so that the first timestamp ever encountered is 0
                    if (lastTimestamps[bf->runtimeId] == 0)
                        lastTimestamps[bf->runtimeId] = bf->nextLogTimestamp;
                    uint64_t lastLastTimestamp = lastTimestamps[bf->runtimeId];
                    hasMore = bf->decompressNextLogStatement(outputFd,
                                                    linesPrinted,
                                                    lastTimestamps[bf->runtimeId],
                                                    checkpoint,
                                                    aggregationTargetId,
                                                    aggregationFn);
                    if (callRCDF)
                        callRCDF->push_back(lastTimestamps[bf->runtimeId] - lastLastTimestamp);
                }
                break;
            }
            case EntryType::CHECKPOINT:
                if (!readCheckpoint(checkpoint, inputFd))
                    good = false;
                else if (outputFd)
                    fprintf(outputFd, "\r\n# New execution started\r\n");

                break;
            case EntryType::LOG_MSG:
                printf("Internal Error: Found a log message outside a "
                        "BufferFragment. Log may be malformed!\r\n");
                good = false;
                break;
            case EntryType::INVALID:
                // Consume whitespace
                while (!feof(inputFd) && peekEntryType(inputFd) == INVALID)
                    fgetc(inputFd);
                break;
        }
    }

    if (outputFd)
        fprintf(outputFd, "\r\n\r\n# Decompression Complete after printing %lu "
            "log messages\r\n", linesPrinted);

    if (logMsgsPrinted)
        *logMsgsPrinted = linesPrinted;

    freeBufferFragment(bf);
    return true;
}

/**
 * Decompress the log file that was open()-ed and print the log messages out
 * in chronological order.
 *
 * \param outputFd
 *      The file descriptor to print the log messages to
 * \param logMsgsToPrint
 *      Number of log messages to print
 * \param logMsgsPrinted
 *      Actual number of log messages printed
 *
 * \return
 *      true indicates the operation succeeded without problems.
 *      false indicates that there was an error and an incomplete log was
 *      outputted to outputFd;
 */
bool
Log::Decoder::internalDecompressOrdered(FILE* outputFd,
                                                uint64_t logMsgsToPrint,
                                                uint64_t *logMsgsPrinted)
{
    if (logMsgsPrinted)
        *logMsgsPrinted = 0;

    if (!filename || !inputFd)
        return false;

    // In ordered decompression, we must sort the entries by time which means
    // we need to buffer in 3 rounds of NanoLog output. We need more than one
    // round of output because the compression is non-quiescent, which means
    // that as we're outputting the nth buffer, new entries may be added to
    // n-1 and n, and the entries in n could logically come /before/ the new
    // entries added to n-1. The reason why we need at least 3 is due to an
    // implementation detail in StagingBuffer whereby one peek() does not
    // return all the data and at least 2 peek()'s are needed to deplete a
    // buffer.
    static const uint32_t numStages = 3;
    std::vector<BufferFragment*> stages[numStages];

    // Running number of stages being kept in stages
    uint32_t stagesBuffered = 0;

    // Indicates that all stages must be depleted before continuing
    // processing the log file. This should only be true when we detect
    // the start of a new execution(s) log appended to inputFd or we
    // reached the end of the current file
    bool mustDepleteAllStages = false;

    // Indicates that the log message was malformed
    bool malformed = false;

    // Last timestamp printed (this primarily used to compute differences);
    uint64_t lastTimestamp = 0;

    // Skip past first checkpoint, but just that
    fseek(inputFd, sizeof(Checkpoint), SEEK_SET);

    uint64_t linesPrinted = 0;

    while (!feof(inputFd) && !malformed && linesPrinted < logMsgsToPrint) {

        // Step 1: Read in up to 3 stages of BufferFragments
        mustDepleteAllStages = false;
        while (!feof(inputFd) && !malformed && !mustDepleteAllStages) {
            EntryType entry = peekEntryType(inputFd);
            bool newStage = false;

            switch (entry) {
                case EntryType::BUFFER_EXTENT:
                {
                    BufferFragment *bf = allocateBufferFragment();
                    if (bf->readBufferExtent(inputFd, &newStage)) {
                        stages[stagesBuffered].push_back(bf);
                    }
                    else
                        malformed = true;
                    break;
                }
                case EntryType::CHECKPOINT:
                    // New log start detected! Deplete all the logs and then
                    // parse the checkpoint
                    if (stages[0].empty()) {
                        // We're safe, all the stages are empty
                        if(readCheckpoint(checkpoint, inputFd)) {
                            fprintf(outputFd,"\r\n# New execution started\r\n");
                        } else {
                            malformed = true;
                        }
                    } else {
                        mustDepleteAllStages = true;
                    }
                    break;
                case EntryType::LOG_MSG:
                    printf("Internal Error: Found a log message outside a "
                            "BufferFragment. Log may be malformed!\r\n");
                    malformed = true;
                    break;
                case EntryType::INVALID:
                    // Consume whitespace
                    while (!feof(inputFd) && peekEntryType(inputFd) == INVALID)
                        fgetc(inputFd);
                    break;
            }

            if (newStage && (++stagesBuffered) == numStages) {
                break;
            }

            if (feof(inputFd))
                ++stagesBuffered;
        }

        if (feof(inputFd))
            mustDepleteAllStages = true;

        // Step 2: Sort all BufferFragments within the stages from
        // front=max to back=min
        for (auto &stage : stages) {
            std::sort(stage.begin(), stage.end(),
                [](const BufferFragment *a, const BufferFragment *b) -> bool
                {
                    return a->getNextLogTimestamp() > b->getNextLogTimestamp();
                });
        }

        // Step 3: Deplete the first stage
        while (true) {
            // Step a: Find the minimum amongst the stages
            std::vector<BufferFragment*> *minStage = nullptr;
            for (uint32_t i = 0; i < stagesBuffered; ++i) {
                if (stages[i].empty())
                    continue;

                uint64_t next = stages[i].back()->getNextLogTimestamp();
                if (minStage == nullptr ||
                        next < minStage->back()->getNextLogTimestamp()) {
                    minStage = &(stages[i]);
                }
            }

            // If nothing was found, we're done
            if (minStage == nullptr)
                break;

            // Step b: Output the log message
            BufferFragment *bf = minStage->back();
            bool hasMore = bf->decompressNextLogStatement(outputFd,
                                                            linesPrinted,
                                                            lastTimestamp,
                                                            checkpoint);

            if (hasMore) {
                std::sort(minStage->begin(), minStage->end(),
                    [](const BufferFragment *a, const BufferFragment *b) -> bool
                    {
                        return a->getNextLogTimestamp() >
                                                       b->getNextLogTimestamp();
                    });
            } else {
                // Buffer is depleted, remove it
                minStage->pop_back();
                freeBufferFragment(bf);
            }

            // Step c: Check for exit conditions
            if (stages[0].empty()) {
                for (uint32_t i = 0; i < stagesBuffered - 1; ++i) {
                    stages[i] = stages[i+1];
                }
                stages[stagesBuffered - 1].clear();

                --stagesBuffered;
                if (stages[0].empty() && !mustDepleteAllStages)
                    break;
            }
        }
    }


    if (logMsgsPrinted)
        *logMsgsPrinted = linesPrinted;

    if (outputFd)
        fprintf(outputFd, "\r\n\r\n# Decompression Complete after printing %lu "
            "log messages\r\n", linesPrinted);

    return malformed;
}

/**
 * Decompress and print the file open()-ed to a file descriptor. This will
 * output the log messages in sorted order (by machine time).
 *
 * \param outputFd
 *      File descriptor to output the log messages to
 * \param logMsgsToPrint
 *      Maximum number of log messages to output to the descriptor
 * \return
 *      true indicates success, false indicates an error occurred and a subset
 *      of logs may be outputted.
 */
bool
Log::Decoder::decompressTo(FILE* outputFd, uint64_t logMsgsToPrint) {
    //TODO(syang0) change the API such that successive calls to decompress
    // will continue the decompression at a specific point.
    return internalDecompressOrdered(outputFd, logMsgsToPrint);
}

/**
 * Decompress the file open()-ed to a file descriptor. This invocation will
 * not attempt to sort the log entries by time, but otherwise functions
 * in the same way as decompressTo().
 *
 * \param outputFd
 *      File descriptor to output the log messages to
 * \param logMsgsToPrint
 *      Number of log messages to print before returning
 * \return
 *      true indicates success, false indicates an error occurred and a subset
 *      of logs may be outputted.
 */
bool
Log::Decoder::decompressUnordered(FILE* outputFd, uint64_t logMsgsToPrint) {
    return internalDecompressUnordered(outputFd, logMsgsToPrint);
}
