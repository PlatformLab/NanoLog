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

#include <ctime>
#include <vector>

#include <assert.h>
#include <stdio.h>

#include "Config.h"
#include "Common.h"
#include "Cycles.h"
#include "Packer.h"
#include "TestUtil.h"
#include "Util.h"
#include "NanoLog.h"

#ifndef LOG_H
#define LOG_H
/**
 * The Log namespace contains a collection of data structures and functions
 * to manage the metadata in the various buffers of the NanoLog system.
 * Here the term "compressed log" shall be used to refer to the runtime output
 * of the NanoLog system and the "uncompressed log" refers to the entries within
 * the StagingBuffers.
 *
 * The rough interaction between the various components of NanoLog involving
 * buffers is detailed in the diagrams below:
 *
 * At runtime:
 * (Generated Log code) ==> StagingBuffer ==> Encoder ==> (Compressed Log File)
 *
 * At post execution/decompression
 * (Compressed Log File) ==> Decoder ===> (Human-Readable Output)
 *
 * The format of the StagingBuffer looks something like this:
 * **************************
 * * UncompressedLogEntry   *
 * **************************
 * * Uncompressed Arguments *
 * **************************
 * *        .....           *
 *
 * Here, the format of UncompressedLogEntry is controlled by this file, but the
 * Uncompressed arguments are variable size and its format is controlled by
 * the generated code. We can only interact with them via the compressFnArray
 * and dceompressAndPrintFnArray's defined in BufferStuffer.h
 *
 * The format of the Compressed Log File is determined by the Encoder and
 * Decoder classes below.
 */
namespace Log {
    /**
     * Marks the beginning of a log entry within the StagingBuffer waiting
     * for compression. Every instance of this header in the StagingBuffer
     * corresponds to a user invocation of the log function in the NanoLog
     * system and thus every field is uncompressed to lower the compute time
     * for that invocation.
     */
    struct UncompressedEntry {
        // Uniquely identifies a log message by its format string and file
        // location, assigned at compile time by the preprocessor.
        uint32_t fmtId;

        // Number of bytes for this header and the various uncompressed
        // log arguments after it
        uint32_t entrySize;

        // Stores the rdtsc() value at the time of the log function invocation
        uint64_t timestamp;

        // After this header are the uncompressed arguments required by
        // the original format string
        char argData[];
    };

    /**
     * 2-bit enum that differentiates entries in the compressed log. These
     * two bits **MUST** be at the beginning of each entry in the log
     * to facilitate decoding.
     */
    enum EntryType : uint8_t {
        // Marks an invalid entry in the compressed log. This value is
        // deliberately 0 since \0's are used to pad the output to 512B
        // in the final output.
        INVALID = 0,

        // Indicates a CompressedRecordEntry that can be decompressed
        LOG_MSG = 1,

        // Indicates a BufferExtent struct
        BUFFER_EXTENT = 2,

        // Indicates a CheckPoint struct
        CHECKPOINT = 3
    };

    /**
     * All data structures in the compressed log must contain the EntryType
     * in the first two bits and this structure is used to extract those bits
     * when the type/identify is unknown.
     */
    struct UnknownHeader {
        uint8_t entryType:2;
        uint8_t other:6;
    } __attribute((packed));

    static_assert(sizeof(UnknownHeader) == 1, "Unknown Header should have a"
            " byte size of 1 to ensure that we can always determine the entry"
            " that follows with 1 byte peeks.");

    /**
     * Marks the beginning of a compressed log message and after this structure
     * comes the compressed arguments. The exact layout of the compressed
     * arguments is generated at compile-time (see the Python preprocessor),
     * but what comes immediately after this header are:
     *      (1-4 bytes) pack()-ed FormatId
     *      (1-8 bytes) pack()-ed rdtsc() timestamp
     *      (0-n bytes) arguments (determined by preprocessor)
     */
    struct CompressedEntry {
        // Byte representation of an EntryType::LOG_MSG to identify this as
        // a CompressedRecordEntry.
        uint8_t entryType:2;

        // Value returned by pack(formatId), subtracted by 1 to save space.
        // i.e. if pack() returned 2 this value is 1.
        // TODO(syang0) this is an abstraction failure; it's not treating
        // the value returned by pack() as a black box.
        uint8_t additionalFmtIdBytes:2;

        // Value returned by pack(timestamp)
        uint8_t additionalTimestampBytes:4;
    } __attribute__((packed));

