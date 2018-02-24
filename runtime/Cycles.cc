/* Copyright (c) 2011-2017 Stanford University
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

#include <errno.h>
#include <sys/time.h>
#include <string>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


#include "Cycles.h"
#include "Initialize.h"
#include "Util.h"

namespace PerfUtils {

double Cycles::cyclesPerSec = 0;
uint64_t Cycles::mockTscValue = 0;
double Cycles::mockCyclesPerSec = 0;
static Initialize _(Cycles::init);

/**
 * Perform once-only overall initialization for the Cycles class, such
 * as calibrating the clock frequency.  This method is invoked automatically
 * during initialization, but it may be invoked explicitly by other modules
 * to ensure that initialization occurs before those modules initialize
 * themselves.
 */
void
Cycles::init() {
    if (cyclesPerSec != 0)
        return;

    // Compute the frequency of the fine-grained CPU timer: to do this,
    // take parallel time readings using both rdtsc and gettimeofday.
    // After 10ms have elapsed, take the ratio between these readings.

    struct timeval startTime, stopTime;
    uint64_t startCycles, stopCycles, micros;
    double oldCycles;

    // There is one tricky aspect, which is that we could get interrupted
    // between calling gettimeofday and reading the cycle counter, in which
    // case we won't have corresponding readings.  To handle this (unlikely)
    // case, compute the overall result repeatedly, and wait until we get
    // two successive calculations that are within 0.001% of each other (or
    // in other words, a drift of up to 10µs per second).
    oldCycles = 0;
    while (1) {
        if (gettimeofday(&startTime, NULL) != 0) {
            PERFUTILS_DIE("Cycles::init couldn't read clock: %s", strerror(errno));
        }
        startCycles = rdtsc();
        while (1) {
            if (gettimeofday(&stopTime, NULL) != 0) {
                PERFUTILS_DIE("Cycles::init couldn't read clock: %s",
                        strerror(errno));
            }
            stopCycles = rdtsc();
            micros = (stopTime.tv_usec - startTime.tv_usec) +
                    (stopTime.tv_sec - startTime.tv_sec)*1000000;
            if (micros > 10000) {
                cyclesPerSec = static_cast<double>(stopCycles - startCycles);
                cyclesPerSec = 1000000.0*cyclesPerSec/
                        static_cast<double>(micros);
                break;
            }
        }
        double delta = cyclesPerSec/100000.0;
        if ((oldCycles > (cyclesPerSec - delta)) &&
                (oldCycles < (cyclesPerSec + delta))) {
            goto exit;
        }
        oldCycles = cyclesPerSec;
    }

    exit:
        ;
    //printf("Cycles per second: %f\n", cyclesPerSec);
}

/**
 * Return the number of CPU cycles per second.
double
Cycles::perSecond()
{
    return getCyclesPerSec();
}
 */

/**
 * Given an elapsed time measured in cycles, return a floating-point number
 * giving the corresponding time in seconds.
 * \param cycles
 *      Difference between the results of two calls to rdtsc.
 * \param cyclesPerSec
 *      Optional parameter to specify the frequency of the counter that #cycles
 *      was taken from. Useful when converting a remote machine's tick counter
 *      to seconds. The default value of 0 will use the local processor's
 *      computed counter frequency.
 * \return
 *      The time in seconds corresponding to cycles.
 */
double
Cycles::toSeconds(int64_t cycles, double cyclesPerSec)
{
    if (cyclesPerSec == 0)
        cyclesPerSec = getCyclesPerSec();
    return static_cast<double>(cycles)/cyclesPerSec;
}

/**
 * Given a time in seconds, return the number of cycles that it
 * corresponds to.
 * \param seconds
 *      Time in seconds.
 * \param cyclesPerSec
 *      Optional parameter to specify the frequency of the counter that #cycles
 *      was taken from. Useful when converting a remote machine's tick counter
 *      to seconds. The default value of 0 will use the local processor's
 *      computed counter frequency.
 * \return
 *      The approximate number of cycles corresponding to #seconds.
 */
uint64_t
Cycles::fromSeconds(double seconds, double cyclesPerSec)
{
    if (cyclesPerSec == 0)
        cyclesPerSec = getCyclesPerSec();
    return (uint64_t) (seconds*cyclesPerSec + 0.5);
}

/**
 * Given an elapsed time measured in cycles, return an integer
 * giving the corresponding time in microseconds. Note: toSeconds()
 * is faster than this method.
 * \param cycles
 *      Difference between the results of two calls to rdtsc.
 * \param cyclesPerSec
 *      Optional parameter to specify the frequency of the counter that #cycles
 *      was taken from. Useful when converting a remote machine's tick counter
 *      to seconds. The default value of 0 will use the local processor's
 *      computed counter frequency.
 * \return
 *      The time in microseconds corresponding to cycles (rounded).
 */
uint64_t
Cycles::toMicroseconds(uint64_t cycles, double cyclesPerSec)
{
    return toNanoseconds(cycles, cyclesPerSec) / 1000;
}

/**
 * Given an elapsed time measured in cycles, return an integer
 * giving the corresponding time in nanoseconds. Note: toSeconds()
 * is faster than this method.
 * \param cycles
 *      Difference between the results of two calls to rdtsc.
 * \param cyclesPerSec
 *      Optional parameter to specify the frequency of the counter that #cycles
 *      was taken from. Useful when converting a remote machine's tick counter
 *      to seconds. The default value of 0 will use the local processor's
 *      computed counter frequency.
 * \return
 *      The time in nanoseconds corresponding to cycles (rounded).
 */
uint64_t
Cycles::toNanoseconds(uint64_t cycles, double cyclesPerSec)
{
    if (cyclesPerSec == 0)
        cyclesPerSec = getCyclesPerSec();
    return (uint64_t) (1e09*static_cast<double>(cycles)/cyclesPerSec + 0.5);
}

/**
 * Given a number of nanoseconds, return an approximate number of
 * cycles for an equivalent time length.
 * \param ns
 *      Number of nanoseconds.
 * \param cyclesPerSec
 *      Optional parameter to specify the frequency of the counter that #cycles
 *      was taken from. Useful when converting a remote machine's tick counter
 *      to seconds. The default value of 0 will use the local processor's
 *      computed counter frequency.
 * \return
 *      The approximate number of cycles for the same time length.
 */
uint64_t
Cycles::fromNanoseconds(uint64_t ns, double cyclesPerSec)
{
    if (cyclesPerSec == 0)
        cyclesPerSec = getCyclesPerSec();
    return (uint64_t) (static_cast<double>(ns)*cyclesPerSec/1e09 + 0.5);
}

/**
 * Busy wait for a given number of microseconds.
 * Callers should use this method in most reasonable cases as opposed to
 * usleep for accurate measurements. Calling usleep may put the the processor
 * in a low power mode/sleep state which reduces the clock frequency.
 * So, each time the process/thread wakes up from usleep, it takes some time
 * to ramp up to maximum frequency. Thus meausrements often incur higher
 * latencies.
 * \param us
 *      Number of microseconds.
 */
void
Cycles::sleep(uint64_t us)
{
    uint64_t stop = Cycles::rdtsc() + Cycles::fromNanoseconds(1000*us);
    while (Cycles::rdtsc() < stop);
}
} // end PerfUtils
