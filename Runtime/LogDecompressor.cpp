/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   dmangler.cpp
 * Author: syang0
 *
 * Created on October 12, 2016, 1:49 AM
 */

#include <cstdlib>
#include <fstream>          // std::ifstream

#include "BufferUtils.h"
#include "BufferStuffer.h"

using namespace BufferUtils;

/*
 *
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
                "for decompressing failed\r\n", 1<<26);
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
            DecompressedMetadata dm = BufferUtils::decompressMetadata(in, lastFmtId, lastTimestamp);

            printf("\r\nDealing with fmtId=%u\r\n", dm.fmtId);

            // Decompress can fail if we haven't read enough into the buffer
            decompressAndPrintFnArray[dm.fmtId](in);

            lastFmtId = dm.fmtId;
            lastTimestamp = dm.timestamp;

        } else if (nextType == EntryType::CHECKPOINT) {
            // Read in the rest of the checkpoint and process
            Checkpoint cp = BufferUtils::readCheckpoint(in);
            printf("Found a checkpoint\r\n");
        } else if (nextType == EntryType::INVALID) {
            // It's possible we hit a pad byte, double check.

            //TODO(syang0) Is this really where this logic should go?
            //TODO(syang0) This can be done faster if we know the file will
            // be offset by 512 bytes at a time.
            while(in.peek() == 0 && in.good()) {
                in.get();
            }
        } else {
            printf("Entry type read in metadata does not match anything...at %d\r\n", nextType);
        }
    }

    printf("\r\n\r\nDecompression Complete\r\n");


    return 0;
}

