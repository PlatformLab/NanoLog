/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/*
 * File:   BufferUtils.h
 * Author: syang0
 *
 * Created on October 7, 2016, 3:41 AM
 */

#include <ctime>

//TODO(syang0) Maybe this should be in a cc file so we don't include this everyhwere...
#include <assert.h>
#include <fstream>

#include "Cycles.h"
#include "Packer.h"

#ifndef BUFFERUTILS_H
#define BUFFERUTILS_H

namespace BufferUtils {

    // Describes an entry within the staging buffer
    struct RecordEntry {
        // Stores the format ID assigned to the log message by the preprocessor
        // component. A value of 0 is used to indicate an invalid entry.
        uint32_t fmtId;

        // Stores the rdtsc value at the time of log function invocation
        uint64_t timestamp;

        // Number of bytes the entry takes up (includes this metadata)
        uint32_t entrySize:24;

        uint32_t argMetadataBytes:8;

        // After this comes regular, uncompressed arguments related to the
        // log message
        //TODO(syang0) Maybe omit this since it's not valid at decompression phase
        char argData[];
    };

    enum EntryType : uint8_t {
        INVALID = 0,
        LOG_MSG = 1,
        CHECKPOINT = 2,
        END_OF_FILE = 3
    };

    struct Nibble {
        uint8_t first:4;
        uint8_t second:4;
    } __attribute__((packed));

    // Output Buffer metdata that describes the entry coming afterwards.
    struct CompressedMetadata {
        uint8_t entryType:2;
        uint8_t additionalFmtIdBytes:2;
        uint8_t additionalTimestampBytes:3;

        // After this, format id that's 1 + additionalFmtIdBytes long,
        // timestamp diff that's 1 + additionalTimeStampBytes long,
        // and then the arguments. The exact layout defined in the python scripts.

    } __attribute__((packed));

    struct DecompressedMetadata {
        uint8_t type;
        uint32_t fmtId;
        uint64_t timestamp;
    };

    struct Checkpoint : CompressedMetadata {
        uint64_t rdtsc;
        time_t unixTime;
        double cyclesPerSecond;
        void *relativePointer;
    } __attribute__((packed));

    template<typename T>
    static inline void
    recordPrimitive(char* &buff, T t) {
        *(reinterpret_cast<T*>(buff)) = t;
        buff += sizeof(T);
    }

    template<typename T>
    static inline T*
    interpretAndBump(char** buff) {
        T* val = reinterpret_cast<T*>(*buff);
        *buff += sizeof(T);
        return val;
    }
    inline void
    recordMetadata(RecordEntry* m, uint32_t fmtId, uint32_t maxArgSize, uint32_t compressedArgsMetaBytes) {
        m->fmtId = fmtId;
        m->timestamp = PerfUtils::Cycles::rdtsc();
        //TODO(syang0) This seems kinda retarded.... Because the +=sizeof(RecordEntry) is completely hidden and not obvious
        m->entrySize = maxArgSize += sizeof(RecordEntry);
        m->argMetadataBytes = compressedArgsMetaBytes;
    }

    inline void
    compressMetadata(RecordEntry *m, char** out, uint64_t lastTimestamp, uint32_t lastFmtId) {
        CompressedMetadata *mo = interpretAndBump<CompressedMetadata>(out);
        mo->entryType = EntryType::LOG_MSG;

        mo->additionalFmtIdBytes = BufferUtils::pack(out, m->fmtId - lastFmtId) - 1; // TODO check for when fmtId = 0
        mo->additionalTimestampBytes = BufferUtils::pack(out, m->timestamp - lastTimestamp) - 1;
    }

    inline DecompressedMetadata
    decompressMetadata(std::ifstream &in, uint32_t lastFmtId, uint64_t lastTimestamp) {
        DecompressedMetadata dm;
        CompressedMetadata cm;
        in.read(reinterpret_cast<char*>(&cm), sizeof(CompressedMetadata));

        dm.type = cm.entryType;
        dm.fmtId = BufferUtils::unpack<uint32_t>(in, cm.additionalFmtIdBytes + 1);
        dm.timestamp = BufferUtils::unpack<uint64_t>(in, cm.additionalTimestampBytes + 1);

        dm.fmtId += lastFmtId;
        dm.timestamp += lastTimestamp;

        assert(cm.entryType == EntryType::LOG_MSG);
        return dm;
    }

    inline EntryType
    peekEntryType(std::ifstream &in) {
        // CompressedMetadata only takes a byte, so we can peek it and determine
        // the entry type.
        static_assert(sizeof(CompressedMetadata) == 1,
                "CompressedMetadata should only be 1 byte "
                "so we can peek() it at decompression");

        int type = in.peek();
        if (type == EOF)
            return EntryType::END_OF_FILE;
        if (type < 0 || type > 255)
            return EntryType::INVALID;

        CompressedMetadata *cm = reinterpret_cast<CompressedMetadata*>(&type);
        return EntryType(cm->entryType);
    }

    // Inserts an uncompressed checkpoint into an output buffer. This a fairly
    // expensive operation in terms of storage size, so only use it when log files
    // are bifercated or explicit resynchronization is needed
    static inline bool
    insertCheckpoint(char** out, char *outLimit, void* relativePointer) {
        if (outLimit - *out < sizeof(Checkpoint))
            return false;

        Checkpoint *ck = interpretAndBump<Checkpoint>(out);
        ck->entryType = EntryType::CHECKPOINT;
        ck->rdtsc = PerfUtils::Cycles::rdtsc();
        ck->unixTime = std::time(nullptr);
        ck->cyclesPerSecond = PerfUtils::Cycles::getCyclesPerSec();
        ck->relativePointer = relativePointer;

        return true;
    }

    inline Checkpoint
    readCheckpoint(std::ifstream &in) {
        Checkpoint cp;
        in.read(reinterpret_cast<char*>(&cp), sizeof(Checkpoint));
        return cp;
    }

}; /* Buffer Utils */

#endif /* BUFFERUTILS_H */

