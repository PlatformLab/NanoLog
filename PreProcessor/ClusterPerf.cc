/* Copyright (c) 2011-2016 Stanford University
 * Copyright (c) 2011 Facebook
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

// This program runs a RAMCloud client along with a collection of benchmarks
// for measuring the performance of a RAMCloud cluster.  This file works in
// conjunction with clusterperf.py, which starts up the cluster servers
// along with one or more instances of this program. This file contains the
// low-level benchmark code.
//
// TO ADD A NEW BENCHMARK:
// 1. Decide on a symbolic name for the new test.
// 2. Write the function that implements the benchmark.  It goes in the
//    section labeled "test functions" below, in alphabetical order.  The
//    name of the function should be the same as the name of the test.
//    Tests generally work in one of two ways:
//    * The first way is to generate one or more individual metrics;
//      "basic" and "readNotFound" are examples of this style.  Be sure
//      to print results in the same way as existing tests, for consistency.
//    * The second style of test is one that generates a graph;  "readLoaded"
//      is an example of this style.  The test should output graph data in
//      gnuplot format (comma-separated values), with comments at the
//      beginning describing the data and including the name of the test
//      that generated it.
//  3. Add an entry for the new test in the "tests" table below; this is
//     used to dispatch to the test.
//  4. Add code for this test to clusterperf.py, following the instructions
//     in that file.

#include <boost/program_options.hpp>
#include <boost/version.hpp>
#include <algorithm>
#include <iostream>
#include <unordered_set>
namespace po = boost::program_options;

#include "assert.h"
#include "BasicTransport.h"
#include "btreeRamCloud/Btree.h"
#include "ClientLeaseAgent.h"
#include "CycleCounter.h"
#include "Cycles.h"
#include "PerfStats.h"
#include "IndexLookup.h"
#include "RamCloud.h"
#include "Util.h"
#include "TimeTrace.h"
#include "Transaction.h"

using namespace RAMCloud;

// Shared state for client library.
Context *context;

// Used to invoke RAMCloud operations.
static RamCloud* cluster;

// Total number of clients that will be participating in this test.
static int numClients;

// Number of virtual clients each physical client should simulate.
static int numVClients;

// Index of this client among all of the participating clients (between
// 0 and numClients-1).  Client 0 acts as master to control the overall
// flow of the test; the other clients are slaves that respond to
// commands from the master.
static int clientIndex;

// Value of the "--count" command-line option: used by some tests
// to determine how many times to invoke a particular operation.
static int count;

// Value of the "--size" command-line option: used by some tests to
// determine the number of bytes in each object.  -1 means the option
// wasn't specified, so each test should pick an appropriate default.
static int objectSize;

// Value of the "--numObjects" command-line option: used by some tests to
// determine the number of objects that should be part of a given operation.
static int numObjects;

// Value of the "--numTables" command-line option: used by some tests
// to specify the number of tables to create.
static int numTables;

// Value of the "--numIndexlet" command-line option: used by some tests
// to specify the number of indexlets to create.
static int numIndexlet;

// Value of the "--numIndexes" command-line option: used by some tests
// to specify the number of indexes/object.
static int numIndexes;

// Value of the "--warmup" command-line option: in some tests this
// determines how many times to invoke the operation before starting
// measurements (e.g. to make sure that caches are loaded).
static int warmupCount;

// Value of the "--workload" command-line option: used by some tests
// to specify the name of the workload the additional clients should
// run to provide load on the system.
static string workload;     // NOLINT

// Value of the "--targetOps" command-line option: used by some tests
// to specify the operations per second each load generating client
// should try to achieve.
static int targetOps;

// Value of the "--txSpan" command-line option: used by some tests
// the server span of a transaction.
static int txSpan;

// Identifier for table that is used for test-specific data.
uint64_t dataTable = -1;

// Identifier for table that is used to communicate between the master
// and slaves to coordinate execution of tests.
uint64_t controlTable = -1;

#define MAX_METRICS 8

// The following type holds metrics for all the clients.  Each inner vector
// corresponds to one metric and contains a value from each client, indexed
// by clientIndex.
typedef std::vector<std::vector<double>> ClientMetrics;

// Used to return results about the distribution of times for a
// particular operation.
struct TimeDist {
    double min;                   // Fastest time seen, in seconds.
    double p50;                   // Median time per operation, in seconds.
    double p90;                   // 90th percentile time/op, in seconds.
    double p99;                   // 99th percentile time/op, in seconds.
    double p999;                  // 99.9th percentile time/op, in seconds.
    double p9999;                 // 99.99th percentile time/op, in seconds,
                                  // or 0 if no such measurement.
    double p99999;                // 99.999th percentile time/op, in seconds,
                                  // or 0 if no such measurement.
    double bandwidth;             // Average throughput in bytes/sec., or 0
                                  // if no such measurement.
};

// Forward declarations:
extern void readThroughputMaster(int numObjects, int size, uint16_t keyLength);

//----------------------------------------------------------------------
// Utility functions used by the test functions
//----------------------------------------------------------------------

/**
 * Given a time value, return a string representation of the value
 * with an appropriate scale factor.
 *
 * \param seconds
 *      Time value, in seconds.
 * \result
 *      A string corresponding to the time value, such as "4.2ms".
 *      Appropriate units will be chosen, ranging from nanoseconds to
 *      seconds.
 */
string formatTime(double seconds)
{
    if (seconds < 1.0e-06) {
        return format("%5.1f ns", 1e09*seconds);
    } else if (seconds < 1.0e-03) {
        return format("%5.1f us", 1e06*seconds);
    } else if (seconds < 1.0) {
        return format("%5.1f ms", 1e03*seconds);
    } else {
        return format("%5.1f s ", seconds);
    }
}

/**
 * Given a vector of time values, sort it and return information
 * about various percentiles.
 *
 * \param times
 *      Interval lengths in Cycles::rdtsc units.
 * \param[out] dist
 *      The various percentile values in this structure will be
 *      filled in with times in seconds.
 */
void getDist(std::vector<uint64_t>& times, TimeDist* dist)
{
    int count = downCast<int>(times.size());
    std::sort(times.begin(), times.end());
    dist->min = Cycles::toSeconds(times[0]);
    int index = count/2;
    if (index < count) {
        dist->p50 = Cycles::toSeconds(times.at(index));
    } else {
        dist->p50 = 0;
    }
    index = count - (count+5)/10;
    if (index < count) {
        dist->p90 = Cycles::toSeconds(times.at(index));
    } else {
        dist->p90 = 0;
    }
    index = count - (count+50)/100;
    if (index < count) {
        dist->p99 = Cycles::toSeconds(times.at(index));
    } else {
        dist->p99 = 0;
    }
    index = count - (count+500)/1000;
    if (index < count) {
        dist->p999 = Cycles::toSeconds(times.at(index));
    } else {
        dist->p999 = 0;
    }
    index = count - (count+5000)/10000;
    if (index < count) {
        dist->p9999 = Cycles::toSeconds(times.at(index));
    } else {
        dist->p9999 = 0;
    }
    index = count - (count+50000)/100000;
    if (index < count) {
        dist->p99999 = Cycles::toSeconds(times.at(index));
    } else {
        dist->p99999 = 0;
    }
}

/**
 * Used to generate zipfian distributed random numbers where the distribution is
 * skewed toward the lower integers; e.g. 0 will be the most popular, 1 the next
 * most popular, etc.
 *
 * This class implements the core algorithm from YCSB's ZipfianGenerator; it, in
 * turn, uses the algorithm from "Quickly Generating Billion-Record Synthetic
 * Databases", Jim Gray et al, SIGMOD 1994.
 */
class ZipfianGenerator {
  public:
    /**
     * Construct a generator.  This may be expensive if n is large.
     *
     * \param n
     *      The generator will output random numbers between 0 and n-1.
     * \param theta
     *      The zipfian parameter where 0 < theta < 1 defines the skew; the
     *      smaller the value the more skewed the distribution will be. Default
     *      value of 0.99 comes from the YCSB default value.
     */
    explicit ZipfianGenerator(uint64_t n, double theta = 0.99)
        : n(n)
        , theta(theta)
        , alpha(1 / (1 - theta))
        , zetan(zeta(n, theta))
        , eta((1 - pow(2.0 / static_cast<double>(n), 1 - theta)) /
              (1 - zeta(2, theta) / zetan))
    {}

    /**
     * Return the zipfian distributed random number between 0 and n-1.
     */
    uint64_t nextNumber()
    {
        double u = static_cast<double>(generateRandom()) /
                   static_cast<double>(~0UL);
        double uz = u * zetan;
        if (uz < 1)
            return 0;
        if (uz < 1 + std::pow(0.5, theta))
            return 1;
        return 0 + static_cast<uint64_t>(static_cast<double>(n) *
                                         std::pow(eta*u - eta + 1.0, alpha));
    }

  private:
    const uint64_t n;       // Range of numbers to be generated.
    const double theta;     // Parameter of the zipfian distribution.
    const double alpha;     // Special intermediate result used for generation.
    const double zetan;     // Special intermediate result used for generation.
    const double eta;       // Special intermediate result used for generation.

    /**
     * Returns the nth harmonic number with parameter theta; e.g. H_{n,theta}.
     */
    static double zeta(uint64_t n, double theta)
    {
        double sum = 0;
        for (uint64_t i = 0; i < n; i++) {
            sum = sum + 1.0/(std::pow(i+1, theta));
        }
        return sum;
    }
};

/**
 * Used to generate a run workloads of a specific read/write distribution.  For
 * the most part, the workloads are modeled after the YCSB workload generator.
 */
class WorkloadGenerator {
  public:
    /**
     * Constructor for the workload generator.
     *
     * \param workloadName
     *      Name of the workload to be generated.
     */
    explicit WorkloadGenerator(string workloadName)
        : recordCount(2000000)
        , recordSizeB(objectSize)
        , readPercent(100)
        , generator()
    {
        if (workloadName == "YCSB-A") {
            recordCount = 1000000;
            recordSizeB = objectSize;
            readPercent = 50;
        } else if (workloadName == "YCSB-B") {
            recordCount = 1000000;
            recordSizeB = objectSize;
            readPercent = 95;
        } else if (workloadName == "YCSB-C") {
            recordCount = 1000000;
            recordSizeB = objectSize;
            readPercent = 100;
        } else if (workloadName == "WRITE-ONLY") {
            recordCount = 1000000;
            recordSizeB = objectSize;
            readPercent = 0;
        } else {
            RAMCLOUD_LOG(WARNING,
                "Unknown workload type %s - Using default",
                workloadName.c_str());
        }

        generator.construct(recordCount);
    }

    /**
     * Setup the workload; creates tables and loads working set.
     */
    void setup()
    {
        #define BATCH_SIZE 500
        const uint16_t keyLength = 30;

        // Initialize keys and values
        MultiWriteObject** objects = new MultiWriteObject*[BATCH_SIZE];
        char* keys = new char[BATCH_SIZE * keyLength];
        memset(keys, 0, BATCH_SIZE * keyLength);

        char* charValues = new char[BATCH_SIZE * recordSizeB];
        memset(charValues, 0, BATCH_SIZE * recordSizeB);

        uint64_t i, j;
        for (i = 0; i < static_cast<uint64_t>(recordCount); i++) {
            j = i % BATCH_SIZE;

            char* key = keys + j * keyLength;
            char* value = charValues + j * recordSizeB;
            string("workload").copy(key, 8);
            *reinterpret_cast<uint64_t*>(key + 8) = i;

            Util::genRandomString(value, recordSizeB);
            objects[j] = new MultiWriteObject(dataTable, key, keyLength, value,
                    recordSizeB);

            // Do the write and recycle the objects
            if (j == BATCH_SIZE - 1) {
                cluster->multiWrite(objects, BATCH_SIZE);

                // Clean up the actual MultiWriteObjects
                for (int k = 0; k < BATCH_SIZE; k++)
                    delete objects[k];

                memset(keys, 0, BATCH_SIZE * keyLength);
                memset(charValues, 0, BATCH_SIZE * recordSizeB);
            }
        }

        // Do the last partial batch and clean up, if it exists.
        j = i % BATCH_SIZE;
        if (j < BATCH_SIZE - 1) {
            cluster->multiWrite(objects, static_cast<uint32_t>(j));

            // Clean up the actual MultiWriteObjects
            for (uint64_t k = 0; k < j; k++)
                delete objects[k];
        }

        delete[] keys;
        delete[] charValues;
        delete[] objects;
    }

    /**
     * Run the workload and try to maintain a fix throughput.
     *
     * \param targetOps
     *      Throughput the workload should attempt to maintain; 0 means run at
     *      full throttle.
     */
    void run(uint64_t targetOps = 0)
    {
        const uint16_t keyLen = 30;
        char key[keyLen];
        Buffer readBuf;
        char value[recordSizeB];

        uint64_t readThreshold = (~0UL / 100) * readPercent;
        uint64_t opCount = 0;
        uint64_t targetMissCount = 0;
        uint64_t readCount = 0;
        uint64_t writeCount = 0;
        uint64_t targetNSPO = 0;
        if (targetOps > 0) {
            targetNSPO = 1000000000 / targetOps;
            // Randomize start time
            Cycles::sleep((generateRandom() % targetNSPO) / 1000);
        }

        uint64_t nextStop = 0;
        uint64_t start = Cycles::rdtsc();
        uint64_t stop = 0;

        try
        {
            while (true) {
                // Generate random key.
                memset(key, 0, keyLen);
                string("workload").copy(key, 8);
                *reinterpret_cast<uint64_t*>(key + 8) = generator->nextNumber();

                // Perform Operation
                if (generateRandom() <= readThreshold) {
                    // Do read
                    cluster->read(dataTable, key, keyLen, &readBuf);
                    readCount++;
                } else {
                    // Do write
                    Util::genRandomString(value, recordSizeB);
                    cluster->write(dataTable, key, keyLen, value, recordSizeB);
                    writeCount++;
                }
                opCount++;
                stop = Cycles::rdtsc();

                // throttle
                if (targetNSPO > 0) {
                    nextStop = start +
                               Cycles::fromNanoseconds(
                                    (opCount * targetNSPO) +
                                    (generateRandom() % targetNSPO) -
                                    (targetNSPO / 2));
                    if (Cycles::rdtsc() > nextStop) {
                        targetMissCount++;
                    }
                    while (Cycles::rdtsc() < nextStop);
                }
            }
        } catch (TableDoesntExistException &e) {
            LogLevel ll = NOTICE;
            if (targetMissCount > 0) {
                ll = WARNING;
            }
            RAMCLOUD_LOG(ll,
                    "Actual OPS %.0f / Target OPS %lu",
                    static_cast<double>(opCount) /
                    static_cast<double>(Cycles::toSeconds(stop - start)),
                    targetOps);
            RAMCLOUD_LOG(ll,
                    "%lu Misses / %lu Total -- %lu/%lu R/W",
                    targetMissCount, opCount, readCount, writeCount);
            throw e;
        }
    }
//private:
    int recordCount;
    int recordSizeB;
    int readPercent;
    Tub<ZipfianGenerator> generator;
};

/**
 * Encapsulates the state of a single client so we can simulate multiple virtual
 * clients with a single client executable.
 */
struct VirtualClient {
    explicit VirtualClient(RamCloud* ramcloud)
        : lease(ramcloud)
        , rpcTracker()
    {}

    /**
     * Used to install and uninstall the virtual client's state.  Makes it easy
     * to "context switch" between clients.
     */
    struct Context {
        explicit Context(VirtualClient* virtualClient)
            : virtualClient(virtualClient)
            , originalLeaseAgent(cluster->clientLeaseAgent)
            , originalTracker(cluster->rpcTracker)
        {
            // Set context variables.
            cluster->clientLeaseAgent = &virtualClient->lease;
            cluster->rpcTracker = &virtualClient->rpcTracker;
        }

        ~Context() {
            cluster->clientLeaseAgent = originalLeaseAgent;
            cluster->rpcTracker = originalTracker;
        }

        VirtualClient* virtualClient;
        ClientLeaseAgent* originalLeaseAgent;
        RpcTracker* originalTracker;

        DISALLOW_COPY_AND_ASSIGN(Context);
    };

    ClientLeaseAgent lease;
    RpcTracker rpcTracker;
    DISALLOW_COPY_AND_ASSIGN(VirtualClient);
};

/**
 * Given an integer value, generate a key of a given length
 * that corresponds to that value.
 *
 * \param value
 *      Unique value to encapsulate in the key.
 * \param length
 *      Total number of bytes in the resulting key. Must be at least 4.
 * \param dest
 *      Memory block in which to write the key; must contain at
 *      least length bytes.
 */
void makeKey(int value, uint32_t length, char* dest)
{
    memset(dest, 'x', length);
    *(reinterpret_cast<int*>(dest)) = value;
}

/**
 * Print a performance measurement consisting of a time value.
 *
 * \param name
 *      Symbolic name for the measurement, in the form test.value
 *      where \c test is the name of the test that generated the result,
 *      \c value is a name for the particular measurement.
 * \param seconds
 *      Time measurement, in seconds.
 * \param description
 *      Longer string (but not more than 20-30 chars) with a human-
 *      readable explanation of what the value refers to.
 */
void
printTime(const char* name, double seconds, const char* description)
{
    printf("%-20s ", name);
    if (seconds < 1.0e-06) {
        printf("%5.1f ns   ", 1e09*seconds);
    } else if (seconds < 1.0e-03) {
        printf("%5.1f us   ", 1e06*seconds);
    } else if (seconds < 1.0) {
        printf("%5.1f ms   ", 1e03*seconds);
    } else {
        printf("%5.1f s    ", seconds);
    }
    printf("  %s\n", description);
}

struct ramcloud_log;

//TODO(syang0)
void
evilTestCase(ramcloud_log* log) {
    RAMCLOUD_LOG(ERROR, "RAMCLOUD_LOG() \"RAMCLOUD_LOG(ERROR, \"Hi \")\"");
    RAMCLOUD_LOG(ERROR, "NEW"
            "Lines" "So"
            "Evil %s",
            "RAMCLOUD_LOG()");

    int i = 0;
    ++i; RAMCLOUD_LOG(ERROR, "Yup"); i++;

    { RAMCLOUD_LOG(ERROR, "No %s", std::string("Hello").c_str()); }
    {RAMCLOUD_LOG(ERROR,
        "I am so evil")}

    printf("RAMCLOUD_LOG()")

    RAMCLOUD_LOG(ERROR, "Hello %d",
        // 5
        5);

    /* This */ RAMCLOUD_LOG( /* is */ ERROR, "Hello /* uncool */");


    /*
     * RAMCLOUD_LOG(ERROR, "RAMCLOUD_LOG");
     */

     // RAMCLOUD_LOG(ERROR, "RAMCLOUD_LOG");

     RAMCLOUD_LOG(
        "OLO_SWAG");

     /* // YOLO
      */

     // /*
     RAMCLOUD_LOG(DEBUG, "SDF");
     const char str = ";"
     // */
}

/**
 * Print a performance measurement consisting of a bandwidth.
 *
 * \param name
 *      Symbolic name for the measurement, in the form test.value
 *      where \c test is the name of the test that generated the result,
 *      \c value is a name for the particular measurement.
 * \param bandwidth
 *      Measurement in units of bytes/second.
 * \param description
 *      Longer string (but not more than 20-30 chars) with a human-
 *      readable explanation of what the value refers to.
 */
void
printBandwidth(const char* name, double bandwidth, const char* description)
{
    double gb = 1024.0*1024.0*1024.0;
    double mb = 1024.0*1024.0;
    double kb = 1024.0;
    printf("%-20s ", name);
    if (bandwidth > gb) {
        printf("%5.1f GB/s ", bandwidth/gb);
    } else if (bandwidth > mb) {
        printf("%5.1f MB/s ", bandwidth/mb);
    } else if (bandwidth >kb) {
        printf("%5.1f KB/s ", bandwidth/kb);
    } else {
        printf("%5.1f B/s  ", bandwidth);
    }
    printf("  %s\n", description);
}

/**
 * Print a performance measurement consisting of a rate.
 *
 * \param name
 *      Symbolic name for the measurement, in the form test.value
 *      where \c test is the name of the test that generated the result,
 *      \c value is a name for the particular measurement.
 * \param value
 *      Measurement in units 1/second.
 * \param description
 *      Longer string (but not more than 20-30 chars) with a human-
 *      readable explanation of what the value refers to.
 */
void
printRate(const char* name, double value, const char* description)
{
    printf("%-20s  ", name);
    if (value > 1e09) {
        printf("%5.1f G/s  ", value/1e09);
    } else if (value > 1e06) {
        printf("%5.1f M/s  ", value/1e06);
    } else if (value > 1e03) {
        printf("%5.1f K/s  ", value/1e03);
    } else {
        printf("%5.1f /s   ", value);
    }
    printf("  %s\n", description);
}

/**
 * Print a performance measurement consisting of a percentage.
 *
 * \param name
 *      Symbolic name for the measurement, in the form test.value
 *      where \c test is the name of the test that generated the result,
 *      \c value is a name for the particular measurement.
 * \param value
 *      Measurement in units of %.
 * \param description
 *      Longer string (but not more than 20-30 chars) with a human-
 *      readable explanation of what the value refers to.
 */
void
printPercent(const char* name, double value, const char* description)
{
    printf("%-20s    %.1f %%      %s\n", name, value, description);
}

/**
 * Time how long it takes to do an indexed write/overwrite.
 *
 * \param tableId
 *      Table containing the object.
 * \param numKeys
 *      Number of keys in the object
 * \param keyList
 *      Information about all the keys in the object.
 * \param buf
 *      Pointer to the object's value.
 * \param length
 *      Size in bytes of the object's value.
 * \param numWarmups
 *      Number of warmup runs to do. These execute the same way as when
 *      experiment times are recorded.
 * \param numSamples
 *      Number of experiments to run and record in time vectors.
 * \param [out] writeTimes
 *      Records individual experiment indexed write times.
 * \param [out] overWriteTimes
 *      Records individual experiment indexed overwrite times.
 */
void
timeIndexWrite(uint64_t tableId, uint8_t numKeys, KeyInfo *keyList,
               const void* buf, uint32_t length, uint32_t numWarmups,
               uint32_t numSamples, std::vector<double>& writeTimes,
               std::vector<double>& overWriteTimes)
{
    assert(writeTimes.size() == numSamples);
    assert(overWriteTimes.size() == numSamples);

    uint64_t start;

    int warmups = static_cast<int>(numWarmups);
    int totalSamples = static_cast<int>(numSamples);
    for (int i = -warmups; i < totalSamples; i++) {
        bool warmup = i < 0;
        // sleep before first non-warmup
        if (i == 0)
            Cycles::sleep(100);

        start = Cycles::rdtsc();
        cluster->write(tableId, numKeys, keyList, buf, length);
        if (!warmup)
            writeTimes.at(i) = Cycles::toSeconds(Cycles::rdtsc() - start);

        start = Cycles::rdtsc();
        cluster->write(tableId, numKeys, keyList, buf, length);
        if (!warmup)
            overWriteTimes.at(i) = Cycles::toSeconds(Cycles::rdtsc() - start);

        // Allow time for asynchronous removes of index entries to complete.
        Cycles::sleep(100);

        cluster->remove(tableId, keyList[0].key, keyList[0].keyLength);
        // Allow time for asynchronous removes of index entries to complete.
        Cycles::sleep(100);
    }

    // Final write to facilitate lookup afterwards
    cluster->write(tableId, numKeys, keyList, buf, length);
}

/**
 * Measure lookupIndexKeys, lookupIndexKeys + readHashes,
 * and IndexLookup times.
 *
 * \param tableId
 *      Id of the table in which lookup is to be done.
 * \param indexId
 *      Id of the index for which keys have to be compared.
 * \param expectedFirstPkHash
 *      Primary key of the object that will be returned by
 *      the readHashes operation. Used for sanity checking.
 * \param firstKey
 *      Starting key for the key range in which keys are to be matched.
 *      The key range includes the firstKey.
 *      It does not necessarily have to be null terminated. The caller must
 *      ensure that the storage for this key is unchanged through the life of
 *      the RPC.
 * \param firstKeyLength
 *      Length in bytes of the firstKey.
 * \param lastKey
 *      Ending key for the key range in which keys are to be matched.
 *      The key range includes the lastKey.
 *      It does not necessarily have to be null terminated. The caller must
 *      ensure that the storage for this key is unchanged through the life of
 *      the RPC.
 * \param lastKeyLength
 *      Length in byes of the lastKey.
 * \param numWarmups
 *      Number of warmup runs to do. These execute the same way as when
 *      experiment times are recorded.
 * \param numSamples
 *      Number of experiments to run and record in time vectors.
 * \param objectsExpected
 *      How many objects should be in the given key range.
 *      Used for sanity checking.
 * \param [out] hashLookupTimes
 *      Records individual experiment lookupIndexKeys times.
 * \param [out] lookupAndReadTimes
 *      Records individual experiment lookupIndexKeys + readHashes times.
 * \param [out] indexLookupTimes
 *      Records individual experiment IndexLookup class times.
 */
void
timeIndexedRead(uint64_t tableId, uint8_t indexId, uint64_t expectedFirstPkHash,
        const void* firstKey, uint16_t firstKeyLength,
        const void* lastKey, uint16_t lastKeyLength,
        uint32_t numWarmups, uint32_t numSamples, uint32_t objectsExpected,
        std::vector<double>& hashLookupTimes,
        std::vector<double>& lookupAndReadTimes,
        std::vector<double>& indexLookupTimes)
{
    assert(hashLookupTimes.size() == numSamples);
    assert(lookupAndReadTimes.size() == numSamples);
    assert(indexLookupTimes.size() == numSamples);

    int warmups = static_cast<int>(numWarmups);
    int totalSamples = static_cast<int>(numSamples);
    for (int i = -warmups; i < totalSamples; i++) {
        bool warmup = i < 0;
        // sleep before first non-warmup
        if (i == 0)
            Cycles::sleep(100);

        const uint32_t maxNumHashes = 1000;
        uint32_t totalNumHashes = 0;
        uint32_t numHashes;
        uint64_t firstAllowedKeyHash = 0;
        uint16_t nextKeyLength;
        uint64_t nextKeyHash;

        uint64_t start;
        std::deque<Buffer> pkHashBuffers;
        const void* tempFirstKey = firstKey;
        uint16_t tempFirstKeyLength = firstKeyLength;
        uint64_t hashLookupTime = 0;
        while (true)
        {
            pkHashBuffers.emplace_back();
            Buffer& respBuffer = pkHashBuffers.back();

            start = Cycles::rdtsc();
            cluster->lookupIndexKeys(tableId, indexId, tempFirstKey,
                    tempFirstKeyLength, firstAllowedKeyHash, lastKey,
                    lastKeyLength, maxNumHashes, &respBuffer, &numHashes,
                    &nextKeyLength, &nextKeyHash);
            hashLookupTime += Cycles::rdtsc() - start;

            totalNumHashes += numHashes;

            if (nextKeyHash == 0) {
                break;
            } else {
                firstAllowedKeyHash = nextKeyHash;
                tempFirstKeyLength = nextKeyLength;

                uint32_t off = respBuffer.size() - nextKeyLength;
                tempFirstKey = respBuffer.getRange(off, nextKeyLength);
            }
        }
        if (!warmup)
            hashLookupTimes.at(i) = Cycles::toSeconds(hashLookupTime);

        assert(totalNumHashes == objectsExpected);
        assert(expectedFirstPkHash ==
                *pkHashBuffers.front().getOffset<uint64_t>(
                        sizeof32(WireFormat::LookupIndexKeys::Response)));

        Buffer readObjects;
        uint32_t numObjects;
        uint32_t totalNumObjects = 0;

        uint64_t readHashTime = 0;
        for (auto it = pkHashBuffers.begin(); it != pkHashBuffers.end(); it++) {
            numHashes = (it->getStart<WireFormat::LookupIndexKeys::Response>())
                    ->numHashes;
            it->truncateFront(sizeof32(WireFormat::LookupIndexKeys::Response));

            start = Cycles::rdtsc();
            uint32_t numReturnedHashes = cluster->readHashes(
                    tableId, numHashes, &(*it), &readObjects, &numObjects);
            readHashTime += Cycles::rdtsc() - start;

            assert(numReturnedHashes == numHashes); // else collision
            totalNumObjects += numObjects;
        }
        if (!warmup)
            lookupAndReadTimes.at(i) = hashLookupTimes.at(i) +
                Cycles::toSeconds(readHashTime);

        assert(totalNumObjects == objectsExpected);

        // now do IndexLookup
        totalNumObjects = 0;
        firstAllowedKeyHash = 0;
        start = Cycles::rdtsc();

        IndexKey::IndexKeyRange keyRange(indexId, firstKey, firstKeyLength,
                lastKey, lastKeyLength);
        IndexLookup rangeLookup(cluster, tableId, keyRange);

        while (rangeLookup.getNext()) {
            totalNumObjects++;
        };

        if (!warmup)
            indexLookupTimes.at(i) = Cycles::toSeconds(Cycles::rdtsc() - start);

        assert(totalNumObjects == objectsExpected);
    }
}

