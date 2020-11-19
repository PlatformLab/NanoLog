/* Copyright (c) 2016-2018 Stanford University
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

// This program contains a collection of low-level performance measurements
// for FastLogger, which can be run either individually or altogether.  These
// tests measure performance in a single stand-alone process.
// Invoke the program like this:
//
//     Perf test1 test2 ...
//
// test1 and test2 are the names of individual performance measurements to
// run.  If no test names are provided then all of the performance tests
// are run.
//
// To add a new test:
// * Write a function that implements the test.  Use existing test functions
//   as a guideline, and be sure to generate output in the same form as
//   other tests.
// * Create a new entry for the test in the #tests table.

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <thread>

#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include <syscall.h>
#include <stdio.h>
#include <xmmintrin.h>

#include "Cycles.h"
#include "Log.h"
#include "PerfHelper.h"
#include "Portability.h"
#include "Util.h"
#include "Fence.h"

using namespace PerfUtils;
using namespace NanoLogInternal;
/**
 * Ask the operating system to pin the current thread to a given CPU.
 *
 * \param cpu
 *      Indicates the desired CPU and hyperthread; low order 2 bits
 *      specify CPU, next bit specifies hyperthread.
 */
void bindThreadToCpu(int cpu)
{
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity((pid_t)syscall(SYS_gettid), sizeof(set), &set);
}

/*
 * This function just discards its argument. It's used to make it
 * appear that data is used,  so that the compiler won't optimize
 * away the code we're trying to measure.
 *
 * \param value
 *      Pointer to arbitrary value; it's discarded.
 */
void discard(void* value) {
    int x = *reinterpret_cast<int*>(value);
    if (x == 0x43924776) {
        printf("Value was 0x%x\n", x);
    }
}

static uint64_t cntr = 0;

void function(uint64_t cycles) {
    cntr = cycles*2%100 + PerfUtils::Cycles::rdtsc();
}

/**
 * Return the current value of the fine-grain CPU cycle counter
 * (accessed via the RDTSC instruction).
 */
static NANOLOG_ALWAYS_INLINE
uint64_t
rdtsc()
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
    return (((uint64_t)hi << 32) | lo);
}

/**
 * Return the current value of the fine-grain CPU cycle counter
 * (accessed via the RDTSCP instruction).
 */
static NANOLOG_ALWAYS_INLINE
uint64_t
rdtscp()
{
    uint32_t lo, hi;
    __asm__ __volatile__("rdtscp" : "=a" (lo), "=d" (hi) : : "%rcx");
    return (((uint64_t)hi << 32) | lo);
}

//----------------------------------------------------------------------
// Test functions start here
//----------------------------------------------------------------------

double aggregate_customFunction() {
    int count = 1000000;
    int total = 0;

    uint64_t start = Cycles::rdtsc();
    for (int j = 0; j < count; ++j) {
        total += PerfHelper::sum4(j, j, j, j);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&total);
    return Cycles::toSeconds(stop - start)/count;
}

double aggregate_templates() {
    int count = 1000000;
    int total = 0;

    uint64_t start = Cycles::rdtsc();
    for (int j = 0; j < count; ++j) {
        total += PerfHelper::templateSum(4, j, j, j, j);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&total);
    return Cycles::toSeconds(stop - start)/count;
}

double aggregate_va_args() {
    int count = 1000000;
    int total = 0;

    uint64_t start = Cycles::rdtsc();
    for (int j = 0; j < count; ++j) {
        total += PerfHelper::va_argSum(4, j, j, j, j);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&total);
    return Cycles::toSeconds(stop - start)/count;
}

/**
 * Reads a number of bytes from an ifstream repeatedly and times it
 *
 * \param readSize
 *          Number of bytes to read at once
 *
 * \return total time taken to read the readSize in seconds
 */
double ifstreamReadHelper(int readSize) {
    int dataLen = 1000000;
    char *backing_buffer = static_cast<char*>(malloc(dataLen));

    std::ofstream oFile;
    oFile.open("/tmp/testLog.dat");
    oFile.write(backing_buffer, dataLen);
    oFile.close();

    // Read it back
    std::ifstream iFile;
    iFile.open("/tmp/testLog.dat");

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < dataLen/readSize; ++i) {
        iFile.read(backing_buffer, readSize);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(backing_buffer);
    std::remove("/tmp/testLog.dat");
    free(backing_buffer);

    return Cycles::toSeconds(stop - start)/(dataLen/readSize);
}

double ifstreamRead1() {
    return ifstreamReadHelper(1);
}

