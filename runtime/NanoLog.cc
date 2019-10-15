#include "NanoLog.h"
#include "RuntimeLogger.h"

/**
 * This file implements the public API to NanoLog
 */
namespace NanoLog {
    using namespace NanoLogInternal;

    std::string getStats() {
        return RuntimeLogger::getStats();
    }

    void printConfig() {
        constexpr double MB = 1024*1024;
        printf("==== NanoLog Configuration ====\r\n");

        printf("StagingBuffer size: %4.2lf MB\r\n",
               NanoLogConfig::STAGING_BUFFER_SIZE / MB);
        printf("Output Buffer size: %4.2lf MB\r\n",
               NanoLogConfig::OUTPUT_BUFFER_SIZE / MB);
        printf("Release Threshold : %4.2lf MB\r\n",
               NanoLogConfig::RELEASE_THRESHOLD / MB);
        printf("Low Work Threshold: %4.2lf MB\r\n",
               NanoLogConfig::LOW_WORK_THRESHOLD / MB);
        printf("Idle Poll Interval: %u µs\r\n",
               NanoLogConfig::POLL_INTERVAL_NO_WORK_US);
        printf("IO Poll Interval  : %u µs\r\n",
               NanoLogConfig::POLL_INTERVAL_DURING_LOW_WORK_US);
    }

    void preallocate() {
        RuntimeLogger::preallocate();
    }

    void setLogFile(const char *filename) {
        RuntimeLogger::setLogFile(filename);
    }

    LogLevel getLogLevel() {
        return RuntimeLogger::getLogLevel();
    }

    void setLogLevel(LogLevel logLevel) {
        RuntimeLogger::setLogLevel(logLevel);
    }

    void sync() {
        RuntimeLogger::sync();
    }

    int getCoreIdOfBackgroundThread() {
        return RuntimeLogger::getCoreIdOfBackgroundThread();
    }
};