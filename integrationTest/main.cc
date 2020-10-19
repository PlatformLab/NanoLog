/* Copyright (c) 2016-18 Stanford University
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

#ifndef PREPROCESSOR_NANOLOG
#include "NanoLogCpp17.h"
#endif

#include "Cycles.h"

#include "folder/Sample.h"
#include "SimpleTestObject.h"

using namespace NanoLog::LogLevels;

// forward decl
struct NANO_LOG;
static void
evilTestCase(NANO_LOG* log) {
    ////////
    // Basic Tests
    ////////
    NANO_LOG(NOTICE, "Simple times");

    NANO_LOG(NOTICE, "More simplicity");

    NANO_LOG(NOTICE, "How about a number? %d", 1900);

    NANO_LOG(NOTICE, "How about a second number? %d", 1901);

    NANO_LOG(NOTICE, "How about three numbers without a space? %d%d%d", 1, 2, 3);

    NANO_LOG(NOTICE, "How about a double? %lf", 0.11);

    NANO_LOG(NOTICE, "How about a nice little string? %s", "Stephen Rocks!");

    NANO_LOG(NOTICE, "A middle \"%s\" string?", "Stephen Rocks!");

    NANO_LOG(NOTICE, "And another string? %s", "yolo swag! blah.");

    NANO_LOG(NOTICE, "One that should be \"end\"? %s", "end\0 FAIL!!!");

    int cnt = 2;
    NANO_LOG(NOTICE, "Hello world number %d of %d (%0.2lf%%)! This is %s!", cnt, 10, 1.0*cnt/10, "Stephen");

    NANO_LOG(NOTICE,
                "This is a string of many strings, like %s, %s, and %s"
                " with a number %d and a final string with spacers %*s",
                "this one",
                "this other one",
                "this third one",
                12345670,
                20,
                "far out");

    void* pointer = (void*)0x7ffe075cbe7d;
    const void* const_ptr = pointer;

    NANO_LOG(NOTICE, "A const void* pointer %p", const_ptr);

    NANO_LOG(NOTICE, "I'm a small log with a small %s", "string");

    uint8_t small = 10;
    uint16_t medium = 33;
    uint32_t large = 99991;
    uint64_t ultra_large = -1;

    float Float = 121.121f;
    double Double = 212.212;

    NANO_LOG(NOTICE, "Let's try out all the types! "
                "Pointer = %p! "
                "uint8_t = %u! "
                "uint16_t = %u! "
                "uint32_t = %u! "
                "uint64_t = %lu! "
                "float = %f! "
                "double = %lf! "
                "hexadecimal = %x! "
                "Just a normal character = %c",
            pointer, small, medium, large, ultra_large, Float, Double,
            0xFF, 'a'
            );

    int8_t smallNeg = -10;
    int16_t mediumNeg= -9991;
    int32_t largeNeg = -99991;
    int64_t ultra_large_neg = -1;
    NANO_LOG(NOTICE, "how about some negative numbers? "
        "int8_t %d; "
        "int16_t %d; "
        "int32_t %d; "
        "int64_t %ld; "
        "int %d",
        smallNeg, mediumNeg, largeNeg, ultra_large_neg, -12356
        );

    NANO_LOG(NOTICE, "How about variable width + precision? %*.*lf %*d %10s", 9, 2, 12345.12345, 10, 123, "end");

    NANO_LOG(NOTICE, "How about a variable length string that should end %.*s", 4, "here, but not here.");
    NANO_LOG(NOTICE, "And another one that should end %.4s", "here, but not here.");

// The Long string tests are commented out due to incompatibilities with certain versions of gcc.
    // // Long strings!
    // const wchar_t *longString = L"asdf";
    // NANO_LOG(WARNING, "longString: %d %ls", 1, longString);

    // What happens when strings are not const?
    char stringArray[10];
    // wchar_t longStringArray[10];
    strcpy(stringArray, "bcdefg");
    // wcscpy(longStringArray, longString);

    // NANO_LOG(WARNING, "NonConst %s ls=%ls s=%s",
    //          stringArray, longStringArray, stringArray);
    NANO_LOG(WARNING, "NonConst %s and %s", stringArray, stringArray);
    NANO_LOG(WARNING, "A Character %c", 'd');

    ////////
    // Name Collision Tests
    ////////
    const char *falsePositive = "NANO_LOG(\"yolo\")";
    ++falsePositive;
    NANO_LOG(NOTICE, "10) NANO_LOG() \"NANO_LOG(\"Hi \")\"");

    printf("Regular Print: NANO_LOG()");


    ////////
    // Joining of strings
    ////////
    NANO_LOG(NOTICE, "11) " "SD" "F");
    NANO_LOG(NOTICE, "12) NEW"
            "Lines" "So"
            "Evil %s",
            "NANO_LOG()");

    int i = 0;
    ++i; NANO_LOG(NOTICE, "13) Yup\n" "ieieieieieieie1"); ++i;
    NANO_LOG(NOTICE, "13.5) This should be =2: %d", i);


    ////////
    // Preprocessor's ability to rip out strange comments
    ////////
    NANO_LOG(NOTICE, "14) Hello %d",
        // 5
        5);

    NANO_LOG(NOTICE, "14) He"
        "ll"
        // "o"
        "o %d",
        6);

    int id = 0;
    NANO_LOG(NOTICE, "15) This should not be incremented twice (=1):%d", ++id);

    id++; NANO_LOG(NOTICE, "15) This should be incremented once more (=2):%d", id++); ++id;

    /* This */ NANO_LOG(NOTICE /* log */,  /* is */ "16) Hello /* uncool */");

    NANO_LOG(NOTICE, "17) This is " /* comment */ "rediculous");


    /*
     * NANO_LOG(NOTICE, "NANO_LOG");
     */

     // NANO_LOG(NOTICE, "NANO_LOG");

     NANO_LOG(NOTICE,
        "18) OLO_SWAG");

     /* // YOLO
      */

     // /*
     NANO_LOG(NOTICE, "11) SDF");
     const char *dummy = ";";
     ++dummy;
     // */

    ////////
    // Preprocessor substitutions
    ////////
     LOG(NOTICE, "sneaky #define LOG");
     hiddenInHeaderFilePrint();


    { NANO_LOG(NOTICE, "No %s", std::string("Hello").c_str()); }
    {NANO_LOG(NOTICE,
        "I am so evil"); }

    ////////
    // Non const strings
    ////////
    const char *myString = "non-const fmt String";
    // NANO_LOG(myString);   // This will error out the system since we do not yet support arbitrary strings
    NANO_LOG(NOTICE, "%s", myString);

    const char *nonConstString = "Lol";
    NANO_LOG(NOTICE, "NonConst: %s", nonConstString);

    ////////
    // Strange Syntax
    ////////
    NANO_LOG(NOTICE, "{{\"(( False curlies and brackets! %d", 1);

    NANO_LOG(NOTICE, "Same line, bad form");      ++i; NANO_LOG(NOTICE, "Really bad")   ; ++i  ;

    NANO_LOG(
        NOTICE
        ,
        "Ending on different lines"
    )
    ;

    NANO_LOG(NOTICE, "Make sure that the inserted code is before the ++i"); ++i;

    NA\
