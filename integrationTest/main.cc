/* Copyright (c) 2016 Stanford University
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

/**
 * This file contains a slew of test cases for the parsing component of
 * the FastLogger system.
 */

#include <string>

#include "FastLogger.h"
#include "Cycles.h"

#include "folder/Sample.h"
#include "SimpleTestObject.h"

// forward decl
struct fast_log;
static void
evilTestCase(fast_log* log) {
    ////////
    // Basic Tests
    ////////
    FAST_LOG("Simple times");

    FAST_LOG("More simplicity");

    FAST_LOG("How about a number? %d", 1900);

    FAST_LOG("How about a second number? %d", 1901);

    FAST_LOG("How about a double? %lf", 0.11);

    FAST_LOG("How about a nice little string? %s", "Stephen Rocks!");

    FAST_LOG("A middle \"%s\" string?", "Stephen Rocks!");

    FAST_LOG("And another string? %s", "yolo swag! blah.");

    FAST_LOG("One that should be \"end\"? %s", "end\0 FAIL!!!");

    int cnt = 2;
    FAST_LOG("Hello world number %d of %d (%0.2lf%%)! This is %s!", cnt, 10, 1.0*cnt/10, "Stephen");


    void *pointer = (void*)0x7ffe075cbe7d;
    uint8_t small = 10;
    uint16_t medium = 33;
    uint32_t large = 99991;
    uint64_t ultra_large = -1;

    float Float = 121.121;
    double Double = 212.212;

    FAST_LOG("Let's try out all the types!\n"
                "Pointer = %p\n"
                "uint8_t = %u\n"
                "uint16_t = %u\n"
                "uint32_t = %u\n"
                "uint64_t = %lu\n"
                "float = %f\n"
                "double = %lf\n"
                "hexadecimal = %x\n"
                "Just a normal character = %c",
            pointer, small, medium, large, ultra_large, Float, Double,
            0xFF, 'a'
            );

    int8_t smallNeg = -10;
    int16_t mediumNeg= -33;
    int32_t largeNeg = -99991;
    int64_t ultra_large_neg = -1;
    FAST_LOG("how about some negative numbers?\n"
        "int8_t %d\n"
        "int16_t %d\n"
        "int32_t %d\n"
        "int64_t %ld\n"
        "int %d",
        smallNeg, mediumNeg, largeNeg, ultra_large_neg, -12356
        );

    FAST_LOG("How about variable width + precision? %*.*lf %*d %10s", 9, 2, 12345.12345, 10, 123, "end");

    ////////
    // Name Collision Tests
    ////////
    const char *falsePositive = "FAST_LOG(\"yolo\")";
    FAST_LOG("10) FAST_LOG() \"FAST_LOG(\"Hi \")\"");

    printf("Regular Print: FAST_LOG()\r\n");


    ////////
    // Joining of strings
    ////////
    FAST_LOG("11) " "SD" "F");
    FAST_LOG("12) NEW"
            "Lines" "So"
            "Evil %s",
            "FAST_LOG()");

    int i = 0;
    ++i; FAST_LOG("13) Yup\n" "ie"); ++i;
    FAST_LOG("13.5) This should be =2: %d", i);


    ////////
    // Preprocessor's ability to rip out strange comments
    ////////
    FAST_LOG("14) Hello %d",
        // 5
        5);

    FAST_LOG("14) He"
        "ll"
        // "o"
        "o %d",
        6);

    int id = 0;
    FAST_LOG("15) This should not be incremented twice (=1):%d", ++id);

    id++; FAST_LOG("15) This should be incremented once more (=2):%d", id++); ++id;

    /* This */ FAST_LOG( /* is */ "16) Hello /* uncool */");

    FAST_LOG("17) This is " /* comment */ "rediculous");


    /*
     * FAST_LOG("FAST_LOG");
     */

     // FAST_LOG("FAST_LOG");

     FAST_LOG(
        "18) OLO_SWAG");

     /* // YOLO
      */

     // /*
     FAST_LOG("11) SDF");
     const char *str = ";";
     // */

    ////////
    // Preprocessor substitutions
    ////////
     LOG("sneaky #define LOG");
     hiddenInHeaderFilePrint();


    { FAST_LOG("No %s", std::string("Hello").c_str()); }
    {FAST_LOG(
        "I am so evil"); }

    ////////
    // Non const strings
    ////////
    const char *myString = "non-const fmt String";
    // FAST_LOG(myString);   // This will error out the system since we do not yet support arbitrary strings
    FAST_LOG("%s", myString);

    const char *nonConstString = "Lol";
    FAST_LOG("NonConst: %s", nonConstString);

    ////////
    // Strange Syntax
    ////////
    FAST_LOG("{{\"(( False curlies and brackets! %d", 1);

    FAST_LOG("Same line, bad form");      ++i; FAST_LOG("Really bad")   ; ++i  ;

    FAST_LOG("Ending on different lines"
    )
    ;

    FAST_LOG("Make sure that the inserted code is before the ++i"); ++i;

    FA\
ST_LOG("The worse");

    FAST_LOG
    ("TEST");

    // This is currently unfixed in the system
    // FAST_LOG("Hello %s %\x64", "a", 5);

    ////////
    // Repeats of random logs
    ////////
    FAST_LOG("14) He"
        "ll"
        // "o"
        "o %d",
        5);

    ++i; FAST_LOG("13) Yup\n" "ie"); ++i;

    FAST_LOG("Ending on different lines"
    )
    ;


    FAST_LOG("1) Simple times");
}

////////
// More Name Collision Tests
////////
int
FAST_LOG_FAILURE(int FAST_LOG, int FAST_LOG2) {
    // This is tricky!

    return FAST_LOG + FAST_LOG2 + FAST_LOG;
}

int
NOT_QUITE_FAST_LOG(int NOT_FAST_LOG, int NOT_REALLY_FAST_LOG, int RA_0FAST_LOG) {
    return NOT_FAST_LOG + NOT_REALLY_FAST_LOG + RA_0FAST_LOG;
}

void gah()
{
    int FAST_LOG = 10;
    FAST_LOG_FAILURE((uint32_t) FAST_LOG, FAST_LOG);
}

int main()
{
    PerfUtils::FastLogger::setLogFile("./compressedLog");
    evilTestCase(NULL);

    int count = 10;
    uint64_t start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        FAST_LOG("Loop test!");
    }

    uint64_t stop = PerfUtils::Cycles::rdtsc();

    double time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Total time 'benchmark recording' %d events took %0.2lf seconds "
            "(%0.2lf ns/event avg)\r\n",
            count, time, (time/count)*1e9);

    SimpleTest st(10);
    st.logSomething();
    st.wholeBunchOfLogStatements();
    st.logStatementsInHeader();
    st.logSomething();
    st.logSomething();

    PerfUtils::FastLogger::sync();

    printf("\r\nNote: This app is used in the integration tests, but"
        "is not the test runner. \r\nTo run the actual test, invoke "
        "\"make run-test\"\r\n\r\n");
}
