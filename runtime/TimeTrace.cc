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

#include <vector>

#include "TimeTrace.h"


using std::string;
using std::vector;
namespace PerfUtils {
__thread TimeTrace::Buffer* TimeTrace::threadBuffer = NULL;
std::vector<TimeTrace::Buffer*> TimeTrace::threadBuffers;
std::mutex TimeTrace::mutex;
const char* TimeTrace::filename = NULL;

/**
 * Creates a thread-private TimeTrace::Buffer object for the current thread,
 * if one doesn't already exist.
 */
void
TimeTrace::createThreadBuffer()
{
    std::lock_guard<std::mutex> guard(mutex);
    if (threadBuffer == NULL) {
        threadBuffer = new Buffer;
        threadBuffers.push_back(threadBuffer);
    }
}


/**
 * Return a string containing all of the trace records from all of the
 * thread-local buffers.
 */
string
TimeTrace::getTrace()
{
    std::vector<TimeTrace::Buffer*> buffers;
    string s;

    // Make a copy of the list of traces, so we can do the actual tracing
    // without holding a lock and without fear of the list changing.
    {
        std::lock_guard<std::mutex> guard(mutex);
        buffers = threadBuffers;
    }
    TimeTrace::printInternal(&buffers, &s);
    return s;
}

/**
 * Print all existing trace records to either a user-specified file or to
 * stdout.
 */
void
TimeTrace::print()
{
    std::vector<TimeTrace::Buffer*> buffers;
    {
        std::lock_guard<std::mutex> guard(mutex);
        buffers = threadBuffers;
    }

    printInternal(&buffers, NULL);
}

/**
 * Construct a TimeTrace::Buffer.
 */
TimeTrace::Buffer::Buffer()
    : nextIndex(0)
    , activeReaders(0)
    , events()
{
    // Mark all of the events invalid.
    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        events[i].format = NULL;
    }
}

/**
 * Destructor for TimeTrace::Buffer.
 */
TimeTrace::Buffer::~Buffer()
{
}

/**
 * Record an event in the buffer.
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
 *      method. This pointer is stored in the buffer, so the caller must
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
void TimeTrace::Buffer::record(uint64_t timestamp, const char* format,
        uint32_t arg0, uint32_t arg1, uint32_t arg2, uint32_t arg3)
{
    if (activeReaders > 0) {
        return;
    }

    Event* event = &events[nextIndex];
    nextIndex = (nextIndex + 1) & BUFFER_MASK;

    // There used to be code here for prefetching the next few events,
    // in order to minimize cache misses on the array of events. However,
    // performance measurements indicate that this actually slows things
    // down by 2ns per invocation.
    // prefetch(event+1, NUM_PREFETCH*sizeof(Event));

    event->timestamp = timestamp;
    event->format = format;
    event->arg0 = arg0;
    event->arg1 = arg1;
    event->arg2 = arg2;
    event->arg3 = arg3;
}

/**
 * Return a string containing a printout of the records in the buffer.
 */
string TimeTrace::Buffer::getTrace()
{
    string s;
    std::vector<TimeTrace::Buffer*> buffers;
    buffers.push_back(this);
    printInternal(&buffers, &s);
    return s;
}

/**
 * Print all existing trace records to either a user-specified file or to
 * stdout.
 */
void TimeTrace::Buffer::print()
{
    std::vector<TimeTrace::Buffer*> buffers;
    buffers.push_back(this);
    printInternal(&buffers, NULL);
}

/**
 * Discard any existing trace records.
 */
void TimeTrace::Buffer::reset()
{
    for (uint32_t i = 0; i < BUFFER_SIZE; i++) {
        if (events[i].format == NULL) {
            break;
        }
        events[i].format = NULL;
    }
    nextIndex = 0;
}

/**
 * Discards all records in all of the thread-local buffers. Intended
 * primarily for unit testing.
 */
void
TimeTrace::reset()
{
    std::lock_guard<std::mutex> guard(mutex);
    for (uint32_t i = 0; i < TimeTrace::threadBuffers.size(); i++) {
        TimeTrace::threadBuffers[i]->reset();
    }
}

/**
 * This private method does most of the work for both printToLog and
 * getTrace.
 *
 * \param buffers
 *      Contains one or more TimeTrace::Buffers, whose contents will be merged
 *      in the resulting output. Note: some of the buffers may extend
 *      farther back in time than others. The output will cover only the
 *      time period covered by *all* of the traces, ignoring older entries
 *      from some traces.
 * \param s
 *      If non-NULL, refers to a string that will hold a printout of the
 *      time trace. If NULL, the trace will be printed on the system log.
 */
