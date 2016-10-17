#include <vector>

#include <fcntl.h>

#include "LogCompressor.h"
#include "BufferStuffer.h"
#include "BufferUtils.h"
#include "FastLogger.h"
#include "Cycles.h"
#include "Util.h"

namespace PerfUtils {

LogCompressor::LogCompressor(const char *logFile) :
        mutex(),
        workerThread(),
        workAdded(),
        queueEmptied(),
        outputFd(0),
        syncRequested(false),
        run(true),
        numBuffersProcessed(0),
        cyclesSearchingForWork(0),
        cyclesAioAndFsync(0),
        cyclesCompressing(0),
        padBytesWritten(0),
        totalBytesRead(0),
        totalBytesWritten(0),
        eventsProcessed(0),
        hasOustandingOperation(false),
        aioCb(),
        bufferSize(FastLogger::StagingBuffer::BUFFER_SIZE),
        outputBuffer(NULL),
        endOfOutputBuffer(NULL),
        posixBuffer(NULL)
    {
        outputFd = open(logFile, fileParams);
        memset(&aioCb, 0, sizeof(aioCb));

        //TODO(syang0) Shouldn't the buffer size be abstracted away? It seems
        // strange that we're using the StagingBuffer's size to allocate our own.

        int err = posix_memalign(reinterpret_cast<void**>(&outputBuffer),
                                                            512, bufferSize);
        if (err) {
            perror("Memory alignment for the LogCompressor's output buffer failed");
            std::exit(-1);
        }

        endOfOutputBuffer = outputBuffer + bufferSize;

        err = posix_memalign(reinterpret_cast<void**>(&posixBuffer),
                                                            512, bufferSize);
        if (err) {
            perror("Memory alignment for the LogCompressor's output buffer failed");
            std::exit(-1);
        }

        workerThread = std::thread(
                            &PerfUtils::LogCompressor::threadMain, this);
    }

LogCompressor::~LogCompressor() {
    if (outputFd > 0)
        close(outputFd);

    outputFd = 0;
}

void
LogCompressor::threadMain() {
    FastLogger::StagingBuffer *buff = NULL;
    uint32_t lastBufferIndex = 0;
    uint32_t currentBufferIndex = 0;
    uint32_t bytesInOutputBuffer = 0;

    char *out = outputBuffer;
    char *endOfOutputBuffer = outputBuffer + bufferSize;

    // Literally pick a random pointer to start the pointer compression
    // difference from. It could even be 0, but we choose output buffer since
    // that will be somewhere near the dynamic memory allocated.
    void *lastPtr = outputBuffer;

    // Amortizing
    uint64_t lastTimestamp = 0;
    uint32_t lastFmtId = 0;

    while(run) {
        bool foundWork = false;

        // Design Decision: Here, we choose to let the loop run until
        // it reaches 0 again before doing the final output.

        //TODO(syang0) Sync() could enver finish if we don't update the
        // the lastBufferIndex

        // Step 1: Find a buffer with valid data inside
        do {
            uint64_t start = Cycles::rdtsc();
            foundWork = false;

            BufferUtils::RecordEntry *re = nullptr;
            buff = nullptr;

            do {
                std::unique_lock<std::mutex> lock(FastLogger::bufferMutex);
                if (FastLogger::threadBuffers.empty())
                    break;

                currentBufferIndex =
                    (currentBufferIndex + 1) % FastLogger::threadBuffers.size();

                buff = FastLogger::threadBuffers[currentBufferIndex];

                // Could be that the thread was deallocated
                if (buff == nullptr)
                    continue;

                re = buff->peek();
                if (re == nullptr) {
                    buff = nullptr;
                    continue;
                }

                foundWork = true;
            } while (!foundWork && currentBufferIndex != lastBufferIndex);
            cyclesSearchingForWork += (Cycles::rdtsc() - start);

            if (!foundWork || buff == nullptr)
                break;


            // Step 2: Start processing the data we found
            start = Cycles::rdtsc();
            BufferUtils::insertCheckpoint(&out, endOfOutputBuffer, lastPtr);

            while (re != nullptr) {
                // Check that we have enough space
                if (re->entrySize + re->argMetadataBytes > bufferSize - bytesInOutputBuffer)
                    break;

                char *outPrev = out;
                //TODO(syang0) You could redo this such that a compression state
                // is passed around
                BufferUtils::compressMetadata(re, &out, lastTimestamp, lastFmtId);
                compressFnArray[re->fmtId](re, &out);

                //TODO(syang0) This could be handled elsewhere I think...
                lastFmtId = re->fmtId;
                lastTimestamp = re->timestamp;

                eventsProcessed++;
                totalBytesRead += re->entrySize;
                bytesInOutputBuffer += (out - outPrev);

                buff->consumeNext();
                re = buff->peek();
            }
            ++numBuffersProcessed;
            cyclesCompressing += Cycles::rdtsc() - start;

        } while(foundWork && lastBufferIndex != currentBufferIndex);

        // Check for more to do? We don't really get efficient until
        // more is done.

        // Step 3: Start outputting the compressed data!
        Util::serialize();
        uint64_t start = Cycles::rdtsc();

        uint32_t bytesToWrite = bytesInOutputBuffer;
        if (fileParams & O_DIRECT) {
            uint32_t bytesOver = bytesInOutputBuffer%512;

            if (bytesOver != 0) {
                bytesToWrite = bytesInOutputBuffer + 512 - bytesOver;
                padBytesWritten += (512 - bytesOver);
            }
        }

        if (bytesToWrite) {
            if (useAIO) {
                //TODO(syang0) This could be swapped out for aio_err or something
                if (hasOustandingOperation) {
                    // burn time waiting for io (could aio_suspend)
                    // and could signal back when ready via a register.
                    while (aio_error(&aioCb) == EINPROGRESS);
                    int err = aio_error(&aioCb);
                    int ret = aio_return(&aioCb);

                    if (err != 0) {
                        printf("PosixAioWritePoll Failed with code %d: %s\r\n",
                                err, strerror(err));
                    }

                    if (ret < 0) {
                        perror("Posix AIO Write operation failed");
                    }
                }

                aioCb.aio_fildes = outputFd;
                aioCb.aio_buf = outputBuffer;
                aioCb.aio_nbytes = bytesToWrite;

                if (aio_write(&aioCb) == -1)
                    printf(" Error at aio_write(): %s\n", strerror(errno));

                hasOustandingOperation = true;
                totalBytesWritten += bytesInOutputBuffer;
                bytesInOutputBuffer = 0;

                // Swap buffers
                char *tmp = outputBuffer;
                outputBuffer = posixBuffer;
                posixBuffer = tmp;
                endOfOutputBuffer = outputBuffer + bufferSize;

            } else {
                if (bytesToWrite != write(outputFd, outputBuffer, bytesToWrite))
                    perror("Error dumping log");
            }

            //TODO(syang0) We can do a bit better estimating the bandwidth if we
            // can optionally spawn a thread to handle "aio" instead of POSIX AIO
            cyclesAioAndFsync += (Cycles::rdtsc() - start);
        }

        if (!foundWork) {
            //TODO(syang0) It's basically going to spin forever...
            std::unique_lock<std::mutex> lock(mutex);
            if (syncRequested)
                syncRequested = false;
            else
                queueEmptied.notify_all();
        }

    }

    // This is just temporary.
    printf("\r\n\r\n\r\nExiting, printing stats\r\n");
    printStats();
}

void LogCompressor::printStats() {
    // Leaks abstraction, but basically flush so we get all the time
    uint64_t start = Cycles::rdtsc();
    fdatasync(outputFd);
    uint64_t stop = Cycles::rdtsc();
    cyclesAioAndFsync += (stop - start);

    double outputTime = Cycles::toSeconds(cyclesAioAndFsync);
//    double lookingForWork = Cycles::toSeconds(cyclesSearchingForWork);
    double compressTime = Cycles::toSeconds(cyclesCompressing);
    double workTime = outputTime + compressTime;

    printf("Wrote %lu events (%0.2lf MB) in %0.3lf seconds "
            "(%0.3lf seconds spent compressing)\r\n",
            eventsProcessed,
            totalBytesWritten/1.0e6,
            outputTime,
            compressTime);
    printf("Final fsync time was %lf sec\r\n",
                Cycles::toSeconds(stop - start));

    printf("On average, that's\r\n"
            "\t%0.2lf MB/s or %0.2lf ns/byte w/ processing\r\n"
            "\t%0.2lf MB/s or %0.2lf ns/byte raw output\r\n"
            "\t%0.2lf MB per flush with %0.1lf bytes/event\r\n",
            (totalBytesWritten/1.0e6)/(workTime),
            (workTime*1.0e9)/totalBytesWritten,
            (totalBytesWritten/1.0e6)/outputTime,
            (outputTime)*1.0e9/totalBytesWritten,
            (totalBytesWritten/1.0e6)/numBuffersProcessed,
            totalBytesWritten*1.0/eventsProcessed);

    printf("\t%0.2lf ns/event in total\r\n"
            "\t%0.2lf ns/event compressing\r\n",
            (outputTime + compressTime)*1.0e9/eventsProcessed,
            compressTime*1.0e9/eventsProcessed);

    printf("The compression ratio was %0.2lf-%0.2lfx (%lu bytes in, %lu bytes out, %lu pad bytes)\n",
                    1.0*totalBytesRead/(totalBytesWritten + padBytesWritten),
                    1.0*totalBytesRead/totalBytesWritten,
                    totalBytesRead,
                    totalBytesWritten,
                    padBytesWritten);
}

void
LogCompressor::sync()
{
    std::unique_lock<std::mutex> lock(mutex);
    syncRequested = true;
    workAdded.notify_all();
    printf("Waiting on sync\r\n");
    queueEmptied.wait(lock);
}

void
LogCompressor::exit()
{
    {
        std::unique_lock<std::mutex> lock(mutex);
        run = false;
        workAdded.notify_all();
    }
    printf("Waiting on join\n");
    workerThread.join();
}
} // Namespace

