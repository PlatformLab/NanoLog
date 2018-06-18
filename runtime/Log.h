/* Copyright (c) 2016-2018 Stanford University
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
 * and decompressAndPrintFnArray's defined in BufferStuffer.h
 *
 * The format of the Compressed Log File is determined by the Encoder and
 * Decoder classes below.
 */
namespace NanoLogInternal {
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

        // Number of bytes following this checkpoint that are used to encode
        // metadata for new log messages.
        uint32_t newMetadataBytes;

        // Number of unique log messages that the runtime has provided
        // metadata for thus far. This should represent the maximum formatId
        // that can appear in the log until the next checkpoint. It is assumed
        // that the reader/writer will number the log messages sequentially
        // in the log file.
        uint32_t totalMetadataEntries;

    } __attribute__((packed));


    /**
     * Describes a unique log message within the user sources. The order in
     * which this structure appears in the log file determines the associated
     * logId. Following this structure are the source filename and
     * PrintFragments required for this message.
     */
    struct FormatMetadata {
        // Number of nibbles in the dynamic data stream used to pack() arguments
        uint8_t numNibbles;

        // Number of PrintFragments following this data structure
        uint8_t numPrintFragments;

        // Log level of the LOG statement in the original source file
        uint8_t logLevel;

        // Line number of the LOG statement in the original source file
        uint32_t lineNumber;

        // Number of bytes in filename[] (including the null character)
        uint16_t filenameLength;

        // Filename for the original source file containing the LOG statement
        char filename[];
    } __attribute__((packed));

    /**
     * Describes how to interpret the dynamic log stream and partially
     * reconstruct the original log message.
     */
    struct PrintFragment {
        // The type of the argument to pull from the dynamic buffer to the
        // partial format string (formatFragment)
        uint8_t argType:5;

        // Indicates that the fragment requires a dynamic width/precision
        // argument in addition to one required by the format specifier.
        bool hasDynamicWidth:1;
        bool hasDynamicPrecision:1;

        // Length of the format fragment
        uint16_t fragmentLength;

        // A fragment of the original LOG statement that contains at most
        // one format specifier.
        char formatFragment[];
    } __attribute__((packed));


    /**
     * These enums help encode LOG parameter types in the dynamic paramter
     * stream. These enums should match types generated by the preprocessor
     * and have a 1:1 correspondence to the actual type without the additional
     * underscores and "_t" at the end.
     */
    enum FormatType : uint8_t {
        NONE,

        unsigned_char_t,
        unsigned_short_int_t,
        unsigned_int_t,
        unsigned_long_int_t,
        unsigned_long_long_int_t,
        uintmax_t_t,
        size_t_t,
        wint_t_t,

        signed_char_t,
        short_int_t,
        int_t,
        long_int_t,
        long_long_int_t,
        intmax_t_t,
        ptrdiff_t_t,

        double_t,
        long_double_t,
        const_void_ptr_t,
        const_char_ptr_t,
        const_wchar_t_ptr_t,

        MAX_FORMAT_TYPE
    };

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
    compressLogHeader(const UncompressedEntry *re, char** out,
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
        if (!reinterpret_cast<const UnknownHeader*>(*in)->entryType
                                                        == EntryType::LOG_MSG)
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


    bool insertCheckpoint(char** out,
                          char *outLimit,
                          bool writeDictionary);

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
    };

    /**
     * This class embodies a runtime log statement returned from
     * Decoder::getNextLogStatement(). It stores the static and dynamic
     * information associated with the log statement. The only information
     * missing is the type information for the dynamic arguments. Users of this
     * class must know the dynamic arguments' types to push() and get() them.
     *
     * Also, this class stores all the arguments as 8-byte parameters, so
     * 16-byte dynamic arguments (i.e. long long double) are not supported.
     */
    class LogMessage {
    PRIVATE:
        // Starting number of arguments that the base structure can store
        // without allocating more space.
        static const int INITIAL_SIZE = 10;

        // Pointer to the log statement's static arguments; a value of
        // nullptr indicates that this LogMessage's contents are invalid.
        FormatMetadata *metadata;

        // Identifier for the log statement assigned by the preprocessor.
        // A value of uint32_t(-1) is invalid.
        uint32_t logId;

        // Runtime timestamp of the log statement.
        uint64_t rdtsc;

        // Number of runtime arguments currently stored in the structure
        int numArgs;

        // Total number of arguments that can be stored in this structure
        // without additional memory allocation
        int totalCapacity;

        // Initial container for arguments
        uint64_t rawArgs[INITIAL_SIZE];

        // Extension for the arguments if we run out of space above
        uint64_t *rawArgsExtension;

        void reserve(int nparams);

    PUBLIC:
        explicit LogMessage();
        ~LogMessage();

        bool valid();
        int getNumArgs();
        uint32_t getLogId();
        uint64_t getTimestamp();
        void reset(FormatMetadata *fm= nullptr, uint32_t logId=uint32_t(-1),
                        uint64_t rdtsc=0);

        /**
         * Add a dynamic log argument into the structure.
         *
         * Note: only 8-byte arguments are supported; this means that the
         * "long double" type is not supported.
         *
         * \tparam T
         *      Type of the argument to be added to the structure (deduced)
         * \param in
         *      The argument to add to the structure
         */
        template<typename T>
        inline void
        push(T in) {
            if (numArgs == totalCapacity)
                reserve(numArgs + 1);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
            if (numArgs < INITIAL_SIZE)
                *(reinterpret_cast<T*>(&rawArgs[numArgs])) = in;
            else
                *(reinterpret_cast<T*>(
                        &rawArgsExtension[numArgs - INITIAL_SIZE])) = in;
            ++numArgs;
#pragma GCC diagnostic pop
        }

        /**
         * Return the n-th argument to the log statement (0-based).
         *
         * \tparam T
         *      Type of the argument to return (required). No type checking is
         *      performed, so users must be sure the argument type is correct.
         * \param argNum
         *      The n-th argument to return (0-based)
         *
         * \return
         *      The argument
         */
        template<typename T>
        inline typename std::enable_if<!std::is_same<T, long double>::value, T>::type
        get(int argNum) {
            assert(argNum < totalCapacity);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
            if (argNum < INITIAL_SIZE)
                return *(reinterpret_cast<T*>(&rawArgs[argNum]));
            else
                return *(reinterpret_cast<T*>(&rawArgsExtension[argNum - 10]));
#pragma GCC diagnostic pop
        }

        /**
         * Overload for push() that does nothing for the "long double" type
         * since it's too wide. Attempting to access this parameter as a long
         * double will result in an error.
         *
         * \param phony
         *      long double argument
         */
        inline void
        push(long double phony) {
            push<int>(-1);
        }

        /**
        * Specialization for get() that will raise an error when accessing
        * an argument as a "long double". We do not support this currently
        * since LogStatement is built with the assumption that types are at
        * most 8-bytes wide.
        *
        * \tparam T
        *      Type of the argument to return (long double).
        * \param argNum
        *      The n-th argument to return (0-based)
        *
        * \return
        *      The argument (but really an error).
        */
        template<typename T>
        inline typename std::enable_if<std::is_same<T, long double>::value,
                                        T>::type
        get(int argNum) {
            fprintf(stderr, "**ERROR** Aggregating on Long Doubles is "
                            "currently unsupported\r\n");
#ifndef TESTUTIL_H
            exit(2);
#endif
            return -1.0;
        }

        DISALLOW_COPY_AND_ASSIGN(LogMessage);
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

        int64_t decompressUnordered(FILE *outputFd);
        int64_t decompressTo(FILE *outputFd);

        bool getNextLogStatement(LogMessage &logMsg,
                                 FILE *outputFd= nullptr);

    PRIVATE:
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
                                 uint64_t &logMsgsProcessed,
                                 LogMessage &logArguments,
                                 const Checkpoint &checkpoint,
                                 std::vector<void*>& fmtId2metadata,
                                 long aggregationFilterId=-1,
                                 void (*aggregationFn)(const char*, ...)=NULL);
            uint64_t getNextLogTimestamp() const;
        };

        bool readDictionary(FILE *fd, bool flushOldDictionary);

        BufferFragment *allocateBufferFragment();
        void freeBufferFragment(BufferFragment *bf);
        bool internalDecompressUnordered(FILE *outputFd,
                                uint32_t aggregationTargetId=-1,
                                void (*aggregationFn)(const char*,...)=nullptr);

        // The symbolic file being operated on by the decoder. A string of
        // length 0 indicates that no valid file is currently opened.
        std::string filename;

        // The handle for the log file currently being operated on
        FILE *inputFd;

        // The number of log messages that has been outputted from the
        // current file
        uint64_t logMsgsPrinted;

        // Stores the log position in an iterative, unsorted decompression
        BufferFragment *bufferFragment;

        // Indicates that the file open()-ed is still readable and no errors
        // have occurred thus far in the execution.
        bool good;

        // Saves the checkpoint header found in inputFd. Values are invalid
        // if inputFd is nullptr.
        Checkpoint checkpoint;

        // Maintains a list of BufferFragments that are unused. These buffers
        // will be freed upon destruction of the Decoder object.
        std::vector<BufferFragment*> freeBuffers;

        // Mapping of id to FormatMetadata*'s within the rawMetadata buffer
        std::vector<void*> fmtId2metadata;

        // Mapping of fmtId to format strings; this is an auxiliary structure
        // built from FormatMetadata's.
        std::vector<std::string> fmtId2fmtString;

        // Contains the raw metadata to interpret log messages,
        // directly read from the log file
        char *rawMetadata;

        // End of the valid bytes in rawMetadata
        char *endOfRawMetadata;

        // Metric: Number of BufferFragment's read in the decompression
        uint32_t numBufferFragmentsRead;

        // Metric: Number of Checkpoint's read in the decompression
        uint32_t numCheckpointsRead;

        DISALLOW_COPY_AND_ASSIGN(Decoder);
    };
}; /* namespace Log */
}; /* namespace NanoLogInternal */

#endif /* LOG_H */