double ifstreamRead10() {
    return ifstreamReadHelper(10);
}

double ifstreamRead100() {
    return ifstreamReadHelper(100);
}

/**
 * Reads a number of bytes from an fread repeatedly and times it
 *
 * \param readSize
 *          Number of bytes to read at once
 *
 * \return total time taken to read the readSize in seconds
 */
double freadHelper(int readSize) {
    int dataLen = 1000000;
    char *backing_buffer = static_cast<char*>(malloc(dataLen));

    std::ofstream oFile;
    oFile.open("/tmp/testLog.dat");
    oFile.write(backing_buffer, dataLen);
    oFile.close();

    // Read it back
    FILE *fd = fopen ("/tmp/testLog.dat", "r");
    if (!fd) {
        printf("Error: fread test could not open temp file/tmp/testLog.dat\r\n");
        exit(1);
    }

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < dataLen/readSize; ++i) {
        fread(backing_buffer, 1, readSize, fd);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(backing_buffer);
    free(backing_buffer);

    fclose(fd);
    std::remove("/tmp/testLog.dat");

    return Cycles::toSeconds(stop - start)/(dataLen/readSize);
}

double fread1() {
    return freadHelper(1);
}

double fread10() {
    return freadHelper(10);
}

double fread100() {
    return freadHelper(100);
}

double fgetcFn() {
    int dataLen = 1000000;
    char *backing_buffer = static_cast<char*>(malloc(dataLen));

    std::ofstream oFile;
    oFile.open("/tmp/testLog.dat");
    oFile.write(backing_buffer, dataLen);
    oFile.close();

    // Read it back
    FILE *fd = fopen ("/tmp/testLog.dat", "r");
    if (!fd) {
        printf("Error: fread test could not open temp file/tmp/testLog.dat\r\n");
        exit(1);
    }

    uint8_t cnt = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < dataLen; ++i) {
        cnt += fgetc(fd);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&cnt);
    free(backing_buffer);

    fclose(fd);
    std::remove("/tmp/testLog.dat");

    return Cycles::toSeconds(stop - start)/(dataLen);
}

double sched_getcpu_rdtscp_test() {
    int count = 1000000;
    int cpuSum = 0;
    uint64_t timeSum = 0;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        timeSum += Cycles::rdtsc();
        cpuSum += sched_getcpu();
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&cpuSum);
    return Cycles::toSeconds(stop - start)/count;
}

double compressBinarySearch() {
    const int count = 1000000;
    uint64_t *buffer = static_cast<uint64_t*>(malloc(count*sizeof(uint64_t)));

    srand(0);
    for (int i = 0; i < count; i++) {
        buffer[i] = 1UL << (rand()%64);
    }

    int sumOfBytes = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        int numBytes;
        uint64_t val = buffer[i];
        if (val < (1ULL << 32))
        {
            if (val < (1U << 24))
            {
                if (val < (1U << 16))
                {
                    if (val < (1U << 8))
                        numBytes = 1;
                    else
                        numBytes = 2;
                }
                else numBytes = 3;
            }
            else numBytes = 4;
        }
        else
        {
            if (val < (1ULL << 56))
            {
                if (val < (1ULL << 48))
                {
                    if (val < (1ULL << 40))
                        numBytes = 5;
                    else
                        numBytes = 6;
                }
                else numBytes = 7;
            }
            else numBytes = 8;
        }

        sumOfBytes += numBytes;
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&sumOfBytes);

    free(buffer);
    return Cycles::toSeconds(stop - start)/count;
}

double compressLinearSearch() {
    const int count = 1000000;
    uint64_t *buffer = static_cast<uint64_t*>(malloc(count*sizeof(uint64_t)));

    srand(0);
    for (int i = 0; i < count; i++) {
        buffer[i] = 1UL << (rand()%64);
    }

    int sumOfBytes = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        int numBytes;
        uint64_t val = buffer[i];

        if (val < (1UL << 8)) {
            numBytes = 1;
        } else if (val < (1UL << 16)) {
            numBytes = 2;
        } else if (val < (1UL << 24)) {
            numBytes = 3;
        } else if (val < (1UL << 32)) {
            numBytes = 4;
        } else if (val < (1UL << 40)) {
            numBytes = 5;
        } else if (val < (1UL << 48)) {
            numBytes = 6;
        } else if (val < (1UL << 56)) {
            numBytes = 7;
        } else {
            numBytes = 8;
        }

        sumOfBytes += numBytes;
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&sumOfBytes);

    free(buffer);
    return Cycles::toSeconds(stop - start)/count;
}

