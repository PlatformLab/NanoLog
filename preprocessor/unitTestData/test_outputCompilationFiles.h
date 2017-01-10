
#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include "FastLogger.h"
#include "Packer.h"

#include <fstream>
#include <string>

// Since some of the functions/variables output below are for debugging purposes
// only (i.e. they're not used in their current form), squash all gcc complaints
// about unused variables/functions.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

// Start an empty namespace to enclose all the record(debug)/compress/decompress
// functions
namespace {

inline void __syang0__fl__A__mar46h__1__(const char* fmtStr ) {
    extern const uint32_t __fmtId__A__mar46h__1__;

    ;
    size_t allocSize =   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(PerfUtils::FastLogger::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__A__mar46h__1__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = 0;

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    PerfUtils::FastLogger::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__A__mar46h__1__(BufferUtils::UncompressedLogEntry *re, char* out) {
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
decompressPrintArgs__A__mar46h__1__ (std::ifstream &in) {
    BufferUtils::TwoNibbles nib[0];
    in.read(reinterpret_cast<char*>(&nib), 0);

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "A";
    const char *filename = "mar.h";
    const int linenum = 1;

    printf("A" "\r\n" );
}


inline void __syang0__fl__C__mar46cc__200__(const char* fmtStr ) {
    extern const uint32_t __fmtId__C__mar46cc__200__;

    ;
    size_t allocSize =   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(PerfUtils::FastLogger::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__C__mar46cc__200__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = 0;

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    PerfUtils::FastLogger::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__C__mar46cc__200__(BufferUtils::UncompressedLogEntry *re, char* out) {
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
decompressPrintArgs__C__mar46cc__200__ (std::ifstream &in) {
    BufferUtils::TwoNibbles nib[0];
    in.read(reinterpret_cast<char*>(&nib), 0);

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "C";
    const char *filename = "mar.cc";
    const int linenum = 200;

    printf("C" "\r\n" );
}


inline void __syang0__fl__B__mar46cc__294__(const char* fmtStr ) {
    extern const uint32_t __fmtId__B__mar46cc__294__;

    ;
    size_t allocSize =   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(PerfUtils::FastLogger::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__B__mar46cc__294__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = 0;

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    PerfUtils::FastLogger::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__B__mar46cc__294__(BufferUtils::UncompressedLogEntry *re, char* out) {
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
decompressPrintArgs__B__mar46cc__294__ (std::ifstream &in) {
    BufferUtils::TwoNibbles nib[0];
    in.read(reinterpret_cast<char*>(&nib), 0);

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "B";
    const char *filename = "mar.cc";
    const int linenum = 294;

    printf("B" "\r\n" );
}


inline void __syang0__fl__E__del46cc__199__(const char* fmtStr ) {
    extern const uint32_t __fmtId__E__del46cc__199__;

    ;
    size_t allocSize =   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(PerfUtils::FastLogger::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__E__del46cc__199__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = 0;

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    PerfUtils::FastLogger::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__E__del46cc__199__(BufferUtils::UncompressedLogEntry *re, char* out) {
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
decompressPrintArgs__E__del46cc__199__ (std::ifstream &in) {
    BufferUtils::TwoNibbles nib[0];
    in.read(reinterpret_cast<char*>(&nib), 0);

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "E";
    const char *filename = "del.cc";
    const int linenum = 199;

    printf("E" "\r\n" );
}


inline void __syang0__fl__A__mar46cc__293__(const char* fmtStr ) {
    extern const uint32_t __fmtId__A__mar46cc__293__;

    ;
    size_t allocSize =   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(PerfUtils::FastLogger::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__A__mar46cc__293__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = 0;

    char *buffer = re->argData;

    // Record the non-string arguments
    

    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    PerfUtils::FastLogger::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__A__mar46cc__293__(BufferUtils::UncompressedLogEntry *re, char* out) {
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
decompressPrintArgs__A__mar46cc__293__ (std::ifstream &in) {
    BufferUtils::TwoNibbles nib[0];
    in.read(reinterpret_cast<char*>(&nib), 0);

    // Unpack all the non-string argments
    

    // Find all the strings
    

    const char *fmtString = "A";
    const char *filename = "mar.cc";
    const int linenum = 293;

    printf("A" "\r\n" );
}


inline void __syang0__fl__D3237d__s46cc__100__(const char* fmtStr , int arg0) {
    extern const uint32_t __fmtId__D3237d__s46cc__100__;

    ;
    size_t allocSize = sizeof(arg0) +   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(PerfUtils::FastLogger::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId__D3237d__s46cc__100__;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = 1;

    char *buffer = re->argData;

    // Record the non-string arguments
    	BufferUtils::recordPrimitive(buffer, arg0);


    // Record the strings (if any) at the end of the entry
    

    // Make the entry visible
    PerfUtils::FastLogger::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs__D3237d__s46cc__100__(BufferUtils::UncompressedLogEntry *re, char* out) {
    char *originalOutPtr = out;

    // Allocate nibbles
    BufferUtils::TwoNibbles *nib = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += 1;

    char *args = re->argData;

    // Read back all the primitives
    	int arg0 = *reinterpret_cast<int*>(args); args +=sizeof(int);


    // Pack all the primitives
    	nib[0].first = 0x0f & static_cast<uint8_t>(BufferUtils::pack(&out, arg0));


    // memcpy all the strings without compression
    size_t stringBytes = re->entrySize - (sizeof(arg0) +  0)
                                        - sizeof(BufferUtils::UncompressedLogEntry);
    if (stringBytes > 0) {
        memcpy(out, args, stringBytes);
        out += stringBytes;
    }

    return out - originalOutPtr;
}


inline void
decompressPrintArgs__D3237d__s46cc__100__ (std::ifstream &in) {
    BufferUtils::TwoNibbles nib[1];
    in.read(reinterpret_cast<char*>(&nib), 1);

    // Unpack all the non-string argments
    	int arg0 = BufferUtils::unpack<int>(in, nib[0].first);


    // Find all the strings
    

    const char *fmtString = "D %d";
    const char *filename = "s.cc";
    const int linenum = 100;

    printf("D %d" "\r\n" , arg0);
}


} // end empty namespace

// Assignment of numerical ids to format FAST_LOG occurrences
extern const int __fmtId__A__mar46h__1__ = 0; // mar.h:1 "A"
extern const int __fmtId__C__mar46cc__200__ = 1; // mar.cc:200 "C"
extern const int __fmtId__B__mar46cc__294__ = 2; // mar.cc:294 "B"
extern const int __fmtId__E__del46cc__199__ = 3; // del.cc:199 "E"
extern const int __fmtId__A__mar46cc__293__ = 4; // mar.cc:293 "A"
extern const int __fmtId__D3237d__s46cc__100__ = 5; // s.cc:100 "D %d"


// Map of numerical ids to compression functions
ssize_t
(*compressFnArray[6]) (BufferUtils::UncompressedLogEntry *re, char* out)
{
    compressArgs__A__mar46h__1__,
compressArgs__C__mar46cc__200__,
compressArgs__B__mar46cc__294__,
compressArgs__E__del46cc__199__,
compressArgs__A__mar46cc__293__,
compressArgs__D3237d__s46cc__100__
};

// Map of numerical ids to decompression functions
void
(*decompressAndPrintFnArray[6]) (std::ifstream &in)
{
    decompressPrintArgs__A__mar46h__1__,
decompressPrintArgs__C__mar46cc__200__,
decompressPrintArgs__B__mar46cc__294__,
decompressPrintArgs__E__del46cc__199__,
decompressPrintArgs__A__mar46cc__293__,
decompressPrintArgs__D3237d__s46cc__100__
};

// Pop the unused gcc warnings
#pragma GCC diagnostic pop

#endif /* BUFFER_STUFFER */
