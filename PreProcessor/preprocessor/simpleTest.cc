/**
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and
 */

 // This program runs a RAMCloud client along with a collection of benchmarks
// for measuring the perfo

#include <string>

#include "folder/Sample.h"

// forward decl
struct ramcloud_log;
static void
evilTestCase(ramcloud_log* log) {
    RAMCLOUD_LOG(ERROR, "SD" "F");
    const char *falsePositive = "RAMCLOUD_LOG(ERROR, \"yolo\")";
    RAMCLOUD_LOG(ERROR, "RAMCLOUD_LOG() \"RAMCLOUD_LOG(ERROR, \"Hi \")\"");
    RAMCLOUD_LOG(ERROR, "NEW"
            "Lines" "So"
            "Evil %s",
            "RAMCLOUD_LOG()");

    int i = 0;
    ++i; RAMCLOUD_LOG(ERROR, "Yup\n" "ie"); i++;

    { RAMCLOUD_LOG(ERROR, "No %s", std::string("Hello").c_str()); }
    {RAMCLOUD_LOG(ERROR,
        "I am so evil"); }

    printf("Regular Print: RAMCLOUD_LOG()\r\n");

    RAMCLOUD_LOG(ERROR, "Hello %d",
        // 5
        5);

    RAMCLOUD_LOG(ERROR, "He"
        "ll"
        // "o"
        "o %d",
        5);

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


     const char *myString = "sdf";
     RAMCLOUD_LOG(ERROR, myString);

     LOG(ERROR, "ssneaky #define LOG");

     hiddenInHeaderFilePrint();

     RAMCLOUD_LOG(ERROR, "{{\"(( False curlies and brackets! %d", 1);


    RAMCLOUD_LOG(ERROR, "Same line, bad form"); ++i; RAMCLOUD_LOG(ERROR, "Really bad");

    // RAMCLOUD_LOG (ERROR, "TEST");
    RAMCLOUD_LOG
    (ERROR, "TEST");
}

// I'm not sure how to catch this edge case... Do we just name our functions super special things and hope that no one
// overwrites it?
// int RAMCLOUD_LOG(int i) {
//     return 1;
// }


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