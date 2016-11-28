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

#define LOG FAST_LOG

static void
hiddenInHeaderFilePrint()
{
    FAST_LOG("Messages in the Header"
        " File"
        );
}
