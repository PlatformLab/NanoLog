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

#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <xmmintrin.h>

#include "Cycles.h"

namespace PerfUtils {

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
    static inline void record(uint64_t timestamp, const char* format,
            uint32_t arg0 = 0, uint32_t arg1 = 0, uint32_t arg2 = 0,
            uint32_t arg3 = 0) {
        if (threadBuffer == NULL) {
            createThreadBuffer();
        }

        threadBuffer->record(timestamp, format, arg0, arg1, arg2, arg3);
    }
    static inline void record(const char* format, uint32_t arg0 = 0,
            uint32_t arg1 = 0, uint32_t arg2 = 0, uint32_t arg3 = 0) {
        record(Cycles::rdtsc(), format, arg0, arg1, arg2, arg3);
    }
    static void reset();

  public:
    TimeTrace();
    static void createThreadBuffer();
    static void printInternal(std::vector<TimeTrace::Buffer*>* traces,
            std::string* s);

    // Points to a private per-thread TimeTrace::Buffer object; NULL means
    // no such object has been created yet for the current thread.
    static __thread Buffer* threadBuffer;

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

      protected:
        // Determines the number of events we can retain as an exponent of 2
        static const uint8_t BUFFER_SIZE_EXP = 13;

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
        std::atomic<int> activeReaders;

        // Holds information from the most recent calls to the record method.
        TimeTrace::Event events[BUFFER_SIZE];

        friend class TimeTrace;
        DISALLOW_COPY_AND_ASSIGN(Buffer);
    };
};

} // namespace PerfUtils

#endif // PERFUTIL_TIMETRACE_H

