
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

inline void __syang0__fl__A__mar46cc__293__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__A__mar46cc__293__;

    if (level > NanoLog::getLogLevel())
        return;

    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__A__mar46cc__293__;
    re->timestamp = timestamp;
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__A__mar46cc__293__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
decompressPrintArgs__A__mar46cc__293__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "A";
    const char *filename = "mar.cc";
    const int linenum = 293;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "A" "\r\n" );

    if (aggFn)
        (*aggFn)("A" );
}


inline void __syang0__fl__A__mar46h__1__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__A__mar46h__1__;

    if (level > NanoLog::getLogLevel())
        return;

    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__A__mar46h__1__;
    re->timestamp = timestamp;
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__A__mar46h__1__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
decompressPrintArgs__A__mar46h__1__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "A";
    const char *filename = "mar.h";
    const int linenum = 1;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "A" "\r\n" );

    if (aggFn)
        (*aggFn)("A" );
}


inline void __syang0__fl__B__mar46cc__294__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__B__mar46cc__294__;

    if (level > NanoLog::getLogLevel())
        return;

    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__B__mar46cc__294__;
    re->timestamp = timestamp;
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__B__mar46cc__294__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
decompressPrintArgs__B__mar46cc__294__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "B";
    const char *filename = "mar.cc";
    const int linenum = 294;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "B" "\r\n" );

    if (aggFn)
        (*aggFn)("B" );
}


inline void __syang0__fl__C__mar46cc__200__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__C__mar46cc__200__;

    if (level > NanoLog::getLogLevel())
        return;

    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__C__mar46cc__200__;
    re->timestamp = timestamp;
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__C__mar46cc__200__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
decompressPrintArgs__C__mar46cc__200__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "C";
    const char *filename = "mar.cc";
    const int linenum = 200;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "C" "\r\n" );

    if (aggFn)
        (*aggFn)("C" );
}


inline void __syang0__fl__D3237d__s46cc__100__(NanoLog::LogLevel level, const char* fmtStr , int arg0) {
    extern const uint32_t __fmtId__D3237d__s46cc__100__;

    if (level > NanoLog::getLogLevel())
        return;

    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    ;
    size_t allocSize = sizeof(arg0) +   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__D3237d__s46cc__100__;
    re->timestamp = timestamp;
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    	NanoLogInternal::Log::recordPrimitive(buffer, arg0);


    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__D3237d__s46cc__100__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
    char *originalOutPtr = out;

    // Allocate nibbles
    BufferUtils::TwoNibbles *nib = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += 1;

    char *args = re->argData;

    // Read back all the primitives
    	int arg0; std::memcpy(&arg0, args, sizeof(int)); args +=sizeof(int);


    // Pack all the primitives
    	nib[0].first = 0x0f & static_cast<uint8_t>(BufferUtils::pack(&out, arg0));


    if (false) {
        // memcpy all the strings without compression
        size_t stringBytes = re->entrySize - (sizeof(arg0) +  0)
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__D3237d__s46cc__100__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[1];
    memcpy(&nib, (*in), 1);
    (*in) += 1;

    // Unpack all the non-string argments
    	int arg0 = BufferUtils::unpack<int>(in, nib[0].first);


    // Find all the strings
    

    const char *fmtString = "D %d";
    const char *filename = "s.cc";
    const int linenum = 100;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "D %d" "\r\n" , arg0);

    if (aggFn)
        (*aggFn)("D %d" , arg0);
}