void
TimeTrace::printInternal(std::vector<TimeTrace::Buffer*>* buffers, string* s)
{
    bool printedAnything = false;
    for (uint32_t i = 0; i < buffers->size(); i++) {
        buffers->at(i)->activeReaders.fetch_and(1);
    }

    // Initialize file for writing
    FILE* output = NULL;
    if (s == NULL)
        output = filename ? fopen(filename, "a") : stdout;

    // Holds the index of the next event to consider from each trace.
    std::vector<int> current;

    // Find the first (oldest) event in each trace. This will be events[0]
    // if we never completely filled the buffer, otherwise events[nextIndex+1].
    // This means we don't print the entry at nextIndex; this is convenient
    // because it simplifies boundary conditions in the code below.
    for (uint32_t i = 0; i < buffers->size(); i++) {
        TimeTrace::Buffer* buffer = buffers->at(i);
        int index = (buffer->nextIndex + 1) % Buffer::BUFFER_SIZE;
        if (buffer->events[index].format != NULL) {
            current.push_back(index);
        } else {
            current.push_back(0);
        }
    }

    // Decide on the time of the first event to be included in the output.
    // This is most recent of the oldest times in all the traces (an empty
    // trace has an "oldest time" of 0). The idea here is to make sure
    // that there's no missing data in what we print (if trace A goes back
    // farther than trace B, skip the older events in trace A, since there
    // might have been related events that were once in trace B but have since
    // been overwritten).
    uint64_t startTime = 0;
    for (uint32_t i = 0; i < buffers->size(); i++) {
        Event* event = &buffers->at(i)->events[current[i]];
        if ((event->format != NULL) && (event->timestamp > startTime)) {
            startTime = event->timestamp;
        }
    }

    // Skip all events before the starting time.
    for (uint32_t i = 0; i < buffers->size(); i++) {
        TimeTrace::Buffer* buffer = buffers->at(i);
        while ((buffer->events[current[i]].format != NULL) &&
                (buffer->events[current[i]].timestamp < startTime) &&
                (current[i] != buffer->nextIndex)) {
            current[i] = (current[i] + 1) % Buffer::BUFFER_SIZE;
        }
    }

    // Each iteration through this loop processes one event (the one with
    // the earliest timestamp).
    double prevTime = 0.0;
    while (1) {
        TimeTrace::Buffer* buffer;
        Event* event;

        // Check all the traces to find the earliest available event.
        int currentBuffer = -1;
        uint64_t earliestTime = ~0;
        for (uint32_t i = 0; i < buffers->size(); i++) {
            buffer = buffers->at(i);
            event = &buffer->events[current[i]];
            if ((current[i] != buffer->nextIndex) && (event->format != NULL)
                    && (event->timestamp < earliestTime)) {
                currentBuffer = static_cast<int>(i);
                earliestTime = event->timestamp;
            }
        }
        if (currentBuffer < 0) {
            // None of the traces have any more events to process.
            break;
        }
        printedAnything = true;
        buffer = buffers->at(currentBuffer);
        event = &buffer->events[current[currentBuffer]];
        current[currentBuffer] = (current[currentBuffer] + 1)
                % Buffer::BUFFER_SIZE;

        char message[1000];
        double ns = Cycles::toSeconds(event->timestamp - startTime) * 1e09;
        if (s != NULL) {
            if (s->length() != 0) {
                s->append("\n");
            }
            snprintf(message, sizeof(message), "%8.1f ns (+%6.1f ns): ",
                    ns, ns - prevTime);
            s->append(message);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
            snprintf(message, sizeof(message), event->format, event->arg0,
                     event->arg1, event->arg2, event->arg3);
#pragma GCC diagnostic pop
            s->append(message);
        } else {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
            snprintf(message, sizeof(message), event->format, event->arg0,
                     event->arg1, event->arg2, event->arg3);
#pragma GCC diagnostic pop
            fprintf(output, "%8.1f ns (+%6.1f ns): %s", ns, ns - prevTime,
                    message);
            fputc('\n', output);
        }
        prevTime = ns;
    }

    if (!printedAnything) {
        if (s != NULL) {
            s->append("No time trace events to print");
        } else {
            fprintf(output, "No time trace events to print");
        }
    }

    for (uint32_t i = 0; i < buffers->size(); i++) {
        buffers->at(i)->activeReaders.fetch_and(-1);
    }
    if (output && output != stdout)
        fclose(output);
}

} // namespace RAMCloud
