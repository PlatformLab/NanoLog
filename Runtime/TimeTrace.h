/* Copyright (c) 2014-2016 Stanford University
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

#ifndef PERFUTIL_TIMETRACE_H
#define PERFUTIL_TIMETRACE_H

#include <condition_variable>
#include <deque>
#include <fstream>
#include <string>
#include <vector>
#include <mutex>
#include <thread>

#include <xmmintrin.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#include <aio.h>                /* POSIX AIO */

#include "Atomic.h"
#include "Cycles.h"
#include "Util.h"

namespace PerfUtils {
static uint32_t numSwaps;

static const bool useAIO = true;


// A macro to disallow the copy constructor and operator= functions
#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
    TypeName(const TypeName&) = delete;             \
    TypeName& operator=(const TypeName&) = delete;
#endif

/**
 * This class implements a circular buffer of entries, each of which
 * consists of a fine-grain timestamp and a short descriptive string.
 * It's typically used to record times at various points in an operation,
 * in order to find performance bottlenecks. It can record a trace relatively
 * efficiently, and then either return the trace either as a string or
 * print it to the system log.
 *
 * This class is thread-safe.
 */
class TimeTrace {
  public:
    class Printer;
    class Buffer;

    static void setOutputFileName(const char *filename) {
        TimeTrace::filename = filename;
    }

    /**
     * Record an event in a thread-local buffer, creating a new buffer
     * if this is the first record for this thread.
     *
     * \param timestamp
     *      Identifies the time at which the event occurred.
     * \param format
     *      A format string for snprintf that will be used, along with
     *      arg0..arg3, to generate a human-readable message describing what
     *      happened, when the time trace is printed. The message is generated
     *      by calling snprintf as follows:
     *      snprintf(buffer, size, format, arg0, arg1, arg2, arg3)
     *      where format and arg0..arg3 are the corresponding arguments to this
     *      method. This pointer is stored in the time trace, so the caller must
     *      ensure that its contents will not change over its lifetime in the
     *      trace.
     * \param arg0
     *      Argument to use when printing a message about this event.
     * \param arg1
     *      Argument to use when printing a message about this event.
     * \param arg2
     *      Argument to use when printing a message about this event.
     * \param arg3
     *      Argument to use when printing a message about this event.
     */

    template<typename... Args>
    static inline void
    record(uint64_t timestamp, uint32_t fmtId, Args... args) {
        if (threadBuffer == NULL) {
            createThreadBuffer();

            if (printer == NULL) {
                printer = new TimeTrace::Printer(filename);
            }
        }

        int64_t bytes = threadBuffer->record(timestamp, fmtId, args...);
        if (bytes <= 0 ) {
            // Failure to write means we ran out of space, try to catch up
            // to the print thread before calling it a miss.
            if (cachedPrintPos == threadBuffer->printPointer)
                return;

            cachedPrintPos = threadBuffer->printPointer;

            // Try one more time for good measure
            printf("Had to try one more time\r\n");
            if (threadBuffer->record(timestamp, fmtId, args...) <= 0)
                return;
        }
    }

    template<typename... Args>
    static inline void record(uint32_t fmtId, Args... args) {
        record(Cycles::rdtsc(), fmtId, args...);
    }

    static inline void sync() {
        if (printer != NULL) {
            printer->sync();
        }
    }
    static void reset();

    //TODO(syang0): This should be protected
  public:
    TimeTrace();
    static void createThreadBuffer();
    static void printInternal(std::vector<TimeTrace::Buffer*>* traces,
            std::string* s);

    // Points to a private per-thread TimeTrace::Buffer object; NULL means
    // no such object has been created yet for the current thread.
    static __thread Buffer* threadBuffer;

    // Save a copy of the print position so that this thread can record
    // up to it before checking the real value and incurring potential cache
    // coherence performance degregations.
    static __thread char* cachedPrintPos;

    static Printer* printer;

    // Holds pointers to all of the thread-private TimeTrace objects created
    // so far. Entries never get deleted from this object.
    static std::vector<Buffer*> threadBuffers;

    // Provides mutual exclusion on threadBuffers.
    static std::mutex mutex;

    // The name of the file to write records into. If it is null, then we will
    // write to stdout
    static const char* filename;

