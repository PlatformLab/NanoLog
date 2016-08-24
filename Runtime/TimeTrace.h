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
#include "Printer.h"
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
    static std::string getTrace();

    static void setOutputFileName(const char *filename) {
        TimeTrace::filename = filename;
    }
    static void print();

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
    static inline void
    record(uint64_t timestamp, const char* format,
            uint32_t arg0 = 0, uint32_t arg1 = 0, uint32_t arg2 = 0,
            uint32_t arg3 = 0) {
        if (threadBuffer == NULL) {
            createThreadBuffer();

            if (printer == NULL) {
                printer = new TimeTrace::Printer(filename);
            }
        }

        if (threadBuffer->nextIndex == 0) {
            if (threadBufferBackup->activeReaders > 0) {
                return;
            }

            Buffer *swap = threadBufferBackup;
            threadBufferBackup = threadBuffer;
            threadBuffer = swap;

            printer->enqueueWork(threadBufferBackup);
        }

        threadBuffer->record(timestamp, format, arg0, arg1, arg2, arg3);
    }
    static inline void record(const char* format, uint32_t arg0 = 0,
            uint32_t arg1 = 0, uint32_t arg2 = 0, uint32_t arg3 = 0) {
        record(Cycles::rdtsc(), format, arg0, arg1, arg2, arg3);
    }

    // Swaps the buffers buffers and pushes all events to the print thread
    // May busy wait if the print thread is not ready to swap.
    static inline void flush() {
        if (threadBuffer == NULL)
            return;

        while (threadBufferBackup->activeReaders > 0); // Busy wait

        Buffer *swap = threadBufferBackup;
        threadBufferBackup = threadBuffer;
        threadBuffer = swap;

        printer->enqueueWork(threadBufferBackup);
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
    static __thread Buffer* threadBufferBackup;
    static Printer* printer;

    // Holds pointers to all of the thread-private TimeTrace objects created
    // so far. Entries never get deleted from this object.
    static std::vector<Buffer*> threadBuffers;

    // Provides mutual exclusion on threadBuffers.
    static std::mutex mutex;

    // The name of the file to write records into. If it is null, then we will
    // write to stdout
    static const char* filename;

    /**
     * This structure holds one entry in the TimeTrace.
     */
    struct Event {
      uint64_t timestamp;        // Time when a particular event occurred.
      const char* format;        // Format string describing the event.
                                 // NULL means that this entry is unused.
      uint32_t arg0;             // Argument that may be referenced by format
                                 // when printing out this event.
      uint32_t arg1;             // Argument that may be referenced by format
                                 // when printing out this event.
      uint32_t arg2;             // Argument that may be referenced by format
                                 // when printing out this event.
      uint32_t arg3;             // Argument that may be referenced by format
                                 // when printing out this event.
    };

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
        void record(uint64_t timestamp, const char* format, uint32_t arg0 = 0,
                uint32_t arg1 = 0, uint32_t arg2 = 0, uint32_t arg3 = 0);
        void record(const char* format, uint32_t arg0 = 0, uint32_t arg1 = 0,
                uint32_t arg2 = 0, uint32_t arg3 = 0) {
            record(Cycles::rdtsc(), format, arg0, arg1, arg2, arg3);
        }
        void reset();

        //TODO(syang0) This should be protected
      public:
        // Determines the number of events we can retain as an exponent of 2
        static const uint8_t BUFFER_SIZE_EXP = 22;

        // Total number of events that we can retain any given time.
        static const uint32_t BUFFER_SIZE = 1 << BUFFER_SIZE_EXP;

        // Bit mask used to implement a circular event buffer
        static const uint32_t BUFFER_MASK = BUFFER_SIZE - 1;

        // Index within events of the slot to use for the next call to the
        // record method.
        int nextIndex;

        // Count of number of calls to printInternal that are currently active
        // for this buffer; if nonzero, then it isn't safe to log new
        // entries, since this could interfere with readers.
        Atomic<int> activeReaders;

        // Holds information from the most recent calls to the record method.
        Event events[BUFFER_SIZE];

        friend class TimeTrace;
        DISALLOW_COPY_AND_ASSIGN(Buffer);
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
        std::deque<Buffer*> toPrint;
        std::mutex mutex;
        std::mutex queueMutex;
        std::thread printerThread;

        std::condition_variable workAdded;
        std::condition_variable queueEmptied;
        int output;

        bool run;
        uint32_t numBuffersProcessed;
        uint64_t cyclesDequeueing;
        uint64_t cyclesOutputting;
        uint64_t cyclesProcessing;
        uint64_t padBytesWritten;
        uint64_t totalBytesWritten;
        uint64_t totalEventsWritten;

        bool hasOustandingOperation;
        struct aiocb aioCb;

        Printer(const char *logFile) :
         toPrint(),
         mutex(),
         queueMutex(),
         printerThread(),
         workAdded(),
         output(0),
         run(true),
         numBuffersProcessed(0),
         cyclesDequeueing(0),
         cyclesOutputting(0),
         cyclesProcessing(0),
         padBytesWritten(0),
         totalBytesWritten(0),
         totalEventsWritten(0),
         hasOustandingOperation(false),
         aioCb()
        {
            output = open(logFile, fileParams);
            printerThread = std::thread(&PerfUtils::TimeTrace::Printer::threadMain, this);
            memset(&aioCb, 0, sizeof(aioCb));
        }

        ~Printer() {
            close(output);
        }

        void enqueueWork(Buffer *buffer) {
            //TODO(syang0) This seems like a bug. Grabbing the mutex will block the enqueueWork, which defeats the purpose of a queueMutex
            std::lock_guard<std::mutex> lock(mutex);
            std::lock_guard<std::mutex> lock2(queueMutex);
            buffer->activeReaders.inc();
            toPrint.push_back(buffer);
            workAdded.notify_all();
        }

        void sync() {
            std::unique_lock<std::mutex> lock(mutex);
            while (!toPrint.empty()) {
                if (run == false) {
                    run = true;
                    printerThread = std::thread(&PerfUtils::TimeTrace::Printer::threadMain, this);
                }
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
            double lockTime = Cycles::toSeconds(cyclesDequeueing);
            double compressTime = Cycles::toSeconds(cyclesProcessing);

            printf("Wrote %lu events (%0.2lf MB) in %0.3lf seconds "
                    "(%0.3lf seconds spent compressing)\r\n",
                    totalEventsWritten,
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
                    totalBytesWritten*1.0/totalEventsWritten,
                    outputTime*1.0e9/totalEventsWritten,
                    compressTime*1.0e9/totalEventsWritten);

            if (fileParams & O_DIRECT) {
                printf("\t%lu pad bytes written\r\n", padBytesWritten);;
            }
        }

        static inline uint32_t
        compressEvent(Event *eventIn, CompressedEvent *eventOut,
                uint64_t& prevEventTimestamp) {
            int cursor = 0;
            int fmtIdBytes = 0;
            int timestampBytes = 0;


            int fmtId = 1; // hacked
            if (fmtId <= (1 << 8))
                fmtIdBytes = 1;
            else if (fmtId <= (1 << 16))
                fmtIdBytes = 2;
            else if (fmtId <= (1 << 24))
                fmtIdBytes = 3;
            else
                fmtIdBytes = 4;

            eventOut->additionalFmtIdBytes = fmtIdBytes - 1;
            memcpy(&eventOut->data[cursor], &fmtId,
                    fmtIdBytes);
            cursor += fmtIdBytes;

            uint64_t timeStampOut;
            // Record only the diff in cycles unless it is the first
            timeStampOut = eventIn->timestamp - prevEventTimestamp;
            if (timeStampOut <= (1UL << 8))
                timestampBytes = 1;
            else if (timeStampOut <= (1UL << 16))
                timestampBytes = 2;
            else if (timeStampOut <= (1UL << 24))
                timestampBytes = 3;
            else if (timeStampOut <= (1UL << 32))
                timestampBytes = 4;
            else if (timeStampOut <= (1UL << 40))
                timestampBytes = 5;
            else if (timeStampOut <= (1UL << 48))
                timestampBytes = 6;
            else if (timeStampOut <= (1UL << 56))
                timestampBytes = 7;
            else
                timestampBytes = 8;

            prevEventTimestamp = eventIn->timestamp;

            eventOut->additionalTimestampBytes = timestampBytes - 1;
            memcpy(&eventOut->data[cursor], &timeStampOut,
                     timestampBytes);
            cursor += timestampBytes;


            // TODO(syang0) We could compress this even more by
            // compressing the arguments.
            if(eventIn->arg3 > 0)
                eventOut->numArgs = 4;
            else if (eventIn->arg2 > 0)
                eventOut->numArgs =3;
            else if (eventIn->arg1 > 0)
                eventOut->numArgs = 2;
            else if (eventIn->arg0 > 0)
                eventOut->numArgs = 1;
            else
                eventOut->numArgs = 0;

            memcpy(&(eventOut->data[cursor]), &(eventIn->arg0),
                    (eventOut->numArgs)*sizeof(uint32_t));
            cursor += (eventOut->numArgs)*sizeof(uint32_t);

            return sizeof(CompressedEvent) + cursor;
        }

        void threadMain() {
            std::unique_lock<std::mutex> lock(mutex);

            char *buffer;
            uint32_t maxEventSize = CompressedEvent::getMaxSize();
            int err = posix_memalign(reinterpret_cast<void**>(&buffer), 512,
                                            maxEventSize*Buffer::BUFFER_SIZE);
            if (err) {
                perror("Memalign failed");
                std::exit(-1);
            }

            while(run) {
                Util::serialize();
                Buffer *buff = NULL;
                do {
                    uint64_t start = Cycles::rdtsc();
                    buff = NULL;
                    {
                        std::lock_guard<std::mutex> lock(queueMutex);
                        if (toPrint.size() == 0)
                            continue;

                        buff = toPrint.front();
                        toPrint.pop_front();
                    }
                    cyclesDequeueing += (Cycles::rdtsc() - start);

                    // Compressed write
                    Util::serialize();
                    uint64_t compressStart = Cycles::rdtsc();
                    uint64_t prevTimestamp = 0;
                    uint32_t bytesProcessed = 0;

                    for (uint32_t i = 0; i < Buffer::BUFFER_SIZE; ++i) {
                        CompressedEvent *eventOut = (CompressedEvent*)
                                                (&buffer[bytesProcessed]);
                        Event *eventIn = &(buff->events[i]);

                        if (eventIn->format == NULL)
                            break;

                        bytesProcessed +=
                                compressEvent(eventIn, eventOut, prevTimestamp);
                        ++totalEventsWritten;
                        eventIn->format = NULL;
                    }

                    cyclesProcessing += Cycles::rdtsc() - compressStart;
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
                        aioCb.aio_buf = buffer;
                        aioCb.aio_nbytes = bytesToWrite;

                        if (aio_write(&aioCb) == -1) {
                            printf(" Error at aio_write(): %s\n", strerror(errno));
                        }
                        hasOustandingOperation = true;
                    } else {
                        if (bytesToWrite != write(output, buffer, bytesToWrite))
                            perror("Error dumping log");
                    }
                    totalBytesWritten += bytesProcessed;
                    cyclesOutputting += (Cycles::rdtsc() - start);

                    --(buff->activeReaders);
                    ++numBuffersProcessed;

                } while (buff != NULL);


                queueEmptied.notify_all();
                workAdded.wait(lock);
            }

            free(buffer);
        }
    };
};

} // namespace PerfUtils

#endif // PERFUTIL_TIMETRACE_H