/**
 * Time how long it takes to read a set of objects in one multiRead
 * operation repeatedly.
 *
 * \param requests
 *      The set of ReadObjects that encapsulate information about objects
 *      to be read.
 * \param numObjects
 *      The number of objects to be read in a single multiRead operation.
 *
 * \return
 *      The average time, in seconds, to read all the objects in a single
 *      multiRead operation.
 */
double
timeMultiRead(MultiReadObject** requests, int numObjects)
{
    // Do the multiRead once just to warm up all the caches everywhere.
    cluster->multiRead(requests, numObjects);

    uint64_t runCycles = Cycles::fromSeconds(500/1e03);
    uint64_t start = Cycles::rdtsc();
    uint64_t elapsed;
    int count = 0;
    while (true) {
        for (int i = 0; i < 10; i++) {
            cluster->multiRead(requests, numObjects);
        }
        count += 10;
        elapsed = Cycles::rdtsc() - start;
        if (elapsed >= runCycles)
            break;
    }
    return Cycles::toSeconds(elapsed)/count;
}

/**
 * Time how long it takes to write a set of objects in one multiWrite
 * operation repeatedly.
 *
 * \param requests
 *      The set of WriteObjects that encapsulate information about objects
 *      to be written.
 * \param numObjects
 *      The number of objects to be written in a single multiWrite operation.
 *
 * \return
 *      The average time, in seconds, to write all the objects in a single
 *      multiWrite operation.
 */
double
timeMultiWrite(MultiWriteObject** requests, int numObjects)
{
    uint64_t runCycles = Cycles::fromSeconds(500/1e03);
    uint64_t start = Cycles::rdtsc();
    uint64_t elapsed;
    int count = 0;
    while (true) {
        for (int i = 0; i < 10; i++) {
            cluster->multiWrite(requests, numObjects);
        }
        count += 10;
        elapsed = Cycles::rdtsc() - start;
        if (elapsed >= runCycles)
            break;
    }
    return Cycles::toSeconds(elapsed)/count;
}

/**
 * Time how long it takes to read a particular object repeatedly.
 *
 * \param tableId
 *      Table containing the object.
 * \param key
 *      Variable length key that uniquely identifies the object within tableId.
 * \param keyLength
 *      Size in bytes of the key.
 * \param ms
 *      Read the object repeatedly until this many total ms have
 *      elapsed.
 * \param value
 *      The contents of the object will be stored here, in case
 *      the caller wants to examine them.
 *
 * \return
 *      The average time to read the object, in seconds.
 */
double
timeRead(uint64_t tableId, const void* key, uint16_t keyLength,
         double ms, Buffer& value)
{
    uint64_t runCycles = Cycles::fromSeconds(ms/1e03);

    // Read the value once just to warm up all the caches everywhere.
    cluster->read(tableId, key, keyLength, &value);

    uint64_t start = Cycles::rdtsc();
    uint64_t elapsed;
    int count = 0;
    while (true) {
        for (int i = 0; i < 10; i++) {
            cluster->read(tableId, key, keyLength, &value);
        }
        count += 10;
        elapsed = Cycles::rdtsc() - start;
        if (elapsed >= runCycles)
            break;
    }
    return Cycles::toSeconds(elapsed)/count;
}

/**
 * Read a particular object repeatedly and return information about
 * the distribution of read times.
 *
 * \param tableId
 *      Table containing the object.
 * \param key
 *      Variable length key that uniquely identifies the object within tableId.
 * \param keyLength
 *      Size in bytes of the key.
 * \param count
 *      Read the object this many times, unless time runs out.
 * \param timeLimit
 *      Maximum time (in seconds) to spend on this test: if this much
 *      time elapses, then less than count iterations will be run.
 * \param value
 *      The contents of the object will be stored here, in case
 *      the caller wants to check them.
 *
 * \return
 *      Information about how long the reads took.
 */
TimeDist
readObject(uint64_t tableId, const void* key, uint16_t keyLength,
         int count, double timeLimit, Buffer& value)
{
    uint64_t total = 0;
    std::vector<uint64_t> times(count);
    uint64_t stopTime = Cycles::rdtsc() + Cycles::fromSeconds(timeLimit);

    // Read the value once just to warm up all the caches everywhere.
    cluster->read(tableId, key, keyLength, &value);

    for (int i = 0; i < count; i++) {
        uint64_t start = Cycles::rdtsc();
        if (start >= stopTime) {
            LOG(NOTICE, "time expired after %d iterations", i);
            times.resize(i);
            break;
        }
        cluster->read(tableId, key, keyLength, &value);
        uint64_t interval = Cycles::rdtsc() - start;
        total += interval;
        times.at(i) = interval;
    }
    TimeDist result;
    getDist(times, &result);
    double totalBytes = value.size();
    totalBytes *= downCast<int>(times.size());
    result.bandwidth = totalBytes/Cycles::toSeconds(total);

    return result;
}

/**
 * Read randomly-chosen objects from a single table and return information about
 * the distribution of read times.
 *
 * \param tableId
 *      Table containing the object.
 * \param numObjects
 *      Total number of objects in the table. Keys must have been generated
 *      by makeKey(i, keyLength, ...), where 0 <= i < numObjects.
 * \param keyLength
 *      Size in bytes of keys.
 * \param count
 *      Read the object this many times, unless time runs out.
 * \param timeLimit
 *      Maximum time (in seconds) to spend on this test: if this much
 *      time elapses, then fewer than count iterations will be run.
 *
 * \return
 *      Information about how long the reads took.
 */
TimeDist
readRandomObjects(uint64_t tableId, uint32_t numObjects, uint16_t keyLength,
         int count, double timeLimit)
{
    uint64_t total = 0;
    std::vector<uint64_t> times(count);
    char key[keyLength];
    Buffer value;
    uint64_t stopTime = Cycles::rdtsc() + Cycles::fromSeconds(timeLimit);

    for (int i = 0; i < count; i++) {
        uint64_t start = Cycles::rdtsc();
        if (start >= stopTime) {
            LOG(NOTICE, "time expired after %d iterations", i);
            times.resize(i);
            break;
        }
        makeKey(downCast<int>(generateRandom()%numObjects), keyLength, key);
        cluster->read(tableId, key, keyLength, &value);
        uint64_t interval = Cycles::rdtsc() - start;
        total += interval;
        times.at(i) = interval;
    }
    TimeDist result;
    getDist(times, &result);
    double totalBytes = value.size();
    totalBytes *= downCast<int>(times.size());
    result.bandwidth = totalBytes/Cycles::toSeconds(total);

    return result;
}

/**
 * Write a particular object repeatedly and return information about
 * the distribution of write times.
 *
 * \param tableId
 *      Table containing the object.
 * \param key
 *      Variable length key that uniquely identifies the object within tableId.
 * \param keyLength
 *      Size in bytes of the key.
 * \param value
 *      Pointer to first byte of contents to write into the object.
 * \param length
 *      Size of data at \c value.
 * \param count
 *      Write the object this many times.
 * \param timeLimit
 *      Maximum time (in seconds) to spend on this test: if this much
 *      time elapses, then less than count iterations will be run.
 *
 * \return
 *      Information about how long the writes took.
 */
TimeDist
writeObject(uint64_t tableId, const void* key, uint16_t keyLength,
          const void* value, uint32_t length, int count, double timeLimit)
{
    uint64_t total = 0;
    std::vector<uint64_t> times(count);
    uint64_t stopTime = Cycles::rdtsc() + Cycles::fromSeconds(timeLimit);

    // Write the value once just to warm up all the caches everywhere.
    cluster->write(tableId, key, keyLength, value, length);

    for (int i = 0; i < count; i++) {
        uint64_t start = Cycles::rdtsc();
        if (start >= stopTime) {
            LOG(NOTICE, "time expired after %d iterations", i);
            times.resize(i);
            break;
        }
        cluster->write(tableId, key, keyLength, value, length);
        uint64_t interval = Cycles::rdtsc() - start;
        total += interval;
        times.at(i) = interval;
    }
    TimeDist result;
    getDist(times, &result);
    double totalBytes = length;
    totalBytes *= downCast<int>(times.size());
    result.bandwidth = totalBytes/Cycles::toSeconds(total);

    return result;
}

/**
 * Write randomly-chosen objects in a single table and return information about
 * the distribution of write times.
 *
 * \param tableId
 *      Table containing the object.
 * \param numObjects
 *      Total number of objects in the table. Keys must have been generated
 *      by makeKey(i, keyLength, ...), where 0 <= i < numObjects.
 * \param keyLength
 *      Size in bytes of each key.
 * \param valueLength
 *      Number of bytes of data to write into each object; the actual values
 *      will be randomly chosen.
 * \param count
 *      Write this many objects.
 * \param timeLimit
 *      Maximum time (in seconds) to spend on this test: if this much
 *      time elapses, then less than count iterations will be run.
 *
 * \return
 *      Information about how long the writes took.
 */
TimeDist
writeRandomObjects(uint64_t tableId, uint32_t numObjects, uint16_t keyLength,
          uint32_t valueLength, int count, double timeLimit)
{
    uint64_t total = 0;
    std::vector<uint64_t> times(count);
    char key[keyLength];
    char value[valueLength];
    memset(value, 'x', valueLength);
    uint64_t stopTime = Cycles::rdtsc() + Cycles::fromSeconds(timeLimit);

    for (int i = 0; i < count; i++) {
        makeKey(downCast<int>(generateRandom()%numObjects), keyLength, key);
        uint64_t start = Cycles::rdtsc();
        if (start >= stopTime) {
            LOG(NOTICE, "time expired after %d iterations", i);
            times.resize(i);
            break;
        }
        cluster->write(tableId, key, keyLength, value, valueLength);
        uint64_t interval = Cycles::rdtsc() - start;
        total += interval;
        times.at(i) = interval;
    }
    TimeDist result;
    getDist(times, &result);
    double totalBytes = valueLength;
    totalBytes *= downCast<int>(times.size());
    result.bandwidth = totalBytes/Cycles::toSeconds(total);

    return result;
}

/**
 * Fill a buffer with an ASCII value that can be checked later to ensure
 * that no data has been lost or corrupted.  A particular tableId, key and
 * keyLength are incorporated into the value (under the assumption that
 * the value will be stored in that object), so that values stored in
 * different objects will be detectably different.
 *
 * \param buffer
 *      Buffer to fill; any existing contents will be discarded.
 * \param size
 *      Number of bytes of data to place in the buffer.
 * \param tableId
 *      This table identifier will be reflected in the value placed in the
 *      buffer.
 * \param key
 *      This key will be reflected in the value placed in the buffer.
 * \param keyLength
 *      This key Length will be reflected in the value placed in the buffer.
 */
void
fillBuffer(Buffer& buffer, uint32_t size, uint64_t tableId,
           const void* key, uint16_t keyLength)
{
    char chunk[501];
    buffer.reset();
    uint32_t bytesLeft = size;
    int position = 0;
    while (bytesLeft > 0) {
        // Write enough data to completely fill the chunk buffer, then
        // ignore the terminating NULL character that snprintf puts at
        // the end.
        int written = snprintf(chunk, sizeof(chunk),
            "| %d: tableId 0x%lx, key %.*s, keyLength %d %s",
            position, tableId, keyLength, reinterpret_cast<const char*>(key),
            keyLength, "0123456789");
        assert(written >= 0); // encoding error

        uint32_t chunkLength = std::min(
                static_cast<uint32_t>(sizeof(chunk) - 1),
                static_cast<uint32_t>(written));
        chunkLength = std::min(chunkLength, bytesLeft);

        buffer.appendCopy(chunk, chunkLength);
        bytesLeft -= chunkLength;
        position += chunkLength;
    }
}

/**
 * Check the contents of a buffer to ensure that it contains the same data
 * generated previously by fillBuffer.  Generate a log message if a
 * problem is found.
 *
 * \param buffer
 *      Buffer whose contents are to be checked.
 * \param offset
 *      Check the data starting at this offset in the buffer.
 * \param expectedLength
 *      The buffer should contain this many bytes starting at offset.
 * \param tableId
 *      This table identifier should be reflected in the buffer's data.
 * \param key
 *      This key should be reflected in the buffer's data.
 * \param keyLength
 *      This key length should be reflected in the buffer's data.
 *
 * \return
 *      True means the buffer has the "expected" contents; false means
 *      there was an error.
 */
bool
checkBuffer(Buffer* buffer, uint32_t offset, uint32_t expectedLength,
        uint64_t tableId, const void* key, uint16_t keyLength)
{
    uint32_t length = buffer->size();
    if (length != (expectedLength + offset)) {
        RAMCLOUD_LOG(ERROR, "corrupted data: expected %u bytes, "
                "found %u bytes", expectedLength, length - offset);
        return false;
    }
    Buffer comparison;
    fillBuffer(comparison, expectedLength, tableId, key, keyLength);
    for (uint32_t i = 0; i < expectedLength; i++) {
        char c1 = *buffer->getOffset<char>(offset + i);
        char c2 = *comparison.getOffset<char>(i);
        if (c1 != c2) {
            int start = i - 10;
            const char* prefix = "...";
            const char* suffix = "...";
            if (start <= 0) {
                start = 0;
                prefix = "";
            }
            uint32_t length = 20;
            if (start+length >= expectedLength) {
                length = expectedLength - start;
                suffix = "";
            }
            RAMCLOUD_LOG(ERROR, "corrupted data: expected '%c', got '%c' "
                    "(\"%s%.*s%s\" vs \"%s%.*s%s\")", c2, c1, prefix, length,
                    static_cast<const char*>(comparison.getRange(start,
                    length)), suffix, prefix, length,
                    static_cast<const char*>(buffer->getRange(offset + start,
                    length)), suffix);
            return false;
        }
    }
    return true;
}

/**
 * Compute the key for a particular control value in a particular client.
 *
 * \param client
 *      Index of the desired client.
 * \param name
 *      Name of control value: state (current state of the slave),
 *      command (command issued by the master for the slave),
 *      doc (documentation string for use in log messages), or
 *      metrics (statistics returned from slaves back to the master).
 *
 */
string
keyVal(int client, const char* name)
{
    return format("%d:%s", client, name);
}

/**
 * Slaves invoke this function to indicate their current state.
 *
 * \param state
 *      A string identifying what the slave is doing now, such as "idle".
 */
void
setSlaveState(const char* state)
{
    string key = keyVal(clientIndex, "state");
    cluster->write(controlTable, key.c_str(), downCast<uint16_t>(key.length()),
            state);
}

/**
 * Read the value of an object and place it in a buffer as a null-terminated
 * string.
 *
 * \param tableId
 *      Identifier of the table containing the object.
 * \param key
 *      Variable length key that uniquely identifies the object within table.
 * \param keyLength
 *      Size in bytes of the key.
 * \param value
 *      Buffer in which to store the object's value.
 * \param size
 *      Size of buffer.
 *
 * \return
 *      The return value is a pointer to buffer, which contains the contents
 *      of the specified object, null-terminated and truncated if needed to
 *      make it fit in the buffer.
 */
char*
readObject(uint64_t tableId, const void* key, uint16_t keyLength,
           char* value, uint32_t size)
{
    Buffer buffer;
    cluster->read(tableId, key, keyLength, &buffer);
    uint32_t actual = buffer.size();
    if (size <= actual) {
        actual = size - 1;
    }
    buffer.copy(0, size, value);
    value[actual] = 0;
    return value;
}

/**
 * A slave invokes this function to wait for the master to issue it a
 * command other than "idle"; the string value of the command is returned.
 *
 * \param buffer
 *      Buffer in which to store the state.
 * \param size
 *      Size of buffer.
 * \param remove
 *      If true, the command object is removed once we have retrieved it
 *      (so the next indication of this method will wait for a new command).
 *      If false, the command object is left in place, so it will be
 *      returned by future calls to this method, unless someone else
 *      changes it.
 *
 * \return
 *      The return value is a pointer to a buffer, which now holds the
 *      command.
 */
const char*
getCommand(char* buffer, uint32_t size, bool remove = true)
{
    while (true) {
        try {
            string key = keyVal(clientIndex, "command");
            readObject(controlTable, key.c_str(),
                    downCast<uint16_t>(key.length()), buffer, size);
            if (strcmp(buffer, "idle") != 0) {
                if (remove) {
                    // Delete the command value so we don't process the same
                    // command twice.
                    cluster->remove(controlTable, key.c_str(),
                            downCast<uint16_t>(key.length()));
                }
                return buffer;
            }
        }
        catch (TableDoesntExistException& e) {
        }
        catch (ObjectDoesntExistException& e) {
        }
        Cycles::sleep(10000);
    }
}

/**
 * Wait for a particular object to come into existence and, optionally,
 * for it to take on a particular value.  Give up if the object doesn't
 * reach the desired state within a short time period.
 *
 * \param tableId
 *      Identifier of the table containing the object.
 * \param key
 *      Variable length key that uniquely identifies the object within table.
 * \param keyLength
 *      Size in bytes of the key.
 * \param desired
 *      If non-null, specifies a string value; this function won't
 *      return until the object's value matches the string.
 * \param value
 *      The actual value of the object is returned here.
 * \param timeout
 *      Seconds to wait before giving up and throwing an Exception.
 */
void
waitForObject(uint64_t tableId, const void* key, uint16_t keyLength,
              const char* desired, Buffer& value, double timeout = 1.0)
{
    uint64_t start = Cycles::rdtsc();
    size_t length = desired ? strlen(desired) : -1;
    while (true) {
        try {
            cluster->read(tableId, key, keyLength, &value);
            if (desired == NULL) {
                return;
            }
            const char *actual = value.getStart<char>();
            if ((length == value.size()) &&
                    (memcmp(actual, desired, length) == 0)) {
                return;
            }
            double elapsed = Cycles::toSeconds(Cycles::rdtsc() - start);
            if (elapsed > timeout) {
                // Slave is taking too long; time out.
                throw Exception(HERE, format(
                        "Object <%lu, %.*s> didn't reach desired state '%s' "
                        "(actual: '%.*s')",
                        tableId, keyLength, reinterpret_cast<const char*>(key),
                        desired, downCast<int>(value.size()),
                        actual));
                exit(1);
            }
        }
        catch (TableDoesntExistException& e) {
        }
        catch (ObjectDoesntExistException& e) {
        }
    }
}

/**
 * The master invokes this function to wait for a slave to respond
 * to a command and enter a particular state.  Give up if the slave
 * doesn't enter the desired state within a short time period.
 *
 * \param slave
 *      Index of the slave (1 corresponds to the first slave).
 * \param state
 *      A string identifying the desired state for the slave.
 * \param timeout
 *      Seconds to wait before giving up and throwing an Exception.
 */
void
waitSlave(int slave, const char* state, double timeout = 1.0)
{
    Buffer value;
    string key = keyVal(slave, "state");
    waitForObject(controlTable, key.c_str(), downCast<uint16_t>(key.length()),
            state, value, timeout);
}

/**
 * Issue a command to one or more slaves and wait for them to receive
 * the command.
 *
 * \param command
 *      A string identifying what the slave should do next.  If NULL
 *      then no command is sent; we just wait for the slaves to reach
 *      the given state.
 * \param state
 *      The state that each slave will enter once it has received the
 *      command.  NULL means don't wait for the slaves to receive the
 *      command.
 * \param firstSlave
 *      Index of the first slave to interact with.
 * \param numSlaves
 *      Total number of slaves to command.
 */
void
sendCommand(const char* command, const char* state, int firstSlave,
            int numSlaves = 1)
{
    if (command != NULL) {
        for (int i = 0; i < numSlaves; i++) {
            string key = keyVal(firstSlave+i, "command");
            cluster->write(controlTable, key.c_str(),
                    downCast<uint16_t>(key.length()), command);
        }
    }
    if (state != NULL) {
        for (int i = 0; i < numSlaves; i++) {
            waitSlave(firstSlave+i, state);
        }
    }
}

/**
 * Create one table for each slot available in tableIds vector, each on a different
 * master, and create one object in each table.
 *
 * \param tableIds
 *      Vector to fill with table ids for the tables created. The size
 *      of the vector provided determines how many tables are created.
 * \param objectSize
 *      Number of bytes in the object to create each table.
 * \param key
 *      Key to use for the created object in each table.
 * \param keyLength
 *      Size in bytes of the key.
 *
 */
void
createTables(std::vector<uint64_t> &tableIds, int objectSize, const void* key,
        uint16_t keyLength)
{
    // Create the tables in backwards order to reduce possible correlations
    // between clients, tables, and servers (if we have 60 clients and 60
    // servers, with clients and servers colocated and client i accessing
    // table i, we wouldn't want each client reading a table from the
    // server on the same machine).
    for (int i = downCast<int>(tableIds.size())-1; i >= 0;  i--) {
        char tableName[20];
        snprintf(tableName, sizeof(tableName), "table%d", i);
        cluster->createTable(tableName);
        tableIds.at(i) = cluster->getTableId(tableName);
        Buffer data;
        fillBuffer(data, objectSize, tableIds.at(i), key, keyLength);
        cluster->write(tableIds.at(i), key, keyLength,
                data.getRange(0, objectSize), objectSize);
    }
}

/**
 * Obtain tableIds for tables created on multiple masters by createTable().
 *
 * \param tableIds
 *      Vector to fill with table ids for the tables created using createTable
 *      by another client.  The size of the vector provided determines how many
 *      tables are assumed to have been created.
 */
void
getTableIds(std::vector<uint64_t> &tableIds)
{
    for (int i = downCast<int>(tableIds.size())-1; i >= 0;  i--) {
        char tableName[20];
        snprintf(tableName, sizeof(tableName), "table%d", i);
        tableIds.at(i) = cluster->getTableId(tableName);
    }
}

/**
 * Obtain tableIds for tables created on multiple maters by createTable().
 *
 * \param numTables
 *      How many tables created before using #createTables().
 *
 * \return
 *      Pointer to array of tableIds.
 */
uint64_t*
getTableIds(int numTables)
{
    uint64_t* tableIds = new uint64_t[numTables];

    // Create the tables in backwards order to reduce possible correlations
    // between clients, tables, and servers (if we have 60 clients and 60
    // servers, with clients and servers colocated and client i accessing
    // table i, we wouldn't want each client reading a table from the
    // server on the same machine).
    for (int i = numTables-1; i >= 0;  i--) {
        char tableName[20];
        snprintf(tableName, sizeof(tableName), "table%d", i);
        tableIds[i] = cluster->getTableId(tableName);
    }
    return tableIds;
}

/**
 * Slaves invoke this method to return one or more performance measurements
 * back to the master.
 *
 * \param m0
 *      A performance measurement such as latency or bandwidth.  The precise
 *      meaning is defined by each individual test, and most tests only use
 *      a subset of the possible metrics.
 * \param m1
 *      Another performance measurement.
 * \param m2
 *      Another performance measurement.
 * \param m3
 *      Another performance measurement.
 * \param m4
 *      Another performance measurement.
 * \param m5
 *      Another performance measurement.
 * \param m6
 *      Another performance measurement.
 * \param m7
 *      Another performance measurement.
 */
void
sendMetrics(double m0, double m1 = 0.0, double m2 = 0.0, double m3 = 0.0,
        double m4 = 0.0, double m5 = 0.0, double m6 = 0.0, double m7 = 0.0)
{
    double metrics[MAX_METRICS];
    metrics[0] = m0;
    metrics[1] = m1;
    metrics[2] = m2;
    metrics[3] = m3;
    metrics[4] = m4;
    metrics[5] = m5;
    metrics[6] = m6;
    metrics[7] = m7;
    string key = keyVal(clientIndex, "metrics");
    cluster->write(controlTable, key.c_str(), downCast<uint16_t>(key.length()),
            metrics, sizeof(metrics));
}

/**
 * Masters invoke this method to retrieved performance measurements from
 * slaves.  This method waits for slaves to fill in their metrics, if they
 * haven't already.
 *
 * \param metrics
 *      This vector of vectors is cleared and then filled with the slaves'
 *      performance data.  Each inner vector corresponds to one metric
 *      and contains a value from each of the slaves.
 * \param clientCount
 *      Metrics will be read from this many clients, starting at 0.
 */
void
getMetrics(ClientMetrics& metrics, int clientCount)
{
    // First, reset the result.
    metrics.clear();
    metrics.resize(MAX_METRICS);
    for (int i = 0; i < MAX_METRICS; i++) {
        metrics[i].resize(clientCount);
        for (int j = 0; j < clientCount; j++) {
            metrics[i][j] = 0.0;
        }
    }

    // Iterate over all the slaves to fetch metrics from each.
    for (int client = 0; client < clientCount; client++) {
        Buffer metricsBuffer;
        string key = keyVal(client, "metrics");
        waitForObject(controlTable, key.c_str(),
                downCast<uint16_t>(key.length()), NULL, metricsBuffer);
        const double* clientMetrics = static_cast<const double*>(
                metricsBuffer.getRange(0,
                MAX_METRICS*sizeof32(double)));  // NOLINT
        for (int i = 0; i < MAX_METRICS; i++) {
            metrics[i][client] = clientMetrics[i];
        }
    }
}

/**
 * Return the largest element in a vector.
 *
 * \param data
 *      Input values.
 */
double
max(std::vector<double>& data)
{
    double result = data[0];
    for (int i = downCast<int>(data.size())-1; i > 0; i--) {
        if (data[i] > result)
            result = data[i];
    }
    return result;
}

/**
 * Return the smallest element in a vector.
 *
 * \param data
 *      Input values.
 */
double
min(std::vector<double>& data)
{
    double result = data[0];
    for (int i = downCast<int>(data.size())-1; i > 0; i--) {
        if (data[i] < result)
            result = data[i];
    }
    return result;
}

/**
 * Return the sum of the elements in a vector.
 *
 * \param data
 *      Input values.
 */
double
sum(std::vector<double>& data)
{
    double result = 0.0;
    for (int i = downCast<int>(data.size())-1; i >= 0; i--) {
        result += data[i];
    }
    return result;
}

/**
 * Return the average of the elements in a vector.
 *
 * \param data
 *      Input values.
 */