double delayInBenchmark() {
    int count = 1000000;
    uint64_t x = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        function(0);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&cntr);
    return Cycles::toSeconds(stop - start)/(count);
}

// Measure the cost of a 32-bit divide. Divides don't take a constant
// number of cycles. Values were chosen here semi-randomly to depict a
// fairly expensive scenario. Someone with fancy ALU knowledge could
// probably pick worse values.
double div32()
{
    int count = 1000000;
    uint64_t start = Cycles::rdtsc();
    // NB: Expect an x86 processor exception is there's overflow.
    uint32_t numeratorHi = 0xa5a5a5a5U;
    uint32_t numeratorLo = 0x55aa55aaU;
    uint32_t divisor = 0xaa55aa55U;
    uint32_t quotient;
    uint32_t remainder;
    for (int i = 0; i < count; i++) {
        __asm__ __volatile__("div %4" :
                             "=a"(quotient), "=d"(remainder) :
                             "a"(numeratorLo), "d"(numeratorHi), "r"(divisor) :
                             "cc");
    }
    uint64_t stop = Cycles::rdtsc();
    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of a 64-bit divide. Divides don't take a constant
// number of cycles. Values were chosen here semi-randomly to depict a
// fairly expensive scenario. Someone with fancy ALU knowledge could
// probably pick worse values.
double div64()
{
    int count = 1000000;
    // NB: Expect an x86 processor exception is there's overflow.
    uint64_t start = Cycles::rdtsc();
    uint64_t numeratorHi = 0x5a5a5a5a5a5UL;
    uint64_t numeratorLo = 0x55aa55aa55aa55aaUL;
    uint64_t divisor = 0xaa55aa55aa55aa55UL;
    uint64_t quotient;
    uint64_t remainder;
    for (int i = 0; i < count; i++) {
        __asm__ __volatile__("divq %4" :
                             "=a"(quotient), "=d"(remainder) :
                             "a"(numeratorLo), "d"(numeratorHi), "r"(divisor) :
                             "cc");
    }
    uint64_t stop = Cycles::rdtsc();
    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of calling a non-inlined function.
double functionCall()
{
    int count = 1000000;
    uint64_t x = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        PerfHelper::plusOne(x);
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&count);
    return Cycles::toSeconds(stop - start)/(count);
}

double functionDereference()
{
    const int count = 1000000;
    char indecies[count];

    srand(0);
    for (int i = 0; i < count; ++i)
        indecies[i] = static_cast<char>(rand() % 50);

    uint64_t discardMe = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        discardMe += PerfHelper::functionArray[indecies[i]]();
    }
    uint64_t stop = Cycles::rdtsc();
    discard(&discardMe);

    return Cycles::toSeconds(stop - start)/count;
}

double switchCost() {
    const int count = 1000000;
    char indecies[count];

    srand(0);
    for (int i = 0; i < count; ++i)
        indecies[i] = static_cast<char>(rand() % 50);

    uint64_t discardMe = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        switch(indecies[i]) {
            case 0: discardMe += 0; break;
            case 1: discardMe += 1; break;
            case 2: discardMe += 2; break;
            case 3: discardMe += 3; break;
            case 4: discardMe += 4; break;
            case 5: discardMe += 5; break;
            case 6: discardMe += 6; break;
            case 7: discardMe += 7; break;
            case 8: discardMe += 8; break;
            case 9: discardMe += 9; break;
            case 10: discardMe += 10; break;
            case 11: discardMe += 11; break;
            case 12: discardMe += 12; break;
            case 13: discardMe += 13; break;
            case 14: discardMe += 14; break;
            case 15: discardMe += 15; break;
            case 16: discardMe += 16; break;
            case 17: discardMe += 17; break;
            case 18: discardMe += 18; break;
            case 19: discardMe += 19; break;
            case 20: discardMe += 20; break;
            case 21: discardMe += 21; break;
            case 22: discardMe += 22; break;
            case 23: discardMe += 23; break;
            case 24: discardMe += 24; break;
            case 25: discardMe += 25; break;
            case 26: discardMe += 26; break;
            case 27: discardMe += 27; break;
            case 28: discardMe += 28; break;
            case 29: discardMe += 29; break;
            case 30: discardMe += 30; break;
            case 31: discardMe += 31; break;
            case 32: discardMe += 32; break;
            case 33: discardMe += 33; break;
            case 34: discardMe += 34; break;
            case 35: discardMe += 35; break;
            case 36: discardMe += 36; break;
            case 37: discardMe += 37; break;
            case 38: discardMe += 38; break;
            case 39: discardMe += 39; break;
            case 40: discardMe += 40; break;
            case 41: discardMe += 41; break;
            case 42: discardMe += 42; break;
            case 43: discardMe += 43; break;
            case 44: discardMe += 44; break;
            case 45: discardMe += 45; break;
            case 46: discardMe += 46; break;
            case 47: discardMe += 47; break;
            case 48: discardMe += 48; break;
            case 49: discardMe += 49; break;
        }
    }
    uint64_t stop = Cycles::rdtsc();
    discard(&discardMe);

    return Cycles::toSeconds(stop - start)/count;
}

