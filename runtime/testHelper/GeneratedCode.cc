/* Copyright (c) 2017 Stanford University
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

#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include "NanoLog.h"
#include "Packer.h"

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
    size_t allocSize =   sizeof(Log::UncompressedEntry);
    Log::UncompressedEntry *re = reinterpret_cast<Log::UncompressedEntry*>(NanoLog::__internal_reserveAlloc(allocSize));

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
compressArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__(Log::UncompressedEntry *re, char* out) {
    char *originalOutPtr = out;

    // Allocate nibbles
    BufferUtils::TwoNibbles *nib = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += 0;

    char *args = re->argData;

    // Read back all the primitives


    // Pack all the primitives


    if (false) {
        // memcpy all the strings without compression
        size_t stringBytes = re->entrySize - ( 0)
                                            - sizeof(Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments


    // Find all the strings


    const char *fmtString = "Simple log message with 0 parameters";
    const char *filename = "Benchmark.cc";
    const int linenum = 45;

    if (outputFd)
        fprintf(outputFd, "Simple log message with 0 parameters" "\r\n" );

    if (aggFn)
        (*aggFn)("Simple log message with 0 parameters" );
}


inline void __syang0__fl__This32is32a32string3237s__Benchmark46cc__48__(const char* fmtStr , const char* arg0) {
    extern const uint32_t __fmtId__This32is32a32string3237s__Benchmark46cc__48__;

    size_t str0Len = strlen(arg0);;
    size_t allocSize =  str0Len + 1 +  sizeof(Log::UncompressedEntry);
    Log::UncompressedEntry *re = reinterpret_cast<Log::UncompressedEntry*>(NanoLog::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__This32is32a32string3237s__Benchmark46cc__48__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments


    // Record the strings (if any) at the end of the entry
    memcpy(buffer, arg0, str0Len); buffer += str0Len; *buffer = '\0'; buffer++;

    // Make the entry visible
    NanoLog::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__This32is32a32string3237s__Benchmark46cc__48__(Log::UncompressedEntry *re, char* out) {
    char *originalOutPtr = out;

    // Allocate nibbles
    BufferUtils::TwoNibbles *nib = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += 0;

    char *args = re->argData;

    // Read back all the primitives


    // Pack all the primitives


    if (true) {
        // memcpy all the strings without compression
        size_t stringBytes = re->entrySize - ( 0)
                                            - sizeof(Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__This32is32a32string3237s__Benchmark46cc__48__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments


    // Find all the strings

                const char* arg0 = *in;
                (*in) += strlen(arg0) + 1; // +1 for null terminator


    const char *fmtString = "This is a string %s";
    const char *filename = "Benchmark.cc";
    const int linenum = 48;

    if (outputFd)
        fprintf(outputFd, "This is a string %s" "\r\n" , arg0);

    if (aggFn)
        (*aggFn)("This is a string %s" , arg0);
}


} // end empty namespace

// Assignment of numerical ids to format NANO_LOG occurrences
extern const int __fmtId__Simple32log32message32with32032parameters__Benchmark46cc__45__ = 0; // Benchmark.cc:45 "Simple log message with 0 parameters"
extern const int __fmtId__This32is32a32string3237s__Benchmark46cc__48__ = 1; // Benchmark.cc:48 "This is a string %s"

// Start new namespace for generated ids and code
namespace GeneratedFunctions {

// Map of numerical ids to log message metadata
struct LogMetadata logId2Metadata[2] =
{
    {"Simple log message with 0 parameters", "Benchmark.cc", 45},
{"This is a string %s", "Benchmark.cc", 48}
};

// Map of numerical ids to compression functions
ssize_t
(*compressFnArray[2]) (Log::UncompressedEntry *re, char* out)
{
    compressArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__,
compressArgs__This32is32a32string3237s__Benchmark46cc__48__
};

// Map of numerical ids to decompression functions
void
(*decompressAndPrintFnArray[2]) (const char **in, FILE *outputFd,
                                        void (*aggFn)(const char*, ...))
{
    decompressPrintArgs__Simple32log32message32with32032parameters__Benchmark46cc__45__,
decompressPrintArgs__This32is32a32string3237s__Benchmark46cc__48__
};

// Total number of logIds. Can be used to bounds check array accesses.
size_t numLogIds = 2;

// Pop the unused gcc warnings
#pragma GCC diagnostic pop

}; // GeneratedFunctions

#endif /* BUFFER_STUFFER */