  public:
    /**
     * Represents a sequence of events, typically consisting of all those
     * generated by one thread.  Has a fixed capacity, so slots are re-used
     * on a circular basis.  This class is not thread-safe.
     */
    class Buffer {
      public:
        Buffer();
        ~Buffer();
        std::string getTrace();
        void print();
        void printToLog();
        
        //TODO(syang0) This should be protected
      public:
        // Determines the number of events we can retain as an exponent of 2
        static const uint8_t BUFFER_SIZE_EXP = 26;

        // Total number of events that we can retain any given time.
        static const uint32_t BUFFER_SIZE = 1 << BUFFER_SIZE_EXP;

        // Bit mask used to implement a circular event buffer
        static const uint32_t BUFFER_MASK = BUFFER_SIZE - 1;

        // Index within events of the slot to use for the next call to the
        // record method.
        int nextIndex;

        // Pointer within events[] of where TimeTrace should record next.
        // This value is typically as up to date as possible and is updated
        // once per timeTrace::record().
        char *recordPointer;

        //TODO(syang0) There's got to be a more elegant/portable way to do this
        static const uint32_t BYTES_PER_CACHE_LINE = 64;
        char cacheLine[BYTES_PER_CACHE_LINE];

        // Pointer within events[] of where the printer thread should start
        // printing from. This value is typically as up to date as possible.
        char *printPointer;

        // Count of number of calls to printInternal that are currently active
        // for this buffer; if nonzero, then it isn't safe to log new
        // entries, since this could interfere with readers.
        Atomic<int> activeReaders;

        //  Marks the first invalid byte of events[].
        char *endOfBuffer;

        // Holds information from the most recent calls to the record method.
        char events[BUFFER_SIZE];

        friend class TimeTrace;
        DISALLOW_COPY_AND_ASSIGN(Buffer);

        // First checks our space against the cache before attempting to update
        // it.
        inline bool
        hasSpace(uint64_t req, bool allowWrap) {
            if (recordPointer >= cachedPrintPos) {
                uint64_t remainingSpace = endOfBuffer - recordPointer;
                if (req <= remainingSpace)
                    return true;

                // Not enough space at the end of the buffer, wrap back around
                if (!allowWrap)
                    return false;

                recordPointer = events;
                if (cachedPrintPos - recordPointer >= req)
                    return true;

                // Last chance, update the print pos and check again
                if (cachedPrintPos == printPointer)
                    return false;

                cachedPrintPos = printPointer;
                remainingSpace = cachedPrintPos - recordPointer;
                return (remainingSpace >= req);
            } else {
                if (cachedPrintPos - recordPointer >= req)
                    return true;

                // Try update print pos
                if (cachedPrintPos == printPointer)
                    return false;

                cachedPrintPos = printPointer;
                return hasSpace(req, allowWrap);
            }
        }

        // End of recursion, see templated instances for more detail.
        inline int64_t
        recordRecursive(char **writePtr)
        {
            return 0;
        }

        /**
         * Partial Specialization of record where it takes in a const char *str
         * 
         * @param str
         * @param tail
         * @return
         */
        template<typename... Tail>
        inline int64_t
        recordRecursive(char **writePtr, const char* str, Tail... tail)
        {
            uint32_t length = strlen(str);
            uint32_t spaceReqHint = sizeof...(tail) << 3 + length;
            
            if (!hasSpace(spaceReqHint, false))
                return -(1UL << 63);

            *((uint32_t*)(*writePtr)) = length;
            *writePtr += sizeof(uint32_t);

            memcpy(*writePtr, str, length);
            *writePtr += length;

            return length + recordRecursive(tail...);
        }

        template<typename Head, typename... Tail>
        inline int64_t
        recordRecursive(char **writePtr, Head head, Tail... tail)
        {
            *((Head*)*writePtr) = head;
            *writePtr += sizeof(Head);

          return sizeof(Head) + recordRecursive(writePtr, tail...);
        }

        /**
         * Format to be laid out in the recordBuffer shall be
         * uint64_t timestamp
         * uint8_t fmtId
         * uint8_t numArgs
         * [uint32_t lengthIfString]
         * uintx_t argx
         *
         * where the last 2 can be repeated as many times as needed.
         */

        /**
         * Tries to append a bunch of stuff to the folder. If it fails, it will
         * return a number <= 0.
         * @param timestamp
         * @param fmtId
         * @param args
         * @return
         */
        template<typename... Args>
        inline int64_t
        record(uint64_t timestamp, uint32_t fmtId, Args... args)
        {
            // Ensure that we have a minimum amount of guestimated space.
            uint32_t reqSpaceHint =
                    sizeof...(args) << 3 + sizeof(uint64_t) + sizeof(uint32_t)
                    + sizeof(uint8_t);
            if (!hasSpace(reqSpaceHint, true))
                return -1;

