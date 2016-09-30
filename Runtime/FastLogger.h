#ifndef FASTLOGGER_H
#define FASTLOGGER_H

#include <mutex>
#include <vector>

//TODO(syang0) Maybe move to std::atomics instead of this
#include "Atomic.h"
#include "Common.h"
#include "Cycles.h"
#include "LogCompressor.h"



#include <ctime>
#include "Packer.h"

namespace PerfUtils {

class FastLogger {
public:
    class StagingBuffer;

    FastLogger();
private:

    //TODO(syang0) should be PROTECTED
public:
    static LogCompressor* compressor;
    static __thread StagingBuffer* stagingBuffer;
    static std::vector<StagingBuffer*> threadBuffers;
    static std::mutex mutex;

public:
    static inline
    char* alloc(int nbytes) {
        if (stagingBuffer == NULL) {
            std::lock_guard<std::mutex> guard(mutex);

            if (stagingBuffer == NULL)
                stagingBuffer = new StagingBuffer();

            if (compressor == NULL)
                compressor = new LogCompressor();
        }

        return stagingBuffer->alloc(nbytes);
    }

    class StagingBuffer {
    public:
        // Determines size of buffer
        static const uint8_t BUFFER_SIZE_EXP = 26;
        static const uint32_t BUFFER_SIZE = 1 << BUFFER_SIZE_EXP;
        static const uint32_t BUFFER_MASK = BUFFER_SIZE - 1;

        //TODO(syang0): Should be PRIVATE
        public:
        // Pointer within events[] of where TimeTrace should record next.
        // This value is typically as up to date as possible and is updated
        // once per timeTrace::record().

        // Position within storage[] where the next log message will be placed
        // This is updated once per record()
        char *recordPointer;

        // Position within storage[] where the printer would start printing from
        // This value is a stale value of printPos so that fewer cache coherence
        // messages would be generated when vigorously recording + printing
        char *cachedReadPointer;

        //TODO(syang0) There's got to be a more elegant/portable way to do this
        static const uint32_t BYTES_PER_CACHE_LINE = 64;
        char cacheLine[BYTES_PER_CACHE_LINE];

        // Pointer within events[] of where the printer thread should start
        // printing from. This value is typically as up to date as possible.
        char *readPointer;

        Atomic<int> activeReaders;

        // Number of misses due to buffer being full
        uint64_t allocFailures;

        char storage[BUFFER_SIZE];

        /**
         * Attempt to allocate a contiguous array of bytes from internal storage
         * This is an internal function with an allowRecursion toggle to limit
         * recursion.
         *
         * \param allowRecursion - True enables a single recursive call
         * \param req            - Number of bytes required
         *
         * \return  - pointer to valid space; NULL if not enough space.
         */
        inline char*
        alloc(uint64_t nbytes, bool allowRecursion) {
            static const char *endOfBuffer = storage + BUFFER_SIZE;

            // There's a subtle point here, all the checks for remaining
            // space are strictly < or >, not <= or => because if we allow
            // the record and print positions to overlap, we can't tell
            // if the buffer either completely full or completely empty.
            // Doing this check here ensures that == means completely empty.

            char *ret = NULL;
            if (recordPointer >= cachedReadPointer) {
                uint64_t remainingSpace = endOfBuffer - recordPointer;
                if (nbytes < remainingSpace) {
                    ret = recordPointer;
                    recordPointer += nbytes;
                    return ret;
                }

                // Do we have enough space if we wrap around?
                if (cachedReadPointer - storage > nbytes) {

                    // Clear the last byte so that the Printer knows that
                    // the data following this point is invalid.
                    *recordPointer = 0;

                    // Wrap around
                    recordPointer = storage + nbytes;
                    return storage;
                }

                // There's no space, should now attempt to update the cached
                // print position and check again
                if (cachedReadPointer == readPointer) {
                    ++allocFailures;
                    return NULL;
                }

                cachedReadPointer = readPointer;
                if (cachedReadPointer - storage <= nbytes) {
                    ++allocFailures;
                    return NULL;
                }

                recordPointer = storage + nbytes;
                return storage;
            } else {
                if (cachedReadPointer - recordPointer > nbytes) {
                    ret = recordPointer;
                    recordPointer += nbytes;
                    return ret;
                }

                if (cachedReadPointer == readPointer) {
                    ++allocFailures;
                    return NULL;
                }

                // Try updating print position and try again
                cachedReadPointer = readPointer;

                if (!allowRecursion) {
                    ++allocFailures;
                    return NULL;
                }

                return alloc(nbytes, false);
            }
        }

