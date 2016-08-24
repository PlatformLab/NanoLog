#include <thread>

#include "Cycles.h"
#include "TimeTrace.h"
#include "Util.h"


/*
 *
 */

using namespace PerfUtils;

inline void sleep_cycles(uint64_t cycles) {
    if (cycles == 0)
        return;

    uint64_t stop = Cycles::rdtsc() + cycles;
    while(Cycles::rdtsc() < stop);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: ./Benchmark <nanosecond_delay>\r\n");
        exit(1);
    }

    uint64_t delay_ns = std::stoi(argv[1]);
    uint64_t delayInCycles = Cycles::fromNanoseconds(delay_ns);
    const uint64_t RECORDS = 100000000;
    uint64_t start, stop;
    double time;

    uint32_t maxBufferBytes = TimeTrace::Buffer::BUFFER_SIZE*sizeof(TimeTrace::Event)/1e6;
    printf("Doing %lu records with a buffer size of %d entries (max %u MB)\r\n",
            RECORDS,
            TimeTrace::Buffer::BUFFER_SIZE,
            maxBufferBytes);
    printf("Cycles per second is %lf\r\n", Cycles::perSecond());
    TimeTrace::record("Warm up?");
    Util::serialize();
    start = Cycles::rdtsc();
    for (uint64_t i = 1; i < RECORDS; ++i) {
        TimeTrace::record("Hello World", 0, 0, 0, 0);
        sleep_cycles(delayInCycles);
    }
    stop = Cycles::rdtsc();
    Util::serialize();

    time = Cycles::toSeconds(stop - start);

    TimeTrace::flush();
    TimeTrace::sync();

    uint32_t missedLogs = RECORDS - TimeTrace::printer->totalEventsWritten;
    printf("Recording %lu records took %lf seconds (%0.2lf ns avg)\r\n"
            "There were %u missedLogs (%0.2lf %)\r\n\r\n",
            RECORDS, time, 1e9*(time/RECORDS),
            missedLogs, (missedLogs*100.0)/RECORDS);

    TimeTrace::printer->printStats();

}