    /**
     * Marker in the compressed log that indicates to which StagingBuffer/thread
     * the next contiguous chunk of LOG_MSG's belong to (up to the next
     * BufferExtent marker). These markers correspond to when the compression
     * thread finishes processing a peek() and moves on to outputting the next
     * StagingBuffer.
     */
    struct BufferExtent {
        // Byte representation of EntryType::BUFFER_EXTENT
        uint8_t entryType:2;

        // Indicates that the BufferChange also corresponds with a complete
        // pass through all the StagingBuffers at runtime. This information can
        // be used to determine the maximal temporal reordering that can occur
        // in the linear compressed log.
        uint8_t wrapAround:1;

        // A value of 1 indicates the next 4 bits are a threadId, else
        // the next 4 bits are the 4-bit result of a pack() operation.
        uint8_t isShort:1;

        // Value is either a 4-bit threadId or a Pack() result used to compact
        // the thread id that comes after this header.
        uint8_t threadIdOrPackNibble:4;

        // Indicates the byte size of the extent. This value is purposely
        // left unpack()-ed in-order to allow for delayed assignment (i.e.
        // after all the log messages have been processed).
        uint32_t length;

        // Returns the maximum size the BufferChange structure can be with
        // the Pack()-ed arguments.
        static constexpr uint32_t maxSizeOfHeader() {
            return sizeof(BufferExtent) + sizeof(uint32_t);
        }
    } __attribute__((packed));
    
    /**
     * Synchronization data structure in the compressed log that correlates the
     * runtime machine's rdtsc() with a wall time and the translation between
     * the two in the compressed log. This entry should typically only appear
     * once at the beginning of a new log. If multiple exist in the log file,
     * that means the file has been appended to.
     */
    struct Checkpoint {
        // Byte representation of an EntryType::CHECKPOINT
        uint64_t entryType:2;

        // rdtsc() time that corresponds with the unixTime below
        uint64_t rdtsc;

        // std::time() that corresponds with the rdtsc() above
        time_t unixTime;

        // Conversion factor between cycles returned by rdtsc() and 1 second
        double cyclesPerSecond;
    } __attribute__((packed));

    /**
     * Peek into a data array and identify the next entry embedded in the
     * compressed log (if there is one) and read it back.
     *
     * \param in
     *      Character array to peek into
     * \return
     *      An EntryType specifying what comes next
     */
    inline EntryType
    peekEntryType(const char *in) {
        int type = (uint8_t)(*in);
        if (type == EOF || type < 0 || type > 255)
            return EntryType::INVALID;

        UnknownHeader *header = reinterpret_cast<UnknownHeader*>(&type);
        return EntryType(header->entryType);
    }

    /**
     * Peek into the next byte in the file and identify the next entry embedded
     * in the compressed log (if there is one) and read it back.
     *
     * \param fd
     *      File descriptor to peek into
     * \return
     *      EntryType corresponding to the next entry in the compressed log
     */
    inline EntryType
    peekEntryType(FILE *fd) {
        int type = fgetc(fd);
        ungetc(type, fd);

        if (type == EOF || type < 0 || type > 255)
            return EntryType::INVALID;

        UnknownHeader *header = reinterpret_cast<UnknownHeader*>(&type);
        return EntryType(header->entryType);
    }

    /**
     * Extract the information from an UncompressedLogEntry and re-encode it
     * as a CompressedRecordEntry. Here, the provided lastTimestamp is provided
     * so that the CompressedRecordEntry only needs to store a time difference.
     *
     * This packs the metadata as follows:
     *      1 Byte of CompressedMetadata
     *      1-4 bytes of formatId
     *      1-8 bytes of rtdsc() difference
     *
     * \param re
     *      RecordEntry to compress
     * \param[in/out] out
     *      Output byte buffer to compress the entry into
     * \param lastTimestamp
     *      The timestamp of the last entry compacted. This value is used to
     *      compute the rdtsc() difference in the CompressedRecordEntry. A
     *      value of 0 shall be used for the first entry.
     *
     * \return
     *          Number of bytes written to out
     */
    inline size_t
    compressLogHeader(UncompressedEntry *re, char** out,
                        uint64_t lastTimestamp) {
        CompressedEntry *mo = reinterpret_cast<CompressedEntry*>(*out);
        *out += sizeof(CompressedEntry);

        mo->entryType = EntryType::LOG_MSG;

        // Bitmask is needed to prevent -Wconversion warnings
        mo->additionalFmtIdBytes = 0x03 & static_cast<uint8_t>(
                    BufferUtils::pack(out, re->fmtId) - 1);
        mo->additionalTimestampBytes = 0x0F & static_cast<uint8_t>(
                    BufferUtils::pack(out, static_cast<int64_t>(
                                            re->timestamp - lastTimestamp)));

        return sizeof(CompressedEntry)
                    + mo->additionalFmtIdBytes + 1
                    + (0x7 & mo->additionalTimestampBytes);
    }

