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

#include "../runtime/FastLogger.h"
#include "../runtime/Cycles.h"

#include "folder/Sample.h"

// forward decl
struct fast_log;
static void
evilTestCase(fast_log* log) {
    ////////
    // Basic Tests
    ////////
    FAST_LOG("1) Simple times\r\n");

    FAST_LOG("2) More simplicity");

    FAST_LOG("3) How about a number? %d\n", 1900);

    FAST_LOG("4) How about a second number? %d\n", 1901);

    FAST_LOG("5) How about a double? %lf\n", 0.11);

    FAST_LOG("6) How about a nice little string? %s\n", "Stephen Rocks!");

    FAST_LOG("7) And another string? %s\n", "yolo swag!\nblah\r\n");

    FAST_LOG("8)One that should be \"end\"? %s\n", "end\0 FAIL!!!");

    int cnt = 2;
    FAST_LOG("9) Hello world number %d of %d (%0.2lf%%)! This is %s!\n", cnt, 10, 1.0*cnt/10, "Stephen");

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
        5);

    int id = 0;
    FAST_LOG("15) This should not be incremented twice (=1):%d", ++id);

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
     LOG("ssneaky #define LOG");
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


    FAST_LOG("1) Simple times\r\n");
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
    evilTestCase(NULL);

    int count = 10;
    uint64_t start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        FAST_LOG("Simple Test");
    }

    uint64_t stop = PerfUtils::Cycles::rdtsc();

    double time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Total time 'benchmark recording' %d events took %0.2lf seconds "
            "(%0.2lf ns/event avg)\r\n",
            count, time, (time/count)*1e9);

    PerfUtils::FastLogger::sync();
    PerfUtils::FastLogger::exit();
}
