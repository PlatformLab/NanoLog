/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   mainster.cc.cpp
 * Author: syang0
 *
 * Created on July 20, 2016, 5:31 AM
 */

#include <cstdlib>
#include <fstream>      // std::ifstream
#include <string>     // std::string, std::stoi
#include <stdint.h>
#include <string.h>     //memset

#include "json.h"

using namespace std;

struct CompressedEvent {
        uint8_t additionalFmtIdBytes:2;
        uint8_t additionalTimestampBytes:3;
        uint8_t numArgs:3;

        // After this comes the fmtId, (delta) timestamp,
        // and up to four 4-byte format string arguments.
        uint8_t data[];

        static uint32_t getMaxSize() {
            return 1 // metadata
                    + 4 // format
                    + 8 // timestamp
                    + 4*4; // 4-byte arguments
        }
    } __attribute__((packed));

// Obtained from rc01 testing; will put in the log file later, for now this.
static const double cyclesPerSec = 2933224277.572243;

double cycles2Micros(uint64_t cycles) {
    return (cycles/cyclesPerSec)*1e6;
}

double cycles2Nanos(uint64_t cycles) {
    return (cycles/cyclesPerSec)*1e9;
}
    
/*
 * 
 */
int main(int argc, char** argv) {
    if (argc < 4) {
        printf("Usage: ./demangle <logFile> <mappingFile> <linesToPrint>");
        exit(1);
    }

    int linesToPrint = std::stoi(argv[3]);
    std::ifstream in(argv[1], std::ifstream::binary);
    std::ifstream mappingFile(argv[2], std::ifstream::in);

    nlohmann::json mapping;
    mappingFile >> mapping;

    uint32_t numRead = 0;
    uint64_t accumulatedCycles= 0;
    while (!in.eof()) {
        CompressedEvent metadata;
        uint64_t timeDelta = 0;
        uint32_t fmtId = 0;
        uint32_t args[4];

        if (++numRead > linesToPrint)
            break;

        memset(args, sizeof(uint32_t), sizeof(args)/sizeof(uint32_t));
        in.read((char*)&metadata, sizeof(metadata));
        in.read((char*)&fmtId, metadata.additionalFmtIdBytes + 1);
        in.read((char*)&timeDelta, metadata.additionalTimestampBytes + 1);
        in.read((char*)&args, sizeof(uint32_t)*metadata.numArgs);
        accumulatedCycles += timeDelta;

        double totalTime_ns = cycles2Nanos(accumulatedCycles);
        double timeDelta_ns = cycles2Nanos(timeDelta);

        printf("%8.1f ns (+%6.1f ns): ", totalTime_ns, timeDelta_ns);
        printf(mapping["mappings"][fmtId].dump().c_str(),
                args[0], args[1], args[2], args[3]);
        printf("\r\n");
    }


//    while (cnt < numRecords) {
//        Event_p metadata;
//        uint32_t args[4];
//        in.read((char*)&metadata, sizeof(metadata));
//        in.read((char*)&args, sizeof(uint32_t)*metadata.numArgs);
//
////        printf("<%lu, %lu, %lu> +%lf µs", metadata.fmtId, metadata.numArgs,
////                metadata.timestamp, cycles2Micros(metadata.timestamp - lastTime)
//////                ,mapping["mappings"][0].dump().c_str());
////                );
////
////        for (int i = 0; i < metadata.numArgs; ++i) {
////            printf(", arg%d: %u", i, args[i]);
////        }
////        printf("\r\n");
//
//        // Trace all blips
////        if (metadata.timestamp - lastTime > 200) {
////            printf("Blip at %d (+ %d): %lf µs\r\n",
////                    cnt,
////                    cnt - lastBlip,
////                    cycles2Micros(metadata.timestamp - lastTime));
////            lastBlip = cnt;
////        }
//
//        double micros = cycles2Micros(metadata.timestamp - lastTime);
//        if (micros > 100 || lastArg0 != args[0] || lastArg1 != args[1]) {
//            printf("Blip at %10d: +%15lf µs, arg0:%10u, arg1:%10u\r\n",
//                    cnt,
//                    micros,
//                    args[0],
//                    args[1]);
//        }
//
//        lastTime = metadata.timestamp;
//        lastArg0 = args[0];
//        lastArg1 = args[1];
//        ++cnt;
//    }



    return 0;
}

