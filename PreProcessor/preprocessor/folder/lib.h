// Example program
#include <iostream>
#include <string>
#include <stdio.h>
#include <stdint.h>

#define ERROR 1

__attribute__ ((format (printf, 2, 3)))
static void
RAMCLOUD_LOG(int logLevel, const char* fmt, ... ) {
  printf("This is a slow version of the log that did not get replaced: %s\r\n", fmt);
}

// Fake version of RAMCLOUD_LOG that does absolutely nothing
__attribute__ ((format (printf, 2, 3)))
static inline void
RAMCLOUD_NOP(int logLevel, const char* fmt, ... ) {
}

static void
FAST_LOG(int logLevel, uint32_t id, ...) {
    printf("Fast Log! %u\n", id);
}