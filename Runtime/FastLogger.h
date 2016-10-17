#ifndef FASTLOGGER_H
#define FASTLOGGER_H

#include <assert.h>
#include <ctime>
#include <mutex>
#include <vector>

//TODO(syang0) Maybe move to std::atomics instead of this
#include "Atomic.h"
#include "Common.h"

#include "BufferUtils.h"
#include "LogCompressor.h"


namespace PerfUtils {

class FastLogger {
public:
    class StagingBuffer;

    FastLogger();
private:

    //TODO(syang0) should be PROTECTED
public:
    static PerfUtils::LogCompressor* compressor;
    static __thread StagingBuffer* stagingBuffer;
    static std::vector<StagingBuffer*> threadBuffers;
    static std::mutex bufferMutex;

public:


    //TODO(synag0) There can be a race here if you don't lock....
    static inline void
    sync() {
//        std::lock_guard<std::mutex> guard(bufferMutex);
        if (compressor)
            compressor->sync();
    }

    static inline void
    exit() {
//        std::lock_guard<std::mutex> guard(bufferMutex);
        if (compressor)
            compressor->exit();
    }

    //TODO(synag0) This doesn't seem to be the best place to put this...
    static inline BufferUtils::RecordEntry*
    alloc(int nbytes) {
        if (stagingBuffer == NULL) {
            std::lock_guard<std::mutex> guard(bufferMutex);

            if (stagingBuffer == NULL) {
                stagingBuffer = new StagingBuffer();
                threadBuffers.push_back(stagingBuffer);
            }

            if (compressor == NULL)
                compressor = new LogCompressor();
        }

        return stagingBuffer->reserveAlloc(nbytes);
    }

    static inline void
    finishAlloc(BufferUtils::RecordEntry *re)
    {
        stagingBuffer->finishAlloc(re);
    }

    class StagingBuffer {
    public:
        // Determines size of buffer
        static const uint8_t BUFFER_SIZE_EXP = 26;
        static const uint32_t BUFFER_SIZE = 1 << BUFFER_SIZE_EXP;
        static const uint32_t BUFFER_MASK = BUFFER_SIZE - 1;

        //TODO(syang0): Should be PRIVATE
        public:
        // Position within storage[] where the next log message will be placed
        // This is updated once per alloc
        char *recordHead;

        // Position within storage[] where the printer would start printing from
        // This value is a stale value of printPos so that fewer cache coherence
        // messages would be generated when vigorously recording + printing
        char *cachedReadPointer;

        // Hints as to how much contiguous space one can allocate without
        // rolling over the log head or stalling behind the readPointer.
        // Note that this should be managed by the producer not the consumer
        int hintContiguouslyAllocable;

        int reservedSpace;

        //TODO(syang0) There's got to be a more elegant/portable way to do this
        static const uint32_t BYTES_PER_CACHE_LINE = 64;
        char cacheLine[BYTES_PER_CACHE_LINE];

        // Pointer within events[] of where the printer thread should start
        // printing from. This value is typically as up to date as possible.
        char *readHead;
        char *cachedRecordPointer;

        // Hints at how many contiguous bytes one can read without
        // rolling over the log head or stalling behind the recordPointer
        int hintContiguouslyReadable;

        Atomic<int> activeReaders;

        // Number of misses due to buffer being full
        uint64_t allocFailures;

        char storage[BUFFER_SIZE];

        char *endOfBuffer;

        //TODO(syang0) There's a race condition here whereby the recording
        // thread can be recording data while the printer catches up and
        // reads the data before it's finished.


        inline BufferUtils::RecordEntry*
        reserveAlloc(int argBytes, bool allowRecursion = false) {
            static const char *endOfBuffer = storage + BUFFER_SIZE;

            //TODO(syang0) This seems sorta retarded that alloc doesn't alloc bytes...
            int reqSpace = sizeof(BufferUtils::RecordEntry) + argBytes;

            // There's a subtle point here, all the checks for remaining
            // space are strictly < or >, not <= or => because if we allow
            // the record and print positions to overlap, we can't tell
            // if the buffer either completely full or completely empty.
            // Doing this check here ensures that == means completely empty.

            if (recordHead >= cachedReadPointer) {
                if (reqSpace < endOfBuffer - recordHead) {
                    reservedSpace = reqSpace;
                    return reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);
                }

                // Is there enough space if we wrap around?
                if (cachedReadPointer - storage > reqSpace) {
                    // Clear the last bit of the buffer so that printer knows
                    // it's invalid and that it should wrap around
                    int bytesToClear = hintContiguouslyAllocable;
                    if (hintContiguouslyAllocable > sizeof(BufferUtils::RecordEntry))
                        bytesToClear = sizeof(BufferUtils::RecordEntry);
                    bzero(recordHead, bytesToClear);

                    // Wrap around
                    reservedSpace = reqSpace;
                    recordHead = storage;
                    return reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);
                }

