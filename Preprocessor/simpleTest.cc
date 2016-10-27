/**
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and
 */

 // This program runs a RAMCloud client along with a collection of benchmarks
// for measuring the perfo

#include <string>

// TODO(syang0) This should be included in the future
#include "../Runtime/FastLogger.h"
#include "../Runtime/Cycles.h"

#include "folder/lib.h"
#include "folder/Sample.h"

// forward decl
struct ramcloud_log;
static void
evilTestCase(ramcloud_log* log) {
    ////////
    // Basic Tests
    ////////
    RAMCLOUD_LOG("1) Simple times\r\n");

    RAMCLOUD_LOG("2) More simplicity");

    RAMCLOUD_LOG("3) How about a number? %d\n", 1900);

    RAMCLOUD_LOG("4) How about a second number? %d\n", 1901);

    RAMCLOUD_LOG("5) How about a double? %lf\n", 0.11);

    RAMCLOUD_LOG("6) How about a nice little string? %s\n", "Stephen Rocks!");

    RAMCLOUD_LOG("7) And another string? %s\n", "yolo swag!\nblah\r\n");

    RAMCLOUD_LOG("8)One that should be \"end\"? %s\n", "end\0 FAIL!!!");

    int cnt = 2;
    RAMCLOUD_LOG("9) Hello world number %d of %d (%0.2lf%%)! This is %s!\n", cnt, 10, 1.0*cnt/10, "Stephen");

    ////////
    // Name Collision Tests
    ////////
    const char *falsePositive = "RAMCLOUD_LOG(\"yolo\")";
    RAMCLOUD_LOG("10) RAMCLOUD_LOG() \"RAMCLOUD_LOG(\"Hi \")\"");

    printf("Regular Print: RAMCLOUD_LOG()\r\n");


    ////////
    // Joining of strings
    ////////
    RAMCLOUD_LOG("11) " "SD" "F");
    RAMCLOUD_LOG("12) NEW"
            "Lines" "So"
            "Evil %s",
            "RAMCLOUD_LOG()");

    int i = 0;
    ++i; RAMCLOUD_LOG("13) Yup\n" "ie"); ++i;


    ////////
    // Preprocessor's ability to rip out strange comments
    ////////
    RAMCLOUD_LOG("14) Hello %d",
        // 5
        5);

    RAMCLOUD_LOG("14) He"
        "ll"
        // "o"
        "o %d",
        5);

    int id = 0;
    RAMCLOUD_LOG("15) This should not be incremented twice (=1):%d", ++id);

    /* This */ RAMCLOUD_LOG( /* is */ "16) Hello /* uncool */");

    RAMCLOUD_LOG("17) This is " /* comment */ "rediculous");


    /*
     * RAMCLOUD_LOG("RAMCLOUD_LOG");
     */

     // RAMCLOUD_LOG("RAMCLOUD_LOG");

     RAMCLOUD_LOG(
        "18) OLO_SWAG");

     /* // YOLO
      */

     // /*
     RAMCLOUD_LOG("11) SDF");
     const char *str = ";";
     // */

    ////////
    // Preprocessor substitutions
    ////////
     LOG("ssneaky #define LOG");
     hiddenInHeaderFilePrint();


    { RAMCLOUD_LOG("No %s", std::string("Hello").c_str()); }
    {RAMCLOUD_LOG(
        "I am so evil"); }

    ////////
    // Non const strings
    ////////
    const char *myString = "non-const fmt String";
    // RAMCLOUD_LOG(myString);   // This will error out the system since we do not yet support arbitrary strings
    RAMCLOUD_LOG("%s", myString);

    const char *nonConstString = "Lol";
    RAMCLOUD_LOG("NonConst: %s", nonConstString);

    ////////
    // Strange Syntax
    ////////
    RAMCLOUD_LOG("{{\"(( False curlies and brackets! %d", 1);

    RAMCLOUD_LOG("Same line, bad form");      ++i; RAMCLOUD_LOG("Really bad")   ; ++i  ;

    RAMCLOUD_LOG("Ending on different lines"
    )
    ;

    RAMCLOUD_LOG("Make sure that the inserted code is before the ++i"); ++i;

    RAMCL\
OUD_LOG("The worse");

    RAMCLOUD_LOG
    ("TEST");

    // This is currently unfixed in the system
    // RAMCLOUD_LOG("Hello %s %\x64", "a", 5);

    ////////
    // Repeats of random logs
    ////////
    RAMCLOUD_LOG("14) He"
        "ll"
        // "o"
        "o %d",
        5);

    ++i; RAMCLOUD_LOG("13) Yup\n" "ie"); ++i;

    RAMCLOUD_LOG("Ending on different lines"
    )
    ;


    RAMCLOUD_LOG("1) Simple times\r\n");
}

////////
// More Name Collision Tests
////////
int
RAMCLOUD_LOG_FAILURE(int RAMCLOUD_LOG, int RAMCLOUD_LOG2) {
    // This is tricky!

    return RAMCLOUD_LOG + RAMCLOUD_LOG2 + RAMCLOUD_LOG;
}

int
NOT_QUITE_RAMCLOUD_LOG(int NOT_RAMCLOUD_LOG, int NOT_REALLY_RAMCLOUD_LOG, int RA_0RAMCLOUD_LOG) {
    return NOT_RAMCLOUD_LOG + NOT_REALLY_RAMCLOUD_LOG + RA_0RAMCLOUD_LOG;
}

void gah()
{
    int RAMCLOUD_LOG = 10;
    RAMCLOUD_LOG_FAILURE((uint32_t) RAMCLOUD_LOG, RAMCLOUD_LOG);
}

int main()
{
    evilTestCase(NULL);

    int count = 100000000;
    uint64_t start = PerfUtils::Cycles::rdtsc();
    for (int i = 0; i < count; ++i) {
        RAMCLOUD_LOG("Simple Test");
    }

    uint64_t stop = PerfUtils::Cycles::rdtsc();

    double time = PerfUtils::Cycles::toSeconds(stop - start);
    printf("Total time 'benchmark recording' %d events took %0.2lf seconds "
            "(%0.2lf ns/event avg)\r\n",
            count, time, (time/count)*1e9);

    PerfUtils::FastLogger::sync();
    PerfUtils::FastLogger::exit();
}