double
average(std::vector<double>& data)
{
    double result = 0.0;
    int length = downCast<int>(data.size());
    for (int i = length-1; i >= 0; i--) {
        result += data[i];
    }
    return result / length;
}

/**
 * Print the elements of a vector
 *
 * \param data
 *      Vector whose elements need to be printed
 */
void
printVector(std::vector<double>& data)
{
    for (std::vector<double>::iterator it = data.begin();
                                    it != data.end(); ++it)
        printf("%lf\n", 1e06*(*it));
}

/**
 * Fill a table with a given number of objects of a given size.
 * This method uses multi-writes to do it quickly.
 *
 * \param tableId
 *      Identifier for the table in which the objects should be written.
 * \param numObjects
 *      Number of objects to write in the table. The objects will have keys
 *      generated by passing the values (0..numObjects-1) to makeKey.
 * \param keyLength
 *      Number of bytes in each key.
 * \param valueLength
 *      Size of each object, in bytes.
 */
void fillTable(uint64_t tableId, int numObjects, uint16_t keyLength,
        uint32_t valueLength)
{
    // Compute an appropriate batch size (at most 500, even for small
    // objects, but smaller if needed to keep request length < 1 MB).
    int batchSize = 1000000/valueLength;
    if (batchSize > 500) {
        batchSize = 500;
    } else if (batchSize < 1) {
        batchSize = 1;
    }

    // The following buffer is used to accumulate keys and values for
    // a single multi-write operation.
    Buffer buffer;
    char keys[batchSize*keyLength];
    char value[valueLength];
    memset(value, 'x', valueLength);
    MultiWriteObject* objects[batchSize];

    // Each iteration through the following loop adds one object to
    // the current multi-write, and invokes the multi-write if needed.
    uint64_t writeTime = 0;
    for (int i = 0; i < numObjects; i++) {
        int j = i % batchSize;

        char* key = &keys[j*keyLength];
        makeKey(i, keyLength, key);
        objects[j] = buffer.emplaceAppend<MultiWriteObject>(tableId,
                key, keyLength, &value[0], valueLength);

        // Do the write, if needed.
        if ((j == (batchSize - 1)) || (i == (numObjects-1))) {
            uint64_t start = Cycles::rdtsc();
            cluster->multiWrite(objects, j+1);
            writeTime += Cycles::rdtsc() - start;
            buffer.reset();
        }
    }
    double rate = numObjects/Cycles::toSeconds(writeTime);
    RAMCLOUD_LOG(NOTICE, "write rate for %d-byte objects: %.1f kobjects/sec,"
            " %.1f MB/sec",
            valueLength, rate/1e03, rate*(keyLength+valueLength)/1e06);
}

//----------------------------------------------------------------------
// Test functions start here
//----------------------------------------------------------------------

// Random read and write times for objects of different sizes
void
basic()
{
    if (clientIndex != 0)
        return;
    Buffer input, output;
#define NUM_SIZES 5
    int sizes[] = {100, 1000, 10000, 100000, 1000000};
    TimeDist readDists[NUM_SIZES], writeDists[NUM_SIZES];
    const char* ids[] = {"100", "1K", "10K", "100K", "1M"};
    uint16_t keyLength = 30;
    char name[50], description[50];

    // Each iteration through the following loop measures random reads and
    // writes of a particular object size. Start with the largest object
    // size and work down to the smallest (this way, each iteration will
    // replace all of the objects created by the previous iteration).
    for (int i = NUM_SIZES-1; i >= 0; i--) {
        int size = sizes[i];

        // Generate roughly 500MB of data of the current size. The "20"
        // below accounts for additional overhead per object beyond the
        // key and value.
        uint32_t numObjects = 200000000/(size + keyLength + 20);
        LOG(NOTICE, "Filling table with %d-byte objects", size);
        cluster->logMessageAll(NOTICE,
                "Filling table with %d-byte objects", size);
        fillTable(dataTable, numObjects, keyLength, size);

        LOG(NOTICE, "Starting read test for %d-byte objects", size);
        cluster->logMessageAll(NOTICE,
                "Starting read test for %d-byte objects", size);
        readDists[i] = readRandomObjects(dataTable, numObjects, keyLength,
                100000, 2.0);

        LOG(NOTICE, "Starting write test for %d-byte objects", size);
        cluster->logMessageAll(NOTICE,
                "Starting write test for %d-byte objects", size);
        writeDists[i] =  writeRandomObjects(dataTable, numObjects, keyLength,
                size, 100000, 2.0);
    }
    Logger::get().sync();

    // Print out the results (in a different order):
    for (int i = 0; i < NUM_SIZES; i++) {
        TimeDist* dist = &readDists[i];
        snprintf(description, sizeof(description),
                "read random %sB object (%uB key)", ids[i], keyLength);
        snprintf(name, sizeof(name), "basic.read%s", ids[i]);
        printf("%-20s %s     %s median\n", name, formatTime(dist->p50).c_str(),
                description);
        snprintf(name, sizeof(name), "basic.read%s.min", ids[i]);
        printf("%-20s %s     %s minimum\n", name, formatTime(dist->min).c_str(),
                description);
        snprintf(name, sizeof(name), "basic.read%s.9", ids[i]);
        printf("%-20s %s     %s 90%%\n", name, formatTime(dist->p90).c_str(),
                description);
        if (dist->p99 != 0) {
            snprintf(name, sizeof(name), "basic.read%s.99", ids[i]);
            printf("%-20s %s     %s 99%%\n", name,
                    formatTime(dist->p99).c_str(), description);
        }
        if (dist->p999 != 0) {
            snprintf(name, sizeof(name), "basic.read%s.999", ids[i]);
            printf("%-20s %s     %s 99.9%%\n", name,
                    formatTime(dist->p999).c_str(), description);
        }
        snprintf(name, sizeof(name), "basic.readBw%s", ids[i]);
        snprintf(description, sizeof(description),
                "bandwidth reading %sB objects (%uB key)", ids[i], keyLength);
        printBandwidth(name, dist->bandwidth, description);
    }

    for (int i = 0; i < NUM_SIZES; i++) {
        TimeDist* dist = &writeDists[i];
        snprintf(description, sizeof(description),
                "write random %sB object (%uB key)", ids[i], keyLength);
        snprintf(name, sizeof(name), "basic.write%s", ids[i]);
        printf("%-20s %s     %s median\n", name, formatTime(dist->p50).c_str(),
                description);
        snprintf(name, sizeof(name), "basic.write%s.min", ids[i]);
        printf("%-20s %s     %s minimum\n", name, formatTime(dist->min).c_str(),
                description);
        snprintf(name, sizeof(name), "basic.write%s.9", ids[i]);
        printf("%-20s %s     %s 90%%\n", name, formatTime(dist->p90).c_str(),
                description);
        if (dist->p99 != 0) {
            snprintf(name, sizeof(name), "basic.write%s.99", ids[i]);
            printf("%-20s %s     %s 99%%\n", name,
                    formatTime(dist->p99).c_str(), description);
        }
        if (dist->p999 != 0) {
            snprintf(name, sizeof(name), "basic.write%s.999", ids[i]);
            printf("%-20s %s     %s 99.9%%\n", name,
                    formatTime(dist->p999).c_str(), description);
        }
        snprintf(name, sizeof(name), "basic.writeBw%s", ids[i]);
        snprintf(description, sizeof(description),
                "bandwidth writing %sB objects (%uB key)", ids[i], keyLength);
        printBandwidth(name, dist->bandwidth, description);
    }
#undef NUM_SIZES
}

