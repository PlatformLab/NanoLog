/* Copyright (c) 2016-2017 Stanford University
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
  * This file mocks a simple generated BufferStuffer to be used in gtests.
  */


#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include "NanoLog.h"
#include "Packer.h"

#include <fstream>
#include <string>

// Since some of the functions/variables output below are for debugging purposes
// only (i.e. they're not used in their current form), squash all gcc complaints
// about unused variables/functions.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

/**
 * Describes a log message found in the user sources by the original format
 * string provided, the file where the log message occurred, and the line number
 */
struct LogMetadata {
  const char *fmtString;
  const char *fileName;
  uint32_t lineNumber;
};

// Start an empty namespace to enclose all the record(debug)/compress/decompress
// functions
namespace {

inline void __syang0__fl__Simple32log32message32with32032parameters__Benchmark46cc__45__(const char* fmtStr ) {
    extern const uint32_t __fmtId__Simple32log32message32with32032parameters__Benchmark46cc__45__;

    ;
    size_t allocSize =   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(NanoLog::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__Simple32log32message32with32032parameters__Benchmark46cc__45__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments


    // Record the strings (if any) at the end of the entry


    // Make the entry visible
    NanoLog::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__(BufferUtils::UncompressedLogEntry *re, char* out) {
    char *originalOutPtr = out;

    // Allocate nibbles
    BufferUtils::TwoNibbles *nib = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += 0;

    char *args = re->argData;

    // Read back all the primitives


    // Pack all the primitives


    // memcpy all the strings without compression
    size_t stringBytes = re->entrySize - ( 0)
                                        - sizeof(BufferUtils::UncompressedLogEntry);
    if (stringBytes > 0) {
        memcpy(out, args, stringBytes);
        out += stringBytes;
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__ (std::ifstream &in, FILE *outputFd) {
    BufferUtils::TwoNibbles nib[0];
    in.read(reinterpret_cast<char*>(&nib), 0);

    // Unpack all the non-string argments


    // Find all the strings


    const char *fmtString = "Simple log message with 0 parameters";
    const char *filename = "Benchmark.cc";
    const int linenum = 45;

    if (outputFd)
        fprintf(outputFd, "Simple log message with 0 parameters" "\r\n" );
}


} // end empty namespace

// Assignment of numerical ids to format NANO_LOG occurrences
extern const int __fmtId__Simple32log32message32with32032parameters__Benchmark46cc__45__ = 0; // Benchmark.cc:45 "Simple log message with 0 parameters"

// Start new namespace for generated ids and code
namespace GeneratedFunctions {

// Map of numerical ids to log message metadata
struct LogMetadata logId2Metadata[1] =
{
    {"Simple log message with 0 parameters", "Benchmark.cc", 45}
};

// Map of numerical ids to compression functions
ssize_t
(*compressFnArray[1]) (BufferUtils::UncompressedLogEntry *re, char* out)
{
    compressArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__
};

// Map of numerical ids to decompression functions
void
(*decompressAndPrintFnArray[1]) (std::ifstream &in, FILE *outputFd)
{
    decompressPrintArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__
};

// Total number of logIds. Can be used to bounds check array accesses.
size_t numLogIds = 1;

// Pop the unused gcc warnings
#pragma GCC diagnostic pop

}; // GeneratedFunctions

#endif /* BUFFER_STUFFER */