                // There's no space, should now attempt to update the cached
                // print position and check again
                if (cachedReadPointer == readHead) {
                    ++allocFailures;
                    return NULL;
                }

                cachedReadPointer = readHead;
                if (cachedReadPointer - storage <= reqSpace) {
                    ++allocFailures;
                    return NULL;
                }

                recordHead = storage;
                reservedSpace = reqSpace;
                return reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);
            } else {
                if (cachedReadPointer - recordHead > reqSpace)
                    return reinterpret_cast<BufferUtils::RecordEntry*>(recordHead);

                if (cachedReadPointer == readHead) {
                    ++allocFailures;
                    return NULL;
                }

                // Try updating print position and try again
                cachedReadPointer = readHead;

                if (!allowRecursion) {
                    ++allocFailures;
                    return NULL;
                }

                return reserveAlloc(argBytes, false);
            }
        }

        //TODO(syang0) I think a better thing to do is use a reserve pointer
        // and a record pointer. When reserving, bump the reserve pointer
        // when retiring, bump the recordHead while checking the size.

        inline void
        finishAlloc(BufferUtils::RecordEntry* in) {
            assert(recordHead == reinterpret_cast<char*>(in));

            recordHead += reservedSpace;
        }

        // One way to solve this (tho I'm not sure about the performance)
        // is to allocate a Record Entry object which would contain
        // pointers to the metadata and arguments. And then at destruction
        // it would automagically bump the record pointer.
        // The problem with this is that it may not be the most performant.

        // Intended to be used as reserve() followed by a bunch of allocs
        inline bool
        reserve(int nbytes, bool allowRecursion=false){
            static const char *endOfBuffer = storage + BUFFER_SIZE;

            if (hintContiguouslyAllocable > nbytes)
                return true;

            // There's a subtle point here, all the checks for remaining
            // space are strictly < or >, not <= or => because if we allow
            // the record and print positions to overlap, we can't tell
            // if the buffer either completely full or completely empty.
            // Doing this check here ensures that == means completely empty.
            if (recordHead >= cachedReadPointer) {
                hintContiguouslyAllocable = endOfBuffer - recordHead;
                if (nbytes < hintContiguouslyAllocable)
                    return true;

                // Do we have enough space if we wrap around?
                if (cachedReadPointer - storage > nbytes) {
                    hintContiguouslyAllocable = cachedReadPointer - storage;

                    // Clear the last bit of the buffer so that printer knows
                    // it's invalid and that it should wrap around
                    int bytesToClear = hintContiguouslyAllocable;
                    if (hintContiguouslyAllocable > sizeof(BufferUtils::RecordEntry))
                        bytesToClear = sizeof(BufferUtils::RecordEntry);
                    bzero(recordHead, bytesToClear);

                    // Wrap around
                    recordHead = storage;
                    return true;
                }

                // There's no space, should now attempt to update the cached
                // print position and check again
                if (cachedReadPointer == readHead) {
                    ++allocFailures;
                    return NULL;
                }

                cachedReadPointer = readHead;
                hintContiguouslyAllocable = cachedReadPointer - storage;
                if (hintContiguouslyAllocable <= nbytes) {
                    ++allocFailures;
                    return NULL;
                }

                recordHead = storage;
                return true;
            } else {
                hintContiguouslyAllocable = cachedReadPointer - recordHead;
                if (hintContiguouslyAllocable > nbytes)
                    return true;

                if (cachedReadPointer == readHead) {
                    ++allocFailures;
                    return false;
                }

                // Try updating print position and try again
                cachedReadPointer = readHead;

                if (!allowRecursion) {
                    ++allocFailures;
                    return false;
                }

                return reserve(nbytes, false);
            }
        }
        
        /**
         * Attempt to allocate a contiguous array of bytes from internal storage
         * An optional hint can be provided to reserve a larger chunk of memory
         * and amortize the cost of future alloc invocations.
         * 
         * Note the intended usage is that the user code will alloc with a
         * reservation and then perform regular allocs until the reservation
         * is used up.
         *
         * \param nbytes             - Number of contiguous bytes to allocate
         * \param hintBytesToReserve - Optional hint of how many bytes the user
         *                             will need.
         *
         * \return  - pointer to valid space; NULL if not enough space.
         */
        inline char*
        alloc(int nbytes, int hintBytesToReserve=0) {
            hintBytesToReserve = hintBytesToReserve ? hintBytesToReserve : nbytes;
            
            if (!reserve(nbytes))
                return NULL;

            char* ret = recordHead;
            hintContiguouslyAllocable -= nbytes;
            recordHead += nbytes;
            return ret;
        }

        //TODO(syang0) Change the alloc so that size is embedded.
        // This will allow us to abstract it even more such that
        // read will return the size and we can just interpret the
        // bytes... Yeah the current form just breaks abstractions
        // left and right.


        //TODO(syang0) RACE CONDITION! It's possible to read an alloc before it's
        // ready and vice versa. 


        char*
        peek(uint64_t nbytes, bool allowRecursion=true) {
            static const char *endOfBuffer = storage + BUFFER_SIZE;
            char *ret = NULL;
            
            if (cachedRecordPointer >= readHead) {
                if (cachedRecordPointer - readHead >= nbytes) {
                    ret = readHead;
                    readHead += nbytes;
                    return ret;
                }

                if (cachedRecordPointer == recordHead)
                    return NULL;

                cachedReadPointer = recordHead;

                if (!allowRecursion)
                    return NULL;

                return peek(nbytes, false);
            } else {
                // Roll over event
                if (*readHead == 0) {
                    readHead = storage;
                    return peek(nbytes, readHead == storage);
                }

                if (endOfBuffer - readHead >= nbytes) {
                    ret = readHead;
                    readHead += nbytes;
                    return ret;
                }

                printf("Internal Logger Error! Logger sho" );
            }
        }

        BufferUtils::RecordEntry*
        peek(bool allowRecursion=true)
        {
            static const char *endOfBuffer = storage + BUFFER_SIZE;
            BufferUtils::RecordEntry *re = NULL;

            if (cachedRecordPointer >= readHead) {
                int readableBytes = cachedRecordPointer - readHead;
                if (readableBytes >= sizeof(BufferUtils::RecordEntry)) {

                    re = reinterpret_cast<BufferUtils::RecordEntry*>(readHead);
                    if (readableBytes < re->entrySize) {
                        return NULL;
                    }
                    
                    return re;
                }

                if (cachedRecordPointer == recordHead) {
                    return NULL;
                }

                cachedRecordPointer = recordHead;
                if (!allowRecursion) {
                    return NULL;
                }

                return peek(false);
            } else {
                // Roll over event
                //TODO(syang0) Not quite right..
                if (*readHead == 0) {
                    readHead = storage;
                    return peek(false);
                }

                int readableBytes = endOfBuffer - readHead;
                if (readableBytes >= sizeof(BufferUtils::RecordEntry)) {
                    re = reinterpret_cast<BufferUtils::RecordEntry*>(readHead);

                    if (readableBytes < re->entrySize) {
                        return NULL;
                    }
                    
                    return re;
                }

                printf("Internal Logger Error! Logger sho" );
            }
        }

        BufferUtils::RecordEntry*
        consumeNext() {
            BufferUtils::RecordEntry* re = peek();
            readHead += re->entrySize;

            return re;
        }
      public:

        StagingBuffer()
            : recordHead(storage)
            , cachedReadPointer(storage)
            , hintContiguouslyAllocable(BUFFER_SIZE)
            , reservedSpace(0)
            , cacheLine()
            , readHead(storage)
            , cachedRecordPointer(storage)
            , hintContiguouslyReadable(0)
            , activeReaders(0)
            , storage()
            , endOfBuffer(storage + BUFFER_SIZE)
            , allocFailures(0)
        {
            bzero(storage, BUFFER_SIZE);
        }
    };

};  // FastLogger


}; // namespace PerfUtils

#endif /* FASTLOGGER_H */

