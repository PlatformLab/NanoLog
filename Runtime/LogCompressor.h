/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   LogCompressor.h
 * Author: syang0
 *
 * Created on September 30, 2016, 2:21 AM
 */

#ifndef LOGCOMPRESSOR_H
#define LOGCOMPRESSOR_H

#include <aio.h>                /* POSIX AIO */
#include <condition_variable>
#include <mutex>
#include <thread>

#include <fcntl.h>
#include <sys/stat.h>
#include <xmmintrin.h>


namespace PerfUtils {
class LogCompressor {
public:
    static const bool useAIO = true;
    void sync();
    void exit();

    void threadMain();
    void printStats();

    LogCompressor(const char *logFile="compressedLog");
    ~LogCompressor();
    
public:

    static const int fileParams = O_WRONLY|O_CREAT|O_NOATIME|O_DSYNC|O_DIRECT;

    // Mutex used to lock the condition variables.
    std::mutex mutex;
    
    std::thread workerThread;

    std::condition_variable workAdded;
    std::condition_variable queueEmptied;
    int outputFd;

    bool run;
    bool syncRequested;
    uint32_t numBuffersProcessed;
    uint64_t cyclesSearchingForWork;
    uint64_t cyclesAioAndFsync;
    uint64_t cyclesCompressing;
    uint64_t padBytesWritten;
    uint64_t totalBytesRead;
    uint64_t totalBytesWritten;
    uint64_t eventsProcessed;

    bool hasOustandingOperation;
    struct aiocb aioCb;

    uint32_t bufferSize;

    // Used to stage the compressed log messages before passing it on to the
    // POSIX AIO library.
    char *outputBuffer;

    char *endOfOutputBuffer;

    // Double buffer for outputBuffer that is used to hold compressed log
    // messages while POSIX AIO outputs it to a file.
    char *posixBuffer;

};
}; // namespace PerfUtils

#endif /* LOGCOMPRESSOR_H */

