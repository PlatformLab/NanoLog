/**
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and
 */

 // This program runs a RAMCloud client along with a collection of benchmarks
// for measuring the perfo


 // Total number of clients that will be participating in this test.
static int numClients;

// Number of virtual clients each physical client should simulate.
static int numVClients;


// Example program
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdint.h>

#define ERROR 1

#define LOG RAMCLOUD_LOG

static void
RAMCLOUD_LOG(int logLevel, const char* fmt, ... ) {
    printf("Slow Log: %p\r\n", fmt);
}

static void
FAST_LOG(int logLevel, uint32_t id, ...) {
    printf("Fast Log! %u\n", id);
}

static void
hiddenInHeaderFilePrint()
{
    RAMCLOUD_LOG(ERROR, "Messages in the Header"
        " File"
        );
}