inline void __syang0__fl__E32374s3237424642lf__s46cc__100__(NanoLog::LogLevel level, const char* fmtStr , const char* arg0, int arg1, int arg2, double arg3) {
    extern const uint32_t __fmtId__E32374s3237424642lf__s46cc__100__;

    if (level > NanoLog::getLogLevel())
        return;

    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    size_t str0Len = 1 + strlen(arg0);;
    size_t allocSize = sizeof(arg1) + sizeof(arg2) + sizeof(arg3) +  str0Len +  sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__E32374s3237424642lf__s46cc__100__;
    re->timestamp = timestamp;
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    	NanoLogInternal::Log::recordPrimitive(buffer, arg1);
	NanoLogInternal::Log::recordPrimitive(buffer, arg2);
	NanoLogInternal::Log::recordPrimitive(buffer, arg3);


    // Record the strings (if any) at the end of the entry
    memcpy(buffer, arg0, str0Len); buffer += str0Len;*(reinterpret_cast<std::remove_const<typename std::remove_pointer<decltype(arg0)>::type>::type*>(buffer) - 1) = L'\0';

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__E32374s3237424642lf__s46cc__100__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
    char *originalOutPtr = out;

    // Allocate nibbles
    BufferUtils::TwoNibbles *nib = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += 2;

    char *args = re->argData;

    // Read back all the primitives
    	int arg1; std::memcpy(&arg1, args, sizeof(int)); args +=sizeof(int);
	int arg2; std::memcpy(&arg2, args, sizeof(int)); args +=sizeof(int);
	double arg3; std::memcpy(&arg3, args, sizeof(double)); args +=sizeof(double);


    // Pack all the primitives
    	nib[0].first = 0x0f & static_cast<uint8_t>(BufferUtils::pack(&out, arg1));
	nib[0].second = 0x0f & static_cast<uint8_t>(BufferUtils::pack(&out, arg2));
	nib[1].first = 0x0f & static_cast<uint8_t>(BufferUtils::pack(&out, arg3));


    if (true) {
        // memcpy all the strings without compression
        size_t stringBytes = re->entrySize - (sizeof(arg1) + sizeof(arg2) + sizeof(arg3) +  0)
                                            - sizeof(NanoLogInternal::Log::UncompressedEntry);
        if (stringBytes > 0) {
            memcpy(out, args, stringBytes);
            out += stringBytes;
        }
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__E32374s3237424642lf__s46cc__100__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[2];
    memcpy(&nib, (*in), 2);
    (*in) += 2;

    // Unpack all the non-string argments
    	int arg1 = BufferUtils::unpack<int>(in, nib[0].first);
	int arg2 = BufferUtils::unpack<int>(in, nib[0].second);
	double arg3 = BufferUtils::unpack<double>(in, nib[1].first);


    // Find all the strings
    
                const char* arg0 = reinterpret_cast<const char*>(*in);
                (*in) += (strlen(arg0) + 1)*sizeof(*arg0); // +1 for null terminator
            

    const char *fmtString = "E %4s %*.*lf";
    const char *filename = "s.cc";
    const int linenum = 100;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "E %4s %*.*lf" "\r\n" , arg0, arg1, arg2, arg3);

    if (aggFn)
        (*aggFn)("E %4s %*.*lf" , arg0, arg1, arg2, arg3);
}


inline void __syang0__fl__E__del46cc__199__(NanoLog::LogLevel level, const char* fmtStr ) {
    extern const uint32_t __fmtId__E__del46cc__199__;

    if (level > NanoLog::getLogLevel())
        return;

    uint64_t timestamp = PerfUtils::Cycles::rdtsc();
    ;
    size_t allocSize =   sizeof(NanoLogInternal::Log::UncompressedEntry);
    NanoLogInternal::Log::UncompressedEntry *re = reinterpret_cast<NanoLogInternal::Log::UncompressedEntry*>(NanoLogInternal::RuntimeLogger::reserveAlloc(allocSize));

    re->fmtId = __fmtId__E__del46cc__199__;
    re->timestamp = timestamp;
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    NanoLogInternal::RuntimeLogger::finishAlloc(allocSize);
}


inline ssize_t
compressArgs__E__del46cc__199__(NanoLogInternal::Log::UncompressedEntry *re, char* out) {
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
decompressPrintArgs__E__del46cc__199__ (const char **in,
                        FILE *outputFd,
                        void (*aggFn)(const char*, ...)) {
    BufferUtils::TwoNibbles nib[0];
    memcpy(&nib, (*in), 0);
    (*in) += 0;

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "E";
    const char *filename = "del.cc";
    const int linenum = 199;
    const NanoLog::LogLevel logLevel = DEBUG;

    if (outputFd)
        fprintf(outputFd, "E" "\r\n" );

    if (aggFn)
        (*aggFn)("E" );
}


} // end empty namespace

// Assignment of numerical ids to format NANO_LOG occurrences
extern const int __fmtId__A__mar46cc__293__ = 0; // mar.cc:293 "A"
extern const int __fmtId__A__mar46h__1__ = 1; // mar.h:1 "A"
extern const int __fmtId__B__mar46cc__294__ = 2; // mar.cc:294 "B"
extern const int __fmtId__C__mar46cc__200__ = 3; // mar.cc:200 "C"
extern const int __fmtId__D3237d__s46cc__100__ = 4; // s.cc:100 "D %d"
extern const int __fmtId__E32374s3237424642lf__s46cc__100__ = 5; // s.cc:100 "E %4s %*.*lf"
extern const int __fmtId__E__del46cc__199__ = 6; // del.cc:199 "E"