double arrayPush() {
    const int count = 1000000;
    char *baseBuffer = static_cast<char*>(malloc(count*4*sizeof(uint64_t)));
    char *buffer = baseBuffer;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        *reinterpret_cast<uint64_t*>(buffer) = 1;
        buffer += sizeof(uint64_t);

        *reinterpret_cast<uint64_t*>(buffer ) = 2;
        buffer += sizeof(uint64_t);

        *reinterpret_cast<uint64_t*>(buffer) = 3;
        buffer += sizeof(uint64_t);

        *reinterpret_cast<uint64_t*>(buffer) = 4;
        buffer += sizeof(uint64_t);
    }
    uint64_t stop = Cycles::rdtsc();

    free(baseBuffer);
    return Cycles::toSeconds(stop - start)/count;
}

double arrayStructCast()
{
    const int count = 1000000;
    char *baseBuffer = static_cast<char*>(malloc(count*4*sizeof(uint64_t)));
    char *buffer = baseBuffer;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        struct Quadnums {
            uint64_t num1;
            uint64_t num2;
            uint64_t num3;
            uint64_t num4;
        };

        struct Quadnums *nums = reinterpret_cast<struct Quadnums*>(buffer);
        nums->num1 = 1;
        nums->num2 = 2;
        nums->num3 = 3;
        nums->num4 = 4;
        buffer += sizeof(struct Quadnums);
    }
    uint64_t stop = Cycles::rdtsc();

    free(baseBuffer);
    return Cycles::toSeconds(stop - start)/count;
}

// Measure the time to create and delete an entry in a small
// map.
double mapCreate()
{
    srand(0);

    // Generate an array of random keys that will be used to lookup
    // entries in the map.
    int numKeys = 20;
    uint64_t keys[numKeys];
    for (int i = 0; i < numKeys; i++) {
        keys[i] = rand();
    }

    int count = 10000;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i += 5) {
        std::map<uint64_t, uint64_t> map;
        for (int j = 0; j < numKeys; j++) {
            map[keys[j]] = 1000+j;
        }
        for (int j = 0; j < numKeys; j++) {
            map.erase(keys[j]);
        }
    }
    uint64_t stop = Cycles::rdtsc();
    return Cycles::toSeconds(stop - start)/(count * numKeys);
}

// Measure the time to lookup a random element in a small map.
double mapLookup()
{
    std::map<uint64_t, uint64_t> map;
    srand(0);

    // Generate an array of random keys that will be used to lookup
    // entries in the map.
    int numKeys = 20;
    uint64_t keys[numKeys];
    for (int i = 0; i < numKeys; i++) {
        keys[i] = rand();
        map[keys[i]] = 12345;
    }

    int count = 100000;
    uint64_t sum = 0;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < numKeys; j++) {
            sum += map[keys[j]];
        }
    }
    uint64_t stop = Cycles::rdtsc();
    return Cycles::toSeconds(stop - start)/(count*numKeys);
}

/**
 * Measure the cost of copying cpyBytes from cold/hot source/destinations
 *
 * \param cpySize
 *          Size of the copy
 * \param coldSrc
 *          if false, source will be fixed; otherwise random
 * \param coldDst
 *          if false, destination will be fixed; otherwise random
 * \return
 *        average time per cpyByte copy
 */
double manualCopy(int cpySize, bool coldSrc, bool coldDst) {
    int count = 1000000;
    uint32_t src[count], dst[count];
    int bufSize = 1000000000; // 1GB buffer
    char *buf = static_cast<char*>(malloc(bufSize));

    uint32_t bound = (bufSize - cpySize);
    for (int i = 0; i < count; i++) {
        src[i] = (coldSrc) ? (rand() % bound) : 0;
        dst[i] = (coldDst) ? (rand() % bound) : 0;
    }

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        for (int j = 0; j < cpySize; ++j) {
           *(buf + dst[i]) = *(buf + src[i]);
        }
    }
    uint64_t stop = Cycles::rdtsc();

    free(buf);
    return Cycles::toSeconds(stop - start)/(count);
}