// Measure the time to broadcast a short value from a master to multiple slaves
// using RAMCloud objects.  This benchmark is also useful as a mechanism for
// exercising the master-slave communication mechanisms.
void
broadcast()
{
    if (clientIndex > 0) {
        while (true) {
            char command[20];
            char message[200];
            getCommand(command, sizeof(command));
            if (strcmp(command, "read") == 0) {
                setSlaveState("waiting");
                // Wait for a non-empty "doc" string to appear.
                while (true) {
                    string key = keyVal(0, "doc");
                    readObject(controlTable, key.c_str(),
                            downCast<uint16_t>(key.length()),
                            message, sizeof(message));
                    if (message[0] != 0) {
                        break;
                    }
                }
                setSlaveState(message);
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                RAMCLOUD_LOG(NOTICE, "finished with %s", message);
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }

    // RAMCLOUD_LOG(NOTICE, "master starting");
    uint64_t totalTime = 0;
    int count = 100;
    for (int i = 0; i < count; i++) {
        char message[30];
        snprintf(message, sizeof(message), "message %d", i);
        string key = keyVal(clientIndex, "doc");
        cluster->write(controlTable, key.c_str(),
                downCast<uint16_t>(key.length()), "");
        sendCommand("read", "waiting", 1, numClients-1);
        uint64_t start = Cycles::rdtsc();
        cluster->write(controlTable, key.c_str(),
                downCast<uint16_t>(key.length()), message);
        for (int slave = 1; slave < numClients; slave++) {
            waitSlave(slave, message);
        }
        uint64_t thisRun = Cycles::rdtsc() - start;
        totalTime += thisRun;
    }
    sendCommand("done", "done", 1, numClients-1);
    char description[50];
    snprintf(description, sizeof(description),
            "broadcast message to %d slaves", numClients-1);
    printTime("broadcast", Cycles::toSeconds(totalTime)/count, description);
}

/**
 * This method contains the core of all the "multiRead" tests.
 * It writes objsPerMaster objects on numMasters servers
 * and reads them back in one multiRead operation.
 *
 * \param dataLength
 *      Length of data for each object to be written.
 * \param keyLength
 *      Length of key for each object to be written.
 * \param numMasters
 *      The number of master servers across which the objects written
 *      should be distributed.
 * \param objsPerMaster
 *      The number of objects to be written to each master server.
 * \param randomize
 *      Randomize the order of requests sent from the client.
 *      Note: Randomization can cause bad cache effects on the client
 *      and cause slower than normal operation.
 *
 * \return
 *      The average time, in seconds, to read all the objects in a single
 *      multiRead operation.
 */
double
doMultiRead(int dataLength, uint16_t keyLength,
            int numMasters, int objsPerMaster,
            bool randomize = false)
{
    if (clientIndex != 0)
        return 0;
    Buffer input;

    MultiReadObject requestObjects[numMasters][objsPerMaster];
    MultiReadObject* requests[numMasters][objsPerMaster];
    Tub<ObjectBuffer> values[numMasters][objsPerMaster];
    char keys[numMasters][objsPerMaster][keyLength];

    std::vector<uint64_t> tableIds(numMasters);
    createTables(tableIds, dataLength, "0", 1);

    for (int tableNum = 0; tableNum < numMasters; tableNum++) {
        for (int i = 0; i < objsPerMaster; i++) {
            Util::genRandomString(keys[tableNum][i], keyLength);
            fillBuffer(input, dataLength, tableIds.at(tableNum),
                    keys[tableNum][i], keyLength);

            // Write each object to the cluster
            cluster->write(tableIds.at(tableNum), keys[tableNum][i], keyLength,
                    input.getRange(0, dataLength), dataLength);

            // Create read object corresponding to each object to be
            // used in the multiread request later.
            requestObjects[tableNum][i] =
                    MultiReadObject(tableIds.at(tableNum),
                    keys[tableNum][i], keyLength, &values[tableNum][i]);
            requests[tableNum][i] = &requestObjects[tableNum][i];
        }
    }

    // Scramble the requests. Checking code below it stays valid
    // since the value buffer is a pointer to a Buffer in the request.
    if (randomize) {
        uint64_t numRequests = numMasters*objsPerMaster;
        MultiReadObject** reqs = *requests;

        for (uint64_t i = 0; i < numRequests; i++) {
            uint64_t rand = generateRandom() % numRequests;

            MultiReadObject* tmp = reqs[i];
            reqs[i] = reqs[rand];
            reqs[rand] = tmp;
        }
    }

    double latency = timeMultiRead(*requests, numMasters*objsPerMaster);

    // Check that the values read were the same as the values written.
    for (int tableNum = 0; tableNum < numMasters; ++tableNum) {
        for (int i = 0; i < objsPerMaster; i++) {
            ObjectBuffer* output = values[tableNum][i].get();
            uint32_t offset;
            output->getValueOffset(&offset);
            checkBuffer(output, offset, dataLength, tableIds.at(tableNum),
                    keys[tableNum][i], keyLength);
        }
    }

    return latency;
}

/**
 * This method contains the core of all the "multiWrite" tests.
 * It writes objsPerMaster objects on numMasters servers.
 *
 * \param dataLength
 *      Length of data for each object to be written.
 * \param keyLength
 *      Length of key for each object to be written.
 * \param numMasters
 *      The number of master servers across which the objects written
 *      should be distributed.
 * \param objsPerMaster
 *      The number of objects to be written to each master server.
 * \param randomize
 *      Randomize the order of requests sent from the client.
 *      Note: Randomization can cause bad cache effects on the client
 *      and cause slower than normal operation.
 *
 * \return
 *      The average time, in seconds, to read all the objects in a single
 *      multiRead operation.
 */
double
doMultiWrite(int dataLength, uint16_t keyLength,
            int numMasters, int objsPerMaster,
            bool randomize = false)
{
    if (clientIndex != 0)
        return 0;

    // MultiWrite Objects
    MultiWriteObject writeRequestObjects[numMasters][objsPerMaster];
    MultiWriteObject* writeRequests[numMasters][objsPerMaster];
    Buffer values[numMasters][objsPerMaster];
    char keys[numMasters][objsPerMaster][keyLength];

    std::vector<uint64_t> tableIds(numMasters);
    createTables(tableIds, dataLength, "0", 1);

    for (int tableNum = 0; tableNum < numMasters; tableNum++) {
        for (int i = 0; i < objsPerMaster; i++) {
            Util::genRandomString(keys[tableNum][i], keyLength);
            fillBuffer(values[tableNum][i], dataLength,
                    tableIds.at(tableNum), keys[tableNum][i], keyLength);

            // Create write object corresponding to each object to be
            // used in the multiWrite request later.
            writeRequestObjects[tableNum][i] =
                MultiWriteObject(tableIds.at(tableNum),
                        keys[tableNum][i], keyLength,
                        values[tableNum][i].getRange(0, dataLength),
                        dataLength);
            writeRequests[tableNum][i] = &writeRequestObjects[tableNum][i];
        }
    }

    // Scramble the requests. Checking code below it stays valid
    // since the value buffer is a pointer to a Buffer in the request.
    if (randomize) {
        uint64_t numRequests = numMasters*objsPerMaster;
        MultiWriteObject ** wreqs = *writeRequests;

        for (uint64_t i = 0; i < numRequests; i++) {
            uint64_t rand = generateRandom() % numRequests;

            MultiWriteObject* wtmp = wreqs[i];
            wreqs[i] = wreqs[rand];
            wreqs[rand] = wtmp;
        }
    }

    double latency = timeMultiWrite(*writeRequests, numMasters*objsPerMaster);

    return latency;
}

/**
 * convenience function to generate a secondary key compatible with
 * generateIndexKeyList.
 *
 * \param sk
 *      character array to write secondary key into
 * \param indexId
 *      index table id to embed into the key
 * \param intKey
 *      the item number to embed in the key; this corresponds directly to the
 *      key at index i of the array generated from generateIndexKeyList
 * \param keyLength
 *      Length of the key
 *
 * \return
 *      number of bytes written to the array
 */
inline int
generateIndexSecondaryKey(char *sk, uint32_t indexId, uint32_t intKey,
                        uint16_t keyLength)
{
    return snprintf(sk, keyLength, "s%0*d%0*d",
                    2, indexId, keyLength - 4, intKey);
}

/**
 * Convenience function to generate a key list for use in Indexing operations.
 * The format of the keys is as follows:
 *      Primary Key: "p<zeroPad><id>\0"
 *      Secondary Key(s): "s<02 digits of indexId><zeroPad><id>\0"
 *
 * \param keyList
 *      Key list to store the generated keys
 *
 * \param id
 *      uint32_t number to stringify into keys
 *
 * \param keyLength
 *      The length of all the keys in the keyList
 *
 * \param numKeys
 *      Number of keys to generate (i.e. primary key + number of secondary keys)
 */
inline void
generateIndexKeyList(KeyInfo *keyList, uint32_t id,
        uint16_t keyLength, uint32_t numKeys = 2)
{
    char *pk = reinterpret_cast<char*>(const_cast<void*>(keyList[0].key));
    snprintf(pk, keyLength, "p%0*d", keyLength - 2, id);
    keyList[0].keyLength = keyLength;

    char *sk;
    for (uint32_t j = 1; j < numKeys; j++) {
        sk = reinterpret_cast<char*>(const_cast<void*>(keyList[j].key));
        generateIndexSecondaryKey(sk, j, id, keyLength);
        keyList[j].keyLength = keyLength;
    }
}

/**
 * Generates a list of uint32_t's in the range [0, total) in a random order
 * with each number appearing exactly once.
 *
 * \param total
 *      The total number of uint32_t's to generate in the range [0, total)
 *
 * \return
 *      vector of the uint32_t's in a random order.
 */
inline vector<uint32_t>
generateRandListFrom0UpTo(uint32_t total)
{
    std::vector<uint32_t> randomized;
    randomized.reserve(total + 1);
    for (uint32_t i = 0; i < total; i++)
        randomized.push_back(i);

    for (uint32_t i = 0; i < total; i++) {
        uint64_t randomIndex = ((generateRandom() % (total - i)) + i);
        uint32_t tmp = randomized[i];
        randomized[i] = randomized[randomIndex];
        randomized[randomIndex] = tmp;
    }

    return randomized;
}

/**
 * Measures index write/overwrite, lookups, readHashes, and IndexLookup
 * operation times for either a single object or large range.
 * All objects have a primary key (30B), one secondary key (30B) and value
 * blob (100B).
 *
 * \param doIndexRange
 *      If true, record time to read all objects.
 *      If false, time reads and writes for a single object
 * \param samplesPerOp
 *      Number of experiments to run per operation
 */
void
indexLookupCommon(bool doIndexRange, uint32_t samplesPerOp)
{
    if (clientIndex != 0)
        return;

    // all keys (including primary key) will be 30 bytes long
    const uint32_t keyLength = 30;
    const uint8_t indexId = 1;
    const uint8_t numIndexlets = 1;
    cluster->createIndex(dataTable, indexId, 0 /*index type*/, numIndexlets);

    // number of objects in the table and in the index
    const uint32_t indexSizes[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
    const uint32_t maxNumObjects = indexSizes[6];
    const uint32_t warmupsPerOp = 1;

    // each object has only 1 secondary key because we are only measuring basic
    // indexing performance.
    const uint8_t numKeys = 2;
    const int size = 100; // value size

    int seperatorSpacing, numberSpacing, subColSize;

    if (doIndexRange) {

        printf("# RAMCloud successive lookup hashes and readHashes compared to "
                "using IndexLookup class "
                "with varying number of objects. %d samples per operation "
                "taken after %d warmups.\n"
                "# All keys are %d bytes and the value of the object is fixed "
                "to be %d bytes.\n"
                "# Write and overwrite latencies are measured for the 'nth'"
                "object insertion where the size of the table is 'n-1'.\n",
                samplesPerOp, warmupsPerOp, keyLength, size);

        printf("# Lookup, readHashes, and  latencies are measured by reading "
                "all 'n' objects "
                "when the size of the index is 'n'.\n"
                "# All latency measurements are printed as 10th percentile/ "
                "median/ 90th percentile.\n#\n"
                "# Generated by 'clusterperf.py indexRange'\n#\n");

        seperatorSpacing = 3;
        numberSpacing = 9;
        subColSize = seperatorSpacing + 3*(numberSpacing+1);

        printf("#       n"
                "%*shash lookup(us)%*slookup+read(us)"
                "%*sIndexLookup(us)%*sIndexLookup overhead\n"
                "#--------%s\n",
                subColSize-15, "", subColSize-15, "",
                subColSize-15, "", subColSize-21, "",
                std::string(subColSize*4, '-').c_str());

    } else {

        printf("# RAMCloud index write, overwrite, lookup+readHashes, and "
                "IndexLookup class performance\n"
                "# with varying number of objects. %d samples per operation "
                "taken after %d warmups.\n"
                "# All keys are %d bytes and the value of the object is fixed"
                " to be %d bytes.\n"
                "# Write and overwrite latencies are measured for the 'nth' "
                "object insertion where the size of the table is 'n-1'.\n",
                samplesPerOp, warmupsPerOp, keyLength, size);

        printf("# Lookup, readHashes, and  latencies are measured by reading "
                "a single object "
                "when the size of the index is 'n'.\n"
                "# All latency measurements are printed as 10th percentile/ "
                "median/ 90th percentile.\n#\n"
                "# Generated by 'clusterperf.py indexBasic'\n#\n");

        seperatorSpacing = 3;
        numberSpacing = 6;
        subColSize = seperatorSpacing + 3*(numberSpacing+1);

        printf("#       n"
               "%*swrite latency(us)%*soverwrite latency(us)",
                subColSize-17, "", subColSize-21, "");

        printf("%*shash lookup(us)%*slookup+read(us)"
                "%*sIndexLookup(us)%*sIndexLookup overhead\n"
                "#--------%s\n",
                subColSize-15, "", subColSize-15, "",
                subColSize-15, "", subColSize-21, "",
                std::string(subColSize*6, '-').c_str());
    }

    // These variables used for whole range read
    uint64_t firstPkHash = 0;
    char firstSecondaryKey[keyLength];

    for (uint32_t i = 0, k = 0; i < maxNumObjects; i++) {
        char primaryKey[keyLength];
        snprintf(primaryKey, sizeof(primaryKey), "p%0*d",
                keyLength-2, i);

        char secondaryKey[keyLength];
        snprintf(secondaryKey, sizeof(secondaryKey), "b%ds%0*d",
                i, keyLength, 0);

        if (doIndexRange && i == 0) {
            memcpy(firstSecondaryKey, secondaryKey, keyLength);
            Key pk(dataTable, primaryKey, sizeof(primaryKey));
            firstPkHash = pk.getHash();
        }

        KeyInfo keyList[2];
        keyList[0].keyLength = keyLength;
        keyList[0].key = primaryKey;
        keyList[1].keyLength = keyLength;
        keyList[1].key = secondaryKey;

        Buffer input;
        fillBuffer(input, size, dataTable,
                keyList[0].key, keyList[0].keyLength);

        std::vector<double>
                timeWrites(samplesPerOp), timeOverWrites(samplesPerOp),
                timeHashLookups(samplesPerOp), timeLookupAndReads(samplesPerOp),
                timeIndexLookups(samplesPerOp);

        // Write all objects. Measure performance while writing (i+1)th object,
        // where i is in indexSizes[].
        if ((i + 1) != indexSizes[k]) {
            cluster->write(dataTable, numKeys, keyList,
                    input.getRange(0, size), size);
        } else {
            // Measure the time to write and overwrite.
            timeIndexWrite(dataTable, numKeys, keyList, input.getRange(0, size),
                    size, warmupsPerOp, samplesPerOp, timeWrites,
                    timeOverWrites);

            std::sort(timeWrites.begin(), timeWrites.end());
            std::sort(timeOverWrites.begin(), timeOverWrites.end());

            // Measure lookup, lookup+readHashes, and IndexLookup operations
            if (doIndexRange) {
                timeIndexedRead(dataTable, indexId, firstPkHash,
                        firstSecondaryKey, keyLength, keyList[1].key,
                        keyList[1].keyLength, warmupsPerOp, samplesPerOp,
                        i+1, // number of objects expected as we are reading
                             // entire range from beginning till this object
                        timeHashLookups, timeLookupAndReads, timeIndexLookups);
            } else {
                Key pk(dataTable, keyList[0].key, keyList[0].keyLength);
                timeIndexedRead(dataTable, indexId, pk.getHash(),
                        keyList[1].key, keyList[1].keyLength, keyList[1].key,
                        keyList[1].keyLength, warmupsPerOp, samplesPerOp,
                        1, // number of objects expected as we are reading
                          // only this object
                        timeHashLookups, timeLookupAndReads, timeIndexLookups);
            }

            std::sort(timeHashLookups.begin(), timeHashLookups.end());
            std::sort(timeLookupAndReads.begin(), timeLookupAndReads.end());
            std::sort(timeIndexLookups.begin(), timeIndexLookups.end());

            const size_t tenthSample = samplesPerOp / 10;
            const size_t medianSample = samplesPerOp / 2;
            const size_t ninetiethSample = samplesPerOp * 9 / 10;

            printf("%9d ", indexSizes[k]);
            if (!doIndexRange) {
                printf("%*.1f/%*.1f/%*.1f %*.1f/%*.1f/%*.1f ",
                        numberSpacing + seperatorSpacing,
                        timeWrites.at(tenthSample) *1e6,
                        numberSpacing,
                        timeWrites.at(medianSample) *1e6,
                        numberSpacing,
                        timeWrites.at(ninetiethSample) *1e6,
                        numberSpacing + seperatorSpacing,
                        timeOverWrites.at(tenthSample) *1e6,
                        numberSpacing,
                        timeOverWrites.at(medianSample) *1e6,
                        numberSpacing,
                        timeOverWrites.at(ninetiethSample) *1e6);
            }

            printf("%*.1f/%*.1f/%*.1f %*.1f/%*.1f/%*.1f ",
                    numberSpacing + seperatorSpacing,
                    timeHashLookups.at(tenthSample) *1e6,
                    numberSpacing,
                    timeHashLookups.at(medianSample) *1e6,
                    numberSpacing,
                    timeHashLookups.at(ninetiethSample) *1e6,
                    numberSpacing + seperatorSpacing,
                    timeLookupAndReads.at(tenthSample) *1e6,
                    numberSpacing,
                    timeLookupAndReads.at(medianSample)*1e6,
                    numberSpacing,
                    timeLookupAndReads.at(ninetiethSample) *1e6);

            printf("%*.1f/%*.1f/%*.1f",
                    numberSpacing + seperatorSpacing,
                    timeIndexLookups.at(tenthSample) *1e6,
                    numberSpacing,
                    timeIndexLookups.at(medianSample) *1e6,
                    numberSpacing,
                    timeIndexLookups.at(ninetiethSample) *1e6);

            printf("%*.2f/%*.2f/%*.2f\n",
                    numberSpacing + seperatorSpacing,
                    (timeIndexLookups.at(tenthSample)-
                     timeLookupAndReads.at(tenthSample)) * 1e6,
                    numberSpacing,
                    (timeIndexLookups.at(medianSample)-
                     timeLookupAndReads.at(medianSample)) * 1e6,
                    numberSpacing,
                    (timeIndexLookups.at(ninetiethSample)-
                     timeLookupAndReads.at(ninetiethSample)) *1e6);
            k++;
        }
    }
    cluster->dropIndex(dataTable, indexId);
}

void
indexBasic()
{
    // all keys (including primary key) will be 30 bytes long
    const uint32_t keyLength = 30;
    const int valLen = objectSize;

    const uint8_t indexId = 1;
    const uint8_t numIndexlets = downCast<uint8_t>(numIndexlet);
    cluster->createIndex(dataTable, indexId, 0 /*index type*/, numIndexlets);

    // number of objects in the table and in the index
    const uint32_t indexSizes[] = {1, 10, 100, 1000, 10000, 100000, 1000000};
    const uint32_t numSizes = sizeof(indexSizes)/sizeof(uint32_t);
    const uint32_t maxNumObjects = indexSizes[numSizes - 1];
    const uint32_t samplesPerRun = 1000; // How many times to op the same op

    // each object has only 1 secondary key because we are only measuring basic
    // indexing performance.
    const uint8_t numKeys = 2;

    const int seperatorSpacing = 3, numberSpacing = 6;
    const int subColSize = seperatorSpacing + 3*(numberSpacing+1);
    printf("# RAMCLOUD index write, overwrite, lookup+readHashes, and "
            "IndexLookup class performance with a varying number of objects\n"
            "# and a B+ tree fanout of %d. All keys are %d bytes and the value "
            "of the object is fixed to be %d bytes. Read and Overwrite \n"
            "# latencies are measured by randomly reading and overwriting "
            "%d objects already inserted into the index at size n. \n"
            "# In a similar fashion, write latencies are measured by deleting "
            "an existing object and then immediately re-writing. \n"
            "# All latency measurements are printed as 10th percentile/ "
            "median/ 90th percentile.\n#\n"
            "# Generated by 'clusterperf.py indexBasic'\n#\n",
            IndexBtree::innerslotmax, keyLength, valLen, samplesPerRun);

    printf("#       n"
           "%*swrite latency(us)%*soverwrite latency(us)",
            subColSize-17, "", subColSize-21, "");

    printf("%*shash lookup(us)%*slookup+read(us)"
            "%*sIndexLookup(us)%*sIndexLookup overhead\n"
            "#--------%s\n",
            subColSize-15, "", subColSize-15, "",
            subColSize-15, "", subColSize-21, "",
            std::string(subColSize*6, '-').c_str());
    fflush(stdout);

    KeyInfo keyList[numKeys];
    char primaryKey[keyLength], secondaryKey[keyLength];
    keyList[0].keyLength = keyLength;
    keyList[0].key = primaryKey;
    keyList[1].keyLength = keyLength;
    keyList[1].key = secondaryKey;

    std::vector<double> timeWrites(samplesPerRun),
                        timeOverWrites(samplesPerRun),
                        timeHashLookups(samplesPerRun),
                        timeLookupAndReads(samplesPerRun),
                        timeIndexLookups(samplesPerRun);

    srand(500);
    std::vector<uint32_t> randomized = generateRandListFrom0UpTo(maxNumObjects);

    uint32_t indexSize = 0;
    Buffer val, readValBuff, readHashBuff;
    for (uint32_t k = 0; k < numSizes; k++) {
        // Fill Up to desired Size
        while (indexSize < indexSizes[k]) {
            uint32_t intKey = randomized[indexSize];
            generateIndexKeyList(keyList, intKey, keyLength, numKeys);
            fillBuffer(val, valLen, dataTable, primaryKey, keyLength);
            cluster->write(dataTable, numKeys, keyList,
                    val.getRange(0, valLen), valLen);
            indexSize++;

            // Optionally do a lookup after writing to verify integrity of data.
            bool verify = false;
            if (!verify)
                continue;

            uint32_t totalNumObjects = 0;
            IndexKey::IndexKeyRange keyRange(indexId,
                    keyList[1].key, keyList[1].keyLength, /*first key*/
                    keyList[1].key, keyList[1].keyLength /*last key*/);
            IndexLookup rangeLookup(cluster, dataTable, keyRange);

            while (rangeLookup.getNext())
                totalNumObjects++;

            if (totalNumObjects != 1) {
                printf("Verification failed at insert # %u. "
                        "Found %u objects, expecting 1.\r\n",
                        indexSize, totalNumObjects);
                fflush(stdout);
                exit(-1);
            }
        }

        // Perform Testing
        for (uint32_t i = 0; i < samplesPerRun; i++) {
            uint64_t start, stop;
            uint32_t intKey = randomized[generateRandom() % indexSize];
            generateIndexKeyList(keyList, intKey, keyLength, numKeys);
            fillBuffer(val, valLen, dataTable, primaryKey, keyLength);

            uint16_t nextKeyLength;
            uint32_t numHashes, numObjects;
            uint64_t nextKeyHash;

             // Warmup with 1 lookup
            cluster->lookupIndexKeys(dataTable, indexId,
                    keyList[1].key, keyList[1].keyLength /*First Key*/, 0,
                    keyList[1].key, keyList[1].keyLength /*Last Key*/, 10,
                    &readHashBuff, &numHashes, &nextKeyLength, &nextKeyHash);
            assert(numHashes == 1);

            // Hash Lookup
            start = Cycles::rdtsc();
            cluster->lookupIndexKeys(dataTable, indexId,
                    keyList[1].key, keyList[1].keyLength /*First Key*/, 0,
                    keyList[1].key, keyList[1].keyLength /*Last Key*/, 10,
                    &readHashBuff, &numHashes, &nextKeyLength, &nextKeyHash);
            stop = Cycles::rdtsc();
            timeHashLookups.at(i) = Cycles::toSeconds(stop - start);

            numHashes = (readHashBuff.getStart<
                    WireFormat::LookupIndexKeys::Response>())->numHashes;
            readHashBuff.truncateFront(
                    sizeof32(WireFormat::LookupIndexKeys::Response));

            // Hash Lookup + Read
            start = Cycles::rdtsc();
            uint32_t numReturnedHashes = cluster->readHashes(
                dataTable, numHashes, &readHashBuff, &readValBuff, &numObjects);
            assert(numReturnedHashes == numHashes); // else collision
            stop = Cycles::rdtsc();
            timeLookupAndReads.at(i) = timeHashLookups.at(i) +
                        Cycles::toSeconds(stop - start);

            // IndexLookup
            start = Cycles::rdtsc();
            uint32_t totalNumObjects = 0;
            IndexKey::IndexKeyRange keyRange(indexId,
                    keyList[1].key, keyList[1].keyLength, /*first key*/
                    keyList[1].key, keyList[1].keyLength /*last key*/);
            IndexLookup rangeLookup(cluster, dataTable, keyRange);

            while (rangeLookup.getNext())
                totalNumObjects++;

            stop = Cycles::rdtsc();
            timeIndexLookups.at(i) = Cycles::toSeconds(stop - start);
            assert(totalNumObjects == 1);

            // Overwrite
            start = Cycles::rdtsc();
            cluster->write(dataTable, numKeys, keyList,
                    val.getRange(0, valLen), valLen);
            stop = Cycles::rdtsc();
            timeOverWrites.at(i) = Cycles::toSeconds(stop - start);

            // Erase
            cluster->remove(dataTable, keyList[0].key, keyList[0].keyLength);

            // Allow time for asynchronous removes of index entries to complete.
            Cycles::sleep(100);

            // Final write
            start = Cycles::rdtsc();
            cluster->write(dataTable, numKeys, keyList,
                    val.getRange(0, valLen), valLen);
            timeWrites.at(i) = Cycles::toSeconds(Cycles::rdtsc() - start);
        }

        // Print Out Results
        assert(timeWrites.size() == samplesPerRun);
        assert(timeHashLookups.size() == samplesPerRun);

        std::sort(timeWrites.begin(), timeWrites.end());
        std::sort(timeOverWrites.begin(), timeOverWrites.end());
        std::sort(timeHashLookups.begin(), timeHashLookups.end());
        std::sort(timeLookupAndReads.begin(), timeLookupAndReads.end());
        std::sort(timeIndexLookups.begin(), timeIndexLookups.end());

        const size_t tenthSample = samplesPerRun / 10;
        const size_t medianSample = samplesPerRun / 2;
        const size_t ninetiethSample = samplesPerRun * 9 / 10;

        printf("%9d ", indexSizes[k]);

        printf("%*.1f/%*.1f/%*.1f %*.1f/%*.1f/%*.1f ",
                numberSpacing + seperatorSpacing,
                timeWrites.at(tenthSample) *1e6,
                numberSpacing,
                timeWrites.at(medianSample) *1e6,
                numberSpacing,
                timeWrites.at(ninetiethSample) *1e6,
                numberSpacing + seperatorSpacing,
                timeOverWrites.at(tenthSample) *1e6,
                numberSpacing,
                timeOverWrites.at(medianSample) *1e6,
                numberSpacing,
                timeOverWrites.at(ninetiethSample) *1e6);

        printf("%*.1f/%*.1f/%*.1f %*.1f/%*.1f/%*.1f ",
                numberSpacing + seperatorSpacing,
                timeHashLookups.at(tenthSample) *1e6,
                numberSpacing,
                timeHashLookups.at(medianSample) *1e6,
                numberSpacing,
                timeHashLookups.at(ninetiethSample) *1e6,
                numberSpacing + seperatorSpacing,
                timeLookupAndReads.at(tenthSample) *1e6,
                numberSpacing,
                timeLookupAndReads.at(medianSample)*1e6,
                numberSpacing,
                timeLookupAndReads.at(ninetiethSample) *1e6);

        printf("%*.1f/%*.1f/%*.1f",
                numberSpacing + seperatorSpacing,
                timeIndexLookups.at(tenthSample) *1e6,
                numberSpacing,
                timeIndexLookups.at(medianSample) *1e6,
                numberSpacing,
                timeIndexLookups.at(ninetiethSample) *1e6);

        printf("%*.2f/%*.2f/%*.2f\n",
                numberSpacing + seperatorSpacing,
                (timeIndexLookups.at(tenthSample)-
                 timeLookupAndReads.at(tenthSample)) * 1e6,
                numberSpacing,
                (timeIndexLookups.at(medianSample)-
                 timeLookupAndReads.at(medianSample)) * 1e6,
                numberSpacing,
                (timeIndexLookups.at(ninetiethSample)-
                 timeLookupAndReads.at(ninetiethSample)) *1e6);
        fflush(stdout);
    }

    cluster->dropIndex(dataTable, indexId);
}

void
indexWriteDist()
{
    // all keys (including primary key) will be 30 bytes long
    const uint32_t keyLength = 30;
    const int valLen = objectSize;

    const uint8_t indexId = 1;
    const uint8_t numIndexlets = downCast<uint8_t>(numIndexlet);
    cluster->createIndex(dataTable, indexId, 0 /*index type*/, numIndexlets);

    // number of objects in the table and in the index
    const uint32_t indexSize = numObjects;
    const uint32_t writeSamples = count;

     // each object has only 1 secondary key because we are only measuring basic
    // indexing performance.
    const uint8_t numKeys = 2;

    KeyInfo keyList[numKeys];
    char primaryKey[keyLength], secondaryKey[keyLength];
    keyList[0].keyLength = keyLength;
    keyList[0].key = primaryKey;
    keyList[1].keyLength = keyLength;
    keyList[1].key = secondaryKey;

    Buffer val;
    std::vector<double> timeWrites(writeSamples);
    std::vector<uint32_t> randomized =
            generateRandListFrom0UpTo(indexSize);

    // Fill Up Index to desired Size
    for (uint32_t i = 0; i < indexSize; i++) {
        uint32_t intKey = randomized[i];
        generateIndexKeyList(keyList, intKey, keyLength, numKeys);
        fillBuffer(val, valLen, dataTable, primaryKey, keyLength);
        cluster->write(dataTable, numKeys, keyList,
                val.getRange(0, valLen), valLen);
    }

    // Perform write testing
    // Note: This will only work if duplicates
    // are allowed in the system.
    for (uint32_t i = 0; i < writeSamples; i++) {
        uint64_t start, stop;
        uint32_t intKey = randomized[i % indexSize];
        generateIndexKeyList(keyList, intKey, keyLength, numKeys);
        fillBuffer(val, valLen, dataTable, primaryKey, keyLength);

        // Write
        start = Cycles::rdtsc();
        cluster->write(dataTable, numKeys, keyList,
                val.getRange(0, valLen), valLen);
        stop = Cycles::rdtsc();

        // Allow time for asynchronous removes of index entries to complete.
        Cycles::sleep(100);

        timeWrites.at(i) = Cycles::toSeconds(stop - start);
    }

    // Print out the results
    int valuesInLine = 0;
    for (uint32_t i = 0; i < writeSamples; i++) {
        if (valuesInLine >= 10) {
            valuesInLine = 0;
            printf("\n");
        }
        if (valuesInLine != 0) {
            printf(",");
        }
        double micros = timeWrites.at(i)*1.0e06;
        printf("%.2f", micros);
        valuesInLine++;
    }
    printf("\n");
    fflush(stdout);

    cluster->dropIndex(dataTable, indexId);
}

void
indexReadDist()
{
    // all keys (including primary key) will be 30 bytes long
    const uint32_t keyLength = 30;
    const int valLen = objectSize;

    const uint8_t indexId = 1;
    const uint8_t numIndexlets = downCast<uint8_t>(numIndexlet);
    cluster->createIndex(dataTable, indexId, 0 /*index type*/, numIndexlets);

    // number of objects in the table and in the index
    const uint32_t indexSize = numObjects;
    const uint32_t readSamples = count;

    // each object has only 1 secondary key because we are only measuring basic
    // indexing performance.
    const uint8_t numKeys = 2;

    KeyInfo keyList[numKeys];
    char primaryKey[keyLength], secondaryKey[keyLength];
    keyList[0].keyLength = keyLength;
    keyList[0].key = primaryKey;
    keyList[1].keyLength = keyLength;
    keyList[1].key = secondaryKey;

    std::vector<double> readTimes(readSamples);
    std::vector<uint32_t> randomized = generateRandListFrom0UpTo(indexSize);

    // Fill up the Table/Tablet
    Buffer val;
    for (uint32_t i = 0; i < indexSize; i++) {
        // Fill up Index
        uint32_t intKey = randomized[i];
        generateIndexKeyList(keyList, intKey, keyLength, numKeys);
        fillBuffer(val, valLen, dataTable, primaryKey, keyLength);
        cluster->write(dataTable, numKeys, keyList,
                val.getRange(0, valLen), valLen);
    }

    // Warm up with warmupCount reads
    for (int i = 0; i < warmupCount; i++) {
        uint32_t intKey = randomized[randomNumberGenerator(indexSize)];
        generateIndexKeyList(keyList, intKey, keyLength, numKeys);
        uint64_t totalNumObjects = 0;
        IndexKey::IndexKeyRange keyRange(indexId,
                keyList[1].key, keyList[1].keyLength, /*first key*/
                keyList[1].key, keyList[1].keyLength /*last key*/);
        IndexLookup lookup(cluster, dataTable, keyRange);

        while (lookup.getNext())
            totalNumObjects++;

        assert(1 == totalNumObjects);
    }

    // Perform random reads
    uint64_t start, stop;
    for (uint32_t i = 0; i < readSamples; i++) {
        uint32_t intKey = randomized[randomNumberGenerator(indexSize)];
        generateIndexKeyList(keyList, intKey, keyLength, numKeys);

        start = Cycles::rdtsc();
        uint64_t totalNumObjects = 0;
        IndexKey::IndexKeyRange keyRange(indexId,
                keyList[1].key, keyList[1].keyLength, /*first key*/
                keyList[1].key, keyList[1].keyLength /*last key*/);
        IndexLookup lookup(cluster, dataTable, keyRange);

        while (lookup.getNext())
            totalNumObjects++;

        stop = Cycles::rdtsc();
        readTimes.at(i) = Cycles::toSeconds(stop - start);
        assert(1 == totalNumObjects);
    }

    // Print out the results
    int valuesInLine = 0;
    for (uint32_t i = 0; i < readSamples; i++) {
        if (valuesInLine >= 10) {
            valuesInLine = 0;
            printf("\n");
        }
        if (valuesInLine != 0) {
            printf(",");
        }
        double micros = readTimes.at(i)*1.0e06;
        printf("%.2f", micros);
        valuesInLine++;
    }
    printf("\n");
    fflush(stdout);

    cluster->dropIndex(dataTable, indexId);
}

void
indexRange() {
    if (clientIndex != 0)
        return;

    // all keys (including primary key) will be 30 bytes long
    const uint32_t keyLength = 30;
    const uint32_t maxObjects = uint32_t(numObjects);
    const uint32_t samplesPerRun = uint32_t(count);

    // number of objects in the index/table
    const uint32_t maxNumHashes = 1000; // Maximum of hashes per RPC

    // each object has only 1 secondary key because we are only measuring basic
    // indexing performance.
    const uint8_t numKeys = 2;
    const uint8_t indexId = 1;
    const uint8_t numIndexlets = 1;

    // Printing Parameters
    int seperatorSpacing = 3;
    int numberSpacing = 11;
    int subColSize = seperatorSpacing + 3*(numberSpacing+1);
    printf("# RAMCloud successive lookup hashes and readHashes compared to "
            "using IndexLookup class with a fixed sized table (%d objects)\n"
            "# and variable range query. %d samples per operation taken after "
            "%d warmups. All keys are %d bytes and the value of the object\n"
            "# is fixed to be %d bytes. Lookup, readHashes, and  latencies "
            "are measured by reading  'n' objects.\n",
            maxObjects, samplesPerRun, warmupCount, keyLength, objectSize);

    printf("# All latency measurements are printed as 10th percentile/ "
            "median/ 90th percentile.\n#\n"
            "# Generated by 'clusterperf.py indexRange'\n#\n");

    printf("#       n"
            "%*shash lookup(us)%*slookup+read(us)"
            "%*sIndexLookup(us)%*sIndexLookup overhead"
            "%*sIndexLookup Kobj/sec\n"
            "#--------%s\n",
            subColSize-15, "", subColSize-15, "",
            subColSize-15, "", subColSize-21, "",
            subColSize-21, "",
            std::string(subColSize*5, '-').c_str());

    // Allocate Structures needed
    char keyArrays[3][numKeys + 1][keyLength], value[objectSize];
    KeyInfo insertKey[numKeys], firstKey[numKeys], lastKey[numKeys];
    for (uint32_t i = 0; i < numKeys; i++) {
        insertKey[i].key = keyArrays[0][i];
        firstKey[i].key = keyArrays[1][i];
        lastKey[i].key = keyArrays[2][i];

        insertKey[i].keyLength = keyLength;
        firstKey[i].keyLength = keyLength;
        lastKey[i].keyLength = keyLength;
    }

    // Buffers for keeping track of pkHash buffers and object read buffers
    // for readHash operations
    Buffer readObject;
    std::deque<Buffer> buffers;
    std::vector<Buffer*> freeBuffers;
    std::vector<uint32_t> randomized = generateRandListFrom0UpTo(maxObjects);

    cluster->createIndex(dataTable, indexId, 0 /*index type*/, numIndexlets);

    // Fill table/Index
    for (uint32_t i = 0; i < maxObjects; i++) {
        uint32_t intKey = randomized[i];
        generateIndexKeyList(insertKey, intKey, keyLength);
        cluster->write(dataTable, numKeys, insertKey, value, objectSize);
    }


    uint32_t lookupRange = 1;
    while (lookupRange <= maxObjects)
    {
        std::vector<double> hashLookupTimes(samplesPerRun),
                            lookupAndReadTimes(samplesPerRun),
                            indexLookupTimes(samplesPerRun);

        // Warm up with Single object lookups
        for (int i = 0; i < warmupCount; i++) {
            uint32_t randLookupIndex = (lookupRange == maxObjects) ?
                    0 : randomNumberGenerator(maxObjects - lookupRange);
            uint32_t intFirstKey = randLookupIndex;
            uint32_t intLastKey = randLookupIndex + lookupRange - 1;

            generateIndexKeyList(firstKey, intFirstKey, keyLength);
            generateIndexKeyList(lastKey, intLastKey, keyLength);

            IndexKey::IndexKeyRange keyRange(indexId,
                    firstKey[1].key, firstKey[1].keyLength,
                    lastKey[1].key, lastKey[1].keyLength);
            IndexLookup rangeLookup(cluster, dataTable, keyRange);

            uint32_t totalNumObjects = 0;
            while (rangeLookup.getNext())
                totalNumObjects++;
        }

        // Do the actual tests
        for (uint16_t i = 0; i < samplesPerRun; i++) {
            uint32_t randLookupIndex = (lookupRange == maxObjects) ?
                    0 : randomNumberGenerator(maxObjects - lookupRange);
            uint32_t intFirstKey = randLookupIndex;
            uint32_t intLastKey = randLookupIndex + lookupRange - 1;

            generateIndexKeyList(firstKey, intFirstKey, keyLength);
            generateIndexKeyList(lastKey, intLastKey, keyLength);

            const void *nextLookupKey = firstKey[1].key;
            uint16_t nextLookupKeyLength = firstKey[1].keyLength;

            Key pk(dataTable, firstKey[0].key, firstKey[0].keyLength);
            uint64_t expectedFirstPkHash = pk.getHash();

            // PKHash lookup Method
            uint16_t nextKeyLength;
            uint64_t hashLookupTime = 0;
            uint32_t numHashes, totalNumHashes = 0;
            uint64_t firstAllowedKeyHash = 0, nextKeyHash;
            std::vector<Buffer*> pkHashBuffs;

            while (true) {
                Buffer *pkHashBuffer;
                if (freeBuffers.empty()) {
                    buffers.emplace_back();
                    pkHashBuffer = &buffers.back();
                } else {
                    pkHashBuffer = freeBuffers.back();
                    freeBuffers.pop_back();
                    pkHashBuffer->reset();
                }

                pkHashBuffs.push_back(pkHashBuffer);

                uint64_t start = Cycles::rdtsc();
                cluster->lookupIndexKeys(dataTable, indexId,
                        nextLookupKey, nextLookupKeyLength, firstAllowedKeyHash,
                        lastKey[1].key, lastKey[1].keyLength, maxNumHashes,
                        pkHashBuffer, &numHashes, &nextKeyLength, &nextKeyHash);
                hashLookupTime += Cycles::rdtsc() - start;
                totalNumHashes += numHashes;
                if (nextKeyHash == 0)
                    break;

                firstAllowedKeyHash = nextKeyHash;
                nextLookupKeyLength = nextKeyLength;

                uint32_t off = pkHashBuffer->size() - nextKeyLength;
                nextLookupKey = pkHashBuffer->getRange(off, nextKeyLength);
            }
            hashLookupTimes.at(i) = Cycles::toSeconds(hashLookupTime);

            assert(totalNumHashes == lookupRange);
            assert(expectedFirstPkHash ==
                    *pkHashBuffs.front()->getOffset<uint64_t>(
                            sizeof32(WireFormat::LookupIndexKeys::Response)));

            // Read Hashes
            uint32_t numObjectsInRead;
            uint32_t totalNumObjects = 0;
            uint64_t readHashTime = 0;
            for (auto it = pkHashBuffs.begin(); it != pkHashBuffs.end(); it++) {
                numHashes =
                    ((*it)->getStart<WireFormat::LookupIndexKeys::Response>())
                        ->numHashes;
                (*it)->truncateFront(
                            sizeof32(WireFormat::LookupIndexKeys::Response));

                uint64_t start = Cycles::rdtsc();
                uint32_t numReturnedHashes =
                        cluster->readHashes(dataTable, numHashes, (*it),
                                            &readObject, &numObjectsInRead);
                readHashTime += Cycles::rdtsc() - start;

                assert(numReturnedHashes == numHashes); // else collision
                totalNumObjects += numObjectsInRead;
            }
            assert(totalNumObjects == lookupRange);
            lookupAndReadTimes.at(i) = hashLookupTimes.at(i) +
                    Cycles::toSeconds(readHashTime);

            // Cleanup buffers
            while ( !pkHashBuffs.empty() ) {
                freeBuffers.push_back(pkHashBuffs.back());
                pkHashBuffs.pop_back();
            }

            // now do IndexLookup
            firstAllowedKeyHash = totalNumObjects = 0;

            uint64_t start = Cycles::rdtsc();
            IndexKey::IndexKeyRange keyRange(indexId,
                    firstKey[1].key, firstKey[1].keyLength,
                    lastKey[1].key, lastKey[1].keyLength);
            IndexLookup rangeLookupRpc(cluster, dataTable, keyRange);

            while (rangeLookupRpc.getNext())
                totalNumObjects++;

            indexLookupTimes.at(i) = Cycles::toSeconds(Cycles::rdtsc() - start);

            assert(lookupRange == totalNumObjects);
        }

        // Print Result
        std::sort(hashLookupTimes.begin(), hashLookupTimes.end());
        std::sort(lookupAndReadTimes.begin(), lookupAndReadTimes.end());
        std::sort(indexLookupTimes.begin(), indexLookupTimes.end());

        const size_t tenthSample = samplesPerRun / 10;
        const size_t medianSample = samplesPerRun / 2;
        const size_t ninetiethSample = samplesPerRun * 9 / 10;

        printf("%9d ", lookupRange);

        printf("%*.1f/%*.1f/%*.1f %*.1f/%*.1f/%*.1f ",
                numberSpacing + seperatorSpacing,
                hashLookupTimes.at(tenthSample) *1e6,
                numberSpacing,
                hashLookupTimes.at(medianSample) *1e6,
                numberSpacing,
                hashLookupTimes.at(ninetiethSample) *1e6,
                numberSpacing + seperatorSpacing,
                lookupAndReadTimes.at(tenthSample) *1e6,
                numberSpacing,
                lookupAndReadTimes.at(medianSample)*1e6,
                numberSpacing,
                lookupAndReadTimes.at(ninetiethSample) *1e6);

        printf("%*.1f/%*.1f/%*.1f",
                numberSpacing + seperatorSpacing,
                indexLookupTimes.at(tenthSample) *1e6,
                numberSpacing,
                indexLookupTimes.at(medianSample) *1e6,
                numberSpacing,
                indexLookupTimes.at(ninetiethSample) *1e6);

        printf("%*.2f/%*.2f/%*.2f",
                numberSpacing + seperatorSpacing,
                (indexLookupTimes.at(tenthSample)-
                 lookupAndReadTimes.at(tenthSample)) * 1e6,
                numberSpacing,
                (indexLookupTimes.at(medianSample)-
                 lookupAndReadTimes.at(medianSample)) * 1e6,
                numberSpacing,
                (indexLookupTimes.at(ninetiethSample)-
                 lookupAndReadTimes.at(ninetiethSample)) *1e6);

        printf("%*.2f/%*.2f/%*.2f\n",
                numberSpacing + seperatorSpacing,
                lookupRange/(indexLookupTimes.at(tenthSample)*1e3),
                numberSpacing,
                lookupRange/(indexLookupTimes.at(medianSample)*1e3),
                numberSpacing,
                lookupRange/(indexLookupTimes.at(ninetiethSample)*1e3));

        if (lookupRange < maxObjects && (lookupRange * 2) > maxObjects)
            lookupRange = maxObjects;
        else
            lookupRange *= 2;
    }

    cluster->dropIndex(dataTable, indexId);
}

// Index write and overwrite times for varying number of objects for
// varying number of secondary keys (and corresponding indexes) per object.
void
indexMultiple()
{
    if (clientIndex != 0)
        return;

    // Declare variables needed later.
    uint8_t maxNumKeys = static_cast<uint8_t>(numIndexes + 1); // Includes the
                                                               // primary key.
    int numObjects = 1000; // Number of objects in table (and correspondingly,
                           // number of entries in each index).
    const uint32_t keyLength = 30;
    uint32_t size = 100; // Length of value.
    Buffer lookupResp;
    uint32_t numHashes;
    uint16_t nextKeyLength;
    uint64_t nextKeyHash;
    uint32_t maxNumHashes = 1000;
    uint32_t lookupOffset = sizeof32(WireFormat::LookupIndexKeys::Response);
    uint32_t samplesPerOp = 1000;

    printf("# RAMCloud write/overwrite performance for random object\n"
            "# insertion with a varying number of secondary keys. The\n"
            "# IndexBtree fanout is %d and the size of the table is fixed\n"
            "# at %d objects, each with a %d byte value, a %d byte primary\n"
            "# key, and an additional %d bytes per secondary key. After\n"
            "# filling the table with %d objects in a random order, this test\n"
            "# will overwrite, erase, and re-write %d pre-exiting objects to\n"
            "# measure latency. The latency measurements are printed as\n"
            "# 10th percentile/ median/ 90th percentile\n#\n",
            IndexBtree::innerslotmax, numObjects, size, keyLength, keyLength,
            numObjects, samplesPerOp);
    printf("# Generated by 'clusterperf.py indexMultiple'\n#\n"
            "# Sec. keys/obj        write latency (us)"
            "        overwrite latency (us)\n"
            "#----------------------------------------"
            "------------------------------\n");

    // Randomize Ordering
    std::vector<uint32_t> randomized = generateRandListFrom0UpTo(numObjects);
    for (uint8_t currentNumIndexes = 0; currentNumIndexes <= maxNumKeys - 1;
            currentNumIndexes++) {
        uint8_t currNumKeys = downCast<uint8_t>(currentNumIndexes + 1);

        // Create a data table with name tableName and create indexes
        // corresponding to that table.
        char tableName[20];
        snprintf(tableName, sizeof(tableName), "tableWith%dIndexes",
                currentNumIndexes);

        uint64_t indexTable = cluster->createTable(tableName);
        for (uint8_t z = 1; z <= currentNumIndexes; z++)
            cluster->createIndex(indexTable, z, 0);

        // Insert all the Objects
        char value[size];
        KeyInfo keyList[currNumKeys];
        char key[currNumKeys][keyLength];
        for (int x = 0; x < currNumKeys; x++)
                keyList[x].key = key[x];

        for (int z = 0; z < numObjects; z++) {
            uint32_t intKey = randomized[z];
            snprintf(value, sizeof(value), "Value %0*d", size, 0);
            generateIndexKeyList(keyList, intKey, keyLength, currNumKeys);
            cluster->write(indexTable, currNumKeys, keyList, value, size);
            // Verify written data.
            Key pkey(indexTable, keyList[0].key, keyList[0].keyLength);
            for (uint8_t y = 1; y <= currentNumIndexes; y++) {
                lookupResp.reset();
                cluster->lookupIndexKeys(indexTable, y, keyList[y].key,
                        keyList[y].keyLength, 0, keyList[y].key,
                        keyList[y].keyLength, maxNumHashes,
                        &lookupResp, &numHashes,
                        &nextKeyLength, &nextKeyHash);

                assert(1 == numHashes);
                assert(pkey.getHash()==
                        *lookupResp.getOffset<uint64_t>(lookupOffset));
            }
        }

        // records measurements
        std::vector<double> timeWrites(samplesPerOp),
                timeOverWrites(samplesPerOp);

        for (uint32_t z = 0; z < samplesPerOp; z++) {
            uint64_t start, stop;
            uint32_t intKey = randomized[randomNumberGenerator(numObjects)];
            generateIndexKeyList(keyList, intKey, keyLength, currNumKeys);
            snprintf(value, size, "Value %0*d", size, 0);

            // Overwrite
            start = Cycles::rdtsc();
            cluster->write(indexTable, currNumKeys, keyList, value, size);
            stop = Cycles::rdtsc();
            timeOverWrites.at(z) = Cycles::toSeconds(stop - start);

            // Erase
            cluster->remove(indexTable, keyList[0].key, keyList[0].keyLength);

            // Allow time for asynchronous removes of index entries to complete.
            Cycles::sleep(100);

            // Final write
            start = Cycles::rdtsc();
            cluster->write(indexTable, currNumKeys, keyList, value, size);
            timeWrites.at(z) = Cycles::toSeconds(Cycles::rdtsc() - start);
        }

        // Cleanup.
        for (uint8_t z = 1; z <= currentNumIndexes; z++)
            cluster->dropIndex(indexTable, z);
        cluster->dropTable(tableName);

        // Print stats.
        // Note that sort modifies the underlying vector.
        std::sort(timeWrites.begin(), timeWrites.end());
        std::sort(timeOverWrites.begin(), timeOverWrites.end());
        printf("%15d %11.1f/%6.1f/%6.1f %14.1f/%6.1f/%6.1f\n",
               currentNumIndexes,
               timeWrites.at(timeWrites.size()/10) *1e6,
               timeWrites.at(timeWrites.size()/2) *1e6,
               timeWrites.at(timeWrites.size()*9/10) *1e6,
               timeOverWrites.at(timeOverWrites.size()/10) *1e6,
               timeOverWrites.at(timeOverWrites.size()/2) *1e6,
               timeOverWrites.at(timeOverWrites.size()*9/10) *1e6);
    }
}

/**
 * This method contains the core of the "indexScalability" test; it is
 * shared by the master and slaves and measures the throughput for index lookup
 * operations.
 *
 * \param numIndexlets
 *      Number indexlets of an index available for the scalability test.
 * \param numObjectsPerIndxlet
 *      The total number of objects contained in each indexlet
 * \param range
 *      How many hashes/objects should the IndexLookupKeys an IndexLookup
 *      operations request for
 * \param concurrent
 *      How many rpcs should be sent out in each round of the test
 * \param docString
 *      Information provided by the master about this run; used
 *      in log messages.
 */
void
indexScalabilityCommonLookup(uint8_t numIndexlets, int numObjectsPerIndxlet,
        int range, int concurrent, char *docString)
{
    double ms = 1000;
    uint64_t runCycles = Cycles::fromSeconds(ms/1e03);
    uint8_t indexId = (uint8_t)1;
    uint32_t maxHashes = 1000;
    uint16_t keyLength = 30;

    // Do Read Hashes
    uint64_t lookupStart, lookupEnd;
    uint64_t elapsed = 0;
    int totalHashes = 0;
    int opCount = 0;


    uint64_t lookupTable = cluster->getTableId("indexScalability");
    while (true) {
        int numRequests = concurrent;
//        int numRequests = numIndexlets;
        Buffer lookupResp[numRequests];
        uint32_t numHashes[numRequests];
        uint16_t nextKeyLength[numRequests];
        uint64_t nextKeyHash[numRequests];
        char primaryKey[numRequests][30];
        char firstKey[numRequests][30];
        char lastKey[numRequests][30];

        Tub<LookupIndexKeysRpc> rpcs[numRequests];

        for (int i =0; i < numRequests; i++) {
            char indexIdent = static_cast<char>(('a') +
                    static_cast<int>(generateRandom() % numIndexlets));
            int intKey = (numObjectsPerIndxlet == range) ? 0 :
                    static_cast<int>
                        (randomNumberGenerator(numObjectsPerIndxlet - range));

            snprintf(primaryKey[i], sizeof(primaryKey[i]), "%c:%dp%0*d",
                    indexIdent, intKey, keyLength, 0);
            snprintf(firstKey[i], sizeof(firstKey[i]), "%c:s%0*d",
                    indexIdent, keyLength-4, intKey);
            snprintf(lastKey[i], sizeof(lastKey[i]), "%c:s%0*d",
                    indexIdent, keyLength-4, intKey + range - 1);
        }

        // Send async requests and receive responses to numRequests lookup
        // requests and measure time.
        lookupStart = Cycles::rdtsc();
        for (int i =0; i < numRequests; i++) {
            rpcs[i].construct(cluster, lookupTable, indexId,
                    firstKey[i], keyLength, (uint16_t)0,
                    lastKey[i], keyLength, maxHashes,
                    &lookupResp[i]);
        }

        for (int i = 0; i < numRequests; i++) {
            if (rpcs[i])
              rpcs[i]->wait(&numHashes[i], &nextKeyLength[i], &nextKeyHash[i]);
        }
        lookupEnd = Cycles::rdtsc();

        // Verify data.
        #if DEBUG_BUILD
        for (int i =0; i < numRequests; i++) {
            Key pk(lookupTable, primaryKey[i], 30);
            uint32_t lookupOffset;
            lookupOffset = sizeof32(WireFormat::LookupIndexKeys::Response);
            totalHashes += numHashes[i];
            assert(numHashes[i] == uint32_t(range));
            assert(pk.getHash()==
                    *lookupResp[i].getOffset<uint64_t>(lookupOffset));
        }
        #endif

        uint64_t latency = lookupEnd - lookupStart;
        opCount = opCount + numRequests;
        elapsed += latency;
        if (elapsed >= runCycles)
            break;
    }
    double readHashThroughput = totalHashes/Cycles::toSeconds(elapsed);

    // Do IndexLookup
    int totalObjects = 0;
    elapsed = 0;
    uint64_t sumRpcLatencies = 0;
    uint64_t numRpcs = 0;

    while (true) {
        int numRequests = concurrent;
        char primaryKey[numRequests][30];
        char firstKey[numRequests][30];
        char lastKey[numRequests][30];

        uint64_t startTimes[numRequests];
        uint64_t stopTimes[numRequests];
        Tub<IndexKey::IndexKeyRange> keyRanges[numRequests];
        Tub<IndexLookup> rpcs[numRequests];
        uint32_t readNumObjects[numRequests];

        for (int i =0; i < numRequests; i++) {
            char indexIdent = static_cast<char>(('a') +
                    static_cast<int>(generateRandom() % numIndexlets));
            int intKey = (numObjectsPerIndxlet == range) ? 0 :
                    static_cast<int>
                        (randomNumberGenerator(numObjectsPerIndxlet - range));

            snprintf(primaryKey[i], sizeof(primaryKey[i]), "%c:%dp%0*d",
                    indexIdent, intKey, keyLength, 0);
            snprintf(firstKey[i], sizeof(firstKey[i]), "%c:s%0*d",
                    indexIdent, keyLength-4, intKey);
            snprintf(lastKey[i], sizeof(lastKey[i]), "%c:s%0*d",
                    indexIdent, keyLength-4, intKey + range - 1);
        }

        // Send async requests and receive responses to numRequests lookup
        // requests then indexed read requests and measure time.
        lookupStart = Cycles::rdtsc();
        for (int i = 0; i < numRequests; i++) {
            readNumObjects[i] = 0;
            startTimes[i] = Cycles::rdtsc();

            keyRanges[i].construct(indexId, firstKey[i], keyLength,
                    lastKey[i], keyLength);

            rpcs[i].construct(cluster, lookupTable, *keyRanges[i]);
            numRpcs++;
        }

        bool allDone;
        do {
            allDone = true;
            cluster->poll();
            for (int i = 0; i < numRequests; i++) {
                if (!rpcs[i]->isReady()) {
                    allDone = false;
                    continue;
                }
                while (rpcs[i]->isReady() && rpcs[i]->getNext()) {
                    readNumObjects[i]++;
                    totalObjects++;
                    allDone = false;

                    // Tricky: You need this here to track when
                    // the last object was read.
                    stopTimes[i] = Cycles::rdtsc();
                }
            }
        } while (!allDone);
        lookupEnd = Cycles::rdtsc();

        // Verify-ish
        for (int i = 0; i < numRequests; i++)
            assert(readNumObjects[i] == uint32_t(range));

        for (int i = 0; i < numRequests; i++)
            sumRpcLatencies += (stopTimes[i] - startTimes[i]);

        uint64_t latency = lookupEnd - lookupStart;
        elapsed += latency;
        if (elapsed >= runCycles)
            break;
    }

    double lookupLatency = Cycles::toSeconds(sumRpcLatencies/numRpcs);
    double lookupThroughput = totalObjects/Cycles::toSeconds(elapsed);

    sendMetrics(readHashThroughput, lookupThroughput, lookupLatency);
    if (clientIndex != 0) {
        RAMCLOUD_LOG(NOTICE, "Client:%d %s: hash throughput: %.1f hashes/sec, "
                "Lookup throughput: %.1f lookups/sec with an average rpc "
                "latency of %.2lfus",
                clientIndex, docString, readHashThroughput, lookupThroughput,
                lookupLatency*1e6);
    }
}

// In this test all of the clients repeatedly lookup and/or read objects
// from a collection of indexlets on a single table.  For each lookup/read a
// client chooses an indexlet at random.
void
indexScalability()
{
    uint8_t numIndexlets = (uint8_t)numIndexlet;
    int range = numObjects;
    int numObjectsPerIndexlet = (1000 < numObjects) ? numObjects : 1000;
    int concurrent = count;

    // Gets executed on clients to measure lookup scalability
    // after the servers have been started up and data written (below).
    if (clientIndex > 0) {
        while (true) {
            char command[20];
            char doc[200];
            getCommand(command, sizeof(command));
            if (strcmp(command, "run") == 0) {
                string controlKey = keyVal(0, "doc");
                readObject(controlTable, controlKey.c_str(),
                        downCast<uint16_t>(controlKey.length()),
                        doc, sizeof(doc));
                setSlaveState("running");
                indexScalabilityCommonLookup(numIndexlets,
                        numObjectsPerIndexlet, range, concurrent, doc);
                setSlaveState("idle");
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }

    // Write data to be looked up / read later.
    uint8_t indexId = 1;
    uint8_t numKeys = 2;
    uint64_t firstAllowedKeyHash = 0;
    int size = 100;

    // Declare variables that will be used in the loop.
    Buffer lookupResp;
    uint32_t numHashes;
    uint16_t nextKeyLength;
    uint64_t nextKeyHash;
    uint32_t maxNumHashes = 1000;
    uint32_t lookupOffset =
            sizeof32(WireFormat::LookupIndexKeys::Response);

    std::vector<uint32_t> randomized =
            generateRandListFrom0UpTo(numObjectsPerIndexlet);

    cluster->createTable("indexScalability", numIndexlets);
    uint64_t lookupTable = cluster->getTableId("indexScalability");
    cluster->createIndex(lookupTable, indexId, 0, numIndexlets);
    for (int j = 0; j < numIndexlets; j++) {
        char firstKey = static_cast<char>('a'+j);
        for (int i = 0; i < numObjectsPerIndexlet; i++) {
            int intKey = randomized[i];

            char primaryKey[30];
            snprintf(primaryKey, sizeof(primaryKey), "%c:%dp%0*d",
                    firstKey, intKey, 30, 0);
            char secondaryKey[30];
            snprintf(secondaryKey, sizeof(secondaryKey), "%c:s%0*d",
                    firstKey, 30-4, intKey);

            KeyInfo keyList[2];
            keyList[0].keyLength = 30;
            keyList[0].key = primaryKey;
            keyList[1].keyLength = 30;
            keyList[1].key = secondaryKey;

            Buffer value;
            fillBuffer(value, size, lookupTable,
                    keyList[0].key, keyList[0].keyLength);

            cluster->write(lookupTable, numKeys, keyList,
                    value.getRange(0, size), size);

            // Verify that data was written correctly.
            Key pk(lookupTable, keyList[0].key, keyList[0].keyLength);
            lookupResp.reset();
            cluster->lookupIndexKeys(lookupTable, indexId, keyList[1].key,
                    keyList[1].keyLength, firstAllowedKeyHash, keyList[1].key,
                    keyList[1].keyLength, maxNumHashes,
                    &lookupResp, &numHashes, &nextKeyLength,
                    &nextKeyHash);

            assert(numHashes == 1);
            assert(pk.getHash() ==
                    *lookupResp.getOffset<uint64_t>(lookupOffset));
        }
    }

    // Vary the number of clients and repeat the test for each number.
    printf("# RAMCloud index scalability when 1 or more clients lookup/read\n");
    printf("# %d-byte objects with 30-byte keys chosen at random from\n"
           "# %d indexlets with %d entries each. Each client issues %d\n"
           "# concurrent requests for a range of %d keys\n",
           size, numIndexlets, numObjectsPerIndexlet, concurrent, range);
    printf("# Generated by 'clusterperf.py indexScalability'\n");
    printf("#\n");
    printf("# numClients  throughput(khash/sec)  throughput(kreads/sec)"
            "     Avg.IndexLookup Rpc Latency(us)\n");
    printf("#-------------------------------------\n");
    fflush(stdout);
    for (int numActive = 1; numActive <= numClients; numActive++) {
        char doc[100];
        snprintf(doc, sizeof(doc), "%d active clients", numActive);
        string key = keyVal(0, "doc");
        cluster->write(controlTable, key.c_str(),
                downCast<uint16_t>(key.length()), doc);
        sendCommand("run", "running", 1, numActive-1);
        indexScalabilityCommonLookup(numIndexlets,
                numObjectsPerIndexlet, range, concurrent, doc);
        sendCommand(NULL, "idle", 1, numActive-1);
        ClientMetrics metrics;
        getMetrics(metrics, numActive);
        double hashThroughput = sum(metrics[0])/1e03;
        double readThroughput = sum(metrics[1])/1e03;
        double averageRpcLatency = sum(metrics[2])/numActive;
        printf("%3d               %6.0f               %6.0f"
                "               %6.2f\n", numActive, hashThroughput,
                readThroughput, averageRpcLatency*1e6);
        fflush(stdout);
    }
    sendCommand("done", "done", 1, numClients-1);
    cluster->dropIndex(lookupTable, indexId);
}

// This benchmark measures the multiread times for objects distributed across
// multiple master servers such that there is one object located on each master
// server.
void
multiRead_oneObjectPerMaster()
{
    int dataLength = objectSize;
    uint16_t keyLength = 30;

    printf("# RAMCloud multiRead performance for %u B objects"
           " with %u byte keys\n", dataLength, keyLength);
    printf("# with one object located on each master.\n");
    printf("# Generated by 'clusterperf.py multiRead_oneObjectPerMaster'\n#\n");
    printf("# Num Objs    Num Masters    Objs/Master    "
           "Latency (us)    Latency/Obj (us)\n");
    printf("#--------------------------------------------------------"
            "--------------------\n");

    int objsPerMaster = 1;
    int maxNumMasters = numTables;

    for (int numMasters = 1; numMasters <= maxNumMasters; numMasters++) {
        double latency =
            doMultiRead(dataLength, keyLength, numMasters, objsPerMaster);
        printf("%10d %14d %14d %14.1f %18.2f\n",
            numMasters*objsPerMaster, numMasters, objsPerMaster,
            1e06*latency, 1e06*latency/numMasters/objsPerMaster);
    }
}

// This benchmark measures the multiread times for objects on a single master
// server.
void
multiRead_oneMaster()
{
    int dataLength = objectSize;
    uint16_t keyLength = 30;

    printf("# RAMCloud multiRead performance for %u B objects"
           " with %u byte keys\n", dataLength, keyLength);
    printf("# located on a single master.\n");
    printf("# Generated by 'clusterperf.py multiRead_oneMaster'\n#\n");
    printf("# Num Objs    Num Masters    Objs/Master    "
           "Latency (us)    Latency/Obj (us)\n");
    printf("#--------------------------------------------------------"
            "--------------------\n");

    int numMasters = 1;
    int maxObjsPerMaster = 5000;

    for (int objsPerMaster = 1; objsPerMaster <= maxObjsPerMaster;
         objsPerMaster = (objsPerMaster < 10) ?
            objsPerMaster + 1 : (objsPerMaster < 100) ?
            objsPerMaster + 10 : (objsPerMaster < 1000) ?
                objsPerMaster + 100 : objsPerMaster + 1000) {

        double latency =
            doMultiRead(dataLength, keyLength, numMasters, objsPerMaster);
        printf("%10d %14d %14d %14.1f %18.2f\n",
            numMasters*objsPerMaster, numMasters, objsPerMaster,
            1e06*latency, 1e06*latency/numMasters/objsPerMaster);
    }
}

// This benchmark measures the multiread times for an approximately fixed
// number of objects distributed evenly across varying number of master
// servers.
void
multiRead_general()
{
    int dataLength = objectSize;
    uint16_t keyLength = 30;

    printf("# RAMCloud multiRead performance for "
           "an approximately fixed number\n");
    printf("# of %u B objects with %u byte keys\n", dataLength, keyLength);
    printf("# distributed evenly across varying number of masters.\n");
    printf("# Generated by 'clusterperf.py multiRead_general'\n#\n");
    printf("# Num Objs    Num Masters    Objs/Master    "
           "Latency (us)    Latency/Obj (us)\n");
    printf("#--------------------------------------------------------"
            "--------------------\n");

    int totalObjs = 5000;
    int maxNumMasters = numTables;

    for (int numMasters = 1; numMasters <= maxNumMasters; numMasters++) {
        int objsPerMaster = totalObjs / numMasters;
        double latency =
            doMultiRead(dataLength, keyLength, numMasters, objsPerMaster);
        printf("%10d %14d %14d %14.1f %18.2f\n",
            numMasters*objsPerMaster, numMasters, objsPerMaster,
            1e06*latency, 1e06*latency/numMasters/objsPerMaster);
    }
}

// This benchmark measures the multiread times for an approximately fixed
// number of objects distributed evenly across varying number of master
// servers. Requests are issued in a random order.
void
multiRead_generalRandom()
{
    int dataLength = objectSize;
    uint16_t keyLength = 30;

    printf("# RAMCloud multiRead performance for "
           "an approximately fixed number\n");
    printf("# of %u B objects with %u byte keys\n", dataLength, keyLength);
    printf("# distributed evenly across varying number of masters.\n");
    printf("# Requests are issued in a random order.\n");
    printf("# Generated by 'clusterperf.py multiRead_generalRandom'\n#\n");
    printf("# Num Objs    Num Masters    Objs/Master    "
           "Latency (us)    Latency/Obj (us)\n");
    printf("#--------------------------------------------------------"
            "--------------------\n");

    int totalObjs = 5000;
    int maxNumMasters = numTables;

    for (int numMasters = 1; numMasters <= maxNumMasters; numMasters++) {
        int objsPerMaster = totalObjs / numMasters;
        double latency =
            doMultiRead(dataLength, keyLength, numMasters, objsPerMaster, true);
        printf("%10d %14d %14d %14.1f %18.2f\n",
            numMasters*objsPerMaster, numMasters, objsPerMaster,
            1e06*latency, 1e06*latency/numMasters/objsPerMaster);
    }
}

// This benchmark measures the total throughput of a single server under
// a workload consisting of multiRead operations from several clients.
void
multiReadThroughput()
{
    const uint16_t keyLength = 30;
    int size = objectSize;
    if (size < 0)
        size = 100;
    const int numObjects = 400000000/objectSize;
#define MRT_BATCH_SIZE 80
    if (clientIndex == 0) {
        // This is the master client. Fill in the table, then measure
        // throughput while gradually increasing the number of workers.
        printf("# RAMCloud multi-read throughput of a single server with a\n"
                "# varying number of clients issuing %d-object multi-reads on\n"
                "# randomly-chosen %d-byte objects with %d-byte keys\n",
                MRT_BATCH_SIZE, size, keyLength);
        printf("# Generated by 'clusterperf.py multiReadThroughput'\n");
        readThroughputMaster(numObjects, size, keyLength);
    } else {
        // Slaves execute the following code, which creates load by
        // issuing randomized multi-reads.
        bool running = false;

        // The following buffer is used to accumulate  keys and values for
        // a single multi-read operation.
        Buffer buffer;
        char keys[MRT_BATCH_SIZE*keyLength];
        Tub<ObjectBuffer> values[MRT_BATCH_SIZE];
        MultiReadObject* objects[MRT_BATCH_SIZE];
        uint64_t startTime;
        int objectsRead;
        bool firstRead = true;

        while (true) {
            char command[20];
            if (running) {
                // Write out some statistics for debugging.
                double totalTime = Cycles::toSeconds(Cycles::rdtsc()
                        - startTime);
                double rate = objectsRead/totalTime;
                RAMCLOUD_LOG(NOTICE, "Multi-read rate: %.1f kobjects/sec",
                        rate/1e03);
            }
            getCommand(command, sizeof(command), false);
            if (strcmp(command, "run") == 0) {
                if (!running) {
                    setSlaveState("running");
                    running = true;
                    RAMCLOUD_LOG(NOTICE,
                            "Starting multiReadThroughput benchmark");
                }

                // Perform multi-reads for a second (then check to see
                // if the experiment is over).
                startTime = Cycles::rdtsc();
                objectsRead = 0;
                uint64_t checkTime = startTime + Cycles::fromSeconds(1.0);
                do {
                    if (firstRead) {
                        // Each iteration through the following loop adds one
                        // object to the current multi-read.
                        buffer.reset();
                        for (int i = 0; i < MRT_BATCH_SIZE; i++) {
                            char* key = &keys[i*keyLength];
                            makeKey(downCast<int>(generateRandom()%numObjects),
                                    keyLength, key);
                            values[i].destroy();
                            objects[i] = buffer.emplaceAppend<MultiReadObject>(
                                    dataTable, key, keyLength, &values[i]);
                        }
                        firstRead = false;
                    }
                    cluster->multiRead(objects, MRT_BATCH_SIZE);
                    objectsRead += MRT_BATCH_SIZE;
                } while (Cycles::rdtsc() < checkTime);
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                RAMCLOUD_LOG(NOTICE, "Ending multiReadThroughput benchmark");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }
}

// This benchmark measures the multiwrite times for multiple objects on a
// single master server.
void
multiWrite_oneMaster()
{
    int numMasters = 1;
    int dataLength = objectSize;
    uint16_t keyLength = 30;
    int maxObjsPerMaster = 5000;

    printf("# RAMCloud multiWrite performance for %u B objects"
           " with %u byte keys\n", dataLength, keyLength);
    printf("# located on a single master.\n");
    printf("# Generated by 'clusterperf.py multiWrite_oneMaster'\n#\n");
    printf("# Num Objs    Num Masters    Objs/Master    "
           "Latency (us)    Latency/Obj (us)\n");
    printf("#--------------------------------------------------------"
            "--------------------\n");

    for (int objsPerMaster = 1; objsPerMaster <= maxObjsPerMaster;
         objsPerMaster = (objsPerMaster < 10) ?
            objsPerMaster + 1 : (objsPerMaster < 100) ?
            objsPerMaster + 10 : (objsPerMaster < 1000) ?
                objsPerMaster + 100 : objsPerMaster + 1000) {

        double latency =
            doMultiWrite(dataLength, keyLength, numMasters, objsPerMaster);
        printf("%10d %14d %14d %14.1f %18.2f\n",
            numMasters*objsPerMaster, numMasters, objsPerMaster,
            1e06*latency, 1e06*latency/numMasters/objsPerMaster);
    }

}

static
bool
timedCommit(Transaction& t, uint64_t *elapsed, uint64_t *cummulativeElapsed)
{
    uint64_t start = Cycles::rdtsc();
    bool success = t.commit();
    *elapsed = Cycles::rdtsc() - start;
    *cummulativeElapsed += *elapsed;
    return success;
}

/**
 * This method contains the core of all the transaction tests.
 * It writes objsPerMaster objects on numMasters servers.
 *
 * \param dataLength
 *      Length of data for each object to be written.
 * \param keyLength
 *      Length of key for each object to be written.
 * \param numMasters
 *      The number of master servers across which the objects written
 *      should be distributed.
 * \param objsPerMaster
 *      The number of objects to be read on each master server.
 * \param writeOpPerMaster
 *      The number of objects to be written to each master server.
 * \param randomize
 *      Randomize the order of requests sent from the client.
 *      Note: Randomization can cause bad cache effects on the client
 *      and cause slower than normal operation.
 *
 * \return
 *      The average time, in seconds, to read all the objects in a single
 *      multiRead operation.
 */
double
doTransaction(int dataLength, uint16_t keyLength,
            int numMasters, int objsPerMaster, int writeOpPerMaster,
            bool randomize = false)
{
    assert(writeOpPerMaster <= objsPerMaster);
    // First 'writeOpPerMaster' objects are read & write operations.
    // 'objsPerMaster - writeOpPerMaster' objects are read-only opeartions.

    if (clientIndex != 0)
        return 0;

    Buffer values[numMasters][writeOpPerMaster];
    char keys[numMasters][objsPerMaster][keyLength];

    std::vector<uint64_t> tableIds(numMasters);
    createTables(tableIds, dataLength, "0", 1);

    for (int tableNum = 0; tableNum < numMasters; tableNum++) {
        for (int i = 0; i < objsPerMaster; i++) {
            Util::genRandomString(keys[tableNum][i], keyLength);

            // Adds default object. (Since our tx depends on the existence of
            // current object to lock.)
            cluster->write(tableIds.at(tableNum), keys[tableNum][i], keyLength,
                           "default", 6);
        }
        for (int i = 0; i < writeOpPerMaster; i++) {
            fillBuffer(values[tableNum][i], dataLength,
                    tableIds.at(tableNum), keys[tableNum][i], keyLength);
        }
    }

    // Scramble the requests. Checking code below it stays valid
    // since the value buffer is a pointer to a Buffer in the request.

    // TODO(seojin) randomize

    uint64_t runCycles = Cycles::fromSeconds(500/1e03);
    uint64_t elapsed, cumulativeElapsed = 0;
    int count = 0;
    int abortCount = 0;
    Buffer value;
    while (true) {
        bool txSucceed = false;
        do {
            Transaction t(cluster);
            for (int tableNum = 0; tableNum < numMasters; tableNum++) {
                for (int i = 0; i < objsPerMaster; i++) {
                    t.read(tableIds.at(tableNum), keys[tableNum][i],
                           keyLength, &value);
                }
                for (int i = 0; i < writeOpPerMaster; i++) {
                    t.write(tableIds.at(tableNum), keys[tableNum][i], keyLength,
                             values[tableNum][i].getRange(0, dataLength),
                             dataLength);
                }
            }
            txSucceed = timedCommit(t, &elapsed, &cumulativeElapsed);
            if (!txSucceed) {
                abortCount++;
            }

            // Make sure decisions are sent.
            t.sync();
        } while (!txSucceed);

        count++;
        if (cumulativeElapsed >= runCycles)
            break;
    }
    return Cycles::toSeconds(cumulativeElapsed)/count;
}

// This benchmark measures the transaction commit times for multiple
// 100B objects with 30B keys on a single master server.
void
transaction_oneMaster()
{
    int numMasters = 1;
    int dataLength = 100;
    uint16_t keyLength = 30;
    int maxObjsPerMaster = 30;

    printf("# RAMCloud transaction performance for %u B objects"
           " with %u byte keys\n", dataLength, keyLength);
    printf("# located on a single master.\n");
    printf("# Generated by 'clusterperf.py transaction_oneMaster'\n#\n");
    printf("# Num Objs    Num Masters    Objs/Master  WriteObjs/Master  "
           "Latency (us)    Latency/Obj (us)\n");
    printf("#-----------------------------------------------------------"
            "-------------------------------\n");

    for (int objsPerMaster = 1; objsPerMaster <= maxObjsPerMaster;
         objsPerMaster = (objsPerMaster < 10) ?
            objsPerMaster + 1 : (objsPerMaster < 100) ?
            objsPerMaster + 10 : (objsPerMaster < 1000) ?
                objsPerMaster + 100 : objsPerMaster + 1000) {

        double latency =
            doTransaction(dataLength, keyLength, numMasters,
                          objsPerMaster, objsPerMaster);
        printf("%10d %14d %14d %14d %14.1f %18.2f\n",
            numMasters*objsPerMaster, numMasters, objsPerMaster, objsPerMaster,
            1e06*latency, 1e06*latency/numMasters/objsPerMaster);
    }
}

uint64_t
doShuffleValues(int numIter, int selectivity, int numMasters, int objsPerMaster)
{
    uint64_t elapsed, cumulativeElapsed = 0;
    int commitCount = 0, abortCount = 0;
    int totalObjsSelected = 0;
    int totalTxServerSpan = 0;


    uint64_t* tableIds = getTableIds(numMasters);
    const int keyLength = 4;
    char keys[objsPerMaster][keyLength];

    for (int i = 0; i < objsPerMaster; i++) {
        snprintf(keys[i], keyLength, "%3d", i);
    }

    uint64_t runCycles = Cycles::fromSeconds(5);
    uint64_t start = Cycles::rdtsc();
    while (true) {
        bool txSucceed = false;
        if (Cycles::rdtsc() - start > runCycles) {
            break;
        }
        int serverSpan = 0;
        std::vector<std::pair<int, int> > workingSet;
        RAMCLOUD_LOG(NOTICE, "Selecting objs.");
        for (int tableNum = 0; tableNum < numMasters; tableNum++) {
            bool serverSelected = false;
            for (int i = 0; i < objsPerMaster; i++) {
                if (generateRandom() % selectivity == 0) {
                    workingSet.push_back(std::make_pair(tableNum, i));
                    serverSelected = true;
                }
            }
            if (serverSelected)
                serverSpan++;
        }
        if (workingSet.size() < 2) {
            continue;
        }

        RAMCLOUD_LOG(NOTICE, "Objs selected.");
        do {
            if (Cycles::rdtsc() - start > runCycles) {
                break;
            }
            Transaction t(cluster);
            int sum = 0;
            int written = 0;

            std::vector<std::pair<int, int> >::iterator it;
            for (it = workingSet.begin(); it != workingSet.end(); ++it){
                Buffer value;
                t.read(tableIds[(*it).first], keys[(*it).second], keyLength,
                       &value);
                sum += *(value.getStart<int>());
            }

            it = workingSet.begin();
            for (++it; it != workingSet.end(); ++it) {
                int valToWrite = sum / downCast<int>(workingSet.size()) +
                                 downCast<int>(generateRandom() % 21 - 10);
                t.write(tableIds[(*it).first], keys[(*it).second], keyLength,
                        &valToWrite, sizeof32(valToWrite));
                written += valToWrite;
            }

            it = workingSet.begin();
            int valToWrite = sum - written;
            t.write(tableIds[(*it).first], keys[(*it).second], keyLength,
                    &valToWrite, sizeof32(valToWrite));

            RAMCLOUD_LOG(NOTICE, "Trying to commit. %zu objs selected.",
                         workingSet.size());
            txSucceed = timedCommit(t, &elapsed, &cumulativeElapsed);
            t.sync();
            if (txSucceed) {
                commitCount++;
            } else {
                abortCount++;
            }
            totalTxServerSpan += serverSpan;
            totalObjsSelected += downCast<int>(workingSet.size());
            RAMCLOUD_LOG(NOTICE, "Commit() returned. %zu objs selected. "
                         "Outcome:%d\n", workingSet.size(), txSucceed);
        } while (!txSucceed);
    }

    double latency = Cycles::toSeconds(cumulativeElapsed) *1e06
                     / (commitCount + abortCount);
    RAMCLOUD_LOG(NOTICE, "Average latency: %.1fus", latency);

    {
        char key[30];
        snprintf(key, sizeof(key), "abortCount %3d", clientIndex);
        cluster->write(dataTable, key, (uint16_t)strlen(key),
                       &abortCount, sizeof(abortCount));
    } {
        char key[30];
        snprintf(key, sizeof(key), "commitCount %3d", clientIndex);
        cluster->write(dataTable, key, (uint16_t)strlen(key),
                       &commitCount, sizeof32(commitCount));
    } {
        char key[30];
        snprintf(key, sizeof(key), "latency %3d", clientIndex);
        cluster->write(dataTable, key, (uint16_t)strlen(key),
                       &latency, sizeof32(latency));
    } {
        char key[30];
        snprintf(key, sizeof(key), "serverSpan %3d", clientIndex);
        cluster->write(dataTable, key, (uint16_t)strlen(key),
                       &totalTxServerSpan, sizeof32(totalTxServerSpan));
    } {
        char key[30];
        snprintf(key, sizeof(key), "objsSelected %3d", clientIndex);
        cluster->write(dataTable, key, (uint16_t)strlen(key),
                       &totalObjsSelected, sizeof32(totalObjsSelected));
    }

    return cumulativeElapsed;
}

enum OpType{ READ_TYPE, WRITE_TYPE };
/**
 * Run a specified workload and measure the latencies of the specified opType.
 */
void
doWorkload(OpType type)
{
    WorkloadGenerator loadGenerator(workload);

    if (clientIndex > 0) {
        // Perform slave setup.
        while (true) {
            char command[20];
            getCommand(command, sizeof(command));
            if (strcmp(command, "run") == 0) {
                // Slaves will run the specified workload until the "data" table
                // is dropped.
                setSlaveState("running");
                try
                {
                    loadGenerator.run(static_cast<uint64_t>(targetOps));
                }
                catch (TableDoesntExistException &e)
                {}
                setSlaveState("done");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }

    loadGenerator.setup();

    sendCommand("run", "running", 1, numClients-1);

    const uint16_t keyLen = 30;
    char key[keyLen];
    Buffer readBuf;
    char value[loadGenerator.recordSizeB];

    // Begin counter collection on the server side.
    memset(key, 0, keyLen);
    cluster->objectServerControl(dataTable, key, keyLen,
                            WireFormat::START_PERF_COUNTERS);

    // Force serialization so that writing interferes less with the read
    // benchmark.
    Util::serialize();

    uint64_t readThreshold = (~0UL / 100) * loadGenerator.readPercent;
    uint64_t opCount = 0;
    uint64_t targetMissCount = 0;
    uint64_t readCount = 0;
    uint64_t writeCount = 0;
    uint64_t targetNSPO = 0;
    if (targetOps > 0) {
        targetNSPO = 1000000000 / static_cast<uint64_t>(targetOps);
        // Randomize start time
        Cycles::sleep((generateRandom() % targetNSPO) / 1000);
    }

    string("workload").copy(key, 8);
    *reinterpret_cast<uint64_t*>(key + 8) = 0;
    Buffer statsBuffer;
    cluster->objectServerControl(dataTable, key, keyLen,
                    WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
                    &statsBuffer);
    PerfStats startStats = *statsBuffer.getStart<PerfStats>();

    uint64_t nextStop = 0;
    uint64_t start = Cycles::rdtsc();
    uint64_t stop = 0;

    // Issue the reads back-to-back, and save the times.
    std::vector<uint64_t> ticks;
    ticks.resize(count);
    int i = 0;
    while (i < count) {
        // Generate random key.
        memset(key, 0, keyLen);
        string("workload").copy(key, 8);
        *reinterpret_cast<uint64_t*>(key + 8) =
                loadGenerator.generator->nextNumber();

        // Perform Operation
        if (generateRandom() <= readThreshold) {
            // Do read
            uint64_t start = Cycles::rdtsc();
            cluster->read(dataTable, key, keyLen, &readBuf);
            ticks[i] = Cycles::rdtsc() - start;
            if (type == READ_TYPE) {
                i++;
            }
            readCount++;
        } else {
            // Do write
            Util::genRandomString(value, loadGenerator.recordSizeB);
            uint64_t start = Cycles::rdtsc();
            cluster->write(dataTable, key, keyLen, value,
                    loadGenerator.recordSizeB);
            ticks[i] = Cycles::rdtsc() - start;
            if (type == WRITE_TYPE) {
                i++;
            }
            writeCount++;
        }
        opCount++;
        stop = Cycles::rdtsc();

        // throttle
        if (targetNSPO > 0) {
            nextStop = start +
                       Cycles::fromNanoseconds(
                            (opCount * targetNSPO) +
                            (generateRandom() % targetNSPO) -
                            (targetNSPO / 2));

            if (Cycles::rdtsc() > nextStop) {
                targetMissCount++;
            }
            while (Cycles::rdtsc() < nextStop);
        }
    }

    cluster->objectServerControl(dataTable, key, keyLen,
            WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
            &statsBuffer);
    PerfStats finishStats = *statsBuffer.getStart<PerfStats>();
    double elapsedTime = static_cast<double>(finishStats.collectionTime -
            startStats.collectionTime)/ finishStats.cyclesPerSecond;
    double rate = static_cast<double>(finishStats.readCount +
            finishStats.writeCount -
            startStats.readCount -
            startStats.writeCount) / elapsedTime;

    LogLevel ll = NOTICE;
    if (targetMissCount > 0) {
        ll = WARNING;
    }
    RAMCLOUD_LOG(ll,
            "Actual OPS %.0f / Target OPS %lu",
            static_cast<double>(opCount) /
            static_cast<double>(Cycles::toSeconds(stop - start)),
            static_cast<uint64_t>(targetOps));
    RAMCLOUD_LOG(ll,
            "%lu Misses / %lu Total -- %lu/%lu R/W",
            targetMissCount, opCount, readCount, writeCount);

    // Stop slaves.
    cluster->dropTable("data");
    sendCommand("done", NULL, 1, numClients-1);

    printf("0.0 Max Throughput: %.0f ops\n", rate);

    // Output the times (several comma-separated values on each line).
    Logger::get().sync();
    int valuesInLine = 0;
    for (int i = 0; i < count; i++) {
        if (valuesInLine >= 10) {
            valuesInLine = 0;
            printf("\n");
        }
        if (valuesInLine != 0) {
            printf(",");
        }
        double micros = Cycles::toSeconds(ticks[i])*1.0e06;
        printf("%.2f", micros);
        valuesInLine++;
    }
    printf("\n");
    fflush(stdout);
    #undef NUM_KEYS

    // Wait for slaves to exit.
    sendCommand(NULL, "done", 1, numClients-1);
}

// This benchmark measures test consistency guarantee of transaction
// by several clients trasfer balances among many objects.
void
transaction_collision()
{
    int numMasters = 5;
    const int numObjs = 20, keyLength = 4;
    char keys[numObjs][keyLength];
    int objsPerMaster = numObjs;

    for (int i = 0; i < numObjs; i++) {
        snprintf(keys[i], keyLength, "%3d", i);
    }

    if (clientIndex > 0) {
        // Slaves execute the following code, which moves balances
        // around objects.
        while (true) {
            char command[20];
            getCommand(command, sizeof(command));
            if (strcmp(command, "run") == 0) {
                setSlaveState("running");

                RAMCLOUD_LOG(NOTICE, "Strating shuffling test.");
                doShuffleValues(20, 15, numMasters, objsPerMaster);
                //setSlaveState("idle");
                setSlaveState("done");
                return;
            }  else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }

    // The master executes the following code, which starts up zero or more
    // slaves to generate load, then times the performance of reading.

    std::vector<uint64_t> tableIds(numMasters);
    createTables(tableIds, 0, "0", 1);

    int startingValue = 100;
    for (int tableNum = 0; tableNum < numMasters; tableNum++) {
        for (int i = 0; i < objsPerMaster; i++) {
            cluster->write(tableIds.at(tableNum), keys[i], keyLength,
                           &startingValue, sizeof32(startingValue));
        }
    }
    printf("# RAMCloud transaction collision stress test\n");
    printf("#----------------------------------------------------------\n");
    printf("# Balances after all transactions\n");

    sendCommand("run", NULL, 1, numClients-1);

    for (int i = 1; i < numClients; i++) {
        waitSlave(i, "done", 60);
        RAMCLOUD_LOG(NOTICE, "slave %d is done.", i);
    }

    RAMCLOUD_LOG(NOTICE, "All slaves are done.");
    int sum = 0;
    for (int tableNum = 0; tableNum < numMasters; tableNum++) {
        int localSum = 0;
        printf("master %d | ", tableNum);
        for (int i = 0; i < objsPerMaster; i++) {
            Buffer value;
            cluster->read(tableIds.at(tableNum), keys[i],
                          keyLength, &value);
            printf("%3d ", *(value.getStart<int>()));
            sum += *(value.getStart<int>());
            localSum += *(value.getStart<int>());
        }
        printf(" | row sum: %d\n", localSum);
    }

    printf("#------------------------------------------------------------\n");
    printf("# Total sum: %d\n", sum);

    printf("# clientID | # of commits | # of aborts | avg. serverSpan | "
           "avg. objs | Avg. Latency (us) |\n");
    printf("#------------------------------------------------------------\n");
    for (int i = 1; i < numClients; ++i) {
        char abortCountKey[20], commitCountKey[20], latencyKey[20],
             serverSpanKey[20], objsSelectedKey[20];
        snprintf(abortCountKey, sizeof(abortCountKey), "abortCount %3d", i);
        Buffer value;
        cluster->read(dataTable, abortCountKey, (uint16_t)strlen(abortCountKey),
                      &value);
        int aborts = *(value.getStart<int>());
        value.reset();
        snprintf(commitCountKey, sizeof(commitCountKey), "commitCount %3d", i);
        cluster->read(dataTable, commitCountKey,
                      (uint16_t)strlen(commitCountKey), &value);
        int commits = *(value.getStart<int>());
        snprintf(latencyKey, sizeof(latencyKey), "latency %3d", i);
        value.reset();
        cluster->read(dataTable, latencyKey,
                      (uint16_t)strlen(latencyKey), &value);
        double latency = *(value.getStart<double>());
        value.reset();
        snprintf(serverSpanKey, sizeof(serverSpanKey), "serverSpan %3d", i);
        cluster->read(dataTable, serverSpanKey,
                      (uint16_t)strlen(serverSpanKey), &value);
        int totalServerSpan = *(value.getStart<int>());
        value.reset();
        snprintf(objsSelectedKey, sizeof(objsSelectedKey),
                 "objsSelected %3d", i);
        cluster->read(dataTable, objsSelectedKey,
                      (uint16_t)strlen(objsSelectedKey), &value);
        int totalObjsSelected = *(value.getStart<int>());
        printf(" %9d | %12d | %11d | %15.2f | %9.2f | %6.1fus\n",
               i, commits, aborts,
               static_cast<double>(totalServerSpan) / (commits + aborts),
               static_cast<double>(totalObjsSelected) / (commits + aborts),
               latency);
    }
}

// Commit a transactional read-write on randomly-chosen objects from a large
// table.  Similar to writeDistRandom.
void
transactionDistRandom()
{
    int numKeys = 2000000;
    if (clientIndex != 0)
        return;

    const uint16_t keyLength = 30;

    char key[keyLength];
    char value[objectSize];

    std::vector<uint64_t> tableIds(numTables);
    createTables(tableIds, 0, "0", 1);

    for (int i = 0; i < numTables; i++) {
        fillTable(tableIds.at(i), numKeys, keyLength, objectSize);
    }

    // Issue the writes back-to-back, and save the times.
    std::vector<uint64_t> ticks;
    ticks.resize(count);
    for (int i = 0; i < count; i++) {
        // We generate the random number separately to avoid timing potential
        // cache misses on the client side.
        std::unordered_set<int> keyIds;
        while (keyIds.size() < static_cast<size_t>(numObjects)) {
            int keyId = downCast<int>(generateRandom() % numKeys);
            keyIds.insert(keyId);
        }
        std::unordered_set<int> tableBlacklist;
        txSpan = std::min(txSpan, numTables);
        size_t blacklistSize = static_cast<size_t>(numTables - txSpan);
        while (tableBlacklist.size() < blacklistSize) {
            int tableIndex = downCast<int>(generateRandom() % numTables);
            tableBlacklist.insert(tableIndex);
        }
        std::vector<int> selectedTableIndexes;
        std::unordered_set<int> usedTableIndexes = tableBlacklist;
        while (selectedTableIndexes.size() < static_cast<size_t>(numObjects)) {
            int tableIndex = downCast<int>(generateRandom() % numTables);
            size_t before = usedTableIndexes.size();
            usedTableIndexes.insert(tableIndex);
            size_t after = usedTableIndexes.size();
            if (before < after) {
                selectedTableIndexes.push_back(tableIndex);
            }
            if (static_cast<int>(after) == numTables) {
                usedTableIndexes.clear();
                usedTableIndexes = tableBlacklist;
            }
        }

        Transaction t(cluster);

        // Fill transaction.
        int j = 0;
        for (auto it = keyIds.begin(); it != keyIds.end(); it++) {
            makeKey(*it, keyLength, key);
            Util::genRandomString(value, objectSize);

            Buffer buffer;
            t.read(tableIds.at(selectedTableIndexes.at(j)), key, keyLength,
                    &buffer);
            t.write(tableIds.at(selectedTableIndexes.at(j)), key, keyLength,
                    value, objectSize);
            ++j;
        }

        // Do the benchmark
        uint64_t start = Cycles::rdtsc();
        t.commit();
        ticks[i] = Cycles::rdtsc() - start;
        t.sync();
    }

    // Output the times (several comma-separated values on each line).
    Logger::get().sync();
    int valuesInLine = 0;
    for (int i = 0; i < count; i++) {
        if (valuesInLine >= 10) {
            valuesInLine = 0;
            printf("\n");
        }
        if (valuesInLine != 0) {
            printf(",");
        }
        double micros = Cycles::toSeconds(ticks[i])*1.0e06;
        printf("%.2f", micros);
        valuesInLine++;
    }
    printf("\n");
}

// Commit a transactional read-write on randomly-chosen objects from a large
// table and measure the throughput.
void
transactionThroughput()
{
    int numKeys = 2000000;
    int partitionSize = numKeys / numClients;

    const uint16_t keyLength = 30;

    char key[keyLength];
    char value[objectSize];

    std::vector<uint64_t> tableIds(numTables);

    if (clientIndex == 0) {
        // This is the master client.
        // Setup
        createTables(tableIds, 0, "0", 1);

        for (int i = 0; i < numTables; i++) {
            fillTable(tableIds.at(i), numKeys, keyLength, objectSize);
        }

        printf("# RAMCloud transaction throughput with a varying\n"
                "# number of clients\n");
        printf("# Generated by 'clusterperf.py transactionThroughput'\n");
        printf("#\n");
        printf("# numClients   throughput\n");
        printf("#              (kTx/sec)\n");
        printf("#-------------------------\n");
        for (int numSlaves = 1; numSlaves < numClients; numSlaves++) {
            sendCommand("ready", "done", 1, numSlaves);
            sendCommand("start", "running", 1, numSlaves);
            Cycles::sleep(1000000);
            sendCommand("output", "done", 1, numSlaves);

            sendMetrics(0.0);
            ClientMetrics metrics;
            getMetrics(metrics, numSlaves+1);
            printf("%5d         %8.3f\n", numSlaves, sum(metrics[0])/1e3);
        }
        sendCommand("stop", "stopped", 1, numClients - 1);

        return;
    }


    bool running = false;
    uint64_t startCycles = Cycles::rdtsc();
    int txCount = 0;
    uint64_t runCycles = Cycles::fromMicroseconds(1000000);
    uint64_t elapsed = 0;

    char command[20];

    while (true) {
        getCommand(command, sizeof(command), false);
        if (strcmp(command, "ready") == 0) {
            break;
        }
    }

    getTableIds(tableIds);
    RAMCLOUD_LOG(DEBUG, "Starting Client");
    setSlaveState("done");

    while (true) {
        getCommand(command, sizeof(command), false);
        if (strcmp(command, "stop") == 0) {
            break;
        } else if (strcmp(command, "start") == 0) {
            if (!running) {
                setSlaveState("running");
                running = true;
                txCount = 0;
                elapsed = 0;
                RAMCLOUD_LOG(DEBUG, "Running Client");
                startCycles = Cycles::rdtsc();
            }
        } else if (strcmp(command, "output") == 0) {
            if (running && elapsed >= runCycles) {
                double throughput = txCount/Cycles::toSeconds(elapsed);
                sendMetrics(throughput);
                running = false;
                setSlaveState("done");
                RAMCLOUD_LOG(DEBUG, "Run Complete: %f", throughput);
            }
        }

        // We generate the random number separately to avoid timing potential
        // cache misses on the client side.
        std::unordered_set<int> keyIds;
        while (keyIds.size() < static_cast<size_t>(numObjects)) {
            int keyId = downCast<int>(generateRandom() % partitionSize);
            keyId += partitionSize * clientIndex;
            keyIds.insert(keyId);
        }
        std::unordered_set<int> tableBlacklist;
        txSpan = std::min(txSpan, numTables);
        size_t blacklistSize = static_cast<size_t>(numTables - txSpan);
        while (tableBlacklist.size() < blacklistSize) {
            int tableIndex = downCast<int>(generateRandom() % numTables);
            tableBlacklist.insert(tableIndex);
        }
        std::vector<int> selectedTableIndexes;
        std::unordered_set<int> usedTableIndexes = tableBlacklist;
        while (selectedTableIndexes.size() < static_cast<size_t>(numObjects)) {
            int tableIndex = downCast<int>(generateRandom() % numTables);
            size_t before = usedTableIndexes.size();
            usedTableIndexes.insert(tableIndex);
            size_t after = usedTableIndexes.size();
            if (before < after) {
                selectedTableIndexes.push_back(tableIndex);
            }
            if (static_cast<int>(after) == numTables) {
                usedTableIndexes.clear();
                usedTableIndexes = tableBlacklist;
            }
        }

        Transaction t(cluster);

        // Fill transaction.
        int j = 0;
        for (auto it = keyIds.begin(); it != keyIds.end(); it++) {
            makeKey(*it, keyLength, key);
            Util::genRandomString(value, objectSize);

            Buffer buffer;
            t.read(tableIds.at(selectedTableIndexes.at(j)), key, keyLength,
                    &buffer);
            t.write(tableIds.at(selectedTableIndexes.at(j)), key, keyLength,
                    value, objectSize);
            ++j;
        }

        // Do the benchmark
        t.commit();
        t.sync();
        elapsed = Cycles::rdtsc() - startCycles;

        if (elapsed < runCycles) {
            txCount++;
            RAMCLOUD_CLOG(DEBUG, "txCount++");
        }
    }

    RAMCLOUD_LOG(DEBUG, "Client Stopped");
    setSlaveState("stopped");
}

void
transactionContention()
{
    int numKeys = 2000000;

    const uint16_t keyLength = 30;

    char key[keyLength];
    char value[objectSize];

    std::vector<uint64_t> tableIds(numTables);

    if (clientIndex == 0) {
        // This is the master client.
        // Setup
        createTables(tableIds, 0, "0", 1);

        for (int i = 0; i < numTables; i++) {
            fillTable(tableIds.at(i), numKeys, keyLength, objectSize);
        }

        printf("# RAMCloud transaction throughput under contention\n"
                "# with a varying number of clients\n");
        printf("# Generated by 'clusterperf.py transactionContention'\n");
        printf("#\n");
        printf("# numClients   throughput     raw throughput.\n");
        printf("#              (kTx/sec)      (kTx/sec)\n");
        printf("#--------------------------------------------\n");
        for (int numSlaves = 1; numSlaves < numClients; numSlaves++) {
            sendCommand("ready", "done", 1, numSlaves);
            sendCommand("start", "running", 1, numSlaves);
            Cycles::sleep(1000000);
            sendCommand("output", "done", 1, numSlaves);

            sendMetrics(0.0, 0.0);
            ClientMetrics metrics;
            getMetrics(metrics, numSlaves+1);
            printf("%5d         %8.3f        %8.3f\n",
                    numSlaves, sum(metrics[0])/1e3, sum(metrics[1])/1e3);
        }
        sendCommand("stop", "stopped", 1, numClients - 1);

        return;
    }


    bool running = false;
    uint64_t startCycles = Cycles::rdtsc();
    int txCount = 0;
    int rawTxCount = 0;
    uint64_t runCycles = Cycles::fromMicroseconds(1000000);
    uint64_t elapsed = 0;

    ZipfianGenerator generator(numKeys);

    char command[20];

    while (true) {
        getCommand(command, sizeof(command), false);
        if (strcmp(command, "ready") == 0) {
            break;
        }
    }

    getTableIds(tableIds);
    RAMCLOUD_LOG(DEBUG, "Starting Client");
    setSlaveState("done");

    bool committed = true;
    std::unordered_set<int> keyIds;

    while (true) {
        getCommand(command, sizeof(command), false);
        if (strcmp(command, "stop") == 0) {
            break;
        } else if (strcmp(command, "start") == 0) {
            if (!running) {
                setSlaveState("running");
                running = true;
                txCount = 0;
                rawTxCount = 0;
                elapsed = 0;
                RAMCLOUD_LOG(DEBUG, "Running Client");
                startCycles = Cycles::rdtsc();
            }
        } else if (strcmp(command, "output") == 0) {
            if (running && elapsed >= runCycles) {
                double throughput = txCount/Cycles::toSeconds(elapsed);
                double rawThroughput = rawTxCount/Cycles::toSeconds(elapsed);
                sendMetrics(throughput, rawThroughput);
                running = false;
                setSlaveState("done");
                RAMCLOUD_LOG(DEBUG,
                        "Run Complete: %f %f", throughput, rawThroughput);
            }
        }

        // If the last transaction committed the next one will be a new one.
        // Otherwise, the transaction should retry.
        if (committed) {
            keyIds.clear();
            while (keyIds.size() < static_cast<size_t>(numObjects)) {
                int keyId = downCast<int>(generator.nextNumber());
                keyIds.insert(keyId);
            }
        }


        Transaction t(cluster);

        // Fill transaction.
        int j = 0;
        for (auto it = keyIds.begin(); it != keyIds.end(); it++) {
            makeKey(*it, keyLength, key);
            Util::genRandomString(value, objectSize);

            Buffer buffer;
            t.read(tableIds.at(j), key, keyLength, &buffer);
            t.write(tableIds.at(j), key, keyLength, value, objectSize);
            j = (j + 1) % numTables;
        }

        // Do the benchmark
        committed = t.commit();
        t.sync();
        elapsed = Cycles::rdtsc() - startCycles;

        if (elapsed < runCycles) {
            if (committed) {
                txCount++;
            }
            rawTxCount++;
        }
    }

    RAMCLOUD_LOG(DEBUG, "Client Stopped");
    setSlaveState("stopped");
}

// This benchmark measures overall network bandwidth using many clients, each
// reading repeatedly a single large object on a different server.  The goal
// is to stress the internal network switching fabric without overloading any
// particular client or server.
void
netBandwidth()
{
    const char* key = "123456789012345678901234567890";
    uint16_t keyLength = downCast<uint16_t>(strlen(key));

    // Duration of the test, in ms.
    int ms = 100;

    if (clientIndex > 0) {
        // Slaves execute the following code.  First, wait for the master
        // to set everything up, then open the table we will use.
        char command[20];
        getCommand(command, sizeof(command));
        char tableName[20];
        snprintf(tableName, sizeof(tableName), "table%d", clientIndex);
        uint64_t tableId = cluster->getTableId(tableName);
        RAMCLOUD_LOG(NOTICE, "Client %d reading from table %lu", clientIndex,
                tableId);
        setSlaveState("running");

        // Read a value from the table repeatedly, and compute bandwidth.
        Buffer value;
        double latency = timeRead(tableId, key, keyLength, ms, value);
        double bandwidth = value.size()/latency;
        sendMetrics(bandwidth);
        setSlaveState("done");
        RAMCLOUD_LOG(NOTICE,
                "Bandwidth (%u-byte object with %u-byte key): %.1f MB/sec",
                value.size(), keyLength, bandwidth/(1024*1024));
        return;
    }

    // The master executes the code below.  First, create a table for each
    // slave, with a single object.

    int size = objectSize;
    if (size < 0)
        size = 1024*1024;
    std::vector<uint64_t> tableIds(numClients);
    createTables(tableIds, objectSize, key, keyLength);

    // Start all the slaves running, and read our own local object.
    sendCommand("run", "running", 1, numClients-1);
    RAMCLOUD_LOG(DEBUG, "Master reading from table %lu", tableIds.at(0));
    Buffer value;
    double latency = timeRead(tableIds.at(0), key, keyLength, 100, value);
    double bandwidth = (keyLength + value.size())/latency;
    sendMetrics(bandwidth);

    // Collect statistics.
    ClientMetrics metrics;
    getMetrics(metrics, numClients);
    RAMCLOUD_LOG(DEBUG,
            "Bandwidth (%u-byte object with %u-byte key): %.1f MB/sec",
            value.size(), keyLength, bandwidth/(1024*1024));

    printBandwidth("netBandwidth", sum(metrics[0]),
            "many clients reading from different servers");
    printBandwidth("netBandwidth.max", max(metrics[0]),
            "fastest client");
    printBandwidth("netBandwidth.min", min(metrics[0]),
            "slowest client");
}

// Each client reads a single object from each master.  Good for
// testing that each host in the cluster can send/receive RPCs
// from every other host.
void
readAllToAll()
{
    const char* key = "123456789012345678901234567890";
    uint16_t keyLength = downCast<uint16_t>(strlen(key));

    if (clientIndex > 0) {
        char command[20];
        do {
            getCommand(command, sizeof(command));
            Cycles::sleep(10 * 1000);
        } while (strcmp(command, "run") != 0);
        setSlaveState("running");

        for (int tableNum = 0; tableNum < numTables; ++tableNum) {
            string tableName = format("table%d", tableNum);
            try {
                uint64_t tableId = cluster->getTableId(tableName.c_str());

                Buffer result;
                uint64_t startCycles = Cycles::rdtsc();
                ReadRpc read(cluster, tableId, key, keyLength, &result);
                while (!read.isReady()) {
                    context->dispatch->poll();
                    double secsWaiting =
                        Cycles::toSeconds(Cycles::rdtsc() - startCycles);
                    if (secsWaiting > 1.0) {
                        RAMCLOUD_LOG(ERROR,
                                    "Client %d couldn't read from table %s",
                                    clientIndex, tableName.c_str());
                        read.cancel();
                        continue;
                    }
                }
                read.wait();
            } catch (ClientException& e) {
                RAMCLOUD_LOG(ERROR,
                    "Client %d got exception reading from table %s: %s",
                    clientIndex, tableName.c_str(), e.what());
            } catch (...) {
                RAMCLOUD_LOG(ERROR,
                    "Client %d got unknown exception reading from table %s",
                    clientIndex, tableName.c_str());
            }
        }
        setSlaveState("done");
        return;
    }

    int size = objectSize;
    if (size < 0)
        size = 100;
    std::vector<uint64_t> tableIds(numTables);
    createTables(tableIds, size, key, keyLength);

    for (int i = 0; i < numTables; ++i) {
        uint64_t tableId = tableIds.at(i);
        Buffer result;
        uint64_t startCycles = Cycles::rdtsc();
        ReadRpc read(cluster, tableId, key, keyLength, &result);
        while (!read.isReady()) {
            context->dispatch->poll();
            if (Cycles::toSeconds(Cycles::rdtsc() - startCycles) > 1.0) {
                RAMCLOUD_LOG(ERROR,
                            "Master client %d couldn't read from tableId %lu",
                            clientIndex, tableId);
                return;
            }
        }
        read.wait();
    }

    for (int slaveIndex = 1; slaveIndex < numClients; ++slaveIndex) {
        sendCommand("run", "running", slaveIndex);
        // Give extra time if clients have to contact a lot of masters.
        waitSlave(slaveIndex, "done", 1.0 + 0.1 * numTables);
    }
}
// Read a single object many times, and compute a cumulative distribution
// of read times.
void
readDist()
{
    if (clientIndex != 0)
        return;

    // Create an object to read, and verify its contents (this also
    // loads all caches along the way).
    const char* key = "123456789012345678901234567890";
    uint16_t keyLength = downCast<uint16_t>(strlen(key));
    Buffer input, value;
    fillBuffer(input, objectSize, dataTable, key, keyLength);
    cluster->write(dataTable, key, keyLength,
                input.getRange(0, objectSize), objectSize);
    cluster->read(dataTable, key, keyLength, &value);
    checkBuffer(&value, 0, objectSize, dataTable, key, keyLength);

    // Begin counter collection on the server side.
    cluster->objectServerControl(dataTable, key, keyLength,
            WireFormat::START_PERF_COUNTERS);

    // Force serialization so that writing interferes less with the read
    // benchmark.
    Util::serialize();

    // Warmup, if desired
    for (int i = 0; i < warmupCount; i++) {
        cluster->read(dataTable, key, keyLength, &value);
    }

    // Issue the reads as quickly as possible, and save the times.
    std::vector<uint64_t> ticks(count);
    uint64_t start = Cycles::rdtsc();
    for (int i = 0; i < count; i++) {
        cluster->read(dataTable, key, keyLength, &value);
        ticks.at(i) = Cycles::rdtsc();
    }

    // Output the times (several comma-separated values on each line).
    Logger::get().sync();
    int valuesInLine = 0;
    for (int i = 0; i < count; i++) {
        if (valuesInLine >= 10) {
            valuesInLine = 0;
            printf("\n");
        }
        if (valuesInLine != 0) {
            printf(",");
        }
        double micros = Cycles::toSeconds(ticks.at(i) - start)*1.0e06;
        printf("%.2f", micros);
        valuesInLine++;
        start = ticks.at(i);
    }
    printf("\n");
}

// Read randomly-chosen objects from a large table (so that there will be cache
// misses on the hash table and the object) and compute a cumulative
// distribution of read times.
void
readDistRandom()
{
    int numKeys = 2000000;
    #define BATCH_SIZE 500
    if (clientIndex != 0)
        return;

    const uint16_t keyLength = 30;
    char key[keyLength];
    Buffer value;

    // The following variables divide the read times into intervals
    // of 20 µs and keep track of how many read times fall in each
    // interval.
    #define MICROS_PER_BUCKET 20
    uint64_t bucketTicks = MICROS_PER_BUCKET * Cycles::fromNanoseconds(1000);
    #define NUM_BUCKETS 10
    int timeBuckets[NUM_BUCKETS];
    for (int i = 0; i < NUM_BUCKETS; i++) {
        timeBuckets[i] = 0;
    }

    fillTable(dataTable, numKeys, keyLength, objectSize);

    // Begin counter collection on the server side.
    memset(key, 0, keyLength);
    cluster->objectServerControl(dataTable, key, keyLength,
                            WireFormat::RESET_METRICS);
    cluster->objectServerControl(dataTable, key, keyLength,
                            WireFormat::START_PERF_COUNTERS);

    // Force serialization so that writing interferes less with the read
    // benchmark.
    Util::serialize();

    // The following variable is used to stop the test after 10 seconds
    // if we haven't read count keys by then.
    uint64_t stop = Cycles::rdtsc() + Cycles::fromSeconds(10.0);

    // Issue the reads back-to-back, and save the times.
    std::vector<uint64_t> ticks(count);
    for (int i = 0; i < count; i++) {
        // We generate the random number separately to avoid timing potential
        // cache misses on the client side.
        makeKey(downCast<int>(generateRandom() % numKeys), keyLength, key);

        // Do the benchmark
        uint64_t start = Cycles::rdtsc();
        cluster->read(dataTable, key, keyLength, &value);
        uint64_t now = Cycles::rdtsc();
        uint64_t interval = now - start;
        ticks.at(i) = interval;
        int index = downCast<int>(interval/bucketTicks);
        if (index < NUM_BUCKETS) {
            timeBuckets[index]++;
        }
        if (now >= stop) {
            count = i+1;
            LOG(NOTICE, "Time exceeded: stopping test after %d measurements",
                    count);
            break;
        }
    }

    // Output the times (several comma-separated values on each line).
    Logger::get().sync();
    int valuesInLine = 0;
    for (int i = 0; i < count; i++) {
        if (valuesInLine >= 10) {
            valuesInLine = 0;
            printf("\n");
        }
        if (valuesInLine != 0) {
            printf(",");
        }
        double micros = Cycles::toSeconds(ticks.at(i))*1.0e06;
        printf("%.2f", micros);
        valuesInLine++;
    }
    printf("\n");
    fflush(stdout);
    for (int i = 0; i <  NUM_BUCKETS; i++) {
        LOG(NOTICE, "Number of times between %d and %d usecs: %d (%.2f%%)",
                MICROS_PER_BUCKET*i, MICROS_PER_BUCKET*(i+1),
                timeBuckets[i], 100.0*timeBuckets[i]/count);
    }
}

// Perform the specified workload and measure the read latencies.
void
readDistWorkload()
{
    doWorkload(READ_TYPE);
}

/**
 *  This method implements almost all of the functionality for both
 * readInterference and writeInterference.
 *
 * \param read
 *      True means measure read times; false means measure write times.
 * */
void interferenceCommon(bool read)
{
    if (clientIndex != 0)
        return;

    // Write one object of each of several different sizes.
    char buffer[1000000];
#define NUM_SIZES 5
    int sizes[] = {100, 1000, 10000, 100000, 1000000};
    const char* keys[] = {"size100", "size1000", "size10000", "size100000",
            "size1000000"};
    uint16_t keyLengths[] = {7, 8, 9, 10, 11};
    for (int i = 0; i < NUM_SIZES; i++) {
        cluster->write(dataTable, keys[i], keyLengths[i], buffer, sizes[i]);
    }

    // Repeatedly read/write all of the objects in parallel, recording how long
    // each operation takes.
# define MAX_OPS 1000000
    std::vector<uint64_t> ticks[NUM_SIZES];
    for (int i = 0; i < NUM_SIZES; i++) {
        // Touch all of the counters to make sure we don't incur page
        // faults later.
        for (int j = 0; j < MAX_OPS; j++) {
            ticks[i].push_back(0);
        }
        ticks[i].resize(0);
    }
    uint64_t startTime[NUM_SIZES];
    Tub<ReadRpc> readRpcs[NUM_SIZES];
    Tub<WriteRpc> writeRpcs[NUM_SIZES];
    Buffer results[NUM_SIZES];

    for (int i = 0; i < NUM_SIZES; i++) {
        startTime[i] = Cycles::rdtsc();
        if (read) {
            readRpcs[i].construct(cluster, dataTable, keys[i], keyLengths[i],
                    &results[i]);
        } else {
            writeRpcs[i].construct(cluster, dataTable, keys[i], keyLengths[i],
                    buffer, sizes[i]);
        }
    }

    uint64_t endTime = Cycles::rdtsc() + Cycles::fromSeconds(10);
    while (1) {
        for (int i = 0; i < NUM_SIZES; i++) {
            if (read) {
                if (!readRpcs[i]->isReady()) {
                    break;
                }
                readRpcs[i]->wait();
            } else {
                if (!writeRpcs[i]->isReady()) {
                    break;
                }
                writeRpcs[i]->wait();
            }

            uint64_t now = Cycles::rdtsc();
            ticks[i].push_back(now - startTime[i]);
            if ((ticks[i].size() >= MAX_OPS) || (now >= endTime)) {
                goto done;
            }
            if (read) {
                readRpcs[i].destroy();
                readRpcs[i].construct(cluster, dataTable, keys[i],
                        keyLengths[i], &results[i]);
            } else {
                writeRpcs[i].destroy();
                writeRpcs[i].construct(cluster, dataTable, keys[i],
                        keyLengths[i], buffer, sizes[i]);
            }
            startTime[i] = now;
        }
        cluster->clientContext->dispatch->poll();
    }
    done:

    const char *type = "Read";
    const char *test = "readInterference";
    if (!read) {
        type = "Write";
        test = "writeInterference";
    }
    printf("# %s latencies where there are concurrent requests for\n", type);
    printf("# objects of different sizes. One request of each size is\n");
    printf("# active at a time. All times are in microseconds. \n");
    printf("# Generated by 'clusterperf.py %s'\n", test);
    printf("#\n");
    printf("#   size   kOps    avg    min    median      90%%      99%%    "
            "99.9%%      max\n");
    printf("#----------------------------------------------------------"
            "----------------\n");
    for (int i = 0; i < NUM_SIZES; i++) {
        std::sort(ticks[i].begin(), ticks[i].end());
        int count = downCast<int>(ticks[i].size());
        uint64_t sum = 0;
        for (int j = 0; j < count; j++) {
            sum += ticks[i][j];
        }
        double average = Cycles::toSeconds(sum)*1e06/count;
        printf("%8d %6.1f %6.1f %6.1f  %8.1f %8.1f %8.1f %8.1f %8.1f\n",
                sizes[i],
                static_cast<double>(ticks[i].size())/1000.0,
                average,
                Cycles::toSeconds(ticks[i][0])*1e06,
                Cycles::toSeconds(ticks[i][count/2])*1e06,
                Cycles::toSeconds(ticks[i][count - 1 - count/10])*1e06,
                Cycles::toSeconds(ticks[i][count - 1 - count/100])*1e06,
                Cycles::toSeconds(ticks[i][count - 1 - count/1000])*1e06,
                Cycles::toSeconds(ticks[i][count - 1])*1e06);
    }
#undef NUM_SIZES
}

// Issue read requests simultaneously for objects of several different
// sizes, and measure the performance for each of the different sizes.
// This benchmark measures whether large requests impact the performance
// of small ones (e.g., is there head-of-line blocking?).
void
readInterference()
{
    interferenceCommon(true);
}

// This benchmark measures the latency and server throughput for reads
// when several clients are simultaneously reading the same object.
void
readLoaded()
{
    const char* key = "123456789012345678901234567890";
    uint16_t keyLength = downCast<uint16_t>(strlen(key));

    if (clientIndex > 0) {
        // Slaves execute the following code, which creates load by
        // repeatedly reading a particular object.
        while (true) {
            char command[20];
            char doc[200];
            getCommand(command, sizeof(command));
            if (strcmp(command, "run") == 0) {
                string controlKey = keyVal(0, "doc");
                readObject(controlTable, controlKey.c_str(),
                        downCast<uint16_t>(controlKey.length()),
                        doc, sizeof(doc));
                setSlaveState("running");

                // Although the main purpose here is to generate load, we
                // also measure performance, which can be checked to ensure
                // that all clients are seeing roughly the same performance.
                // Only measure performance when the size of the object is
                // nonzero (this indicates that all clients are active)
                uint64_t start = 0;
                Buffer buffer;
                int count = 0;
                int size = 0;
                while (true) {
                    cluster->read(dataTable, key, keyLength, &buffer);
                    int currentSize = buffer.size();
                    if (currentSize != 0) {
                        if (start == 0) {
                            start = Cycles::rdtsc();
                            size = currentSize;
                        }
                        count++;
                    } else {
                        if (start != 0)
                            break;
                    }
                }
                RAMCLOUD_LOG(NOTICE, "Average latency (object size %d, "
                        "key size %u): %.1fus (%s)", size, keyLength,
                        Cycles::toSeconds(Cycles::rdtsc() - start)*1e06/count,
                        doc);
                setSlaveState("idle");
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }

    // The master executes the following code, which starts up zero or more
    // slaves to generate load, then times the performance of reading.
    int size = objectSize;
    if (size < 0)
        size = 100;
    printf("# RAMCloud read performance as a function of load (1 or more\n");
    printf("# clients all reading a single %d-byte object with %d-byte key\n"
           "# repeatedly).\n", size, keyLength);
    printf("# Generated by 'clusterperf.py readLoaded'\n");
    printf("#\n");
    printf("# numClients  readLatency(us)  throughput(total kreads/sec)\n");
    printf("#----------------------------------------------------------\n");
    for (int numSlaves = 0; numSlaves < numClients; numSlaves++) {
        char message[100];
        Buffer input, output;
        snprintf(message, sizeof(message), "%d active clients", numSlaves+1);
        string controlKey = keyVal(0, "doc");
        cluster->write(controlTable, controlKey.c_str(),
                downCast<uint16_t>(controlKey.length()),
                message);
        cluster->write(dataTable, key, keyLength, "");
        sendCommand("run", "running", 1, numSlaves);
        fillBuffer(input, size, dataTable, key, keyLength);
        cluster->write(dataTable, key, keyLength,
                input.getRange(0, size), size);
        double t = timeRead(dataTable, key, keyLength, 100, output);
        cluster->write(dataTable, key, keyLength, "");
        checkBuffer(&output, 0, size, dataTable, key, keyLength);
        printf("%5d     %10.1f          %8.0f\n", numSlaves+1, t*1e06,
                (numSlaves+1)/(1e03*t));
        sendCommand(NULL, "idle", 1, numSlaves);
    }
    sendCommand("done", "done", 1, numClients-1);
}

// Read an object that doesn't exist. This excercises some exception paths that
// are supposed to be fast. This comes up, for example, in workloads in which a
// RAMCloud is used as a cache with frequent cache misses.
void
readNotFound()
{
    if (clientIndex != 0)
        return;

    uint64_t runCycles = Cycles::fromSeconds(.1);

    // Similar to timeRead but catches the exception
    uint64_t start = Cycles::rdtsc();
    uint64_t elapsed;
    int count = 0;
    while (true) {
        for (int i = 0; i < 10; i++) {
            Buffer output;
            try {
                cluster->read(dataTable, "55", 2, &output);
            } catch (const ObjectDoesntExistException& e) {
                continue;
            }
            throw Exception(HERE, "Object exists?");
        }
        count += 10;
        elapsed = Cycles::rdtsc() - start;
        if (elapsed >= runCycles)
            break;
    }
    double t = Cycles::toSeconds(elapsed)/count;

    printTime("readNotFound", t, "read object that doesn't exist");
}

/**
 * This method contains the core of the "readRandom" test; it is
 * shared by the master and slaves.
 *
 * \param tableIds
 *      Vector of table identifiers available for the test.
 * \param docString
 *      Information provided by the master about this run; used
 *      in log messages.
 */
void readRandomCommon(std::vector<uint64_t> &tableIds, char *docString)
{
    // Duration of test.
    double ms = 100;
    uint64_t startTime = Cycles::rdtsc();
    uint64_t endTime = startTime + Cycles::fromSeconds(ms/1e03);
    uint64_t slowTicks = Cycles::fromSeconds(10e-06);
    uint64_t readStart, readEnd;
    uint64_t maxLatency = 0;
    int count = 0;
    int slowReads = 0;

    const char* key = "123456789012345678901234567890";
    uint16_t keyLength = downCast<uint16_t>(strlen(key));

    // Each iteration through this loop issues one read operation to a
    // randomly-selected table.
    while (true) {
        uint64_t tableId = tableIds.at(generateRandom() % numTables);
        readStart = Cycles::rdtsc();
        Buffer value;
        cluster->read(tableId, key, keyLength, &value);
        readEnd = Cycles::rdtsc();
        count++;
        uint64_t latency = readEnd - readStart;

        // When computing the slowest read, skip the first reads so that
        // everything has a chance to get fully warmed up.
        if ((latency > maxLatency) && (count > 100))
            maxLatency = latency;
        if (latency > slowTicks)
            slowReads++;
        if (readEnd > endTime)
            break;
    }
    double thruput = count/Cycles::toSeconds(readEnd - startTime);
    double slowPercent = 100.0 * slowReads / count;
    sendMetrics(thruput, Cycles::toSeconds(maxLatency), slowPercent);
    if (clientIndex != 0) {
        RAMCLOUD_LOG(NOTICE,
                "%s: throughput: %.1f reads/sec., max latency: %.1fus, "
                "reads > 20us: %.1f%%", docString,
                thruput, Cycles::toSeconds(maxLatency)*1e06, slowPercent);
    }
}

// In this test all of the clients repeatedly read objects from a collection
// of tables on different servers.  For each read a client chooses a table
// at random.
void
readRandom()
{
    std::vector<uint64_t> tableIds;

    if (clientIndex > 0) {
        // This is a slave: execute commands coming from the master.
        while (true) {
            char command[20];
            char doc[200];
            getCommand(command, sizeof(command));
            if (strcmp(command, "run") == 0) {
                if (tableIds.empty()) {
                    // Open all the tables.
                    for (int i = 0; i < numTables; i++) {
                        char tableName[20];
                        snprintf(tableName, sizeof(tableName), "table%d", i);
                        tableIds.emplace_back(cluster->getTableId(tableName));
                    }
                }
                string controlKey = keyVal(0, "doc");
                readObject(controlTable, controlKey.c_str(),
                        downCast<uint16_t>(controlKey.length()), doc,
                        sizeof(doc));
                setSlaveState("running");
                readRandomCommon(tableIds, doc);
                setSlaveState("idle");
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }

    // This is the master: first, create the tables.
    int size = objectSize;
    if (size < 0)
        size = 100;
    const char* key = "123456789012345678901234567890";
    uint16_t keyLength = downCast<uint16_t>(strlen(key));

    tableIds.clear();
    tableIds.resize(numTables);
    createTables(tableIds, size, key, keyLength);

    // Vary the number of clients and repeat the test for each number.
    printf("# RAMCloud read performance when 1 or more clients read\n");
    printf("# %d-byte objects with %u-byte keys chosen at random from\n"
           "# %d servers.\n", size, keyLength, numTables);
    printf("# Generated by 'clusterperf.py readRandom'\n");
    printf("#\n");
    printf("# numClients  throughput(total kreads/sec)  slowest(ms)  "
                "reads > 10us\n");
    printf("#--------------------------------------------------------"
                "------------\n");
    fflush(stdout);
    for (int numActive = 1; numActive <= numClients; numActive++) {
        char doc[100];
        snprintf(doc, sizeof(doc), "%d active clients", numActive);
        string key = keyVal(0, "doc");
        cluster->write(controlTable, key.c_str(),
                downCast<uint16_t>(key.length()), doc);
        sendCommand("run", "running", 1, numActive-1);
        readRandomCommon(tableIds, doc);
        sendCommand(NULL, "idle", 1, numActive-1);
        ClientMetrics metrics;
        getMetrics(metrics, numActive);
        printf("%3d               %6.0f                    %6.2f"
                "          %.1f%%\n",
                numActive, sum(metrics[0])/1e03, max(metrics[1])*1e03,
                sum(metrics[2])/numActive);
        fflush(stdout);
    }
    sendCommand("done", "done", 1, numClients-1);
}

/**
 * This method implements the client-0 (master) functionality for both
 * readThroughput and multiReadThroughput.
 * \param numObjects
 *      Number of objects to create in the table.
 * \param size
 *      Size of each object, in bytes.
 * \param keyLength
 *      Size of keys, in bytes.
 */
void
readThroughputMaster(int numObjects, int size, uint16_t keyLength)
{
    // This is the master client. Fill in the table, then measure
    // throughput while gradually increasing the number of workers.
    printf("#\n");
    printf("# numClients   throughput     worker     dispatch\n");
    printf("#             (kreads/sec)    utiliz.     utiliz.\n");
    printf("#------------------------------------------------\n");
    fillTable(dataTable, numObjects, keyLength, size);
    string stats[5];
    for (int numSlaves = 1; numSlaves < numClients; numSlaves++) {
        sendCommand("run", "running", numSlaves, 1);
        Buffer beforeStats;
        cluster->serverControlAll(WireFormat::ControlOp::GET_PERF_STATS,
                NULL, 0, &beforeStats);
        Buffer statsBuffer;
        cluster->objectServerControl(dataTable, "abc", 3,
                WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
                &statsBuffer);
        PerfStats startStats = *statsBuffer.getStart<PerfStats>();
        Cycles::sleep(1000000);
        cluster->objectServerControl(dataTable, "abc", 3,
                WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
                &statsBuffer);
        Buffer afterStats;
        cluster->serverControlAll(WireFormat::ControlOp::GET_PERF_STATS,
                NULL, 0, &afterStats);
        if (numSlaves <= 5) {
            stats[numSlaves-1] = PerfStats::printClusterStats(&beforeStats,
                    &afterStats).c_str();
        }
        PerfStats finishStats = *statsBuffer.getStart<PerfStats>();
        double elapsedTime = static_cast<double>(finishStats.collectionTime -
                startStats.collectionTime)/ finishStats.cyclesPerSecond;
        double rate = static_cast<double>(finishStats.readCount -
                startStats.readCount) / elapsedTime;
        double utilization = static_cast<double>(
                finishStats.workerActiveCycles -
                startStats.workerActiveCycles) / static_cast<double>(
                finishStats.collectionTime - startStats.collectionTime);
        double dispatchUtilization = static_cast<double>(
                finishStats.dispatchActiveCycles -
                startStats.dispatchActiveCycles) / static_cast<double>(
                finishStats.collectionTime - startStats.collectionTime);
        printf("%5d         %8.0f      %8.3f   %8.3f\n",
                numSlaves, rate/1e03, utilization, dispatchUtilization);
    }
    sendCommand("done", "done", 1, numClients-1);
#if 0
    for (int i = 0; i < 5; i++) {
        printf("\nStats for %d clients:\n%s", i+1, stats[i].c_str());
    }
#endif
}

// This benchmark measures total throughput of a single server (in objects
// read per second) under a workload consisting of individual random object
// reads.
void
readThroughput()
{
    const uint16_t keyLength = 30;
    int size = objectSize;
    if (size < 0)
        size = 100;
    const int numObjects = 40000000/size;
    if (clientIndex == 0) {
        // This is the master client.
        printf("# RAMCloud read throughput of a single server with a varying\n"
                "# number of clients issuing individual reads on randomly\n"
                "# chosen %d-byte objects with %d-byte keys\n",
                size, keyLength);
        printf("# Generated by 'clusterperf.py readThroughput'\n");
        readThroughputMaster(numObjects, size, keyLength);
    } else {
        // Slaves execute the following code, which creates load by
        // issuing individual reads.
        bool running = false;

        uint64_t startTime;
        int objectsRead;

        while (true) {
            char command[20];
            if (running) {
                // Write out some statistics for debugging.
                double totalTime = Cycles::toSeconds(Cycles::rdtsc()
                        - startTime);
                double rate = objectsRead/totalTime;
                RAMCLOUD_LOG(NOTICE, "Read rate: %.1f kobjects/sec",
                        rate/1e03);
            }
            getCommand(command, sizeof(command), false);
            if (strcmp(command, "run") == 0) {
                if (!running) {
                    setSlaveState("running");
                    running = true;
                    RAMCLOUD_LOG(NOTICE,
                            "Starting readThroughput benchmark");
                }

                // Perform reads for a second (then check to see
                // if the experiment is over).
                startTime = Cycles::rdtsc();
                objectsRead = 0;
                uint64_t checkTime = startTime + Cycles::fromSeconds(1.0);
                do {
                    char key[keyLength];
                    Buffer value;
                    makeKey(downCast<int>(generateRandom() % numObjects),
                            keyLength, key);
                    cluster->read(dataTable, key, keyLength, &value);
                    ++objectsRead;
                } while (Cycles::rdtsc() < checkTime);
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                RAMCLOUD_LOG(NOTICE, "Ending readThroughput benchmark");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }
}

// Read times for objects with string keys of different lengths.
void
readVaryingKeyLength()
{
    if (clientIndex != 0)
        return;
    Buffer input, output;
    uint16_t keyLengths[] = {
         1, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
         55, 60, 65, 70, 75, 80, 85, 90, 95, 100,
         200, 300, 400, 500, 600, 700, 800, 900, 1000,
         2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
         20000, 30000, 40000, 50000, 60000
    };
    int dataLength = objectSize;

    printf("# RAMCloud read performance for %u B objects\n", dataLength);
    printf("# with keys of various lengths.\n");
    printf("# Generated by 'clusterperf.py readVaryingKeyLength'\n#\n");
    printf("# Key Length      Latency (us)     Bandwidth (MB/s)\n");
    printf("#--------------------------------------------------------"
            "--------------------\n");

    foreach (uint16_t keyLength, keyLengths) {
        char key[keyLength];
        Util::genRandomString(key, keyLength);

        fillBuffer(input, dataLength, dataTable, key, keyLength);
        cluster->write(dataTable, key, keyLength,
                input.getRange(0, dataLength), dataLength);
        double t = readObject(dataTable, key, keyLength, 1000, 1.0, output).p50;
        checkBuffer(&output, 0, dataLength, dataTable, key, keyLength);

        printf("%12u %16.1f %19.1f\n", keyLength, 1e06*t,
               ((keyLength + dataLength) / t)/(1024.0*1024.0));
    }
}

// Write times for objects with string keys of different lengths.
void
writeVaryingKeyLength()
{
    if (clientIndex != 0)
        return;
    Buffer input, output;
    uint16_t keyLengths[] = {
         1, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50,
         55, 60, 65, 70, 75, 80, 85, 90, 95, 100,
         200, 300, 400, 500, 600, 700, 800, 900, 1000,
         2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000,
         20000, 30000, 40000, 50000, 60000
    };
    int dataLength = objectSize;

    printf("# RAMCloud write performance for %u B objects\n", dataLength);
    printf("# with keys of various lengths.\n");
    printf("# Generated by 'clusterperf.py writeVaryingKeyLength'\n#\n");
    printf("# Key Length      Latency (us)     Bandwidth (MB/s)\n");
    printf("#--------------------------------------------------------"
            "--------------------\n");

    foreach (uint16_t keyLength, keyLengths) {
        char key[keyLength];
        Util::genRandomString(key, keyLength);

        fillBuffer(input, dataLength, dataTable, key, keyLength);
        cluster->write(dataTable, key, keyLength,
                input.getRange(0, dataLength), dataLength);
        TimeDist dist = writeObject(dataTable, key, keyLength,
                input.getRange(0, dataLength), dataLength, 1000, 1.0);
        Buffer output;
        cluster->read(dataTable, key, keyLength, &output);
        checkBuffer(&output, 0, dataLength, dataTable, key, keyLength);

        printf("%12u %16.1f %19.1f\n", keyLength, 1e06*dist.p50,
               ((keyLength+dataLength) / dist.p50)/(1024.0*1024.0));
    }
}

// This benchmark measures the latency and server throughput for write
// when some data is written asynchronously and then some smaller value
// is written synchronously.
void
writeAsyncSync()
{
    if (clientIndex > 0)
        return;

    const uint32_t count = 100;
    const uint32_t syncObjectSize = 100;
    const uint32_t asyncObjectSizes[] = { 100, 1000, 10000, 100000, 1000000 };
    const uint32_t arrayElts = static_cast<uint32_t>
                               (sizeof32(asyncObjectSizes) /
                               sizeof32(asyncObjectSizes[0]));

    uint32_t maxSize = syncObjectSize;
    for (uint32_t j = 0; j < arrayElts; ++j)
        maxSize = std::max(maxSize, asyncObjectSizes[j]);

    std::vector<char> blankChars(maxSize);

    const char* key = "123456789012345678901234567890";
    uint16_t keyLength = downCast<uint16_t>(strlen(key));

    // prime
    cluster->write(dataTable, key, keyLength, &blankChars[0], syncObjectSize);
    cluster->write(dataTable, key, keyLength, &blankChars[0], syncObjectSize);

    printf("# Gauges impact of asynchronous writes on synchronous writes.\n"
           "# Write two values. The size of the first varies over trials\n"
           "# (its size is given as 'firstObjectSize'). The first write is\n"
           "# either synchronous (if firstWriteIsSync is 1) or asynchronous\n"
           "# (if firstWriteIsSync is 0). The response time of the first\n"
           "# write is given by 'firstWriteLatency'. The second write is\n"
           "# a %u B object which is always written synchronously (its \n"
           "# response time is given by 'syncWriteLatency'\n"
           "# Both writes use a %u B key.\n"
           "# Generated by 'clusterperf.py writeAsyncSync'\n#\n"
           "# firstWriteIsSync firstObjectSize firstWriteLatency(us) "
               "syncWriteLatency(us)\n"
           "#--------------------------------------------------------"
               "--------------------\n",
           syncObjectSize, keyLength);
    for (int sync = 0; sync < 2; ++sync) {
        for (uint32_t j = 0; j < arrayElts; ++j) {
            const uint32_t asyncObjectSize = asyncObjectSizes[j];
            uint64_t asyncTicks = 0;
            uint64_t syncTicks = 0;
            for (uint32_t i = 0; i < count; ++i) {
                {
                    CycleCounter<> _(&asyncTicks);
                    cluster->write(dataTable, key, keyLength, &blankChars[0],
                                   asyncObjectSize, NULL, NULL, !sync);
                }
                {
                    CycleCounter<> _(&syncTicks);
                    cluster->write(dataTable, key, keyLength, &blankChars[0],
                                   syncObjectSize);
                }
            }
            printf("%18d %15u %21.1f %20.1f\n", sync, asyncObjectSize,
                   Cycles::toSeconds(asyncTicks) * 1e6 / count,
                   Cycles::toSeconds(syncTicks) * 1e6 / count);
        }
    }
}

// Write or overwrite randomly-chosen objects from a large table (so that there
// will be cache misses on the hash table and the object) and compute a
// cumulative distribution of write times.
void
writeDistRandom()
{
    int numKeys = 2000000;
    if (clientIndex != 0)
        return;

    const uint16_t keyLength = 30;

    char key[keyLength];
    char value[objectSize];

    fillTable(dataTable, numKeys, keyLength, objectSize);

    // The following variable is used to stop the test after 10 seconds
    // if we haven't read count keys by then.
    uint64_t stop = Cycles::rdtsc() + Cycles::fromSeconds(10.0);

    // Issue the writes back-to-back, and save the times.
    std::vector<uint64_t> ticks;
    ticks.resize(count);
    for (int i = 0; i < count; i++) {
        // We generate the random number separately to avoid timing potential
        // cache misses on the client side.
        makeKey(downCast<int>(generateRandom() % numKeys), keyLength, key);
        Util::genRandomString(value, objectSize);
        // Do the benchmark
        uint64_t start = Cycles::rdtsc();
        cluster->write(dataTable, key, keyLength, value, objectSize);
        uint64_t now = Cycles::rdtsc();
        ticks.at(i) = now - start;
        if (now >= stop) {
            count = i+1;
            LOG(NOTICE, "Time exceeded: stopping test after %d measurements",
                    count);
            break;
        }
    }

    // Output the times (several comma-separated values on each line).
    Logger::get().sync();
    int valuesInLine = 0;
    for (int i = 0; i < count; i++) {
        if (valuesInLine >= 10) {
            valuesInLine = 0;
            printf("\n");
        }
        if (valuesInLine != 0) {
            printf(",");
        }
        double micros = Cycles::toSeconds(ticks.at(i))*1.0e06;
        printf("%.2f", micros);
        valuesInLine++;
    }
    printf("\n");
}

// Perform the specified workload and measure the write latencies.
void
writeDistWorkload()
{
    doWorkload(WRITE_TYPE);
}

// Issue write requests simultaneously for objects of several different
// sizes, and measure the performance for each of the different sizes.
// This benchmark measures whether large requests impact the performance
// of small ones (e.g., is there head-of-line blocking?).
void
writeInterference()
{
    interferenceCommon(false);
}

/**
 * This method implements the client-0 (master) functionality for
 * linearizableWriteThroughput.
 * \param numObjects
 *      Number of objects to create in the table.
 * \param size
 *      Size of each object, in bytes.
 * \param keyLength
 *      Size of keys, in bytes.
 */
void
writeThroughputMaster(int numObjects, int size, uint16_t keyLength)
{
    // This is the master client. Fill in the table, then measure
    // throughput while gradually increasing the number of workers.
    printf("#\n");
    printf("# clients   throughput   worker   cleaner  compactor  "
            "cleaner  dispatch  netOut    netIn   replic    sync\n");
    printf("#           (kops/sec)   cores     cores    free %%    "
            "free %%   utiliz.   (MB/s)    (MB/s)   eff.     frac\n");
    printf("#------------------------------------------------------"
            "------------------------------------------------------\n");
    fillTable(dataTable, numObjects, keyLength, size);
    Cycles::sleep(2000000);
    string stats[5];
    for (int numSlaves = 1; numSlaves < numClients; numSlaves++) {
        sendCommand("run", "running", numSlaves, 1);
        Buffer statsBuffer;
        Buffer beforeStats;
        cluster->serverControlAll(WireFormat::ControlOp::GET_PERF_STATS,
                NULL, 0, &beforeStats);
        cluster->objectServerControl(dataTable, "abc", 3,
                WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
                &statsBuffer);
        PerfStats startStats = *statsBuffer.getStart<PerfStats>();
        Cycles::sleep(1000000);
        cluster->objectServerControl(dataTable, "abc", 3,
                WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
                &statsBuffer);
        Buffer afterStats;
        cluster->serverControlAll(WireFormat::ControlOp::GET_PERF_STATS,
                NULL, 0, &afterStats);
        if (numSlaves <= 5) {
            stats[numSlaves-1] = PerfStats::printClusterStats(&beforeStats,
                    &afterStats).c_str();
        }
        PerfStats finishStats = *statsBuffer.getStart<PerfStats>();
        double elapsedCycles = static_cast<double>(
                finishStats.collectionTime - startStats.collectionTime);
        double elapsedTime = elapsedCycles/ finishStats.cyclesPerSecond;
        double rate = static_cast<double>(finishStats.writeCount -
                startStats.writeCount) / elapsedTime;
        double utilization = static_cast<double>(
                finishStats.workerActiveCycles -
                startStats.workerActiveCycles) / elapsedCycles;
        double cleanerUtilization = static_cast<double>(
                (finishStats.compactorActiveCycles +
                finishStats.cleanerActiveCycles) -
                (startStats.compactorActiveCycles +
                startStats.cleanerActiveCycles)) / elapsedCycles;
        double compactorFreePct =
                static_cast<double>(finishStats.compactorInputBytes -
                startStats.compactorInputBytes) -
                static_cast<double>(finishStats.compactorSurvivorBytes -
                startStats.compactorSurvivorBytes);
        if (compactorFreePct != 0) {
            compactorFreePct = 100.0 * compactorFreePct /
                    static_cast<double>(finishStats.compactorInputBytes -
                    startStats.compactorInputBytes);
        }
        double cleanerFreePct =
                static_cast<double>(finishStats.cleanerInputDiskBytes -
                startStats.cleanerInputDiskBytes) -
                static_cast<double>(finishStats.cleanerSurvivorBytes -
                startStats.cleanerSurvivorBytes);
        if (cleanerFreePct != 0) {
            cleanerFreePct = 100.0 * cleanerFreePct /
                    static_cast<double>(finishStats.cleanerInputDiskBytes -
                    startStats.cleanerInputDiskBytes);
        }
        double dispatchUtilization = static_cast<double>(
                finishStats.dispatchActiveCycles -
                startStats.dispatchActiveCycles) / elapsedCycles;
        double netOutRate = static_cast<double>(
                finishStats.networkOutputBytes -
                startStats.networkOutputBytes) / elapsedTime;
        double netInRate = static_cast<double>(
                finishStats.networkInputBytes -
                startStats.networkInputBytes) / elapsedTime;
        double repEfficiency = static_cast<double>(finishStats.writeCount -
                startStats.writeCount)/static_cast<double>(
                finishStats.replicationRpcs - startStats.replicationRpcs);
        double syncFraction = static_cast<double>(finishStats.logSyncCycles -
                startStats.logSyncCycles) / elapsedCycles;
        printf("%5d       %8.2f   %8.3f %8.3f %8.1f  %8.1f  %8.3f "
                "%8.2f  %8.2f %7.2f  %7.2f\n",
                numSlaves, rate/1e03, utilization, cleanerUtilization,
                compactorFreePct, cleanerFreePct, dispatchUtilization,
                netOutRate/1e06, netInRate/1e06, repEfficiency, syncFraction);
    }
    sendCommand("done", "done", 1, numClients-1);
#if 0
    for (int i = 0; i < 5; i++) {
        printf("\nStats for %d clients:\n%s", i+1, stats[i].c_str());
    }
#endif
}

// This benchmark measures total throughput of a single server (in objects
// writes per second) under a workload consisting of individual random object
// write.
void
writeThroughput()
{
    const uint16_t keyLength = 30;
    int size = objectSize;
    if (size < 0)
        size = 100;
    const int numObjects = 400000000/objectSize;
    if (clientIndex == 0) {
        // This is the master client.
        printf("# RAMCloud write throughput of a single server with a varying\n"
                "# number of clients issuing individual write on randomly\n"
                "# chosen %d-byte objects with %d-byte keys\n",
                size, keyLength);
        printf("# Generated by 'clusterperf.py writeThroughput'\n");
        writeThroughputMaster(numObjects, size, keyLength);
    } else {
        // Slaves execute the following code, which creates load by
        // issuing individual reads.
        bool running = false;

        uint64_t startTime;
        int objectsWritten;

        while (true) {
            char command[20];
            if (running) {
                // Write out some statistics for debugging.
                double totalTime = Cycles::toSeconds(Cycles::rdtsc()
                        - startTime);
                double rate = objectsWritten/totalTime;
                RAMCLOUD_LOG(NOTICE, "Write rate: %.1f kobjects/sec",
                        rate/1e03);
            }
            getCommand(command, sizeof(command), false);
            if (strcmp(command, "run") == 0) {
                if (!running) {
                    setSlaveState("running");
                    running = true;
                    RAMCLOUD_LOG(NOTICE,
                            "Starting writeThroughput benchmark");
                }

                // Perform reads for a second (then check to see
                // if the experiment is over).
                startTime = Cycles::rdtsc();
                objectsWritten = 0;
                uint64_t checkTime = startTime + Cycles::fromSeconds(1.0);
                do {
                    char key[keyLength];
                    char value[objectSize];
                    makeKey(downCast<int>(generateRandom() % numObjects),
                            keyLength, key);
                    Util::genRandomString(value, objectSize);
                    cluster->write(dataTable, key, keyLength, value, objectSize,
                            NULL, NULL, false);
                    ++objectsWritten;
                } while (Cycles::rdtsc() < checkTime);
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                RAMCLOUD_LOG(NOTICE,
                             "Ending writeThroughput benchmark");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }
}

// This benchmark measures total throughput of a single server (in operations
// per second) under a specified workload.
void
workloadThroughput()
{
    if (clientIndex == 0) {
        // This is the master client.

        printf("# RAMCloud %s throughput of a single server with a varying\n"
                "# number of clients running the workload.\n",
                workload.c_str());
        printf("# Generated by 'clusterperf.py workloadThroughput'\n");
        // This is the master client. Fill in the table, then measure
        // throughput while gradually increasing the number of workers.
        printf("#\n");
        printf("# numClients   throughput     worker utiliz.\n");
        printf("#              (kops)\n");
        printf("#-------------------------------------------\n");

        sendCommand("setup", "ready", 1, numClients-1);
        for (int numSlaves = 1; numSlaves < numClients; numSlaves++) {
            sendCommand("run", "running", numSlaves, 1);

            Buffer statsBuffer;
            cluster->objectServerControl(dataTable, "abc", 3,
                    WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
                    &statsBuffer);
            PerfStats startStats = *statsBuffer.getStart<PerfStats>();
            Cycles::sleep(1000000);
            cluster->objectServerControl(dataTable, "abc", 3,
                    WireFormat::ControlOp::GET_PERF_STATS, NULL, 0,
                    &statsBuffer);
            PerfStats finishStats = *statsBuffer.getStart<PerfStats>();
            double elapsedTime = static_cast<double>(
                    finishStats.collectionTime -
                    startStats.collectionTime)/ finishStats.cyclesPerSecond;
            double rate = static_cast<double>(finishStats.readCount +
                    finishStats.writeCount -
                    startStats.readCount -
                    startStats.writeCount) / elapsedTime;
            double utilization = static_cast<double>(
                    finishStats.workerActiveCycles -
                    startStats.workerActiveCycles) / static_cast<double>(
                    finishStats.collectionTime - startStats.collectionTime);
            printf("%5d         %8.0f        %8.3f\n", numSlaves, rate/1e03,
                    utilization);
        }
        cluster->dropTable("data");
        sendCommand("done", "done", 1, numClients-1);
    } else {
        // Slaves execute the following code, which creates load by
        // issuing individual workload requests.
        WorkloadGenerator loadGenerator(workload);
        while (true) {
            char command[20];
            getCommand(command, sizeof(command));
            if (strcmp(command, "setup") == 0) {
                if (clientIndex == 1) {
                    // Setup need only be performed by one slave.
                    loadGenerator.setup();
                    setSlaveState("ready");
                } else {
                    // All other slaves should wait for setup to finish.
                    sendCommand(NULL, "ready", 1, 1);
                    setSlaveState("ready");
                }
            } else if (strcmp(command, "run") == 0) {
                // Slaves will run the specified workload until the "data" table
                // is dropped.
                setSlaveState("running");
                try
                {
                    loadGenerator.run(static_cast<uint64_t>(targetOps));
                }
                catch (TableDoesntExistException &e)
                {}
                setSlaveState("idle");
            } else if (strcmp(command, "done") == 0) {
                setSlaveState("done");
                return;
            } else {
                RAMCLOUD_LOG(ERROR, "unknown command %s", command);
                return;
            }
        }
    }
}

// The following struct and table define each performance test in terms of
// a string name and a function that implements the test.
struct TestInfo {
    const char* name;             // Name of the performance test; this is
                                  // what gets typed on the command line to
                                  // run the test.
    void (*func)();               // Function that implements the test.
};

TestInfo tests[] = {
    {"basic", basic},
    {"broadcast", broadcast},
    {"indexBasic", indexBasic},
    {"indexRange", indexRange},
    {"indexMultiple", indexMultiple},
    {"indexScalability", indexScalability},
    {"indexWriteDist", indexWriteDist},
    {"indexReadDist", indexReadDist},
    {"transaction_oneMaster", transaction_oneMaster},
    {"transaction_collision", transaction_collision},
    {"transactionContention", transactionContention},
    {"transactionDistRandom", transactionDistRandom},
    {"transactionThroughput", transactionThroughput},
    {"multiWrite_oneMaster", multiWrite_oneMaster},
    {"multiRead_oneMaster", multiRead_oneMaster},
    {"multiRead_oneObjectPerMaster", multiRead_oneObjectPerMaster},
    {"multiRead_general", multiRead_general},
    {"multiRead_generalRandom", multiRead_generalRandom},
    {"multiReadThroughput", multiReadThroughput},
    {"netBandwidth", netBandwidth},
    {"readAllToAll", readAllToAll},
    {"readDist", readDist},
    {"readDistRandom", readDistRandom},
    {"readDistWorkload", readDistWorkload},
    {"readInterference", readInterference},
    {"readLoaded", readLoaded},
    {"readNotFound", readNotFound},
    {"readRandom", readRandom},
    {"readThroughput", readThroughput},
    {"readVaryingKeyLength", readVaryingKeyLength},
    {"writeVaryingKeyLength", writeVaryingKeyLength},
    {"writeAsyncSync", writeAsyncSync},
    {"writeDistRandom", writeDistRandom},
    {"writeDistWorkload", writeDistWorkload},
    {"writeInterference", writeInterference},
    {"writeThroughput", writeThroughput},
    {"workloadThroughput", workloadThroughput},
};

int
main(int argc, char *argv[])
try
{
    // Parse command-line options.
    vector<string> testNames;
    string coordinatorLocator, logFile;
    string logLevel("NOTICE");
    po::options_description desc(
            "Usage: ClusterPerf [options] testName testName ...\n\n"
            "Runs one or more benchmarks on a RAMCloud cluster and outputs\n"
            "performance information.  This program is not normally invoked\n"
            "directly; it is invoked by the clusterperf script.\n\n"
            "Allowed options:");
    desc.add_options()
        ("clientIndex", po::value<int>(&clientIndex)->default_value(0),
                "Index of this client (first client is 0)")
        ("coordinator,C", po::value<string>(&coordinatorLocator),
                "Service locator for the cluster coordinator (required)")
        ("count,c", po::value<int>(&count)->default_value(100000),
                "Number of times to invoke operation for test")
        ("logFile", po::value<string>(&logFile),
                "Redirect all output to this file")
        ("logLevel,l", po::value<string>(&logLevel)->default_value("NOTICE"),
                "Print log messages only at this severity level or higher "
                "(ERROR, WARNING, NOTICE, DEBUG)")
        ("help,h", "Print this help message")
        ("numClients", po::value<int>(&numClients)->default_value(1),
                "Total number of clients running")
        ("numVClients", po::value<int>(&numVClients)->default_value(1),
                "Total number of virtual clients to simulate pre client")
        ("size,s", po::value<int>(&objectSize)->default_value(100),
                "Size of objects (in bytes) to use for test")
        ("numObjects", po::value<int>(&numObjects)->default_value(1),
                "Number of object per operation to use for test")
        ("numTables", po::value<int>(&numTables)->default_value(10),
                "Number of tables to use for test")
        ("testName", po::value<vector<string>>(&testNames),
                "Name(s) of test(s) to run")
        ("warmup", po::value<int>(&warmupCount)->default_value(100),
                "Number of times to invoke operation before beginning "
                "measurements")
        ("workload", po::value<string>(&workload)->default_value("YCSB-A"),
                "Workload of additional load generating clients"
                "(YCSB-A, YCSB-B, YCSB-C)")
        ("targetOps", po::value<int>(&targetOps)->default_value(0),
                "Operations per second that each load generating client"
                "will try to achieve (0 means run as fast as possible)")
        ("txSpan", po::value<int>(&txSpan)->default_value(1),
                "Number of servers that each transaction should span")
        ("numIndexlet", po::value<int>(&numIndexlet)->default_value(1),
                "number of Indexlets")
        ("numIndexes", po::value<int>(&numIndexes)->default_value(1),
                "number of secondary keys per object");
    po::positional_options_description desc2;
    desc2.add("testName", -1);
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
            options(desc).positional(desc2).run(), vm);
    po::notify(vm);
    if (logFile.size() != 0) {
        // Redirect both stdout and stderr to the log file.
        FILE* f = fopen(logFile.c_str(), "w");
        if (f == NULL) {
            RAMCLOUD_LOG(ERROR, "couldn't open log file '%s': %s",
                    logFile.c_str(), strerror(errno));
            exit(1);
        }
        stdout = stderr = f;
        Logger::get().setLogFile(fileno(f));
    }
    Logger::get().setLogLevels(logLevel);
    if (vm.count("help")) {
        std::cout << desc << '\n';
        exit(0);
    }
    if (coordinatorLocator.empty()) {
        RAMCLOUD_LOG(ERROR, "missing required option --coordinator");
        exit(1);
    }

    Context realContext;
    context = &realContext;
    RamCloud r(&realContext, coordinatorLocator.c_str());
    cluster = &r;
    cluster->createTable("data");
    dataTable = cluster->getTableId("data");
    cluster->createTable("control");
    controlTable = cluster->getTableId("control");

    if (testNames.size() == 0) {
        // No test names specified; run all tests.
        foreach (TestInfo& info, tests) {
            info.func();
        }
    } else {
        // Run only the tests that were specified on the command line.
        foreach (string& name, testNames) {
            bool foundTest = false;
            foreach (TestInfo& info, tests) {
                if (name.compare(info.name) == 0) {
                    foundTest = true;
                    info.func();
                    break;
                }
            }
            if (!foundTest) {
                printf("No test named '%s'\n", name.c_str());
            }
        }
    }

    // Flush printout of all data before timetrace gets dumped.
    fflush(stdout);

    if (clientIndex == 0) {
        cluster->serverControlAll(WireFormat::LOG_TIME_TRACE);
        cluster->serverControlAll(WireFormat::LOG_CACHE_TRACE);
        cluster->serverControlAll(WireFormat::LOG_BASIC_TRANSPORT_ISSUES);
    }
    BasicTransport::logIssueStats();
    TimeTrace::printToLog();
}
catch (std::exception& e) {
    RAMCLOUD_LOG(ERROR, "%s", e.what());
    exit(1);
}
