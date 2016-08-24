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

#ifndef PERFUTIL_CACHETRACE_H
#define PERFUTIL_CACHETRACE_H

#include <string>
#include "Cycles.h"
#include "Atomic.h"
#include "Util.h"

namespace PerfUtils {

/**
 * This class implements a circular buffer of entries, each of which
 * consists of a count of cache misses and a short descriptive string.
 * It's typically used to record counts at various points in an operation,
 * in order to find performance bottlenecks. It can record a trace relatively
 * efficiently, and then either return the trace either as a string or
 * print it to a file that is specified by the constructor.
 *
 * This class is not synchronized, and is therefore not thread-safe.  Moreover,
 * it is meaningless to collect a trace including counters from multiple
 * threads, because the counters which are read by rdpmc are specific to each
 * CPU core.
 *
 * Please read the note on rdpmc() carefully before using this class.
 */
class CacheTrace {
  public:
    CacheTrace(const char* filename);
    ~CacheTrace();
    void record(const char* message,
            uint64_t lastLevelMissCount = Util::rdpmc(0));

    /**
     * Simple wrapper for convenient serialized calls to readPmc, which will
     * force earlier instructions to finish executing and prevent later
     * instructions from executing until after the call to readPmc.
     */
    void serialRecord(const char* message) {
        record(message, Util::serialReadPmc(0));
    }
    void print();
    std::string getTrace();
    void reset();
    static CacheTrace* getGlobalInstance();

  private:
    void printInternal(std::string* s);

    /**
     * This structure holds one entry in the CacheTrace.
     */
    struct Event {
      // Value of rdpmc output (cumulative count of last-level cache
      // misses) when this event was recorded.
      uint64_t count;

      // Static string describing the event.  NULL means that this entry is
      // unused.
      const char* message;

    };

    // Total number of events that we can retain at any given time.
    static const int BUFFER_SIZE = 10000;

    // Holds information from the most recent calls to the record method.
    Event events[BUFFER_SIZE];

    // Index within events of the slot to use for the next call to the
    // record method.
    volatile int nextIndex;

    // The name of the file to write records into. If it is null, then we will
    // write to stdout
    const char* filename;

    // Global instance
    static CacheTrace* globalTrace;
};

} // namespace PerfUtils

#endif // PERFUTIL_CACHETRACE_H

