#include <thread>

#include "Cycles.h"
#include "TimeTrace.h"
#include "Util.h"

using namespace PerfUtils;

inline void sleep_cycles(uint64_t cycles) {
    if (cycles == 0)
        return;

    uint64_t stop = Cycles::rdtsc() + cycles;
    while(Cycles::rdtsc() < stop);
}

static inline bool
hasSpace(TimeTrace::Buffer& b, uint64_t req, bool allowWrap) {
    if (b.recordPointer >= TimeTrace::cachedPrintPos) {
        uint64_t remainingSpace = b.endOfBuffer - b.recordPointer;
        if (req <= remainingSpace)
            return true;

        // Not enough space at the end of the buffer, wrap back around
        if (!allowWrap)
            return false;

        b.recordPointer = b.events;
        if (TimeTrace::cachedPrintPos - b.recordPointer >= req)
            return true;

        // Last chance, update the print pos and check again
        if (TimeTrace::cachedPrintPos == b.printPointer)
            return false;

        TimeTrace::cachedPrintPos = b.printPointer;
        remainingSpace = TimeTrace::cachedPrintPos - b.recordPointer;
        return (remainingSpace >= req);
    } else {
        if (TimeTrace::cachedPrintPos - b.recordPointer >= req)
            return true;

        // Try update print pos
        if (TimeTrace::cachedPrintPos == b.printPointer)
            return false;

        TimeTrace::cachedPrintPos = b.printPointer;
        return hasSpace(b, req, allowWrap);
    }
}

template<typename T>
void inline
insert(TimeTrace::Buffer &b, T t)
{
    *((T*)b.recordPointer) = t;
    b.recordPointer += sizeof(T);
}

// Faked inline code for maximum fun
static inline void
recordTimeTraceLike(uint32_t fmtId, uint32_t arg0, uint32_t arg1,
                                    uint32_t arg2, uint32_t arg3)
{
    TimeTrace::Buffer& b = *TimeTrace::threadBuffer;
    // Step 1 - Check Space
    uint32_t reqSpace = 5*sizeof(uint32_t) + sizeof(uint64_t);
    if (hasSpace(b, reqSpace, true)) {
        uint64_t cycles = Cycles::rdtsc();
        char *start = TimeTrace::threadBuffer->recordPointer;
        insert(b, cycles);
        insert(b, fmtId);
        insert(b, arg0);
        insert(b, arg1);
        insert(b, arg2);
        insert(b, arg3);

        char *end = TimeTrace::threadBuffer->recordPointer;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: ./Benchmark <nanosecond_delay>\r\n");
        exit(1);
    }

    uint64_t delay_ns = std::stoi(argv[1]);
    uint64_t delayInCycles = Cycles::fromNanoseconds(delay_ns);
    const uint64_t RECORDS = 10000000;
    uint64_t start, stop;
    double time;

    printf("Doing %lu records with a buffer size of %d bytes\r\n",
            RECORDS,
            TimeTrace::Buffer::BUFFER_SIZE);
    printf("Cycles per second is %lf\r\n", Cycles::perSecond());
    TimeTrace::record(uint32_t(1), uint32_t(1), uint32_t(2), uint32_t(3), uint32_t(4));
    // Fake inline code
    Util::serialize();
    start = Cycles::rdtsc();
    for (uint64_t i = 1; i < RECORDS; ++i) {
        recordTimeTraceLike(2, 4, 6, 8, 10);
        sleep_cycles(delayInCycles);
    }
    stop = Cycles::rdtsc();
    Util::serialize();

    time = Cycles::toSeconds(stop - start);

    TimeTrace::sync();

    uint32_t missedLogs = RECORDS - TimeTrace::printer->eventsProcessed;
    printf("Recording %lu records took %lf seconds (%0.2lf ns avg)\r\n"
            "There were %u missedLogs (%0.2lf %)\r\n\r\n",
            RECORDS, time, 1e9*(time/RECORDS),
            missedLogs, (missedLogs*100.0)/RECORDS);

    TimeTrace::printer->printStats();

}