            char* writePtr = recordPointer;

            *((uint64_t*)(writePtr)) = timestamp;
            writePtr += sizeof(uint64_t);

            *((uint32_t*)(writePtr)) = fmtId;
            writePtr += sizeof(uint32_t);
            
            int64_t ret = recordRecursive(&writePtr, args...);
            if (ret <= 0) {
                return -1;
            }

            int64_t diff = writePtr - recordPointer;
            recordPointer = writePtr;
            return diff;
        }

        void reset() {
            recordPointer = &events[0];
            printPointer = &events[0];
        }
    };













    class Printer {
        struct CompressedEvent {
            uint8_t additionalFmtIdBytes:2;
            uint8_t additionalTimestampBytes:3;
            uint8_t numArgs:3;

            // After this comes the fmtId, (delta) timestamp,
            // and up to four 4-byte format string arguments.
            uint8_t data[];

            static uint32_t getMaxSize() {
                return 1 // metadata
                        + 4 // format
                        + 8 // timestamp
                        + 4*4; // 4-byte arguments
            }
        } __attribute__((packed));



    public:
        static const int fileParams = O_WRONLY|O_CREAT|O_NOATIME|O_DSYNC|O_DIRECT;
        std::mutex mutex;
        std::thread printerThread;

        std::condition_variable workAdded;
        std::condition_variable queueEmptied;
        int output;

        bool run;
        uint32_t numBuffersProcessed;
        uint64_t cyclesWaitingForWork;
        uint64_t cyclesOutputting;
        uint64_t cyclesProcessing;
        uint64_t padBytesWritten;
        uint64_t totalBytesWritten;
        uint64_t eventsProcessed;

        bool hasOustandingOperation;
        struct aiocb aioCb;
        char *outputBuffer;
        char *posixBuffer;

        Printer(const char *logFile) :
            mutex(),
            printerThread(),
            workAdded(),
            output(0),
            run(true),
            numBuffersProcessed(0),
            cyclesWaitingForWork(0),
            cyclesOutputting(0),
            cyclesProcessing(0),
            padBytesWritten(0),
            totalBytesWritten(0),
            eventsProcessed(0),
            hasOustandingOperation(false),
            aioCb(),
            outputBuffer(NULL),
            posixBuffer(NULL)
        {
            output = open(logFile, fileParams);
            memset(&aioCb, 0, sizeof(aioCb));

            int err = posix_memalign(reinterpret_cast<void**>(&outputBuffer),
                                        512, Buffer::BUFFER_SIZE);
            if (err) {
                perror("Memalign failed");
                std::exit(-1);
            }

            err = posix_memalign(reinterpret_cast<void**>(&posixBuffer),
                                        512, Buffer::BUFFER_SIZE);
            if (err) {
                perror("Memalign failed");
                std::exit(-1);
            }

            printerThread = std::thread(
                    &PerfUtils::TimeTrace::Printer::threadMain, this);
        }

        ~Printer() {
            close(output);
        }

        void sync() {
            std::unique_lock<std::mutex> lock(mutex);
            workAdded.notify_all();

            bool stillHasWork = false;
            for (Buffer *b : threadBuffers) {
                if (b->recordPointer != b->printPointer) {
                    stillHasWork = true;
                }
            }

            if (stillHasWork) {
                workAdded.notify_all();
                queueEmptied.wait(lock);
            }
        }

        void exit() {
            {
                std::lock_guard<std::mutex> lock(mutex);
                if (run == false)
                    return;

                run = false;
                workAdded.notify_all();
            }
            printerThread.join();
        }