      public:

        StagingBuffer()
            : recordPointer(storage)
            , cachedReadPointer(storage)
            , cacheLine()
            , readPointer(storage)
            , activeReaders(0)
            , storage()
            , allocFailures(0)
        {
            storage[0] = 0;
        }

        /**
         * Attempt to return a contiguous array of bytes from internal storage.
         *
         * \param nbytes - Number of bytes requested
         * \return - pointer to contiguous space; NULL if not enough space
         */
        inline char*
        alloc(int nbytes) {
            return alloc(nbytes, true);
        }
    };
    
    // Describes an entry within the staging buffer
    struct RecordMetadata {
        // Stores the format ID assigned to the log message by the preprocessor
        // component. A value of 0 is used to indicate an invalid entry.
        uint32_t fmtId;

        // Stores the rdtsc value at the time of invocation
        uint64_t timestamp;

        // The number of bytes in uncompress bytes the entry takes up (includes metadata size)

        // The maximum number of bytes the argument compressor could use to
        // represent this particular entry (includes metadata); The reason why
        // this is specified is because the argument compression decisions are
        // mad ein the python script and the code here is oblivious to its methods
        uint32_t maxSizeOfCompressedArgs;

        // After this comes regular, uncompressed arguments related to the
        // log message
    };

    enum EntryType : int {
        INVALID = 0,
        LOG_MSG = 1,
        CHECKPOINT = 2
    };

    struct Nibble {
        int first:4;
        int second:4;
    } __attribute__((packed));

    // Output Buffer metdata that describes the entry coming afterwards.
    struct CompressedMetadata {
        int entryType:2;
        int additionalFmtIdBytes:2;
        int additionalTimestampBytes:3;

        // After this, format id that's 1 + additionalFmtIdBytes long,
        // timestamp diff that's 1 + additionalTimeStampBytes long,
        // and then the arguments. The exact layout defined in the python scripts.

    } __attribute__((packed));

    struct Checkpoint : CompressedMetadata {
        uint64_t rdtsc;
        time_t unixTime;
        double cyclesPerSecond;
        void *relativePointer;
    } __attribute__((packed));

    template<typename T>
    static inline void
    recordPrimitive(char* &buff, T t) {
        *(reinterpret_cast<T*>(buff)) = t;
        buff += sizeof(T);
    }

    template<typename T>
    static inline T*
    interpretAndBump(char* &buff) {
        T* val = reinterpret_cast<T*>(buff);
        buff += sizeof(T);
        return val;
    }

    static inline void
    recordMetadata(char* &buff, uint32_t fmtId, uint32_t maxArgSize) {
        RecordMetadata *m = reinterpret_cast<RecordMetadata*>(buff);
        m->fmtId = fmtId;
        m->timestamp = Cycles::rdtsc();
        m->maxSizeOfCompressedArgs = maxArgSize;

        buff += sizeof(RecordMetadata);
    }

    static inline void
    compressMetadata(RecordMetadata *m, char* &out, uint64_t &lastTimestamp, uint32_t &lastFmtId) {
        CompressedMetadata *mo = interpretAndBump<CompressedMetadata>(out);
        mo->entryType = EntryType::LOG_MSG;
        mo->additionalFmtIdBytes = pack(&out, m->fmtId - lastFmtId) - 1; // TODO check for when fmtId = 0
        mo->additionalTimestampBytes = pack(&out, m->timestamp - lastTimestamp) - 1;
        lastTimestamp = m->timestamp;
        lastFmtId = m->fmtId;
    }

    // Inserts an uncompressed checkpoint into an output buffer. This a fairly
    // expensive operation in terms of storage size, so only use it when log files
    // are bifercated or explicit resynchronization is needed
    static inline void
    insertCheckpoint(char* &out, void* relativePointer) {
        Checkpoint *ck = interpretAndBump<Checkpoint>(out);
        ck->entryType = EntryType::CHECKPOINT;
        ck->rdtsc = Cycles::rdtsc();
        ck->unixTime = std::time(nullptr);
        ck->cyclesPerSecond = Cycles::getCyclesPerSec();
        ck->relativePointer = relativePointer;
    }
};


}; // namespace PerfUtils

#endif /* FASTLOGGER_H */