NO_LOG(NOTI\
CE, "The worse");

    NANO_LOG
    (NOTICE, "TEST");

    // This is currently unfixed in the system
    // NANO_LOG(NOTICE, "Hello %s %\x64", "a", 5);

    ////////
    // Repeats of random logs
    ////////
    NANO_LOG(NOTICE,
        "14) He"
        "ll"
        // "o"
        "o %d",
        5);

    ++i; NANO_LOG(NOTICE, "13) Yup\n" "ieieieieieieie2"); ++i;

    NANO_LOG(NOTICE, "Ending on different lines"
    )
    ;


    NANO_LOG(NOTICE, "1) Simple times");

    //////
    // Special case string precision
    //////

    // This test is accompanied by a log size checker in the main file.
    // It should ensure that only 4 bytes are logged, not 1,000,000 bytes
    void* largeString = malloc(1000000);
    memset(largeString, 'a', 1000000);
    NANO_LOG(NOTICE, "This string should end soon with 4 'a''s here: %.4s",
                static_cast<const char*>(largeString));

    int length = 5;
    NANO_LOG(NOTICE, "Another string that should end soon with 5 'a''s here: %.*s",
                length, static_cast<const char*>(largeString));

    NANO_LOG(NOTICE, "A string that's just one 'a': %.1000000s", "a");
    free(largeString);
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

// Test all log levels and make sure that the logs are correctly omitted.
void logLevelTest() {
    LogLevel startingLevel = NanoLog::getLogLevel();

    NanoLog::setLogLevel(DEBUG);
    NANO_LOG(DEBUG, "Debug");
    NANO_LOG(NOTICE, "Notice");
    NANO_LOG(WARNING, "Warning");
    NANO_LOG(ERROR, "Error");

    NanoLog::setLogLevel(NOTICE);
    NANO_LOG(DEBUG, "Debug");
    NANO_LOG(NOTICE, "Notice");
    NANO_LOG(WARNING, "Warning");
    NANO_LOG(ERROR, "Error");

    NanoLog::setLogLevel(WARNING);
    NANO_LOG(DEBUG, "Debug");
    NANO_LOG(NOTICE, "Notice");
    NANO_LOG(WARNING, "Warning");
    NANO_LOG(ERROR, "Error");

    NanoLog::setLogLevel(ERROR);
    NANO_LOG(DEBUG, "Debug");
    NANO_LOG(NOTICE, "Notice");
    NANO_LOG(WARNING, "Warning");
    NANO_LOG(ERROR, "Error");

    NanoLog::setLogLevel(SILENT_LOG_LEVEL);
    NANO_LOG(DEBUG, "Debug");
    NANO_LOG(NOTICE, "Notice");
    NANO_LOG(WARNING, "Warning");
    NANO_LOG(ERROR, "Error");

    // Restoring the previous log level
    NanoLog::setLogLevel(startingLevel);
}