        void printStats() {
            // Leaks abstraction, but basically flush so we get all the time
            uint64_t start = Cycles::rdtsc();
            fdatasync(output);
            uint64_t stop = Cycles::rdtsc();
            cyclesOutputting += (stop - start);

            double outputTime = Cycles::toSeconds(cyclesOutputting);
            double lockTime = Cycles::toSeconds(cyclesWaitingForWork);
            double compressTime = Cycles::toSeconds(cyclesProcessing);

            printf("Wrote %lu events (%0.2lf MB) in %0.3lf seconds "
                    "(%0.3lf seconds spent compressing)\r\n",
                    eventsProcessed,
                    totalBytesWritten/1.0e6,
                    outputTime,
                    compressTime);
            printf("Final fsync time was %lf sec\r\n",
                        Cycles::toSeconds(stop - start));

            printf("On average, that's\r\n"
                    "\t%0.2lf MB/s or %0.2lf ns/byte w/ processing\r\n"
                    "\t%0.2lf MB/s or %0.2lf ns/byte raw output\r\n"
                    "\t%0.2lf MB per flush with %0.1lf bytes/event\r\n"
                    "\t%0.2lf ns/event in total\r\n"
                    "\t%0.2lf ns/event processing + compressing\r\n",
                    (totalBytesWritten/1.0e6)/outputTime,
                    (outputTime*1.0e9)/totalBytesWritten,
                    (totalBytesWritten/1.0e6)/(outputTime - compressTime - lockTime),
                    (outputTime - compressTime - lockTime)*1.0e9/totalBytesWritten,
                    (totalBytesWritten/1.0e6)/numBuffersProcessed,
                    totalBytesWritten*1.0/eventsProcessed,
                    outputTime*1.0e9/eventsProcessed,
                    compressTime*1.0e9/eventsProcessed);

            if (fileParams & O_DIRECT) {
                printf("\t%lu pad bytes written\r\n", padBytesWritten);;
            }
        }

//        static inline uint32_t
//        compressEvent(Event *eventIn, CompressedEvent *eventOut,
//                uint64_t& prevEventTimestamp) {
//            int cursor = 0;
//            int fmtIdBytes = 0;
//            int timestampBytes = 0;
//
//
//            int fmtId = 1; // hacked
//            if (fmtId <= (1 << 8))
//                fmtIdBytes = 1;
//            else if (fmtId <= (1 << 16))
//                fmtIdBytes = 2;
//            else if (fmtId <= (1 << 24))
//                fmtIdBytes = 3;
//            else
//                fmtIdBytes = 4;
//
//            eventOut->additionalFmtIdBytes = fmtIdBytes - 1;
//            memcpy(&eventOut->data[cursor], &fmtId,
//                    fmtIdBytes);
//            cursor += fmtIdBytes;
//
//            uint64_t timeStampOut;
//            // Record only the diff in cycles unless it is the first
//            timeStampOut = eventIn->timestamp - prevEventTimestamp;
//            if (timeStampOut <= (1UL << 8))
//                timestampBytes = 1;
//            else if (timeStampOut <= (1UL << 16))
//                timestampBytes = 2;
//            else if (timeStampOut <= (1UL << 24))
//                timestampBytes = 3;
//            else if (timeStampOut <= (1UL << 32))
//                timestampBytes = 4;
//            else if (timeStampOut <= (1UL << 40))
//                timestampBytes = 5;
//            else if (timeStampOut <= (1UL << 48))
//                timestampBytes = 6;
//            else if (timeStampOut <= (1UL << 56))
//                timestampBytes = 7;
//            else
//                timestampBytes = 8;
//
//            prevEventTimestamp = eventIn->timestamp;
//
//            eventOut->additionalTimestampBytes = timestampBytes - 1;
//            memcpy(&eventOut->data[cursor], &timeStampOut,
//                     timestampBytes);
//            cursor += timestampBytes;
//
//
//            // TODO(syang0) We could compress this even more by
//            // compressing the arguments.
//            if(eventIn->arg3 > 0)
//                eventOut->numArgs = 4;
//            else if (eventIn->arg2 > 0)
//                eventOut->numArgs =3;
//            else if (eventIn->arg1 > 0)
//                eventOut->numArgs = 2;
//            else if (eventIn->arg0 > 0)
//                eventOut->numArgs = 1;
//            else
//                eventOut->numArgs = 0;
//
//            memcpy(&(eventOut->data[cursor]), &(eventIn->arg0),
//                    (eventOut->numArgs)*sizeof(uint32_t));
//            cursor += (eventOut->numArgs)*sizeof(uint32_t);
//
//            return sizeof(CompressedEvent) + cursor;
//        }