    /**
     * Read in and decompress the log metadata from a CompressedRecordEntry
     *
     * \param in
     *      Character array to read the entry from
     * \param lastTimestamp
     *      Timestamp of the previous entry dencoded before this one
     * \param[out] logid
     *      The logId decoded
     * \param[out] timestamp
     *      The timestamp decoded
     * \return
     *      true indicates success, false indicates the next bytes do NOT
     *      encode a CompressedRecordEntry
     */
    inline bool
    decompressLogHeader(const char **in, uint64_t lastTimestamp,
                            uint32_t &logId, uint64_t &timestamp) {
        if (!(reinterpret_cast<const UnknownHeader*>(*in)->entryType
                                                        == EntryType::LOG_MSG))
            return false;

        CompressedEntry cre;
        memcpy(&cre, (*in), sizeof(CompressedEntry));
        (*in) += sizeof(CompressedEntry);

        logId = BufferUtils::unpack<uint32_t>(in,
                            static_cast<uint8_t>(cre.additionalFmtIdBytes + 1));
        timestamp = BufferUtils::unpack<int64_t>(in,
                            static_cast<uint8_t>(cre.additionalTimestampBytes));

        timestamp += lastTimestamp;

        return true;
    }

    /**
     * Insert a checkpoint into an output buffer. This operation is fairly
     * expensive so it is typically performed once per new log file.
     *
     * \param out[in/out]
     *      Output array to insert the checkpoint into
     * \param outLimit
     *      Pointer to the end of out (i.e. first invalid byte to write to)
     *
     * \return
     *      True if operation succeed, false if there's not enough space
     */
    static inline bool
    insertCheckpoint(char** out, char *outLimit) {
        if (static_cast<uint64_t>(outLimit - *out) < sizeof(Checkpoint))
            return false;

        Checkpoint *ck = reinterpret_cast<Checkpoint*>(*out);
        *out += sizeof(Checkpoint);

        ck->entryType = EntryType::CHECKPOINT;
        ck->rdtsc = PerfUtils::Cycles::rdtsc();
        ck->unixTime = std::time(nullptr);
        ck->cyclesPerSecond = PerfUtils::Cycles::getCyclesPerSec();

        return true;
    }

    /**
     * Extracts a checkpoint from a file descriptor.
     *
     * \param[out] cp
     *      Checkpoint structure to read the data into
     * \param fd
     *      Stream to read checkpoint from
     *
     * \return
     *      Whether the operation succeeded or failed due to lack of
     *      space/malformed log
     */
    inline bool
    readCheckpoint(Checkpoint &cp, FILE *fd) {
        cp.entryType = EntryType::INVALID;
        long numRead = fread(&cp, sizeof(Checkpoint), 1UL, fd);

        if (!numRead)
            return false;

        assert(cp.entryType == EntryType::CHECKPOINT);
        return true;
    }

    /**
     * Copies a primitive to a character array and bumps the array pointer.
     * This is used by the injected record code to save primitives to the
     * staging buffer.
     *
     * \param buffer
     *      Buffer to copy primitive to
     * \param val
     *      value of primitive
     */
    template<typename T>
    static inline void
    recordPrimitive(char* &buffer, T val) {
        *(reinterpret_cast<T*>(buffer)) = val;
        buffer += sizeof(T);
    }

    /**
     * Encapsulates the knowledge on how to transform UncompresedLogMessage's
     * created by the generated code into a compressed log for a Decoder
     * object to interpret later.
     * 
     * The intended usage pattern is for the user to create an Encoder object
     * per compressed log file that they intend to create, feed it a character
     * buffer array to operate on, and repeatedly invoke encodeLogMsgs() until
     * the character buffer is full at which point the caller should invoke
     * swapBuffer() to reclaim the character buffer (for I/O or other
     * operations).
     *
     * The encoder will lay out the compressed log in the following fashion:
     *  - There shall be a Checkpoint at the beginning of every file (and at
     *    every place where a new NanoLog execution appends to the log file)
     *  - Following the Checkpoint shall be a series of BufferExtents which
     *    identify to which runtime StagingBuffer/ThreadId to associate the
     *    log messages after it.
     *  - Log Messages live within BufferExtents and do not span BufferExtents.
     */
    class Encoder {
    PUBLIC:
        Encoder(char *buffer, size_t bufferSize, bool skipCheckpoint=false);

