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

#include <cstdlib>
#include <fstream>

#include "BufferUtils.h"
#include "BufferStuffer.h"

using namespace BufferUtils;

/**
 * Simple program to decompress log files produced by the FastLogging System.
 * Note that this executable must be compiled with the same BufferStuffer.h
 * as the LogCompressor that generated the compressedLog for this to work.
 */
int main(int argc, char** argv) {
    uint32_t bufferSize = 1<<26;

    if (argc < 3) {
        printf("Usage: ./demangle <logFile> <linesToPrint>");
        exit(1);
    }

    char *scratchBufferSpace = static_cast<char*>(calloc(1, bufferSize));
    if (!scratchBufferSpace) {
        printf("Malloc of a %d byte array as a staging buffer "
                "for decompressing failed\r\n", bufferSize);
        exit(-1);
    }

    int linesToPrint = std::stoi(argv[2]);
    std::ifstream in(argv[1], std::ifstream::binary);

    int linesPrinted = 0;
    uint32_t lastFmtId = 0;
    uint64_t lastTimestamp = 0;
    while (!in.eof()) {
        if (linesToPrint > 0 && ++linesPrinted >= linesToPrint)
            break;

        EntryType nextType = BufferUtils::peekEntryType(in);

        if (nextType == EntryType::LOG_MSG) {
            DecompressedMetadata dm =
                BufferUtils::decompressMetadata(in, lastFmtId, lastTimestamp);

            decompressAndPrintFnArray[dm.fmtId](in);

            lastFmtId = dm.fmtId;
            lastTimestamp = dm.timestamp;

        } else if (nextType == EntryType::CHECKPOINT) {
            // Read in the rest of the checkpoint and don't process (for now)
            Checkpoint cp = BufferUtils::readCheckpoint(in);
            printf("Found a checkpoint\r\n");
        } else if (nextType == EntryType::INVALID) {
            // It's possible we hit a pad byte, double check.
            while(in.peek() == 0 && in.good())
                in.get();
        } else {
            printf ("Entry type read in metadata does not match anything "
                                                        "(%d)\r\n", nextType);
        }
    }

    printf("\r\n\r\nDecompression Complete after printing %d lines\r\n",
            linesPrinted);

    return 0;
}

