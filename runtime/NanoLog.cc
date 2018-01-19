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
        printf("==== NanoLog Configuration ====\r\n");

        printf("StagingBuffer size: %u MB\r\n",
               NanoLogConfig::STAGING_BUFFER_SIZE / 1000000);
        printf("Output Buffer size: %u MB\r\n",
               NanoLogConfig::OUTPUT_BUFFER_SIZE / 1000000);
        printf("Release Threshold : %u MB\r\n",
               NanoLogConfig::RELEASE_THRESHOLD / 1000000);
        printf("Idle Poll Interval: %u µs\r\n",
               NanoLogConfig::POLL_INTERVAL_NO_WORK_US);
        printf("IO Poll Interval  : %u µs\r\n",
               NanoLogConfig::POLL_INTERVAL_DURING_IO_US);
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