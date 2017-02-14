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
 * the NanoLog system.
 */

#include <string>

#include "NanoLog.h"
#include "Cycles.h"

#include "folder/Sample.h"
#include "SimpleTestObject.h"

// forward decl
struct NANO_LOG;
static void
evilTestCase(NANO_LOG* log) {
    ////////
    // Basic Tests
    ////////
    NANO_LOG("Simple times");

    NANO_LOG("More simplicity");

    NANO_LOG("How about a number? %d", 1900);

    NANO_LOG("How about a second number? %d", 1901);

    NANO_LOG("How about a double? %lf", 0.11);

    NANO_LOG("How about a nice little string? %s", "Stephen Rocks!");

    NANO_LOG("A middle \"%s\" string?", "Stephen Rocks!");

    NANO_LOG("And another string? %s", "yolo swag! blah.");

    NANO_LOG("One that should be \"end\"? %s", "end\0 FAIL!!!");

    int cnt = 2;
    NANO_LOG("Hello world number %d of %d (%0.2lf%%)! This is %s!", cnt, 10, 1.0*cnt/10, "Stephen");

    NANO_LOG("This is a string of many strings, like %s, %s, and %s"
                " with a number %d and a final string with spacers %*s\r\n",
                "this one",
                "this other one",
                "this third one",
                12345670,
                20,
                "far out");

    void* pointer = (void*)0x7ffe075cbe7d;
    const void* const_ptr = pointer;

    NANO_LOG("A const void* pointer %p", const_ptr);

    uint8_t small = 10;
    uint16_t medium = 33;
    uint32_t large = 99991;
    uint64_t ultra_large = -1;

    float Float = 121.121;
    double Double = 212.212;

    NANO_LOG("Let's try out all the types!\n"
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
    NANO_LOG("how about some negative numbers?\n"
        "int8_t %d\n"
        "int16_t %d\n"
        "int32_t %d\n"
        "int64_t %ld\n"
        "int %d",
        smallNeg, mediumNeg, largeNeg, ultra_large_neg, -12356
        );

    NANO_LOG("How about variable width + precision? %*.*lf %*d %10s", 9, 2, 12345.12345, 10, 123, "end");

    ////////
    // Name Collision Tests
    ////////
    const char *falsePositive = "NANO_LOG(\"yolo\")";
    NANO_LOG("10) NANO_LOG() \"NANO_LOG(\"Hi \")\"");

    printf("Regular Print: NANO_LOG()\r\n");


    ////////
    // Joining of strings
    ////////
    NANO_LOG("11) " "SD" "F");
    NANO_LOG("12) NEW"
            "Lines" "So"
            "Evil %s",
            "NANO_LOG()");

    int i = 0;
    ++i; NANO_LOG("13) Yup\n" "ie"); ++i;
    NANO_LOG("13.5) This should be =2: %d", i);


    ////////
    // Preprocessor's ability to rip out strange comments
    ////////
    NANO_LOG("14) Hello %d",
        // 5
        5);

    NANO_LOG("14) He"
        "ll"
        // "o"
        "o %d",
        6);

    int id = 0;
    NANO_LOG("15) This should not be incremented twice (=1):%d", ++id);

    id++; NANO_LOG("15) This should be incremented once more (=2):%d", id++); ++id;

    /* This */ NANO_LOG( /* is */ "16) Hello /* uncool */");

    NANO_LOG("17) This is " /* comment */ "rediculous");


    /*
     * NANO_LOG("NANO_LOG");
     */

     // NANO_LOG("NANO_LOG");

     NANO_LOG(
        "18) OLO_SWAG");

     /* // YOLO
      */

     // /*
     NANO_LOG("11) SDF");
     const char *str = ";";
     // */

    ////////
    // Preprocessor substitutions
    ////////
     LOG("sneaky #define LOG");
     hiddenInHeaderFilePrint();


    { NANO_LOG("No %s", std::string("Hello").c_str()); }
    {NANO_LOG(
        "I am so evil"); }

    ////////
    // Non const strings
    ////////
    const char *myString = "non-const fmt String";
    // NANO_LOG(myString);   // This will error out the system since we do not yet support arbitrary strings
    NANO_LOG("%s", myString);

    const char *nonConstString = "Lol";
    NANO_LOG("NonConst: %s", nonConstString);

    ////////
    // Strange Syntax
    ////////
    NANO_LOG("{{\"(( False curlies and brackets! %d", 1);

    NANO_LOG("Same line, bad form");      ++i; NANO_LOG("Really bad")   ; ++i  ;

    NANO_LOG("Ending on different lines"
    )
    ;

    NANO_LOG("Make sure that the inserted code is before the ++i"); ++i;

    NA\
NO_LOG("The worse");

    NANO_LOG
    ("TEST");

    // This is currently unfixed in the system
    // NANO_LOG("Hello %s %\x64", "a", 5);

    ////////
    // Repeats of random logs
    ////////
    NANO_LOG("14) He"
        "ll"
        // "o"
        "o %d",
        5);

    ++i; NANO_LOG("13) Yup\n" "ie"); ++i;

    NANO_LOG("Ending on different lines"
    )
    ;


    NANO_LOG("1) Simple times");
}

////////
// More Name Collision Tests
////////
int
NANO_LOG_FAILURE(int NANO_LOG, int NANO_LOG2) {
    // This is tricky!

    return NANO_LOG + NANO_LOG2 + NANO_LOG;
}

int
NOT_QUITE_NANO_LOG(int NOT_NANO_LOG, int NOT_REALLY_NANO_LOG, int RA_0NANO_LOG) {
    return NOT_NANO_LOG + NOT_REALLY_NANO_LOG + RA_0NANO_LOG;
}

void gah()
{
    int NANO_LOG = 10;
    NANO_LOG_FAILURE((uint32_t) NANO_LOG, NANO_LOG);
}

int main()
{
    NanoLog::setLogFile("/tmp/testLog");
    evilTestCase(NULL);

    int count = 10;
    uint64_t start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        NANO_LOG("Loop test!");
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

    NanoLog::sync();

    printf("\r\nNote: This app is used in the integration tests, but"
        "is not the test runner. \r\nTo run the actual test, invoke "
        "\"make run-test\"\r\n\r\n");
}
