/**
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and
 */

 // This program runs a RAMCloud client along with a collection of benchmarks
// for measuring the perfo

#include <string>

// TODO(syang0) This should be included in the future
#include "../../Runtime/FastLogger.h"

#include "folder/lib.h"
#include "folder/Sample.h"

// forward decl
struct ramcloud_log;
static void
evilTestCase(ramcloud_log* log) {
    ////////
    // Basic Tests
    ////////
    RAMCLOUD_LOG(ERROR, "Simple times");

    int cnt = 2;
    RAMCLOUD_LOG(ERROR, "Hello world number %d of %d (%0.2lf%%)! This is %s!", cnt, 10, 1.0*cnt/10, "Stephen");

    ////////
    // Name Collision Tests
    ////////
    const char *falsePositive = "RAMCLOUD_LOG(ERROR, \"yolo\")";
    RAMCLOUD_LOG(ERROR, "RAMCLOUD_LOG() \"RAMCLOUD_LOG(ERROR, \"Hi \")\"");

    printf("Regular Print: RAMCLOUD_LOG()\r\n");


    ////////
    // Joining of strings
    ////////
    RAMCLOUD_LOG(ERROR, "SD" "F");
    RAMCLOUD_LOG(ERROR, "NEW"
            "Lines" "So"
            "Evil %s",
            "RAMCLOUD_LOG()");

    int i = 0;
    ++i; RAMCLOUD_LOG(ERROR, "Yup\n" "ie"); ++i;


    ////////
    // Preprocessor's ability to rip out strange comments
    ////////
    RAMCLOUD_LOG(ERROR, "Hello %d",
        // 5
        5);

    RAMCLOUD_LOG(ERROR, "He"
        "ll"
        // "o"
        "o %d",
        5);

    int id = 0;
    RAMCLOUD_LOG(ERROR, "This should not be incremented twice (=1):%id", ++id);

    /* This */ RAMCLOUD_LOG( /* is */ ERROR, "Hello /* uncool */");

    RAMCLOUD_LOG(ERROR, "This is " /* comment */ "rediculous");


    /*
     * RAMCLOUD_LOG(ERROR, "RAMCLOUD_LOG");
     */

     // RAMCLOUD_LOG(ERROR, "RAMCLOUD_LOG");

     RAMCLOUD_LOG( ERROR,
        "OLO_SWAG");

     /* // YOLO
      */

     // /*
     RAMCLOUD_LOG(ERROR, "SDF");
     const char *str = ";";
     // */

    ////////
    // Preprocessor substitutions
    ////////
     LOG(ERROR, "ssneaky #define LOG");
     hiddenInHeaderFilePrint();


    { RAMCLOUD_LOG(ERROR, "No %s", std::string("Hello").c_str()); }
    {RAMCLOUD_LOG(ERROR,
        "I am so evil"); }

    ////////
    // Non const strings
    ////////
    const char *myString = "non-const fmt String";
    RAMCLOUD_LOG(ERROR, myString);
    RAMCLOUD_LOG(ERROR, "%s", myString);

    const char *nonConstString = "Lol";
    RAMCLOUD_LOG(ERROR, "NonConst: %s", nonConstString);

    ////////
    // Strange Syntax
    ////////
    RAMCLOUD_LOG(ERROR, "{{\"(( False curlies and brackets! %d", 1);

    RAMCLOUD_LOG(ERROR, "Same line, bad form");      ++i; RAMCLOUD_LOG(ERROR, "Really bad")   ; ++i  ;

    RAMCLOUD_LOG(ERROR, "Ending on different lines"
    )
    ;

    RAMCLOUD_LOG(ERROR, "Make sure that the inserted code is before the ++i"); ++i
;

    RAMCL\
OUD_LOG(ERROR, "The worse");

    RAMCLOUD_LOG
    (ERROR, "TEST");

    // This is currently unfixed in the system
    // RAMCLOUD_LOG(ERROR, "Hello %s %\x64", "a", 5);
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
}