// Start new namespace for generated ids and code
namespace GeneratedFunctions {

// Map of numerical ids to log message metadata
struct LogMetadata logId2Metadata[7] =
{
    {"A", "mar.cc", 293, DEBUG},
{"A", "mar.h", 1, DEBUG},
{"B", "mar.cc", 294, DEBUG},
{"C", "mar.cc", 200, DEBUG},
{"D %d", "s.cc", 100, DEBUG},
{"E %4s %*.*lf", "s.cc", 100, DEBUG},
{"E", "del.cc", 199, DEBUG}
};

// Map of numerical ids to compression functions
ssize_t
(*compressFnArray[7]) (NanoLogInternal::Log::UncompressedEntry *re, char* out)
{
    compressArgs__A__mar46cc__293__,
compressArgs__A__mar46h__1__,
compressArgs__B__mar46cc__294__,
compressArgs__C__mar46cc__200__,
compressArgs__D3237d__s46cc__100__,
compressArgs__E32374s3237424642lf__s46cc__100__,
compressArgs__E__del46cc__199__
};

// Map of numerical ids to decompression functions
void
(*decompressAndPrintFnArray[7]) (const char **in,
                                        FILE *outputFd,
                                        void (*aggFn)(const char*, ...))
{
    decompressPrintArgs__A__mar46cc__293__,
decompressPrintArgs__A__mar46h__1__,
decompressPrintArgs__B__mar46cc__294__,
decompressPrintArgs__C__mar46cc__200__,
decompressPrintArgs__D3237d__s46cc__100__,
decompressPrintArgs__E32374s3237424642lf__s46cc__100__,
decompressPrintArgs__E__del46cc__199__
};

// Writes the metadata needed by the decompressor to interpret the log messages
// generated by compressFn.
long int writeDictionary(char *buffer, char *endOfBuffer) {
    using namespace NanoLogInternal::Log;
    char *startPos = buffer;
    
{
    // mar.cc:293 - "A"
    FormatMetadata *fm;
    PrintFragment *pf;
    if (buffer + sizeof(FormatMetadata) + 7 >= endOfBuffer)
        return -1;

    fm = reinterpret_cast<FormatMetadata*>(buffer);
    buffer += sizeof(FormatMetadata);

    fm->numNibbles = 0;
    fm->numPrintFragments = 1;
    fm->logLevel = DEBUG;
    fm->lineNumber = 293;
    fm->filenameLength = 7;

    buffer = stpcpy(buffer, "mar.cc") + 1;

            // Fragment 0
            if (buffer + sizeof(PrintFragment)
                        + sizeof("A")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = NONE;
            pf->hasDynamicWidth = false;
            pf->hasDynamicPrecision = false;
            pf->fragmentLength = sizeof("A")/sizeof(char);

            buffer = stpcpy(buffer, "A") + 1;
}




{
    // mar.h:1 - "A"
    FormatMetadata *fm;
    PrintFragment *pf;
    if (buffer + sizeof(FormatMetadata) + 6 >= endOfBuffer)
        return -1;

    fm = reinterpret_cast<FormatMetadata*>(buffer);
    buffer += sizeof(FormatMetadata);

    fm->numNibbles = 0;
    fm->numPrintFragments = 1;
    fm->logLevel = DEBUG;
    fm->lineNumber = 1;
    fm->filenameLength = 6;

    buffer = stpcpy(buffer, "mar.h") + 1;

            // Fragment 0
            if (buffer + sizeof(PrintFragment)
                        + sizeof("A")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = NONE;
            pf->hasDynamicWidth = false;
            pf->hasDynamicPrecision = false;
            pf->fragmentLength = sizeof("A")/sizeof(char);

            buffer = stpcpy(buffer, "A") + 1;
}




{
    // mar.cc:294 - "B"
    FormatMetadata *fm;
    PrintFragment *pf;
    if (buffer + sizeof(FormatMetadata) + 7 >= endOfBuffer)
        return -1;

    fm = reinterpret_cast<FormatMetadata*>(buffer);
    buffer += sizeof(FormatMetadata);

    fm->numNibbles = 0;
    fm->numPrintFragments = 1;
    fm->logLevel = DEBUG;
    fm->lineNumber = 294;
    fm->filenameLength = 7;

    buffer = stpcpy(buffer, "mar.cc") + 1;

            // Fragment 0
            if (buffer + sizeof(PrintFragment)
                        + sizeof("B")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = NONE;
            pf->hasDynamicWidth = false;
            pf->hasDynamicPrecision = false;
            pf->fragmentLength = sizeof("B")/sizeof(char);

            buffer = stpcpy(buffer, "B") + 1;
}




{
    // mar.cc:200 - "C"
    FormatMetadata *fm;
    PrintFragment *pf;
    if (buffer + sizeof(FormatMetadata) + 7 >= endOfBuffer)
        return -1;

    fm = reinterpret_cast<FormatMetadata*>(buffer);
    buffer += sizeof(FormatMetadata);

    fm->numNibbles = 0;
    fm->numPrintFragments = 1;
    fm->logLevel = DEBUG;
    fm->lineNumber = 200;
    fm->filenameLength = 7;

    buffer = stpcpy(buffer, "mar.cc") + 1;

            // Fragment 0
            if (buffer + sizeof(PrintFragment)
                        + sizeof("C")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = NONE;
            pf->hasDynamicWidth = false;
            pf->hasDynamicPrecision = false;
            pf->fragmentLength = sizeof("C")/sizeof(char);

            buffer = stpcpy(buffer, "C") + 1;
}




{
    // s.cc:100 - "D %d"
    FormatMetadata *fm;
    PrintFragment *pf;
    if (buffer + sizeof(FormatMetadata) + 5 >= endOfBuffer)
        return -1;

    fm = reinterpret_cast<FormatMetadata*>(buffer);
    buffer += sizeof(FormatMetadata);

    fm->numNibbles = 1;
    fm->numPrintFragments = 1;
    fm->logLevel = DEBUG;
    fm->lineNumber = 100;
    fm->filenameLength = 5;

    buffer = stpcpy(buffer, "s.cc") + 1;

            // Fragment 0
            if (buffer + sizeof(PrintFragment)
                        + sizeof("D %d")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = int_t;
            pf->hasDynamicWidth = false;
            pf->hasDynamicPrecision = false;
            pf->fragmentLength = sizeof("D %d")/sizeof(char);

            buffer = stpcpy(buffer, "D %d") + 1;
}




{
    // s.cc:100 - "E %4s %*.*lf"
    FormatMetadata *fm;
    PrintFragment *pf;
    if (buffer + sizeof(FormatMetadata) + 5 >= endOfBuffer)
        return -1;

    fm = reinterpret_cast<FormatMetadata*>(buffer);
    buffer += sizeof(FormatMetadata);

    fm->numNibbles = 3;
    fm->numPrintFragments = 2;
    fm->logLevel = DEBUG;
    fm->lineNumber = 100;
    fm->filenameLength = 5;

    buffer = stpcpy(buffer, "s.cc") + 1;

            // Fragment 0
            if (buffer + sizeof(PrintFragment)
                        + sizeof("E %4s")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = const_char_ptr_t;
            pf->hasDynamicWidth = false;
            pf->hasDynamicPrecision = false;
            pf->fragmentLength = sizeof("E %4s")/sizeof(char);

            buffer = stpcpy(buffer, "E %4s") + 1;

            // Fragment 1
            if (buffer + sizeof(PrintFragment)
                        + sizeof(" %*.*lf")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = double_t;
            pf->hasDynamicWidth = true;
            pf->hasDynamicPrecision = true;
            pf->fragmentLength = sizeof(" %*.*lf")/sizeof(char);

            buffer = stpcpy(buffer, " %*.*lf") + 1;
}




{
    // del.cc:199 - "E"
    FormatMetadata *fm;
    PrintFragment *pf;
    if (buffer + sizeof(FormatMetadata) + 7 >= endOfBuffer)
        return -1;

    fm = reinterpret_cast<FormatMetadata*>(buffer);
    buffer += sizeof(FormatMetadata);

    fm->numNibbles = 0;
    fm->numPrintFragments = 1;
    fm->logLevel = DEBUG;
    fm->lineNumber = 199;
    fm->filenameLength = 7;

    buffer = stpcpy(buffer, "del.cc") + 1;

            // Fragment 0
            if (buffer + sizeof(PrintFragment)
                        + sizeof("E")/sizeof(char) >= endOfBuffer)
                return -1;

            pf = reinterpret_cast<PrintFragment*>(buffer);
            buffer += sizeof(PrintFragment);

            pf->argType = NONE;
            pf->hasDynamicWidth = false;
            pf->hasDynamicPrecision = false;
            pf->fragmentLength = sizeof("E")/sizeof(char);

            buffer = stpcpy(buffer, "E") + 1;
}


    return buffer - startPos;
}

// Total number of logIds. Can be used to bounds check array accesses.
size_t numLogIds = 7;

// Pop the unused gcc warnings
#pragma GCC diagnostic pop

}; // GeneratedFunctions

#endif /* BUFFER_STUFFER */
