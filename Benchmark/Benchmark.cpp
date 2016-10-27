#include "Cycles.h"
#include "FastLogger.h"

using namespace PerfUtils;

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: ./Benchmark <nanosecond_delay>\r\n");
        exit(1);
    }

    uint64_t delay_ns = std::stoi(argv[1]);
    uint64_t delayInCycles = Cycles::fromNanoseconds(delay_ns);
    const uint64_t RECORDS = 100000000;
    uint64_t start, stop;

    start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < RECORDS; ++i)
        RAMCLOUD_LOG("Simple Test");
    stop = PerfUtils::Cycles::rdtsc();

    double time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Total time 'benchmark recording' %d events took %0.2lf seconds "
            "(%0.2lf ns/event avg)\r\n",
            RECORDS, time, (time/RECORDS)*1e9);

    PerfUtils::FastLogger::sync();
    PerfUtils::FastLogger::exit();
}