        long encodeLogMsgs(char *from, uint64_t nbytes,
                                    uint32_t bufferId, bool wrapAround,
                                    uint64_t *numEventsCompressed);
        size_t getEncodedBytes();
        void swapBuffer(char *inBuffer, size_t inSize,
                        char **outBuffer=nullptr, size_t *outLength=nullptr,
                        size_t *outSize=nullptr);

    PRIVATE:
        bool encodeBufferExtentStart(uint32_t bufferId, bool wrapAround);

        // Used to store the compressed log messages and related metadata
        char *backing_buffer;

        // Position within the backing_buffer where the next log entries or
        // log metadata should be placed.
        char *writePos;

        // Marks the first invalid byte in the backing_buffer
        char *endOfBuffer;

        // Saves the last bufferId encoded to determine when a new BufferExtent
        // needs to be encoded. A value of (-1) indicates no extent was encoded.
        uint32_t lastBufferIdEncoded;

        // A pointer to the last encoded BufferExtent's length to allow updating
        // the value as the user performs more encodeLogMsgs with the same id.
        uint32_t *currentExtentSize;

        // Saves the last timestamp encoded in the current BufferExtent
        uint64_t lastTimestamp;
    };
 
    /**
     * Encapsulates the knowledge for interpreting a compressed file produced
     * by an Encoder and producing a human-readable representation of the log
     * file. The Encoder class is intended to be reused as it maintains
     * fairly large data structures to hold fragments of the compressed logs.
     */
    class Decoder {
    public:
        Decoder();
        ~Decoder();

        bool open(const char *filename);
        bool decompressUnordered(FILE *outputFd, uint64_t logMsgsToPrint);
        bool decompressTo(FILE *outputFd, uint64_t linesToPrint);

        /**
         * Reads and stores a BufferExtent from the compressed log and
         * facilitates the interpretation of the log messages contained in the
         * extent.
         */
        struct BufferFragment {
            // Stores the bytes in a compressed log BufferExtent. The size is
            // chosen to be a little bigger than the size of a runtime
            // StagingBuffer to account for any other entries that may be
            // inserted at runtime.
            char storage[NanoLogConfig::STAGING_BUFFER_SIZE
                                             + BufferExtent::maxSizeOfHeader()];

            // Number of valid bytes in storage.
            uint64_t validBytes;

            // The runtime StagingBuffer id associated with this extent.
            uint32_t runtimeId;

            // For efficient IO, we read the entire fragment in once and then
            // keep track of a read position within the buffer
            const char *readPos;

            // Marks the first invalid byte in storage
            char *endOfBuffer;

            // Indicates if there are more log messages that can be decompressed
            bool hasMoreLogs;

            // For sorting, store the metadata for the next log message to be
            // decompressed so we can access its absolute rdtsc timestamp.
            uint32_t nextLogId;
            uint64_t nextLogTimestamp;

            BufferFragment();
            void reset();
            bool hasNext();
            bool readBufferExtent(FILE *fd, bool *wrapAround=nullptr);
            bool decompressNextLogStatement(FILE *outputFd,
                                 uint64_t &logMsgsPrinted,
                                 uint64_t &lastTimestamp,
                                 const Checkpoint &checkpoint,
                                 long aggregationFilterId=-1,
                                 void (*aggregationFn)(const char*, ...)=NULL);
            uint64_t getNextLogTimestamp() const;
        };

        BufferFragment *allocateBufferFragment();
        void freeBufferFragment(BufferFragment *bf);
        bool internalDecompressUnordered(FILE *outputFd,
                                uint64_t logMsgsToPrint,
                                uint64_t *logMsgsPrinted=nullptr,
                                std::vector<uint64_t> *callRCDF=nullptr,
                                uint32_t aggregationTargetId=-1,
                                void (*aggregationFn)(const char*,...)=nullptr);

        bool internalDecompressOrdered(FILE *outputFd,
                                uint64_t logMsgsToPrint,
                                uint64_t *logsMsgsPrinted=nullptr);

        // The symbolic file being operated on by the decoder. A value of
        // nullptr indicates that no valid file is currently opened.
        const char *filename;

        // The handle for the log file currently being operated on
        FILE *inputFd;

        // The number of log messages that has been outputted from the
        // current file
        uint64_t logMsgsPrinted;

        // Saves the checkpoint header found in inputFd. Values are invalid
        // if inputFd is nullptr.
        Checkpoint checkpoint;

        // Maintains a list of BufferFragments that are unused. These buffers
        // will be freed upon destruction of the Decoder object.
        std::vector<BufferFragment*> freeBuffers;
    };
}; /* LOG_H */

#endif /* BUFFERUTILS_H */
