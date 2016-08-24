/* Copyright (c) 2014-2016 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any purpose
 * with or without fee is hereby granted, provided that the above copyright
 * notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "CacheTrace.h"

namespace PerfUtils {

/**
 * Construct a CacheTrace.
 */
CacheTrace::CacheTrace(const char* filename)
    : events()
    , nextIndex(0)
    , filename(filename)
{
    // Mark all of the events invalid.
    for (int i = 0; i < BUFFER_SIZE; i++) {
        events[i].message = NULL;
    }
}

/**
 * Destructor for CacheTrace.
 */
CacheTrace::~CacheTrace()
{
}

/**
 * Record an event in the trace.
 *
 * \param message
 *      A short human-readable string identifying what happened, or the
 *      point in the code where this event was logged. This message is
 *      included in printouts of the time trace. This pointer is stored
 *      in the time trace, so either the string must be static, or the caller
 *      must ensure that its contents will not change over its lifetime
 *      in the trace.
 * \param lastLevelMissCount
 *      Identifies the value of the Last Level Cache Miss counter at which
 *      the event occurred.
 */
void CacheTrace::record(const char* message, uint64_t lastLevelMissCount)
{
    int i = nextIndex;
    nextIndex = (i + 1)%BUFFER_SIZE;
    events[i].count = lastLevelMissCount;
    events[i].message = message;
}

/**
 * Return a string containing a printout of the records in the trace.
 */
std::string CacheTrace::getTrace()
{
    std::string s;
    printInternal(&s);
    return s;
}

/**
 * Print all existing trace records to the system log.
 */
void CacheTrace::print()
{
    printInternal(NULL);
}

/**
 * Discard any existing trace records.
 */
void CacheTrace::reset()
{
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (events[i].message == NULL) {
            break;
        }
        events[i].message = NULL;
    }
    nextIndex = 0;
}

/**
 * This private method does most of the work for both printToLog and
 * getTrace.
 *
 * \param s
 *      If non-NULL, refers to a string that will hold a printout of the
 *      time trace. If NULL, the trace will be printed on the system log.
 */
void CacheTrace::printInternal(std::string* s)
{
    // Initialize file for writing
    FILE* output = NULL;
    if (s == NULL)
        output = filename ? fopen(filename, "a") : stdout;

    // Find the oldest event that we still have (either events[nextIndex],
    // or events[0] if we never completely filled the buffer).
    int i = nextIndex;
    if (events[i].message == NULL) {
        i = 0;
        if (events[0].message == NULL) {
            if (s != NULL) {
                s->append("No cache trace events to print");
            } else {
                fprintf(output, "No cache trace events to print\n");
            }
            return;
        }
    }

    // Retrieve a "starting count" for the number of cache misses counted so we
    // can print individual event counts relative to the starting count.
    uint64_t start = events[i].count;
    uint64_t prevCount = 0;

    // Each iteration through this loop processes one event from the trace.
    do {
        uint64_t miss = events[i].count - start;
        if (s != NULL) {
            char buffer[200];
            if (s->length() != 0) {
                s->append("\n");
            }
            snprintf(buffer, sizeof(buffer), "%6lu misses (+%4lu misses): %s",
                    miss, miss - prevCount, events[i].message);
            s->append(buffer);
        } else {
            fprintf(output, "%6lu misses (+%4lu misses): %s\n", miss,
                    miss - prevCount, events[i].message);
        }
        i = (i+1)%BUFFER_SIZE;
        prevCount = miss;
    } while ((i != nextIndex) && (events[i].message != NULL));

    if (output && output != stdout)
        fclose(output);
}

CacheTrace* CacheTrace::globalTrace = NULL;

/**
 * This method returns the single global instance of CacheTrace for those that want the global view.
 */
CacheTrace* CacheTrace::getGlobalInstance() {
    if (!globalTrace) globalTrace = new CacheTrace("CacheTrace.log");
    return globalTrace;
}

} // namespace PerfUtils
