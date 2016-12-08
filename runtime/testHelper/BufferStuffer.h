/* Copyright (c) 2016 Stanford University
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

#include "FastLogger.h"
#include "Packer.h"

#include <fstream>     // for decompression
#include <string>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"

// Record code in an empty namespace(for debugging)
namespace {

void __syang0__fl__1(const char* fmtStr) {
    ;
    size_t allocSize = 0 + 0 + sizeof(BufferUtils::RecordEntry);
    BufferUtils::RecordEntry *re = reinterpret_cast<BufferUtils::RecordEntry*>(PerfUtils::FastLogger::__internal_reserveAlloc(allocSize));

    if (re == nullptr)
        return;

    re->fmtId = 1;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = 0;

    char *buffer = re->argData;

    // Record the primitives
    ;

    // Make the entry visible
    PerfUtils::FastLogger::__internal_finishAlloc(allocSize);
}


inline ssize_t
compressArgs1(BufferUtils::RecordEntry *re, char* out) {
    char *originalOutPtr = out;
    BufferUtils::TwoNibbles *nib = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += 0;

    char *args = re->argData;


	return out - originalOutPtr;
}

inline void
decompressPrintArg1(std::ifstream &in) {
	BufferUtils::TwoNibbles nib[0];
	in.read(reinterpret_cast<char*>(&nib), 0);



	const char *fmtString = "Simple log message with no parameters";
	const char *filename = "Benchmark.cc";
	const int linenum = 37;

	printf("Simple log message with no parameters");
}

} // end empty namespace

ssize_t (*compressFnArray[2])(BufferUtils::RecordEntry *re, char* out) {
	nullptr,
	compressArgs1
};

void (*decompressAndPrintFnArray[2])(std::ifstream &in) {
	nullptr,
	decompressPrintArg1
};

// Format Id to original Format String
const char* fmtId2Str[1] = {
	"Simple log message with no parameters"
};

// Pop -Wunused-function
#pragma GCC diagnostic pop

#endif /* BUFFER_STUFFER */
