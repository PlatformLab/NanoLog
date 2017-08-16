
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
  NanoLog::LogLevel logLevel;
};

// Start an empty namespace to enclose all the record(debug)/compress/decompress
// and support functions
namespace {

using namespace NanoLog::LogLevels;

inline void __syang0__fl__Error32Level__testHelper47client46cc__25__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__Error32Level__testHelper47client46cc__25__;

    if (level > NanoLog::getLogLevel())
        return;

    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__Error32Level__testHelper47client46cc__25__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__Error32Level__testHelper47client46cc__25__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__Error32Level__testHelper47client46cc__25__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "Error Level";
    const char *filename = "testHelper/client.cc";
    const int linenum = 25;
    const NanoLog::LogLevel logLevel = ERROR;

    if (outputFd)
        fprintf(outputFd, "Error Level" "\r\n" );

    if (aggFn)
        (*aggFn)("Error Level" );
}


inline void __syang0__fl__Warning32Level__testHelper47client46cc__24__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__Warning32Level__testHelper47client46cc__24__;

    if (level > NanoLog::getLogLevel())
        return;

    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__Warning32Level__testHelper47client46cc__24__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__Warning32Level__testHelper47client46cc__24__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__Warning32Level__testHelper47client46cc__24__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "Warning Level";
    const char *filename = "testHelper/client.cc";
    const int linenum = 24;
    const NanoLog::LogLevel logLevel = WARNING;

    if (outputFd)
        fprintf(outputFd, "Warning Level" "\r\n" );

    if (aggFn)
        (*aggFn)("Warning Level" );
}


inline void __syang0__fl__Notice32Level__testHelper47client46cc__23__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__Notice32Level__testHelper47client46cc__23__;

    if (level > NanoLog::getLogLevel())
        return;

    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__Notice32Level__testHelper47client46cc__23__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__Notice32Level__testHelper47client46cc__23__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__Notice32Level__testHelper47client46cc__23__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "Notice Level";
    const char *filename = "testHelper/client.cc";
    const int linenum = 23;
    const NanoLog::LogLevel logLevel = NOTICE;

    if (outputFd)
        fprintf(outputFd, "Notice Level" "\r\n" );

    if (aggFn)
        (*aggFn)("Notice Level" );
}


inline void __syang0__fl__Debug32level__testHelper47client46cc__22__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__Debug32level__testHelper47client46cc__22__;

    if (level > NanoLog::getLogLevel())
        return;

    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__Debug32level__testHelper47client46cc__22__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__Debug32level__testHelper47client46cc__22__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__Debug32level__testHelper47client46cc__22__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "Debug level";
    const char *filename = "testHelper/client.cc";
    const int linenum = 22;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "Debug level" "\r\n" );

    if (aggFn)
        (*aggFn)("Debug level" );
}


inline void __syang0__fl__Simple32log32message32with32032parameters__testHelper47client46cc__19__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__Simple32log32message32with32032parameters__testHelper47client46cc__19__;

    if (level > NanoLog::getLogLevel())
        return;

    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__Simple32log32message32with32032parameters__testHelper47client46cc__19__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__Simple32log32message32with32032parameters__testHelper47client46cc__19__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__Simple32log32message32with32032parameters__testHelper47client46cc__19__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "Simple log message with 0 parameters";
    const char *filename = "testHelper/client.cc";
    const int linenum = 19;
    const NanoLog::LogLevel logLevel = NOTICE;

    if (outputFd)
        fprintf(outputFd, "Simple log message with 0 parameters" "\r\n" );

    if (aggFn)
        (*aggFn)("Simple log message with 0 parameters" );
}