        void threadMain() {
            uint32_t lastBufferIndex = 0;
            while(run) {
                Buffer *buff = NULL;
                uint64_t start = Cycles::rdtsc();
                do {
                    std::unique_lock<std::mutex> lock(mutex);

                    if (!run)
                        return;

                    buff = NULL;
                    for (int i = 0; i < threadBuffers.size(); ++i) {
                        uint32_t index =
                                (i + lastBufferIndex) % threadBuffers.size();
                        Buffer *b = threadBuffers[index];
                        
                        // TODO(syang0) Come back and add caching to this....
                        if (b->recordPointer != b->printPointer) {
                            buff = b;
                            lastBufferIndex = index;
                            break;
                        }
                    }

                    if (buff == NULL) {
                        queueEmptied.notify_all();
                        workAdded.wait(lock);
                    }

                } while (buff == NULL);
                cyclesWaitingForWork += (Cycles::rdtsc() - start);

                // Compressed write
                Util::serialize();
                uint64_t processStart = Cycles::rdtsc();
                uint64_t prevTimestamp = 0;
                uint32_t bytesProcessed = 0;


                // Cache the last record pointer, so that we don't
                // have to worry about cache coherence as much
                char *cachedRecordPointer = buff->recordPointer;

                while (buff->printPointer != cachedRecordPointer) {

                    // Peek at the timestamp to make sure it's valid
                    uint64_t *timestamp = 
                            reinterpret_cast<uint64_t*>(buff->printPointer);

                    if (*timestamp == 0) {
                        // If it is invalid, we must be equal with the 
                        // record pointer, else it's a bug!
                        assert(cachedRecordPointer < buff->printPointer);
                        buff->printPointer = buff->events;
                        continue;
                    }

                    buff->printPointer += sizeof(uint64_t);
                    
                    uint32_t fmtId = *((uint32_t*)buff->printPointer);
                    buff->printPointer += sizeof(uint32_t);

                    //  TODO(syang0) "Lookup" the format
                    uint32_t arg0 = *((uint32_t*)buff->printPointer);
                    buff->printPointer += sizeof(uint32_t);

                    uint32_t arg1 = *((uint32_t*)buff->printPointer);
                    buff->printPointer += sizeof(uint32_t);

                    uint32_t arg2 = *((uint32_t*)buff->printPointer);
                    buff->printPointer += sizeof(uint32_t);

                    uint32_t arg3 = *((uint32_t*)buff->printPointer);
                    buff->printPointer += sizeof(uint32_t);

//                    printf("Found   %lu - %u: %u %u %u %u\r\n",
//                            *timestamp, fmtId, arg0, arg1, arg2, arg3);

                    // Put in the time and call it a day for now
                    bytesProcessed += 4;

                    *timestamp = 0;
                    eventsProcessed++;
                }

                // Check for more to do? We don't really get efficient until
                // more is done. 

                cyclesProcessing += Cycles::rdtsc() - processStart;
                Util::serialize();

                uint32_t bytesToWrite = bytesProcessed;
                if (fileParams & O_DIRECT) {
                    uint32_t bytesOver = bytesProcessed%512;
                    padBytesWritten += bytesOver;
                    if (bytesOver == 0)
                        bytesToWrite = bytesProcessed;
                    else
                        bytesToWrite = bytesProcessed + 512 - bytesOver;
                }

                if (useAIO) {
                    //TODO(syang0) This could be swapped out for aio_err or something
                    if (hasOustandingOperation) {
                        // burn time waiting for io (could aio_suspend)
                        while (aio_error(&aioCb) == EINPROGRESS);
                        int err = aio_error(&aioCb);
                        int ret = aio_return(&aioCb);

                        if (err != 0) {
                            printf("PosixAioWritePoll Failed with code %d: %s\r\n",
                                    err, strerror(err));
                        }

                        if (ret < 0) {
                            perror("Posix AIO Write operation failed");
                        }
                    }

                    aioCb.aio_fildes = output;
                    aioCb.aio_buf = outputBuffer;
                    aioCb.aio_nbytes = bytesToWrite;

                    if (aio_write(&aioCb) == -1) {
                        printf(" Error at aio_write(): %s\n", strerror(errno));
                    }
                    hasOustandingOperation = true;
                } else {
                    if (bytesToWrite != write(output, outputBuffer, bytesToWrite))
                        perror("Error dumping log");
                }
                totalBytesWritten += bytesProcessed;
                cyclesOutputting += (Cycles::rdtsc() - start);

                --(buff->activeReaders);
                ++numBuffersProcessed;

            }
        }
    };
};

} // namespace PerfUtils

#endif // PERFUTIL_TIMETRACE_H