double manualCopyCached1()
{
    return manualCopy(1, false, false);
}

double manualCopyCached10()
{
    return manualCopy(10, false, false);
}

// Measure the cost of copying a given number of bytes with memcpy.
double memcpyShared(int cpySize, bool coldSrc, bool coldDst)
{
    int count = 1000000;
    uint32_t src[count], dst[count];
    int bufSize = 1000000000; // 1GB buffer
    char *buf = static_cast<char*>(malloc(bufSize));

    uint32_t bound = (bufSize - cpySize);
    for (int i = 0; i < count; i++) {
        src[i] = (coldSrc) ? (rand() % bound) : 0;
        dst[i] = (coldDst) ? (rand() % bound) : 0;
    }

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        std::memcpy((buf + dst[i]),
                    (buf + src[i]),
                    cpySize);
    }
    uint64_t stop = Cycles::rdtsc();

    free(buf);
    return Cycles::toSeconds(stop - start)/(count);
}

double memcpyCached1() {
    return memcpyShared(1, false, false);
}

double memcpyCached100()
{
    return memcpyShared(100, false, false);
}

double memcpyCached1000()
{
    return memcpyShared(1000, false, false);
}

double memcpyCachedDst100()
{
    return memcpyShared(100, true, false);
}

double memcpyCachedDst1000()
{
    return memcpyShared(1000, true, false);
}

double memcpyCold100()
{
    return memcpyShared(100, true, true);
}

double memcpyCold1000()
{
    return memcpyShared(1000, true, true);
}


double mm_stream_pi_test()
{
    int count = 10000000;
    char backing_buffer[128];

    char *buffer = (backing_buffer + 64);
    __m64 *ptr = reinterpret_cast<__m64*>(buffer);

    uint64_t start = Cycles::rdtsc();
    for (uint64_t i = 0; i < count; i++) {
        _mm_stream_pi(ptr, (__m64)i);
    }
    uint64_t stop = Cycles::rdtsc();
    discard(ptr);

    return Cycles::toSeconds(stop - start)/(count);
}


static uint64_t variable = 0;
void readerThread(uint64_t *variableToRead,
                    volatile bool *run,
                    pthread_barrier_t *barrier,
                    int core=0)
{
    bindThreadToCpu(core);
    pthread_barrier_wait(barrier);

    uint64_t sum = 0;
    while(*run) {
        sum += variable;
        NanoLogInternal::Fence::lfence();
    }

    discard(&sum);
}

double mm_stream_pi_contended()
{
    int count = 1000;
    char *backing_buffer = static_cast<char*>(malloc(128));
    char *buffer = (backing_buffer + 64);
    __m64 *ptr = reinterpret_cast<__m64*>(buffer);
    uint64_t *writeLocation = (uint64_t*)ptr;

    bool run = true;
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, 2);
    std::thread contenderThread(readerThread, (uint64_t*)(ptr), &run, &barrier, 0);
    pthread_barrier_wait(&barrier);

    uint64_t start = Cycles::rdtsc();
    for (uint64_t i = 0; i < count; i++) {
        // variable += i;
        // *writeLocation += i;
        _mm_stream_pi((__m64*)&variable, (__m64)i);
        NanoLogInternal::Fence::sfence();
    }
    uint64_t stop = Cycles::rdtsc();

    discard(writeLocation);
    free(backing_buffer);
    run = false;
    contenderThread.join();
    pthread_barrier_destroy(&barrier);
    return Cycles::toSeconds(stop - start)/(count);
}