// Test all possible specifiers (except %n) from
// http://www.cplusplus.com/reference/cstdio/printf/ (3/20/18)
void testAllTheTypes() {
    NANO_LOG(WARNING,
            "No Length=%d %i %u %o %x %x %f %F %e %E %g %G %a %A %c %s %p",
            (int)-1,
            (int)-2,
            (unsigned int)3,
            (unsigned int)4,
            (unsigned int)5,
            (unsigned int)6,
            (double)7.0,
            (double)8.0,
            (double)9.0,
            (double)10.0,
            (double)11.0,
            (double)12.0,
            (double)13.0,
            (double)14.0,
            'a',
            "abc",
            (void*)0x1);

    NANO_LOG(WARNING,
            "hh=%hhd %hhi %hhu %hho %hhx %hhx",
            (signed char)-1,
            (signed char)-2,
            (unsigned char)3,
            (unsigned char)4,
            (unsigned char)5,
            (unsigned char)6);

    NANO_LOG(WARNING,
        "h=%hd %hi %hu %ho %hx %hx",
        (short int)-20000,
        (short int)-20001,
        (unsigned short int)20002,
        (unsigned short int)20003,
        (unsigned short int)20004,
        (unsigned short int)20005);

    // const wchar_t *longString = L"asdf";
    NANO_LOG(WARNING,
        "l=%ld %li %lu %lo %lx %lx %%lc %%ls",
        (long int)-(1 << 30),
        (long int)-(1 << 30) -1 ,
        ((unsigned long int)1UL<<30) + 2,
        ((unsigned long int)1UL<<30) + 3,
        ((unsigned long int)1UL<<30) + 4,
        ((unsigned long int)1UL<<30) + 5);//,
        // (wchar_t)'a',
        // (const wchar_t*)longString);

    NANO_LOG(WARNING,
        "ll=%lld %lli %llu %llo %llx %llx",
        (long long int)1LL<<60,
        (long long int)-(1LL<<60),
        (unsigned long long int)1UL<<60,
        (unsigned long long int)1UL<<61,
        (unsigned long long int)1UL<<62,
        (unsigned long long int)1UL<<63);

    NANO_LOG(WARNING,
        "j=%jd %ji %ju %jo %jx %jx",
        (intmax_t)1LL<<60,
        (intmax_t)-(1LL<<60),
        (uintmax_t)1UL<<60,
        (uintmax_t)1UL<<61,
        (uintmax_t)1UL<<62,
        (uintmax_t)1UL<<63);

    NANO_LOG(WARNING,
        "z=%zd %zi %zu %zo %zx %zx",
        (size_t)1LL<<62,
        (size_t)1LL<<61,
        (size_t)1UL<<60,
        (size_t)1UL<<61,
        (size_t)1UL<<62,
        (size_t)1UL<<63);

    NANO_LOG(WARNING,
        "t=%td %ti %tu %to %tx %tx",
        (ptrdiff_t)1LL<<62,
        (ptrdiff_t)1LL<<61,
        (ptrdiff_t)1UL<<60,
        (ptrdiff_t)1UL<<61,
        (ptrdiff_t)1UL<<62,
        (ptrdiff_t)1UL<<63);

    NANO_LOG(WARNING,
        "L=%Lf %LF %Le %LE %Lg %LG %La %LA",
        (long double)7.0,
        (long double)8.0,
        (long double)9.0,
        (long double)10.0,
        (long double)11.0,
        (long double)12.0,
        (long double)13.0,
        (long double)14.0);
}


int main()
{
    NanoLog::setLogFile("testLog");
    evilTestCase(NULL);
    testAllTheTypes();

    int count = 10;
    uint64_t start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
       NANO_LOG(NOTICE, "Loop test!");
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

    logLevelTest();

    NanoLog::sync();

    printf("\r\nNote: This app is used in the integration tests, but "
        "is not the test runner. \r\nTo run the actual test, invoke "
        "\"make run-test\"\r\n\r\n");
}