inline void __syang0__fl__This32is32a32string3237s__testHelper47client46cc__20__(NanoLog::LogLevel level, const char* fmtStr , const char* arg0) {
    extern const uint32_t __fmtId__This32is32a32string3237s__testHelper47client46cc__20__;

    if (level > NanoLog::getLogLevel())
        return;

    size_t str0Len = strlen(arg0);;
    size_t allocSize =  str0Len + 1 +  sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__This32is32a32string3237s__testHelper47client46cc__20__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    memcpy(buffer, arg0, str0Len); buffer += str0Len; *buffer = '\0'; buffer++;

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__This32is32a32string3237s__testHelper47client46cc__20__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__This32is32a32string3237s__testHelper47client46cc__20__ (const char **in,
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
    const char *filename = "testHelper/client.cc";
    const int linenum = 20;
    const NanoLog::LogLevel logLevel = NOTICE;

    if (outputFd)
        fprintf(outputFd, "This is a string %s" "\r\n" , arg0);

    if (aggFn)
        (*aggFn)("This is a string %s" , arg0);
}


} // end empty namespace

// Assignment of numerical ids to format NANO_LOG occurrences
extern const int __fmtId__Error32Level__testHelper47client46cc__25__ = 0; // testHelper/client.cc:25 "Error Level"
extern const int __fmtId__Warning32Level__testHelper47client46cc__24__ = 1; // testHelper/client.cc:24 "Warning Level"
extern const int __fmtId__Notice32Level__testHelper47client46cc__23__ = 2; // testHelper/client.cc:23 "Notice Level"
extern const int __fmtId__Debug32level__testHelper47client46cc__22__ = 3; // testHelper/client.cc:22 "Debug level"
extern const int __fmtId__Simple32log32message32with32032parameters__testHelper47client46cc__19__ = 4; // testHelper/client.cc:19 "Simple log message with 0 parameters"
extern const int __fmtId__This32is32a32string3237s__testHelper47client46cc__20__ = 5; // testHelper/client.cc:20 "This is a string %s"

// Start new namespace for generated ids and code
namespace GeneratedFunctions {

// Map of numerical ids to log message metadata
struct LogMetadata logId2Metadata[6] =
{
    {"Error Level", "testHelper/client.cc", 25, ERROR},
{"Warning Level", "testHelper/client.cc", 24, WARNING},
{"Notice Level", "testHelper/client.cc", 23, NOTICE},
{"Debug level", "testHelper/client.cc", 22, DEBUG},
{"Simple log message with 0 parameters", "testHelper/client.cc", 19, NOTICE},
{"This is a string %s", "testHelper/client.cc", 20, NOTICE}
};

// Map of numerical ids to compression functions
ssize_t
(*compressFnArray[6]) (NanoLogInternal::Log::UncompressedEntry *re, char* out)
{
    compressArgs__Error32Level__testHelper47client46cc__25__,
compressArgs__Warning32Level__testHelper47client46cc__24__,
compressArgs__Notice32Level__testHelper47client46cc__23__,
compressArgs__Debug32level__testHelper47client46cc__22__,
compressArgs__Simple32log32message32with32032parameters__testHelper47client46cc__19__,
compressArgs__This32is32a32string3237s__testHelper47client46cc__20__
};

// Map of numerical ids to decompression functions
void
(*decompressAndPrintFnArray[6]) (const char **in,
                                        FILE *outputFd,
                                        void (*aggFn)(const char*, ...))
{
    decompressPrintArgs__Error32Level__testHelper47client46cc__25__,
decompressPrintArgs__Warning32Level__testHelper47client46cc__24__,
decompressPrintArgs__Notice32Level__testHelper47client46cc__23__,
decompressPrintArgs__Debug32level__testHelper47client46cc__22__,
decompressPrintArgs__Simple32log32message32with32032parameters__testHelper47client46cc__19__,
decompressPrintArgs__This32is32a32string3237s__testHelper47client46cc__20__
};

// Total number of logIds. Can be used to bounds check array accesses.
size_t numLogIds = 6;

// Pop the unused gcc warnings
#pragma GCC diagnostic pop

}; // GeneratedFunctions

#endif /* BUFFER_STUFFER */