// Cost of notifying a condition variable
double notify_all() {
    int count = 1000000;
    std::condition_variable cond;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        cond.notify_all();
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

double notify_one() {
    int count = 1000000;
    std::condition_variable cond;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        cond.notify_one();
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of the Cylcles::toNanoseconds method.
double perfCyclesToNanoseconds()
{
    int count = 1000000;
    uint64_t total = 0;
    uint64_t cycles = 994261;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        total += Cycles::toNanoseconds(cycles);
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of the Cycles::toSeconds method.
double perfCyclesToSeconds()
{
    int count = 1000000;
    double total = 0;
    uint64_t cycles = 994261;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        total += Cycles::toSeconds(cycles);
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of the reading the CPU timestamp counter
double rdtsc_test()
{
    int count = 1000000;
    double total = 0;
    uint64_t cycles = 994261;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        total += rdtsc();
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of the reading the CPU timestamp counter
// with serialization
double rdtscp_test()
{
    int count = 1000000;
    double total = 0;
    uint64_t cycles = 994261;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        total += rdtscp();
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

double sched_getcpu_test()
{
    int count = 1000000;
    int cpuSum = 0;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        cpuSum += sched_getcpu();
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&cpuSum);
    return Cycles::toSeconds(stop - start)/count;
}

double snprintfFileLocation()
{
    int count = 1000000;
    char buffer[1000];

    uint64_t start = Cycles::rdtsc();
    Util::serialize();
    for (int i = 0; i < count; ++i) {
        snprintf(buffer, 1000,
            "%s:%d:%s",
            __FILE__, __LINE__, __func__);
    }
    Util::serialize();
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

double snprintfTime()
{
    struct timespec now;
    int count = 1000000;
    char buffer[1000];

    clock_gettime(CLOCK_REALTIME, &now);
    uint64_t start = Cycles::rdtsc();
    Util::serialize();
    for (int i = 0; i < count; ++i) {
        snprintf(buffer, 1000, "%010lu.%09lu", now.tv_sec, now.tv_nsec);
    }
    Util::serialize();
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

double snprintStatic100Char()
{
    int count = 1000000;
    char buffer[1000];

    uint64_t start = Cycles::rdtsc();
    Util::serialize();
    for (int i = 0; i < count; ++i) {
        snprintf(buffer, 1000,
            "aEw6ppfz3QMmDXBm91v10TxzCWdTaWUUX9ta0Fihl86Ta9nlFN"
            "JtAIDDjg9ApCgEwHvLfYZ2mTCHyMouslDI9Mvq2mvFSaNof8aJ");
    }
    Util::serialize();
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

double snprintfRAMCloud()
{
    struct timespec now;
    int count = 1000000;
    char buffer[1000];

    clock_gettime(CLOCK_REALTIME, &now);
    uint64_t start = Cycles::rdtsc();
    Util::serialize();
    for (int i = 0; i < count; ++i) {
        snprintf(buffer, 1000,
            "%010lu.%09lu %s:%d in %s %s[%d]: %s %0.6lf",
            now.tv_sec, now.tv_nsec, __FILE__, __LINE__,
            __func__, "Debug", 100,
            "Using tombstone ratio balancer with ratio =", 0.4);
    }
    Util::serialize();
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of reading the fine-grain cycle counter.
double rdtscTest()
{
    uint64_t total = 0;
    int count = 1000000;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        total += Cycles::rdtsc();
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of reading high precision time
double high_resolution_clockTest() {
    int count = 1000000;

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        std::chrono::high_resolution_clock::now();
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of printing via ctime
double printTime_ctime() {
    int count = 1000000;
    char buffer[1000];

    uint64_t start = Cycles::rdtsc();
    std::time_t result = std::time(nullptr);

    for (int i = 0; i < count; i++) {
        snprintf(buffer, 1000, "%s", std::ctime(&result));
    }
    uint64_t stop = Cycles::rdtsc();
    discard(&buffer);

    return Cycles::toSeconds(stop - start)/count;
}

double printTime_strftime() {
    int count = 1000000;
    char buffer[1000];

    std::time_t result = std::time(nullptr);
    std:tm *tm = localtime(&result);

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        strftime(buffer, 1000, "%y/%m/%d %H:%M:%S", tm);
    }
    uint64_t stop = Cycles::rdtsc();
    discard(&buffer);

    return Cycles::toSeconds(stop - start)/count;
}

double printTime_strftime_wConversion() {
    int count = 1000000;
    char buffer[1000];

    std::time_t result = std::time(nullptr);

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        std:tm *tm = localtime(&result);
        strftime(buffer, 1000, "%y/%m/%d %H:%M:%S", tm);
    }
    uint64_t stop = Cycles::rdtsc();
    discard(&buffer);

    return Cycles::toSeconds(stop - start)/count;
}

// Measure the cost of cpuid
double serialize() {
    int count = 1000000;
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        Util::serialize();
    }
    uint64_t stop = Cycles::rdtsc();
    return Cycles::toSeconds(stop - start)/count;
}

double cond_wait_for_millisecond() {
    int count = 100;
    std::mutex mutex;
    std::condition_variable cond;
    std::unique_lock<std::mutex> lock(mutex);

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        cond.wait_for(lock, std::chrono::milliseconds(1));
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}

double cond_wait_for_microsecond() {
    int count = 10000;
    std::mutex mutex;
    std::condition_variable cond;
    std::unique_lock<std::mutex> lock(mutex);

    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        cond.wait_for(lock, std::chrono::microseconds(1));
    }
    uint64_t stop = Cycles::rdtsc();

    return Cycles::toSeconds(stop - start)/count;
}


double uncompressedLogEntryIteration() {
    int arraySize = 1000000;

    typedef Log::UncompressedEntry Entry;
    Entry *in = static_cast<Entry*>(malloc(sizeof(Entry)*arraySize));
    uint64_t junk = 0;

    uint64_t start = Cycles::rdtsc();
    for (int j = 0; j < arraySize; ++j) {
        junk += in[j].entrySize;
        junk += in[j].fmtId;
        junk += in[j].timestamp;
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&junk);
    free(in);

    return Cycles::toSeconds(stop - start)/(arraySize);
}

double uncompressedLogEntryIterationWithFence() {
    int arraySize = 1000000;

    typedef Log::UncompressedEntry Entry;
    Entry *in = static_cast<Entry*>(malloc(sizeof(Entry)*arraySize));
    uint64_t junk = 0;

    uint64_t start = Cycles::rdtsc();
    for (int j = 0; j < arraySize; ++j) {
        junk += in[j].entrySize;
        junk += in[j].fmtId;
        junk += in[j].timestamp;

        NanoLogInternal::Fence::lfence();
    }
    uint64_t stop = Cycles::rdtsc();

    discard(&junk);
    free(in);

    return Cycles::toSeconds(stop - start)/(arraySize);
}

// The following struct and table define each performance test in terms of
// a string name and a function that implements the test.
struct TestInfo {
    const char* name;             // Name of the performance test; this is
                                  // what gets typed on the command line to
                                  // run the test.
    double (*func)();             // Function that implements the test;
                                  // returns the time (in seconds) for each
                                  // iteration of that test.
    const char *description;      // Short description of this test (not more
                                  // than about 40 characters, so the entire
                                  // test output fits on a single line).
};
TestInfo tests[] = {
    {"aggregate_customFunction", aggregate_customFunction,
     "Sum 4 int's with a custom function"},
    {"aggregate_templates", aggregate_templates,
     "Sum 4 int's via varadic templates"},
    {"aggregate_va_args", aggregate_va_args,
     "Sum 4 int's via va_args"},
    {"arrayPush", arrayPush,
     "Push 4 uint64_t's into a byte array via cast + pointer bump"},
    {"arrayStructCast", arrayStructCast,
     "Push 4 uint64_t's into a byte array via casting it into a struct"},
    {"cond_wait_micro", cond_wait_for_microsecond,
     "Condition Variable wait with 1 microsecond timeout"},
    {"cond_wait_milli", cond_wait_for_millisecond,
     "Condition Variable wait with 1 millisecond timeout"},
    {"cyclesToSeconds", perfCyclesToSeconds,
     "Convert a rdtsc result to (double) seconds"},
    {"cyclesToNanos", perfCyclesToNanoseconds,
     "Convert a rdtsc result to (uint64_t) nanoseconds"},
    {"compressBinarySearch", compressBinarySearch,
     "Compress 1M uint64_t's via binary searching if-statements"},
    {"compressLinearSearch", compressLinearSearch,
     "Compress 1M uint64_t's via linear searching for-loop"},
    {"delayInBenchmark", delayInBenchmark,
     "Taking an addition, modulo, and rdtsc()"},
    {"div32", div32,
     "32-bit integer division instruction"},
    {"div64", div64,
     "64-bit integer division instruction"},
    {"functionCall", functionCall,
     "Call a function that has not been inlined"},
    {"functionDereference", functionDereference,
     "Randomly dereference a function array of size 50"},
    {"switchStatement", switchCost,
     "Random switch statement of size 50"},
    {"fgetc", fgetcFn,
     "Cost of a single fgetc"},
    {"fread1", fread1,
     "Cost of reading 1 byte via fread"},
    {"fread10", fread10,
     "Cost of reading 10 bytes via fread"},
    {"fread100", fread100,
     "Cost of reading 100 bytes via fread"},
    {"getcpu_rdtscp", sched_getcpu_rdtscp_test,
     "Cost of sched_getcpu + rdtscp"},
    {"ifstreamRead1", ifstreamRead1,
     "Cost of reading 1 byte from an ifstream"},
    {"ifstreamRead10", ifstreamRead10,
     "Cost of reading 10 bytes from an ifstream"},
    {"ifstreamRead100", ifstreamRead100,
     "Cost of reading 100 bytes from an ifstream"},
    {"mapCreate", mapCreate,
     "Create+delete entry in std::map"},
    {"mapLookup", mapLookup,
     "Lookup in std::map<uint64_t, uint64_t>"},
    {"manualCopyCached1", manualCopyCached1,
     "for-loop copy 1 byte with hot/fixed dst and src"},
    {"manualCopyCached10", manualCopyCached10,
     "for-loop copy 10 bytes with hot/fixed dst and src"},
    {"memcpyCached1", memcpyCached1,
     "memcpy 1 bytes with hot/fixed dst and src"},
    {"memcpyCached100", memcpyCached100,
     "memcpy 100 bytes with hot/fixed dst and src"},
    {"memcpyCached1000", memcpyCached1000,
     "memcpy 1000 bytes with hot/fixed dst and src"},
    {"memcpyCachedDst100", memcpyCachedDst100,
     "memcpy 100 bytes with hot/fixed dst and cold src"},
    {"memcpyCachedDst1000", memcpyCachedDst1000,
     "memcpy 1000 bytes with hot/fixed dst and cold src"},
    {"memcpyCold100", memcpyCold100,
     "memcpy 100 bytes with cold dst and src"},
    {"memcpyCold1000", memcpyCold1000,
     "memcpy 1000 bytes with cold dst and src"},
    {"mm_stream_pi", mm_stream_pi_test,
     "Cost to write 8-bytes without polluting cache"},
    {"mm_stream_pi_contended", mm_stream_pi_contended,
     "mm_stream_pi with a second thread reading the variable"},
    {"notify_all", notify_all,
     "condition_variable.notify_all()"},
    {"notify_one", notify_one,
     "condition_variable.notify_one()"},
    {"rdtsc", rdtsc_test,
     "cost of an rdtsc call"},
    {"rdtscp", rdtscp_test,
     "cost of an rdtscp call"},
    {"sched_getcpu", sched_getcpu_test,
     "Cost of sched_getcpu"},
    {"snprintfFileLocation", snprintfFileLocation,
     "snprintf only the __FILE__:__LINE__:__func___"},
    {"snprintfRAMCloud", snprintfRAMCloud,
     "snprintf in RAMCLOUD_LOG style with a 100 char user message"},
    {"snprintStatic100Char", snprintStatic100Char,
     "snprintf a static 100 character string wtih no format specifiers"},
    {"snprintfTime", snprintfTime,
     "snprintf the current time formatted as seconds.nanoseconds"},
    {"ctime", printTime_ctime,
     "snprintf the current time formatted using ctime"},
    {"strftime", printTime_strftime,
     "snprintf the current time formatted using strftime %y/%m/%d %H:%M:%S"},
    {"strftime_wConversion", printTime_strftime_wConversion,
     "snprintf the current time formatted using strftime with tm conversion"},
    {"rdtscTest", rdtscTest,
     "Read the fine-grain cycle counter"},
    {"high_resolution_clock", high_resolution_clockTest,
     "std::chrono::high_resolution_clock::now"},
    {"serialize", serialize,
     "cpuid instruction for serialize"},
    {"LogEntryIteration", uncompressedLogEntryIteration,
      "Per element cost of iterating through log entries"},
    {"LogEntryIterationFence", uncompressedLogEntryIterationWithFence,
      "Per element cost of iterating through log entries with lfences"},

};

/**
 * Runs a particular test and prints a one-line result message.
 *
 * \param info
 *      Describes the test to run.
 */
void runTest(TestInfo& info)
{
    double secs = info.func();
    int width = printf("%-23s ", info.name);
    if (secs < 1.0e-06) {
        width += printf("%8.2fns", 1e09*secs);
    } else if (secs < 1.0e-03) {
        width += printf("%8.2fus", 1e06*secs);
    } else if (secs < 1.0) {
        width += printf("%8.2fms", 1e03*secs);
    } else {
        width += printf("%8.2fs", secs);
    }
    printf("%*s %s\n", 26-width, "", info.description);
}

int
main(int argc, char *argv[])
{
    bindThreadToCpu(3);
    if (argc == 1) {
        // No test names specified; run all tests.
        for (int i = 0; i < sizeof(tests)/sizeof(TestInfo); ++i) {
            runTest(tests[i]);
        }
    } else {
        // Run only the tests that were specified on the command line.
        for (int j = 1; j < argc; j++) {
            bool foundTest = false;
            for (int i = 0; i < sizeof(tests)/sizeof(TestInfo); ++i) {
                if (strcmp(argv[j], tests[i].name) == 0) {
                    foundTest = true;
                    runTest(tests[i]);
                    break;
                }
            }
            if (!foundTest) {
                int width = printf("%-20s ??", argv[j]);
                printf("%*s No such test\n", 26-width, "");
            }
        }
    }
}