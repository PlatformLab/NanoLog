/* Copyright (c) 2016-2020 Stanford University
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

#include <fstream>
#include <iostream>
#include <iosfwd>
#include <cstdio>
#include <vector>
#include <sstream>

#include "gtest/gtest.h"

#include "TestUtil.h"

#include "RuntimeLogger.h"
#include "Packer.h"
#include "Log.h"
#include "GeneratedCode.h"


extern int __fmtId__Simple32log32message32with32032parameters__testHelper47client46cc__20__;
extern int __fmtId__This32is32a32string3237s__testHelper47client46cc__21__;

extern int __fmtId__I32have32an32integer3237d__testHelper47client46cc__28__; // testHelper/client.cc:28 "I have an integer %d"
extern int __fmtId__I32have32a32uint6495t3237lu__testHelper47client46cc__29__; // testHelper/client.cc:29 "I have a uint64_t %lu"
extern int __fmtId__I32have32a32double3237lf__testHelper47client46cc__30__; // testHelper/client.cc:30 "I have a double %lf"
extern int __fmtId__I32have32a32couple32of32things3237d443237f443237u443237s__testHelper47client46cc__31__; // testHelper/client.cc:31 "I have a couple of things %d, %f, %u, %s"


namespace {

using namespace PerfUtils;
using namespace NanoLogInternal;
using namespace Log;

void stopCompressionThread() {
    {
        std::lock_guard<std::mutex> lock(
                RuntimeLogger::nanoLogSingleton.condMutex);
        RuntimeLogger::nanoLogSingleton.compressionThreadShouldExit = true;
        RuntimeLogger::nanoLogSingleton.workAdded.notify_all();
    }

    if (RuntimeLogger::nanoLogSingleton.compressionThread.joinable()) {
        RuntimeLogger::nanoLogSingleton.compressionThread.join();
    }
}

void restartCompressionThread() {
    stopCompressionThread();

    RuntimeLogger::nanoLogSingleton.compressionThreadShouldExit = false;
    RuntimeLogger::nanoLogSingleton.compressionThread =
            std::thread(&RuntimeLogger::compressionThreadMain,
                        &RuntimeLogger::nanoLogSingleton);
}

// The fixture for testing class Foo.
class LogTest : public ::testing::Test {
protected:
// Note, if the tests ever fail to compile due to these symbols not being
// found, it's most likely that someone updated the testHelper/main.cc file.
// In this case, check testHelper/GeneratedCode.cc for updated values and
// change the declarations above and uses below to match.
int noParamsId = __fmtId__Simple32log32message32with32032parameters__testHelper47client46cc__20__;
int stringParamId = __fmtId__This32is32a32string3237s__testHelper47client46cc__21__;

int integerParamId = __fmtId__I32have32an32integer3237d__testHelper47client46cc__28__;
int uint64_tParamId = __fmtId__I32have32a32uint6495t3237lu__testHelper47client46cc__29__;
int doubleParamId = __fmtId__I32have32a32double3237lf__testHelper47client46cc__30__;
int mixParamId = __fmtId__I32have32a32couple32of32things3237d443237f443237u443237s__testHelper47client46cc__31__;
LogTest()
{
    char dictionary[4096];
    dictionaryBytes = GeneratedFunctions::writeDictionary(dictionary,
                                            dictionary + sizeof(dictionary));
    assert(dictionaryBytes > 0);
}

virtual ~LogTest() {
}

// If the constructor and destructor are not enough for setting up
// and cleaning up each test, you can define the following methods:

virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
}

virtual void TearDown() {
// Code here will be called immediately after each test (right
// before the destructor).
}

// Objects declared here can be used by all tests in the test case for Foo.

uint32_t dictionaryBytes;
};

TEST_F(LogTest, maxSizeOfHeader) {
    char buffer[100];
    Encoder encoder(buffer, 100, true);

    size_t encodeStart = encoder.getEncodedBytes();
    encoder.encodeBufferExtentStart(1<<31, false);
    size_t actualLength = encoder.getEncodedBytes() - encodeStart;
    size_t claimedLength = BufferExtent::maxSizeOfHeader();
    EXPECT_EQ(actualLength, claimedLength);
}

TEST_F(LogTest, peekEntryType) {
    const char *testFile = "/tmp/testFile";
    char buffer[100];

    UnknownHeader *header = reinterpret_cast<UnknownHeader*>(buffer);
    header->entryType = EntryType::LOG_MSGS_OR_DIC;
    (++header)->entryType = EntryType::BUFFER_EXTENT;
    (++header)->entryType = EntryType::CHECKPOINT;
    (++header)->entryType = EntryType::INVALID;

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(buffer));
    EXPECT_EQ(EntryType::BUFFER_EXTENT, peekEntryType(buffer + 1));
    EXPECT_EQ(EntryType::CHECKPOINT, peekEntryType(buffer + 2));
    EXPECT_EQ(EntryType::INVALID, peekEntryType(buffer + 3));

    // Write the buffer to a file and read it back
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(buffer, 4);
    oFile.close();

    FILE *in = fopen(testFile, "r");
    ASSERT_NE(nullptr, in);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(in));
    fgetc(in);
    EXPECT_EQ(EntryType::BUFFER_EXTENT, peekEntryType(in));
    fgetc(in);
    EXPECT_EQ(EntryType::CHECKPOINT, peekEntryType(in));
    fgetc(in);
    EXPECT_EQ(EntryType::INVALID, peekEntryType(in));
    fgetc(in);

    // Should be end of file
    fgetc(in);
    EXPECT_TRUE(feof(in));
    EXPECT_EQ(EntryType::INVALID, peekEntryType(in));

    std::remove(testFile);
}

TEST_F(LogTest, compressMetadata)
{
    char buffer[100];
    char *pos = buffer;

    Log::UncompressedEntry re;
    re.fmtId = 100;
    re.timestamp = 1000000000;

    size_t cmpSize = compressLogHeader(&re, &pos, 0);
    EXPECT_EQ(6U, cmpSize);
    EXPECT_EQ(6U, pos - buffer);
}

TEST_F(LogTest, compressMetadata_negativeDeltas)
{
    char buffer[100];
    char *pos = buffer;

    Log::UncompressedEntry re;
    re.fmtId = 100;
    re.timestamp = 100;

    size_t cmpSize = compressLogHeader(&re, &pos, 1000);
    EXPECT_EQ(4U, cmpSize);
    EXPECT_EQ(4U, pos - buffer);

    re.fmtId = 5000000;
    re.timestamp = 90;
    cmpSize = compressLogHeader(&re, &pos, 100);
    EXPECT_EQ(5U, cmpSize);
    EXPECT_EQ(9U, pos - buffer);
}

TEST_F(LogTest, compressMetadata_end2end)
{
    char backing_buffer[100];
    char *buffer = backing_buffer;
    size_t cmpSize;
    UncompressedEntry re;
    uint32_t dLogId;
    uint64_t dTimestamp;

    re.fmtId = 1000;
    re.timestamp = 10000000000000L;
    cmpSize = compressLogHeader(&re, &buffer, 0);
    EXPECT_EQ(9U, cmpSize);
    EXPECT_EQ(9U, buffer - backing_buffer);

    re.fmtId = 10000;
    re.timestamp = 10000;
    cmpSize = compressLogHeader(&re, &buffer, 10000000000000L);
    EXPECT_EQ(9U, cmpSize);
    EXPECT_EQ(18U, buffer - backing_buffer);

    re.fmtId = 1;
    re.timestamp = 100000;
    cmpSize = compressLogHeader(&re, &buffer, 10000);
    EXPECT_EQ(5U, cmpSize);
    EXPECT_EQ(23U, buffer - backing_buffer);

    re.fmtId = 1;
    re.timestamp = 100001;
    cmpSize = compressLogHeader(&re, &buffer, 100000);
    EXPECT_EQ(3U, cmpSize);
    EXPECT_EQ(26U, buffer - backing_buffer);

    cmpSize = compressLogHeader(&re, &buffer, 100001);
    EXPECT_EQ(3U, cmpSize);
    EXPECT_EQ(29U, buffer - backing_buffer);

    const char *readPtr = backing_buffer;

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 0U, dLogId, dTimestamp));
    EXPECT_EQ(1000, dLogId);
    EXPECT_EQ(10000000000000L, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 10000000000000L, dLogId,
                                                                   dTimestamp));
    EXPECT_EQ(10000, dLogId);
    EXPECT_EQ(10000, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 10000, dLogId, dTimestamp));
    EXPECT_EQ(1, dLogId);
    EXPECT_EQ(100000, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 100000, dLogId, dTimestamp));
    EXPECT_EQ(1, dLogId);
    EXPECT_EQ(100001, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 100001, dLogId, dTimestamp));
    EXPECT_EQ(1, dLogId);
    EXPECT_EQ(100001, dTimestamp);

    EXPECT_EQ(EntryType::INVALID, peekEntryType(readPtr));
    EXPECT_EQ(29U, readPtr - backing_buffer);
}

TEST_F(LogTest, insertCheckpoint) {
    char backing_buffer[1000];
    char *writePos = backing_buffer;
    char *endOfBuffer = backing_buffer + 1000;
    Checkpoint *ck = reinterpret_cast<Checkpoint*>(backing_buffer);

    // out of space
    EXPECT_FALSE(insertCheckpoint(&writePos, backing_buffer, false));
    EXPECT_EQ(writePos, backing_buffer);

    EXPECT_FALSE(insertCheckpoint(&writePos, backing_buffer, true));
    EXPECT_EQ(writePos, backing_buffer);

    // Not out of space
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer, false));
    EXPECT_EQ(sizeof(Checkpoint), writePos - backing_buffer);
    EXPECT_EQ(0U, ck->newMetadataBytes);
    EXPECT_EQ(0U, ck->totalMetadataEntries);


    writePos = backing_buffer;
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer, true));
    ck = reinterpret_cast<Checkpoint*>(backing_buffer);
    EXPECT_EQ(sizeof(Checkpoint) + ck->newMetadataBytes,
              writePos - backing_buffer);
    EXPECT_EQ(EntryType::CHECKPOINT, peekEntryType(backing_buffer));

    // Out of space at the very end
    writePos = endOfBuffer - sizeof(Checkpoint) - 1;
    EXPECT_FALSE(insertCheckpoint(&writePos, endOfBuffer, true));

    writePos = endOfBuffer - sizeof(Checkpoint) - 1;
    EXPECT_TRUE(insertCheckpoint(&writePos, endOfBuffer, false));
}

TEST_F(LogTest, insertCheckpoint_end2end) {
    Checkpoint cp1, cp2, cp3, cp4;
    char backing_buffer[2048];
    char *writePos = backing_buffer;
    char *endOfBuffer = backing_buffer + 2048;
    const char *testFile = "/tmp/testFile";

    // Write the buffer to a file and read it back
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(backing_buffer, 0);
    oFile.close();

    FILE *in = fopen(testFile, "r");
    ASSERT_NE(nullptr, in);
    ASSERT_FALSE(readCheckpoint(cp1, in));

    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer, false));
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer, true));
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer, false));

    // Write the buffer to a file and read it back
    fclose(in);
    oFile.open(testFile);
    oFile.write(backing_buffer, writePos - backing_buffer);
    oFile.close();

    in = fopen(testFile, "r");
    ASSERT_NE(nullptr, in);

    ASSERT_TRUE(readCheckpoint(cp1, in));

    // The metadata bytes are tested by the preprocessor unit tests and
    // integration tests.
    ASSERT_TRUE(readCheckpoint(cp2, in));
    ASSERT_EQ(0U, fseeko(in, cp2.newMetadataBytes, SEEK_CUR));

    ASSERT_TRUE(readCheckpoint(cp3, in));
    ASSERT_FALSE(readCheckpoint(cp4, in));
}

TEST_F(LogTest, encoder_constructor) {
    char buffer[1024];

    // Check that a checkpoint was inserted
    Encoder encoder(buffer, sizeof(buffer), false);
    EXPECT_LE(sizeof(Checkpoint), encoder.writePos - encoder.backing_buffer);
    EXPECT_EQ(buffer, encoder.backing_buffer);
    EXPECT_EQ(sizeof(buffer), encoder.endOfBuffer - encoder.backing_buffer);
    EXPECT_EQ(EntryType::CHECKPOINT, peekEntryType(buffer));

    // Check that a checkpoint was not inserted
    memset(buffer, 0, sizeof(buffer));
    Encoder e(buffer, sizeof(buffer), true);
    EXPECT_EQ(0U, e.writePos - e.backing_buffer);
    EXPECT_EQ(buffer, e.backing_buffer);
    EXPECT_EQ(sizeof(buffer), e.endOfBuffer - e.backing_buffer);
}

TEST_F(LogTest, encodeNewDictionaryEntries) {
    char buffer[10*1024];
    uint32_t currentPos = 0;

    std::vector<StaticLogInfo> meta;

    NanoLogInternal::ParamType paramTypes[10];
    meta.emplace_back(nullptr, "File", 123, 0, "Hello World", 0, 0, paramTypes);
    meta.emplace_back(nullptr, "FileA", 99, 2, "Hello World %s", 0, 0, paramTypes);
    meta.emplace_back(nullptr, "FileC", 125, 3, "Hello World %%d", 0, 0, paramTypes);

    // Not enough space, even for a dictionary fragment
    Encoder noSpaceEncoder(buffer, 1, true);
    EXPECT_EQ(0, noSpaceEncoder.encodeNewDictionaryEntries(currentPos, meta));
    EXPECT_EQ(0, currentPos);

    // Normal encoding
    Encoder encoder(buffer, sizeof(buffer), true);

    uint32_t expectedSize = sizeof(DictionaryFragment)
            + 3*sizeof(CompressedLogInfo)
            + strlen(meta[0].filename) + 1
            + strlen(meta[1].filename) + 1
            + strlen(meta[2].filename) + 1
            + strlen(meta[0].formatString) + 1
            + strlen(meta[1].formatString) + 1
            + strlen(meta[2].formatString) + 1;

    EXPECT_EQ(expectedSize,
                encoder.encodeNewDictionaryEntries(currentPos, meta));
    EXPECT_EQ(3, currentPos);

    // Read Header
    char *readPos = buffer;
    DictionaryFragment *df = reinterpret_cast<DictionaryFragment*>(readPos);
    readPos += sizeof(DictionaryFragment);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, df->entryType);
    EXPECT_EQ(expectedSize, df->newMetadataBytes);
    EXPECT_EQ(3, df->totalMetadataEntries);

    // Read back the entries
    for (size_t i = 0; i < meta.size(); ++i) {
        CompressedLogInfo *cli = reinterpret_cast<CompressedLogInfo *>(readPos);
        readPos += sizeof(CompressedLogInfo);

        EXPECT_EQ(meta[i].lineNum, cli->linenum);
        EXPECT_EQ(meta[i].severity, cli->severity);
        EXPECT_EQ(strlen(meta[i].filename) + 1, cli->filenameLength);
        EXPECT_EQ(strlen(meta[i].formatString) + 1, cli->formatStringLength);

        EXPECT_STREQ(meta[i].filename, readPos);
        readPos += strlen(meta[i].filename) + 1;

        EXPECT_STREQ(meta[i].formatString, readPos);
        readPos += strlen(meta[i].formatString) + 1;
    }

    EXPECT_EQ(buffer + expectedSize, readPos);

    // Now let's add another entry to make sure it makes it in okay
    meta.emplace_back(nullptr, "BLALKSD", 125, 3, "H %%d", 0, 0, paramTypes);
    expectedSize = sizeof(DictionaryFragment)
                    + sizeof(CompressedLogInfo)
                    + strlen(meta[3].filename) + 1
                    + strlen(meta[3].formatString) + 1;
    EXPECT_EQ(expectedSize, encoder.encodeNewDictionaryEntries(currentPos,
                                                                meta));

    df = reinterpret_cast<DictionaryFragment*>(readPos);
    readPos += sizeof(DictionaryFragment);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, df->entryType);
    EXPECT_EQ(expectedSize, df->newMetadataBytes);
    EXPECT_EQ(4, df->totalMetadataEntries);

    for (size_t i = 3; i < meta.size(); ++i) {
        CompressedLogInfo *cli = reinterpret_cast<CompressedLogInfo *>(readPos);
        readPos += sizeof(CompressedLogInfo);

        EXPECT_EQ(meta[i].lineNum, cli->linenum);
        EXPECT_EQ(meta[i].severity, cli->severity);
        EXPECT_EQ(strlen(meta[i].filename) + 1, cli->filenameLength);
        EXPECT_EQ(strlen(meta[i].formatString) + 1, cli->formatStringLength);

        EXPECT_STREQ(meta[i].filename, readPos);
        readPos += strlen(meta[i].filename) + 1;

        EXPECT_STREQ(meta[i].formatString, readPos);
        readPos += strlen(meta[i].formatString) + 1;
    }

    // One last entry, but this time we'll run out of space after encoding
    // the dictionary fragment header
    encoder.endOfBuffer = encoder.writePos + sizeof(DictionaryFragment) + 1;
    meta.emplace_back(nullptr, "BLALKSD", 125, 3, "H %%d", 0, 0, paramTypes);
    EXPECT_EQ(sizeof(DictionaryFragment),
                encoder.encodeNewDictionaryEntries(currentPos, meta));

    df = reinterpret_cast<DictionaryFragment*>(readPos);
    readPos += sizeof(DictionaryFragment);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, df->entryType);
    EXPECT_EQ(sizeof(DictionaryFragment), df->newMetadataBytes);
    EXPECT_EQ(4, df->totalMetadataEntries);
    EXPECT_EQ(4, currentPos);

}

TEST_F(LogTest, encodeLogMsgs) {
    char inputBuffer[100], outputBuffer1[1000];

    // We prefill the buffer with log messages. Note in test helper,
    // We have one valid log id with 0 arguments.
    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 101;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    ASSERT_LE(2, GeneratedFunctions::numLogIds);

    uint64_t compressedLogs = 1;
    Encoder e(outputBuffer1, 1000);

    long bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           true,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);

    /**
     * Now let's check the log, it should roughly follow the format of
     *   - Checkpoint
     *   - BufferExtent
     *   - Log message 1
     *   - log message 2
     *       -> sizeof(UncompressedEntry) of a "string"
     */
    const char *readPos = outputBuffer1;
    EXPECT_EQ(EntryType::CHECKPOINT, peekEntryType(readPos));
    const Checkpoint *ck = reinterpret_cast<const Checkpoint*>(readPos);
    readPos += sizeof(Checkpoint);
    readPos += ck->newMetadataBytes;

    EXPECT_EQ(EntryType::BUFFER_EXTENT, peekEntryType(readPos));
    const BufferExtent *be = reinterpret_cast<const BufferExtent*>(readPos);
    readPos += sizeof(BufferExtent);

    // Checking the buffer extents
    EXPECT_EQ(6U + sizeof(BufferExtent)+sizeof(UncompressedEntry), be->length);
    ASSERT_TRUE(be->isShort);
    EXPECT_EQ(5, be->threadIdOrPackNibble);
    EXPECT_TRUE(be->wrapAround);

    // Checking the first log meta
    uint64_t ts;
    uint32_t fmtId;
    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPos));
    decompressLogHeader(&readPos, 0, fmtId, ts);

    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPos));
    decompressLogHeader(&readPos, 0, fmtId, ts);

    // Now we break abstractions a little bit and read the "string" part of it
    readPos += sizeof(UncompressedEntry);
    EXPECT_EQ(readPos, e.writePos);

    // Let's try it again, with the same and different buffers to see if the
    // encoder will encode new BuferExtents
    bytesRead = e.encodeLogMsgs(inputBuffer,
                                    3*sizeof(UncompressedEntry),
                                    6,
                                    false,
                                    &compressedLogs);

    EXPECT_EQ(1 + 2 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(6U, e.lastBufferIdEncoded);

    EXPECT_EQ(EntryType::BUFFER_EXTENT, peekEntryType(readPos));
    be = reinterpret_cast<const BufferExtent*>(readPos);
    EXPECT_TRUE(be->isShort);
    EXPECT_EQ(6U, be->threadIdOrPackNibble);
    readPos += sizeof(BufferExtent);
    EXPECT_EQ(EntryType::LOG_MSGS_OR_DIC, peekEntryType(readPos));

    // Now one last time without without changing the buffers
    readPos = e.writePos;
    bytesRead = e.encodeLogMsgs(inputBuffer,
                                    3*sizeof(UncompressedEntry),
                                    6,
                                    false,
                                    &compressedLogs);

    EXPECT_EQ(1 + 2 + 2 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(6U, e.lastBufferIdEncoded);

    EXPECT_EQ(EntryType::BUFFER_EXTENT, peekEntryType(readPos));

    // One last time, with the same buffer but a newPass
    readPos = e.writePos;
    bytesRead = e.encodeLogMsgs(inputBuffer,
                                    3*sizeof(UncompressedEntry),
                                    6,
                                    true,
                                    &compressedLogs);

    EXPECT_EQ(1 + 2 + 2 + 2 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(6U, e.lastBufferIdEncoded);

    EXPECT_EQ(EntryType::BUFFER_EXTENT, peekEntryType(readPos));
}

TEST_F(LogTest, encodeLogMsgs_notEnoughOutputSpace) {
    char inputBuffer[100], outputBuffer1[4096];

    // We prefill the buffer with log messages. Note in test helper,
    // We have one valid log id with 0 arguments.
    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 100;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    ASSERT_LE(2, GeneratedFunctions::numLogIds);

    uint64_t compressedLogs = 1;
    Encoder e(outputBuffer1, 100 + dictionaryBytes, false, true);

    long bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           true,
                                           &compressedLogs);

    EXPECT_EQ(1 + 1, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);

    // Rough check of what's in the buffer
    EXPECT_EQ(e.writePos - e.backing_buffer, sizeof(Checkpoint)
                                                + dictionaryBytes
                                                + sizeof(BufferExtent)
                                                + sizeof(CompressedEntry) + 2);
    const char *readPos = e.backing_buffer  + sizeof(Checkpoint)
                                            + dictionaryBytes;
    const BufferExtent *be = reinterpret_cast<const BufferExtent*>(readPos);
    EXPECT_EQ(sizeof(BufferExtent) + 3U, be->length);

    // Now let's try one more out of space whereby there's not enough space
    // to encode the buffer extent.
    compressedLogs = 1;
    uint32_t bufferSize = sizeof(Checkpoint)    + dictionaryBytes
                                                + sizeof(BufferExtent) - 1;
    Encoder e2(outputBuffer1, bufferSize, false, true);
    bytesRead = e2.encodeLogMsgs(inputBuffer,
                                    3*sizeof(UncompressedEntry),
                                    100,
                                    true,
                                    &compressedLogs);
    EXPECT_EQ(0U, bytesRead);
    EXPECT_EQ(1U, compressedLogs);
    EXPECT_NE(100U, e2.lastBufferIdEncoded);
    EXPECT_EQ(sizeof(Checkpoint) + dictionaryBytes, e2.getEncodedBytes());

    // One last attempt whereby we have enough space to encode the buffer
    // extent but nothing else.
    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    compressedLogs = 1;

    bufferSize = sizeof(Checkpoint) + dictionaryBytes + sizeof(BufferExtent)
                                    + sizeof(uint32_t);
    Encoder e3(outputBuffer1, bufferSize, false, true);
    bytesRead = e3.encodeLogMsgs(inputBuffer,
                                    3*sizeof(UncompressedEntry),
                                    1,
                                    true,
                                    &compressedLogs);
    EXPECT_EQ(0U, bytesRead);
    EXPECT_EQ(1U, compressedLogs);
    EXPECT_EQ(1U, e3.lastBufferIdEncoded);
    EXPECT_EQ(sizeof(Checkpoint) + dictionaryBytes + sizeof(BufferExtent),
              e3.getEncodedBytes());

    be = reinterpret_cast<const BufferExtent*>(e3.backing_buffer
                                                        + sizeof(Checkpoint)
                                                        + dictionaryBytes);
    EXPECT_EQ(sizeof(BufferExtent), be->length);
}

TEST_F(LogTest, encodeLogMsgs_notEnoughInputSpace) {
     char inputBuffer[100], outputBuffer1[1000];

    // We prefill the buffer with log messages. Note in test helper,
    // We have one valid log id with 0 arguments.
    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 100;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    ASSERT_LE(2, GeneratedFunctions::numLogIds);

    uint64_t compressedLogs = 1;
    Encoder e(outputBuffer1, 1000);

    long bytesRead = e.encodeLogMsgs(inputBuffer,
                                           2*sizeof(UncompressedEntry),
                                           5,
                                           true,
                                           &compressedLogs);

    EXPECT_EQ(1 + 1, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);

    // Once again with less than an uncompressed entry
    bytesRead = e.encodeLogMsgs(inputBuffer,
                                           sizeof(UncompressedEntry) - 1,
                                           5,
                                           true,
                                           &compressedLogs);

    EXPECT_EQ(1 + 1, compressedLogs);
    EXPECT_EQ(0U, bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);
}

TEST_F(LogTest, encodeLogMsgs_logTooBig) {
    char inputBuffer[100], outputBuffer1[1000];


    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = stringParamId;
    ue->entrySize = sizeof(UncompressedEntry)
            + NanoLogConfig::STAGING_BUFFER_SIZE;

    Encoder e(outputBuffer1, 1000);

    testing::internal::CaptureStderr();
    uint64_t compressedLogs = 0;
    long bytesRead = e.encodeLogMsgs(inputBuffer,
                                     2*sizeof(UncompressedEntry),
                                     5,
                                     true,
                                     &compressedLogs);

    EXPECT_EQ(0, compressedLogs);
    EXPECT_STREQ("ERROR: Attempting to log a message that is 1048592 bytes "
                 "while the maximum allowable size is 524288.\r\nThis occurs "
                 "for the log message testHelper/client.cc:21 "
                 "'This is a string %s'\r\n",
                    testing::internal::GetCapturedStderr().c_str());
}

TEST_F(LogTest, encodeBufferExtentStart) {
    char buffer[1000];
    Encoder encoder(buffer, 1000, true);

    // Assert that nothing has been written
    ASSERT_EQ(encoder.backing_buffer, encoder.writePos);
    ASSERT_EQ(0U, encoder.getEncodedBytes());

    ASSERT_TRUE(encoder.encodeBufferExtentStart(15, false));
    const BufferExtent *be = reinterpret_cast<const BufferExtent*>(buffer);

    EXPECT_EQ(EntryType::BUFFER_EXTENT, be->entryType);
    EXPECT_TRUE(be->isShort);
    EXPECT_EQ(sizeof(BufferExtent), be->length);
    EXPECT_EQ(15U, be->threadIdOrPackNibble);
    EXPECT_FALSE(be->wrapAround);

    EXPECT_EQ(&(be->length), encoder.currentExtentSize);
    EXPECT_EQ(15, encoder.lastBufferIdEncoded);


    // Let's try another one whereby id is larger than what can fit internally
    char *secondWritePos = encoder.writePos;
    ASSERT_TRUE(encoder.encodeBufferExtentStart(111111111, true));
    ++be;

    EXPECT_EQ(EntryType::BUFFER_EXTENT, be->entryType);
    EXPECT_FALSE(be->isShort);
    EXPECT_TRUE(be->wrapAround);

    EXPECT_EQ(&(be->length), encoder.currentExtentSize);
    EXPECT_EQ(111111111, encoder.lastBufferIdEncoded);

    const char *pos = buffer + 2*sizeof(BufferExtent);
    uint32_t encodedId = BufferUtils::unpack<uint32_t>(&pos,
                                            uint8_t(be->threadIdOrPackNibble));
    EXPECT_EQ(111111111, encodedId);
    EXPECT_EQ(pos, encoder.writePos);
    EXPECT_EQ(encoder.writePos - secondWritePos, be->length);
}

TEST_F(LogTest, encodeBufferExtentStart_outtaSpace) {
    char buffer[100];
    Encoder encoder(buffer, 1, true);

    EXPECT_FALSE(encoder.encodeBufferExtentStart(1234, true));

    EXPECT_EQ(0U, encoder.getEncodedBytes());
    EXPECT_EQ(nullptr, encoder.currentExtentSize);
}

TEST_F(LogTest, swapBuffer) {
    char buffer1[1000], buffer2[100];
    Encoder encoder(buffer1, 1000, false, true);

    char *outBuffer;
    size_t outLength;
    size_t outSize;

    encoder.swapBuffer(buffer2, 100, &outBuffer, &outLength, &outSize);
    EXPECT_EQ(buffer1, outBuffer);
    EXPECT_EQ(sizeof(Checkpoint) + dictionaryBytes, outLength);
    EXPECT_EQ(1000, outSize);

    EXPECT_EQ(buffer2, encoder.backing_buffer);
    EXPECT_EQ(buffer2, encoder.writePos);
    EXPECT_EQ(buffer2 + 100, encoder.endOfBuffer);
    EXPECT_EQ(uint32_t(-1), encoder.lastBufferIdEncoded);
    EXPECT_EQ(nullptr, encoder.currentExtentSize);
}

TEST_F(LogTest, Decoder_open) {
    char buffer[1000];
    const char *testFile = "/tmp/testFile";
    Decoder dc;

    // Open an invalid file
    testing::internal::CaptureStderr();
    EXPECT_FALSE(dc.open("/dev/null"));
    EXPECT_TRUE(dc.filename.empty());
    EXPECT_EQ(0U, dc.inputFd);
    EXPECT_STREQ("Error: Could not read initial checkpoint, "
                         "the compressed log may be corrupted.\r\n",
                 testing::internal::GetCapturedStderr().c_str());

    // Write back an empty file and try to open it.
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(buffer, 0);
    oFile.close();

    testing::internal::CaptureStderr();
    EXPECT_FALSE(dc.open(testFile));
    EXPECT_TRUE(dc.filename.empty());
    EXPECT_EQ(0U, dc.inputFd);
    EXPECT_STREQ("Error: Could not read initial checkpoint, "
                         "the compressed log may be corrupted.\r\n",
                 testing::internal::GetCapturedStderr().c_str());

    // Write back an invalid, but not-empty file
    oFile.open(testFile);
    bzero(buffer, 1000);
    oFile.write(buffer, 100);
    oFile.close();

    // Finally, open a file that at least has a valid checkpoint
    Encoder encoder(buffer, 1000);
    oFile.open(testFile);
    oFile.write(buffer, encoder.getEncodedBytes());
    oFile.close();

    EXPECT_TRUE(dc.open(testFile));
    EXPECT_NE(nullptr, dc.inputFd);
    EXPECT_STREQ(testFile, dc.filename.c_str());

    std::remove(testFile);
}

TEST_F(LogTest, decoder_insertCheckpoint) {
    char backing_buffer[4096];
    char *writePos = backing_buffer;
    char *endOfBuffer = backing_buffer + sizeof(backing_buffer);
    uint64_t startCycles = PerfUtils::Cycles::rdtsc();
    uint32_t metadataBytes = GeneratedFunctions::writeDictionary(writePos,
                                                                 endOfBuffer);
    uint32_t numEntries = GeneratedFunctions::numLogIds;

    // True Case
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer, true));
    Checkpoint *ck = reinterpret_cast<Checkpoint*>(backing_buffer);

    EXPECT_EQ(Log::EntryType::CHECKPOINT, ck->entryType);
    EXPECT_LT(startCycles, ck->rdtsc);
    EXPECT_GT(PerfUtils::Cycles::rdtsc(), ck->rdtsc);
    EXPECT_EQ(PerfUtils::Cycles::getCyclesPerSec(), ck->cyclesPerSecond);

    EXPECT_EQ(metadataBytes, ck->newMetadataBytes);
    EXPECT_EQ(numEntries, ck->totalMetadataEntries);

    // False Case
    writePos = backing_buffer;
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer, false));
    EXPECT_EQ(Log::EntryType::CHECKPOINT, ck->entryType);
    EXPECT_LT(startCycles, ck->rdtsc);
    EXPECT_GT(PerfUtils::Cycles::rdtsc(), ck->rdtsc);
    EXPECT_EQ(PerfUtils::Cycles::getCyclesPerSec(), ck->cyclesPerSecond);

    EXPECT_EQ(0U, ck->newMetadataBytes);
    EXPECT_EQ(0U, ck->totalMetadataEntries);

    // Out of space case
    char *newEndOfSpace = backing_buffer + metadataBytes;
    writePos = backing_buffer;
    ASSERT_FALSE(insertCheckpoint(&writePos, newEndOfSpace, true));
    EXPECT_EQ(backing_buffer, writePos);

    writePos = backing_buffer;
    ASSERT_TRUE(insertCheckpoint(&writePos, newEndOfSpace, false));
    EXPECT_EQ(backing_buffer + sizeof(Checkpoint), writePos);

    writePos = backing_buffer;
    newEndOfSpace = backing_buffer + sizeof(Checkpoint) - 1;
    ASSERT_FALSE(insertCheckpoint(&writePos, newEndOfSpace, false));
    EXPECT_EQ(backing_buffer, writePos);
}


TEST_F(LogTest, decoder_readDictionary) {
    const char *testFile = "/tmp/testFile";
    char backing_buffer[4096];
    char *writePos = backing_buffer;
    char *endOfBuffer = backing_buffer + sizeof(backing_buffer);
    PrintFragment *pf;
    FormatMetadata *fm, *fm2;
    uint32_t fmOffset, fm2Offset;

    // The true case is tested in integration tests
    insertCheckpoint(&writePos, endOfBuffer, false);
    Checkpoint *ck = reinterpret_cast<Checkpoint*>(backing_buffer);

    // Basic Log that's from asdfasdfasdf:1234 -> "abab %*.*lfabab"
    fmOffset = writePos - backing_buffer - sizeof(Checkpoint);
    fm = reinterpret_cast<FormatMetadata*>(writePos);
    writePos += sizeof(FormatMetadata);
    fm->numNibbles = 1;
    fm->numPrintFragments = 2;
    fm->logLevel = 2;
    fm->lineNumber = 1234;
    fm->filenameLength = sizeof("asdfasdfasdf");
    writePos = stpcpy(fm->filename, "asdfasdfasdf") + 1;

    pf = reinterpret_cast<PrintFragment*>(writePos);
    writePos += sizeof(PrintFragment);
    pf->argType = NanoLogInternal::Log::FormatType::double_t;
    pf->hasDynamicPrecision = true;
    pf->hasDynamicWidth = true;
    pf->fragmentLength = sizeof("abab %*.*lf");
    writePos = stpcpy(writePos, "abab %*.*lf") + 1;

    pf = reinterpret_cast<PrintFragment*>(writePos);
    writePos += sizeof(PrintFragment);
    pf->argType = NONE;
    pf->hasDynamicPrecision = false;
    pf->hasDynamicWidth = false;
    pf->fragmentLength = sizeof("abab");
    writePos = stpcpy(writePos, "abab") + 1;

    // Log that's from asdfasdfasdf:1234 -> "asdflkaldfjasfdlasdfjal;sdfjaslkdfas"
    fm2Offset = writePos - backing_buffer - sizeof(Checkpoint);
    fm2 = reinterpret_cast<FormatMetadata*>(writePos);
    writePos += sizeof(FormatMetadata);
    fm2->numNibbles = 1;
    fm2->numPrintFragments = 1;
    fm2->logLevel = 2;
    fm2->lineNumber = 1234;
    fm2->filenameLength = sizeof("asdfasdfasdf");
    writePos = stpcpy(fm2->filename, "asdfasdfasdf") + 1;

    pf = reinterpret_cast<PrintFragment*>(writePos);
    writePos += sizeof(PrintFragment);
    pf->argType = NONE;
    pf->hasDynamicPrecision = false;
    pf->hasDynamicWidth = false;
    pf->fragmentLength = sizeof("asdflkaldfjasfdlasdfjal;sdfjaslkdfas");
    writePos = stpcpy(writePos, "asdflkaldfjasfdlasdfjal;sdfjaslkdfas") + 1;

    uint32_t dictionaryBytes = writePos - backing_buffer - sizeof(Checkpoint);
    {
        // Test normal case
        ck->totalMetadataEntries = 2;
        ck->newMetadataBytes = dictionaryBytes;

        std::ofstream oFile;
        oFile.open(testFile);
        oFile.write(backing_buffer, writePos - backing_buffer);
        oFile.close();

        FILE *fd = fopen(testFile, "rb");

        Decoder dc;
        EXPECT_TRUE(dc.readDictionary(fd, true));
        EXPECT_EQ(dictionaryBytes, dc.endOfRawMetadata - dc.rawMetadata);
        EXPECT_EQ(0, memcmp(dc.rawMetadata,
                            backing_buffer + sizeof(Checkpoint),
                            dictionaryBytes));

        ASSERT_EQ(2, dc.fmtId2metadata.size());
        EXPECT_EQ(dc.rawMetadata + fmOffset, dc.fmtId2metadata.at(0));
        EXPECT_EQ(dc.rawMetadata + fm2Offset, dc.fmtId2metadata.at(1));

        ASSERT_EQ(2, dc.fmtId2fmtString.size());
        EXPECT_STREQ("abab %*.*lfabab", dc.fmtId2fmtString.at(0).c_str());
        EXPECT_STREQ("asdflkaldfjasfdlasdfjal;sdfjaslkdfas",
                     dc.fmtId2fmtString.at(1).c_str());

        fclose(fd);
        std::remove(testFile);
    }

    {
        // Incremental Read
        ck->totalMetadataEntries = 2;
        ck->newMetadataBytes = dictionaryBytes;

        std::ofstream oFile;
        oFile.open(testFile);
        oFile.write(backing_buffer, writePos - backing_buffer);
        oFile.close();

        FILE *fd = fopen(testFile, "rb");

        Decoder dc;
        EXPECT_TRUE(dc.readDictionary(fd, true));
        EXPECT_EQ(dictionaryBytes, dc.endOfRawMetadata - dc.rawMetadata);
        EXPECT_EQ(0, memcmp(dc.rawMetadata,
                            backing_buffer + sizeof(Checkpoint),
                            dictionaryBytes));

        ASSERT_EQ(2, dc.fmtId2metadata.size());
        EXPECT_EQ(dc.rawMetadata + fmOffset, dc.fmtId2metadata.at(0));
        EXPECT_EQ(dc.rawMetadata + fm2Offset, dc.fmtId2metadata.at(1));

        ASSERT_EQ(2, dc.fmtId2fmtString.size());
        EXPECT_STREQ("abab %*.*lfabab", dc.fmtId2fmtString.at(0).c_str());
        EXPECT_STREQ("asdflkaldfjasfdlasdfjal;sdfjaslkdfas",
                     dc.fmtId2fmtString.at(1).c_str());

        fclose(fd);

        // Second read
        ck->totalMetadataEntries = 4;

        oFile.open(testFile);
        oFile.write(backing_buffer, writePos - backing_buffer);
        oFile.close();

        fd = fopen(testFile, "rb");

        EXPECT_TRUE(dc.readDictionary(fd, false));
        EXPECT_EQ(2*dictionaryBytes, dc.endOfRawMetadata - dc.rawMetadata);
        EXPECT_EQ(0, memcmp(dc.rawMetadata,
                            backing_buffer + sizeof(Checkpoint),
                            dictionaryBytes));

        EXPECT_EQ(0, memcmp(dc.rawMetadata + dictionaryBytes,
                            backing_buffer + sizeof(Checkpoint),
                            dictionaryBytes));

        ASSERT_EQ(4, dc.fmtId2metadata.size());
        EXPECT_EQ(dc.rawMetadata + fmOffset, dc.fmtId2metadata.at(0));
        EXPECT_EQ(dc.rawMetadata + fm2Offset, dc.fmtId2metadata.at(1));
        EXPECT_EQ(dc.rawMetadata + dictionaryBytes + fmOffset,
                  dc.fmtId2metadata.at(2));
        EXPECT_EQ(dc.rawMetadata + dictionaryBytes + fm2Offset,
                  dc.fmtId2metadata.at(3));

        ASSERT_EQ(4, dc.fmtId2fmtString.size());
        EXPECT_STREQ("abab %*.*lfabab", dc.fmtId2fmtString.at(0).c_str());
        EXPECT_STREQ("asdflkaldfjasfdlasdfjal;sdfjaslkdfas",
                     dc.fmtId2fmtString.at(1).c_str());
        EXPECT_STREQ("abab %*.*lfabab", dc.fmtId2fmtString.at(2).c_str());
        EXPECT_STREQ("asdflkaldfjasfdlasdfjal;sdfjaslkdfas",
                     dc.fmtId2fmtString.at(3).c_str());

        fclose(fd);

        // Read no new dictionary
        ck->totalMetadataEntries = 4;
        ck->newMetadataBytes = 0;

        oFile.open(testFile);
        oFile.write(backing_buffer, sizeof(Checkpoint));
        oFile.close();

        fd = fopen(testFile, "rb");

        EXPECT_TRUE(dc.readDictionary(fd, false));
        EXPECT_EQ(2*dictionaryBytes, dc.endOfRawMetadata - dc.rawMetadata);
        EXPECT_EQ(0, memcmp(dc.rawMetadata,
                            backing_buffer + sizeof(Checkpoint),
                            dictionaryBytes));

        EXPECT_EQ(0, memcmp(dc.rawMetadata + dictionaryBytes,
                            backing_buffer + sizeof(Checkpoint),
                            dictionaryBytes));

        ASSERT_EQ(4, dc.fmtId2metadata.size());
        EXPECT_EQ(dc.rawMetadata + fmOffset, dc.fmtId2metadata.at(0));
        EXPECT_EQ(dc.rawMetadata + fm2Offset, dc.fmtId2metadata.at(1));
        EXPECT_EQ(dc.rawMetadata + dictionaryBytes + fmOffset,
                  dc.fmtId2metadata.at(2));
        EXPECT_EQ(dc.rawMetadata + dictionaryBytes + fm2Offset,
                  dc.fmtId2metadata.at(3));

        ASSERT_EQ(4, dc.fmtId2fmtString.size());
        fclose(fd);

        // Read a new dictionary, but reset the old one
        ck->newMetadataBytes = dictionaryBytes;
        ck->totalMetadataEntries = 2;

        oFile.open(testFile);
        oFile.write(backing_buffer, sizeof(Checkpoint) + dictionaryBytes);
        oFile.close();

        fd = fopen(testFile, "rb");

        EXPECT_TRUE(dc.readDictionary(fd, true));
        EXPECT_EQ(dictionaryBytes, dc.endOfRawMetadata - dc.rawMetadata);
        EXPECT_EQ(0, memcmp(dc.rawMetadata,
                            backing_buffer + sizeof(Checkpoint),
                            dictionaryBytes));

        ASSERT_EQ(2, dc.fmtId2metadata.size());
        EXPECT_EQ(dc.rawMetadata + fmOffset, dc.fmtId2metadata.at(0));
        EXPECT_EQ(dc.rawMetadata + fm2Offset, dc.fmtId2metadata.at(1));

        ASSERT_EQ(2, dc.fmtId2fmtString.size());
        EXPECT_STREQ("abab %*.*lfabab", dc.fmtId2fmtString.at(0).c_str());
        EXPECT_STREQ("asdflkaldfjasfdlasdfjal;sdfjaslkdfas",
                     dc.fmtId2fmtString.at(1).c_str());
        fclose(fd);

        std::remove(testFile);
    }

    {
        // Not enough bytes written
        ck->totalMetadataEntries = 2;
        ck->newMetadataBytes = dictionaryBytes;

        std::ofstream oFile;
        oFile.open(testFile);
        oFile.write(backing_buffer, writePos - backing_buffer - 10);
        oFile.close();

        FILE *fd = fopen(testFile, "rb");

        Decoder dc;
        testing::internal::CaptureStderr();
        EXPECT_FALSE(dc.readDictionary(fd, true));
        EXPECT_EQ(0, dc.endOfRawMetadata - dc.rawMetadata);
        EXPECT_STREQ("Error couldn't read metadata header in log file.\r\n",
                     testing::internal::GetCapturedStderr().c_str());

        fclose(fd);
        std::remove(testFile);
    }

    {
        // Missing data
        ck->totalMetadataEntries = 3;
        ck->newMetadataBytes = dictionaryBytes;

        std::ofstream oFile;
        oFile.open(testFile);
        oFile.write(backing_buffer, writePos - backing_buffer);
        oFile.close();

        FILE *fd = fopen(testFile, "rb");

        Decoder dc;
        testing::internal::CaptureStderr();
        EXPECT_FALSE(dc.readDictionary(fd, true));
        EXPECT_STREQ("Error: Missing log metadata detected; "
                             "expected 3 messages, but only found 2\r\n",
                     testing::internal::GetCapturedStderr().c_str());

        fclose(fd);
        std::remove(testFile);
    }

    {
        // Corrupt data
        fm2->filenameLength = 1000;
        ck->totalMetadataEntries = 2;
        ck->newMetadataBytes = dictionaryBytes;

        std::ofstream oFile;
        oFile.open(testFile);
        oFile.write(backing_buffer, writePos - backing_buffer);
        oFile.close();

        FILE *fd = fopen(testFile, "rb");

        Decoder dc;
        testing::internal::CaptureStderr();
        EXPECT_FALSE(dc.readDictionary(fd, true));
        EXPECT_STREQ("Error: Log dictionary is inconsistent; "
                     "expected 107 bytes, but read 1054 bytes\r\n",
                     testing::internal::GetCapturedStderr().c_str());

        fclose(fd);
        std::remove(testFile);
    }
}

TEST_F(LogTest, decoder_destructor) {
    const char *testFile = "/tmp/testFile";
    char buffer[1000];

    // Write back an invalid, but not-empty file
    std::ofstream oFile;
    Encoder encoder(buffer, 1000);
    oFile.open(testFile);
    oFile.write(buffer, encoder.getEncodedBytes());
    oFile.close();

    Decoder *dc = new Decoder();
    ASSERT_TRUE(dc->open(testFile));
    dc->freeBuffers.push_back(new Decoder::BufferFragment());
    dc->~Decoder();

    // I'm touching deallocated memory >=3
    EXPECT_EQ(nullptr, dc->inputFd);
    EXPECT_TRUE(dc->freeBuffers.empty());

    free(dc);
    std::remove(testFile);
}

TEST_F(LogTest, decoder_allocate_free) {
    Decoder dc;
    Decoder::BufferFragment *bf1, *bf2, *bf3;

    // Try allocate 2 new ones
    bf1 = dc.allocateBufferFragment();
    bf2 = dc.allocateBufferFragment();
    EXPECT_EQ(0U, dc.freeBuffers.size());

    // Dallocate one
    dc.freeBufferFragment(bf1);
    EXPECT_EQ(1U, dc.freeBuffers.size());

    // Reallocate that one
    bf3 = dc.allocateBufferFragment();
    EXPECT_EQ(bf1, bf3);
    EXPECT_EQ(0U, dc.freeBuffers.size());

    // bf3 = bf1
    dc.freeBufferFragment(bf2);
    dc.freeBufferFragment(bf3);
}

TEST_F(LogTest, Decoder_readBufferExtent_end2end) {
    const char *testFile = "/tmp/testFile";
    char inputBuffer[100], outputBuffer1[1000];

    // Here we use encoder to prefill the output file
    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 101;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    ASSERT_LE(2, GeneratedFunctions::numLogIds);

    uint64_t compressedLogs = 1;
    Encoder e(outputBuffer1, 1000, true);
    long bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);

    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(outputBuffer1, e.getEncodedBytes());
    oFile.close();

    /// Now we reopen it and do our actual test
    FILE *in = fopen(testFile, "rb");
    ASSERT_TRUE(in);

    Decoder::BufferFragment *bf = new Decoder::BufferFragment();
    ASSERT_NE(nullptr, bf);
    bool wrapAround = true;

    // If this assert fails, then something probably changed in Encoder...
    // just make sure we write() with a BufferExtent and just that only).
    ASSERT_EQ(EntryType::BUFFER_EXTENT, peekEntryType(in));
    ASSERT_TRUE(bf->readBufferExtent(in, &wrapAround));
    EXPECT_EQ(e.getEncodedBytes(), bf->validBytes);

    EXPECT_EQ(5, bf->runtimeId);
    EXPECT_EQ(100UL, bf->nextLogTimestamp);
    EXPECT_EQ(noParamsId, bf->nextLogId);
    EXPECT_FALSE(wrapAround);

    delete bf;
    std::remove(testFile);
}

TEST_F(LogTest, Decoder_readBufferExtent_notEnoughSpace) {
    const char *testFile = "/tmp/testFile";
    char inputBuffer[100], goodBuffer[1000], badBuffer[100];

    // Here we use encoder to prefill the output file
    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 101;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    ASSERT_LE(2, GeneratedFunctions::numLogIds);

    uint64_t compressedLogs = 1;
    Encoder e(goodBuffer, 1000, true);
    long bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);

    // Now we do our real test
    Decoder::BufferFragment *bf = new Decoder::BufferFragment();
    std::ofstream oFile;

    //  Test a file that is too small
    oFile.open(testFile);
    oFile.write(badBuffer, 2);
    oFile.close();
    FILE *in = fopen(testFile, "rb");
    ASSERT_TRUE(in);
    ASSERT_FALSE(bf->readBufferExtent(in));
    fclose(in);

    // Test a file that contains invalid data
    oFile.open(testFile);
    bzero(badBuffer, 100);
    oFile.write(badBuffer, 100);
    oFile.close();
    in = fopen(testFile, "rb");
    ASSERT_TRUE(in);
    ASSERT_FALSE(bf->readBufferExtent(in));
    fclose(in);

    // Test a BufferExtent that's waaaaayyy too large to fit.
    BufferExtent *be = reinterpret_cast<BufferExtent*>(badBuffer);
    be->entryType = EntryType::BUFFER_EXTENT;
    be->isShort = true;
    be->length = sizeof(bf->storage) + 1;
    be->threadIdOrPackNibble = 1;
    be->wrapAround = false;

    oFile.open(testFile);
    oFile.write(badBuffer, sizeof(badBuffer));
    oFile.close();
    in = fopen(testFile, "rb");
    ASSERT_TRUE(in);
    ASSERT_FALSE(bf->readBufferExtent(in));
    fclose(in);

    // Test a file that contains partially correct data
    oFile.open(testFile);
    oFile.write(goodBuffer, e.getEncodedBytes() - 1);
    oFile.close();
    in = fopen(testFile, "rb");
    ASSERT_TRUE(in);
    ASSERT_FALSE(bf->readBufferExtent(in));
    fclose(in);

    // Lastly, try an extent that could work, but a corrupted log message
    be = reinterpret_cast<BufferExtent*>(badBuffer);
    be->entryType = EntryType::BUFFER_EXTENT;
    be->isShort = true;
    be->length = sizeof(bf->storage) + 1;
    be->threadIdOrPackNibble = 1;
    be->wrapAround = false;
    ++be;
    be->entryType = EntryType::BUFFER_EXTENT;

    oFile.open(testFile);
    oFile.write(badBuffer, sizeof(badBuffer));
    oFile.close();
    in = fopen(testFile, "rb");
    ASSERT_TRUE(in);
    ASSERT_FALSE(bf->readBufferExtent(in));
    fclose(in);

    delete bf;
    std::remove(testFile);
}

int numAggregationsRun = 0;
void aggregation(const char*, ...) {
   ++numAggregationsRun;
}

TEST_F(LogTest, decompressNextLogStatement) {
    const char *testFile = "/tmp/testFile";
    char inputBuffer[100], goodBuffer[1000];

    // Here we use encoder to prefill the output file
    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 101;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    ASSERT_LE(2, GeneratedFunctions::numLogIds);

    uint64_t compressedLogs = 1;
    Encoder e(goodBuffer, 1000, true);
    long bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);

    // Write it out and read it back in.
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(goodBuffer, e.getEncodedBytes());
    oFile.close();

    /// Now we reopen it and do our actual test
    Decoder::BufferFragment *bf = new Decoder::BufferFragment();
    FILE *in = fopen(testFile, "rb");
    ASSERT_TRUE(in);
    EXPECT_TRUE(bf->readBufferExtent(in));

    uint64_t logMsgsPrinted = 0;
    Checkpoint checkpoint;
    checkpoint.cyclesPerSecond = 1;
    long aggregationFilterId = stringParamId;
    numAggregationsRun = 0;
    std::vector<void*> fmtId2metadata;
    LogMessage logArguments;
    EXPECT_TRUE(bf->decompressNextLogStatement(NULL,
                                                logMsgsPrinted,
                                                logArguments,
                                                checkpoint,
                                                fmtId2metadata,
                                                aggregationFilterId,
                                                &aggregation));
    EXPECT_EQ(0, numAggregationsRun);
    EXPECT_TRUE(bf->hasNext());

    EXPECT_TRUE(bf->decompressNextLogStatement(NULL,
                                                logMsgsPrinted,
                                                logArguments,
                                                checkpoint,
                                                fmtId2metadata,
                                                aggregationFilterId,
                                                &aggregation));
    EXPECT_EQ(1, numAggregationsRun);
    EXPECT_FALSE(bf->hasNext());

    // There should be no more
    EXPECT_FALSE(bf->decompressNextLogStatement(NULL,
                                                logMsgsPrinted,
                                                logArguments,
                                                checkpoint,
                                                fmtId2metadata,
                                                aggregationFilterId,
                                                &aggregation));
    EXPECT_EQ(1, numAggregationsRun);
    EXPECT_FALSE(bf->hasNext());
    EXPECT_EQ(2U, logMsgsPrinted);

    fclose(in);
    delete bf;

    // Note the large switch statement is a bit difficult to test within the
    // unit test framework, so that is instead saved for the integration tests
}

// These following tests are more end-to-end like

TEST_F(LogTest, Decoder_internalDecompress_end2end) {
    // First we have to create a log file with encoder.
    const char *testFile = "/tmp/testFile";
    const char *decomp = "/tmp/testFile2";
    char inputBuffer[1000], buffer[1000];
    Encoder encoder(buffer, 1000, false);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint = (Checkpoint*)encoder.backing_buffer;
    checkpoint->cyclesPerSecond = 1e9;
    checkpoint->rdtsc = 0;
    checkpoint->unixTime = 1;

    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 90;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 105;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);

    // Okay, this is really starting to dig deep into the implementation of
    // how log messages are interpreted.... so if failures occur... yeah.
    char tmp[sizeof(UncompressedEntry)];
    memset(tmp, 'a', sizeof(UncompressedEntry));
    tmp[sizeof(UncompressedEntry) - 1] = '\0';
    memcpy(ue->argData, tmp, sizeof(tmp));

    ASSERT_LE(2, GeneratedFunctions::numLogIds);


    uint64_t compressedLogs = 0;
    long bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);
    EXPECT_EQ(2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);


    // Now let's swap to a different buffer and encoder two more entries
    // that intersplice between them in time.
    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 93;
    ++ue;
    ue->timestamp = 96;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           10,
                                           false,
                                           &compressedLogs);
    EXPECT_EQ(4, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ++ue;
    ue->timestamp = 111;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           10,
                                           true,
                                           &compressedLogs);
    EXPECT_EQ(6, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 145;
    ++ue;
    ue->timestamp = 156;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           true,
                                           &compressedLogs);
    EXPECT_EQ(8, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);


    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 118;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           sizeof(UncompressedEntry),
                                           10,
                                           false,
                                           &compressedLogs);
    EXPECT_EQ(9, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 91;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           sizeof(UncompressedEntry),
                                           11,
                                           false,
                                           &compressedLogs);
    EXPECT_EQ(10, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 135;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           sizeof(UncompressedEntry),
                                           12,
                                           true,
                                           &compressedLogs);
    EXPECT_EQ(11, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 126;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      sizeof(UncompressedEntry),
                                      7,
                                      true,
                                      &compressedLogs);
    EXPECT_EQ(12, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    /**
     * At this point, the file should look something like this
     * Checkpoint
     * BufferExtent 5
     *      LogMsg0 at time = 90  (order 0)
     *      LogMsg1 at time = 105 (order 5)
     *          'aaaaaaaaaa\0'
     * BufferExtent 10
     *      LogMsg0 at time = 93  (order 2)
     *      LogMsg1 at time = 96  (order 3)
     *          'aaaaaaaaaa\0'
     * BufferExtent 10 ====== newRound =====
     *      LogMsg0 at time = 100  (order 4)
     *      LogMsg1 at time = 111  (order 6)
     *          'aaaaaaaaaa\0'
     * BufferExtent 5 ===== newRound ======
     *      LogMsg0 at time = 145 (order 10)
     *      LogMsg1 at time = 156 (order 11)
     *          'aaaaaaaaaa\0'
     * BufferExtent 10
     *      LogMsg0 at time = 118 (order 7)
     * BuferExtent  11
     *      LogMsg0 at time = 91  (order 1)
     * BufferExtent 12 ===== newRound ======
     *      LogMsg0 at time = 135 (order 9)
     * BufferExtent 7 ===== newRound ======
     *      LogMsg0 at time = 126 (order 8)
     */

    // Write it out and read it back in.
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(buffer, encoder.getEncodedBytes());
    oFile.close();

    // Now let's attempt to parse it back and decompress it to testfile2
    Decoder dc;
    FILE *outputFd;
    std::ifstream iFile;

    ASSERT_TRUE(dc.open(testFile));
    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    EXPECT_TRUE(dc.internalDecompressUnordered(outputFd));
    EXPECT_EQ(12, dc.logMsgsPrinted);
    EXPECT_EQ(8, dc.numBufferFragmentsRead);
    EXPECT_EQ(1, dc.numCheckpointsRead);
    fclose(outputFd);

    // Read it back and compare
    iFile.open(decomp);
    ASSERT_TRUE(iFile.good());

    std::vector<std::string> iLines;
    for (int i = 0; i < 12; ++i) {
        ASSERT_TRUE(iFile.good());
        std::string iLine;
        std::getline(iFile, iLine);
        iLines.push_back(iLine.c_str() + 14); // +14 to skip date and hour
    }

    EXPECT_FALSE(iFile.eof());
    iFile.close();

    EXPECT_STREQ(iLines[0].c_str(),  "00:01.000000090 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[1].c_str(),  "00:01.000000105 testHelper/client.cc:21 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[2].c_str(),  "00:01.000000093 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[3].c_str(),  "00:01.000000096 testHelper/client.cc:21 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[4].c_str(),  "00:01.000000100 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[5].c_str(),  "00:01.000000111 testHelper/client.cc:21 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[6].c_str(),  "00:01.000000145 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[7].c_str(),  "00:01.000000156 testHelper/client.cc:21 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[8].c_str(),  "00:01.000000118 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[9].c_str(),  "00:01.000000091 testHelper/client.cc:20 NOTICE[11]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[10].c_str(), "00:01.000000135 testHelper/client.cc:20 NOTICE[12]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[11].c_str(), "00:01.000000126 testHelper/client.cc:20 NOTICE[7]: Simple log message with 0 parameters\r");

    // try iterative interface
    LogMessage msg;
    ASSERT_TRUE(dc.open(testFile));
    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    while (dc.getNextLogStatement(msg, outputFd));
    EXPECT_EQ(12, dc.logMsgsPrinted);
    EXPECT_EQ(8, dc.numBufferFragmentsRead);
    EXPECT_EQ(1, dc.numCheckpointsRead);
    fclose(outputFd);

    // Read it back and compare
    iFile.open(decomp);
    ASSERT_TRUE(iFile.good());

    iLines.clear();
    for (int i = 0; i < 12; ++i) {
        ASSERT_TRUE(iFile.good());
        std::string iLine;
        std::getline(iFile, iLine);
        iLines.push_back(iLine.c_str() + 14); // +14 to skip date and hour
    }

    EXPECT_STREQ(iLines[0].c_str(),  "00:01.000000090 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[1].c_str(),  "00:01.000000105 testHelper/client.cc:21 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[2].c_str(),  "00:01.000000093 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[3].c_str(),  "00:01.000000096 testHelper/client.cc:21 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[4].c_str(),  "00:01.000000100 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[5].c_str(),  "00:01.000000111 testHelper/client.cc:21 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[6].c_str(),  "00:01.000000145 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[7].c_str(),  "00:01.000000156 testHelper/client.cc:21 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r");
    EXPECT_STREQ(iLines[8].c_str(),  "00:01.000000118 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[9].c_str(),  "00:01.000000091 testHelper/client.cc:20 NOTICE[11]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[10].c_str(), "00:01.000000135 testHelper/client.cc:20 NOTICE[12]: Simple log message with 0 parameters\r");
    EXPECT_STREQ(iLines[11].c_str(), "00:01.000000126 testHelper/client.cc:20 NOTICE[7]: Simple log message with 0 parameters\r");

    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Try the ordered case
    dc.open(testFile);

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    dc.decompressTo(outputFd);
    EXPECT_EQ(12, dc.logMsgsPrinted);
    EXPECT_EQ(8, dc.numBufferFragmentsRead);
    EXPECT_EQ(1, dc.numCheckpointsRead);
    fclose(outputFd);

    const char* orderedLines[] = {
        "1969-12-31 16:00:01.000000090 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000091 testHelper/client.cc:20 NOTICE[11]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000093 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000096 testHelper/client.cc:21 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000100 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000105 testHelper/client.cc:21 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000111 testHelper/client.cc:21 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000118 testHelper/client.cc:20 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000126 testHelper/client.cc:20 NOTICE[7]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000135 testHelper/client.cc:20 NOTICE[12]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000145 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000156 testHelper/client.cc:21 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r"
    };

    std::string iLine;
    iFile.open(decomp);
    for (const char *line : orderedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        EXPECT_STREQ(line +  14, iLine.c_str() + 14); // +14 to skip date
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    std::remove(testFile);
    std::remove(decomp);
}

TEST_F(LogTest, Decoder_internalDecompress_fileBreaks) {
    // This test is what happens if we have a break in a file. It should
    // in theory output up to that point and no more.
    char inputBuffer[1000], outputBuffer[1000];
    const char *testFile = "/tmp/testFile";
    const char *decomp = "/tmp/testFile2";

    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    for (int i = 0; i < 5; ++i) {
        ue->timestamp = i;
        ue->fmtId = noParamsId;
        ue->entrySize = sizeof(UncompressedEntry);
        ++ue;
    }

    /// Simulate a file that's been written to 3x by using
    /// 3 encoders on the same file.
    uint64_t compressedLogs = 0;
    Encoder encoder(outputBuffer, 1000);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint = (Checkpoint*)outputBuffer;
    checkpoint->cyclesPerSecond = 1e9;
    checkpoint->rdtsc = 0;
    checkpoint->unixTime = 1;

    long bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                                    5*sizeof(UncompressedEntry),
                                                    5,
                                                    false,
                                                    &compressedLogs);
    EXPECT_EQ(5, compressedLogs);
    EXPECT_EQ(5*sizeof(UncompressedEntry), bytesRead);

    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(outputBuffer, encoder.getEncodedBytes());

    // Encoder 2 does nothing except output a checkpoint
    Encoder encoder2(outputBuffer, 1000);
    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint2 = (Checkpoint*)outputBuffer;
    checkpoint2->cyclesPerSecond = 1e9;
    checkpoint2->rdtsc = 0;
    checkpoint2->unixTime = 1;

    oFile.write(outputBuffer, encoder2.getEncodedBytes());

    Encoder encoder3(outputBuffer, 1000);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint3 = (Checkpoint*)outputBuffer;
    checkpoint3->cyclesPerSecond = 1e9;
    checkpoint3->rdtsc = 0;
    checkpoint3->unixTime = 1;

    bytesRead = encoder3.encodeLogMsgs(inputBuffer,
                                                    5*sizeof(UncompressedEntry),
                                                    5,
                                                    true,
                                                    &compressedLogs);
    EXPECT_EQ(10, compressedLogs);
    EXPECT_EQ(5*sizeof(UncompressedEntry), bytesRead);
    oFile.write(outputBuffer, encoder3.getEncodedBytes());
    oFile.close();

    // Now let's attempt to parse it back and decompress it to decomp
    Decoder dc;
    ASSERT_TRUE(dc.open(testFile));
    FILE *outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    EXPECT_TRUE(dc.internalDecompressUnordered(outputFd));
    EXPECT_EQ(10, dc.logMsgsPrinted);
    EXPECT_EQ(2, dc.numBufferFragmentsRead);
    EXPECT_EQ(3, dc.numCheckpointsRead);
    fclose(outputFd);

    // Read it back and compare
    std::ifstream iFile;
    iFile.open(decomp);
    ASSERT_TRUE(iFile.good());

    const char* expectedLines[] = {
        "1969-12-31 16:00:01.000000000 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000001 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000002 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000003 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000004 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "\r",
        "# New execution started\r",
        "\r",
        "# New execution started\r",
        "1969-12-31 16:00:01.000000000 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000001 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000002 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000003 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000004 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r"
    };

    std::string iLine;
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Try iterative interface
    LogMessage logMsg;
    dc.open(testFile);

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    while (dc.getNextLogStatement(logMsg, outputFd));
    EXPECT_EQ(10, dc.logMsgsPrinted);
    EXPECT_EQ(2, dc.numBufferFragmentsRead);
    EXPECT_EQ(3, dc.numCheckpointsRead);
    fclose(outputFd);

    iFile.open(decomp);
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Try the ordered case
    dc.open(testFile);

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    dc.decompressTo(outputFd);
    EXPECT_EQ(10, dc.logMsgsPrinted);
    EXPECT_EQ(2, dc.numBufferFragmentsRead);
    EXPECT_EQ(3, dc.numCheckpointsRead);
    fclose(outputFd);

    iFile.open(decomp);
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    std::remove(testFile);
    std::remove(decomp);
}

TEST_F(LogTest, Decoder_internalDecompress_fileBreaks2) {
    // This test is what happens if we have a break in a file. It should
    // in theory output up to that point and no more.
    char inputBuffer[1000], outputBuffer[1000];
    const char *testFile = "/tmp/testFile";
    const char *decomp = "/tmp/testFile2";

    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    for (int i = 0; i < 5; ++i) {
        ue->timestamp = i;
        ue->fmtId = noParamsId;
        ue->entrySize = sizeof(UncompressedEntry);
        ++ue;
    }

    /// Simulate a file that's been written to 3x by using
    /// 3 encoders on the same file.
    uint64_t compressedLogs = 0;
    Encoder encoder(outputBuffer, 1000);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint = (Checkpoint*)outputBuffer;
    checkpoint->cyclesPerSecond = 1e9;
    checkpoint->rdtsc = 0;
    checkpoint->unixTime = 1;

    long bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           1*sizeof(UncompressedEntry),
                                           5,
                                           true,
                                           &compressedLogs);
    EXPECT_EQ(1, compressedLogs);
    EXPECT_EQ(1*sizeof(UncompressedEntry), bytesRead);

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      2*sizeof(UncompressedEntry),
                                      5,
                                      true,
                                      &compressedLogs);
    EXPECT_EQ(3, compressedLogs);
    EXPECT_EQ(2*sizeof(UncompressedEntry), bytesRead);


    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      2*sizeof(UncompressedEntry),
                                      5,
                                      true,
                                      &compressedLogs);
    EXPECT_EQ(5, compressedLogs);
    EXPECT_EQ(2*sizeof(UncompressedEntry), bytesRead);

    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(outputBuffer, encoder.getEncodedBytes());

    Encoder encoder3(outputBuffer, 1000);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint3 = (Checkpoint*)outputBuffer;
    checkpoint3->cyclesPerSecond = 1e9;
    checkpoint3->rdtsc = 0;
    checkpoint3->unixTime = 1;

    bytesRead = encoder3.encodeLogMsgs(inputBuffer,
                                       1*sizeof(UncompressedEntry),
                                       1,
                                       false,
                                       &compressedLogs);
    EXPECT_EQ(6, compressedLogs);
    EXPECT_EQ(1*sizeof(UncompressedEntry), bytesRead);
    oFile.write(outputBuffer, encoder3.getEncodedBytes());
    oFile.close();

    // Now let's attempt to parse it back and read the output
    const char* expectedLines[] = {
            "1969-12-31 16:00:01.000000000 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
            "1969-12-31 16:00:01.000000000 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
            "1969-12-31 16:00:01.000000000 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
            "1969-12-31 16:00:01.000000001 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
            "1969-12-31 16:00:01.000000001 testHelper/client.cc:20 NOTICE[5]: Simple log message with 0 parameters\r",
            "\r",
            "# New execution started\r",
            "1969-12-31 16:00:01.000000000 testHelper/client.cc:20 NOTICE[1]: Simple log message with 0 parameters\r"
    };

    // Try the ordered case
    Decoder dc;
    dc.open(testFile);

    FILE* outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    dc.decompressTo(outputFd);
    EXPECT_EQ(6, dc.logMsgsPrinted);
    EXPECT_EQ(4, dc.numBufferFragmentsRead);
    EXPECT_EQ(2, dc.numCheckpointsRead);
    fclose(outputFd);

    std::string iLine;
    std::ifstream iFile;

    iFile.open(decomp);
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    std::remove(testFile);
    std::remove(decomp);
}

TEST_F(LogTest, Decoder_decompressNextLogStatement_timeTravel) {
    // Tests what happen when the checkpoint is newer than the log message.
    char inputBuffer[1000], outputBuffer[1000];
    const char *testFile = "/tmp/testFile";
    const char *decomp = "/tmp/testFile2";
    LogMessage logMsg;

    UncompressedEntry* ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 10e9 + 1;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    uint64_t compressedLogs = 0;
    Encoder encoder(outputBuffer, 1000);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint = (Checkpoint*)outputBuffer;
    checkpoint->cyclesPerSecond = 1e9;
    checkpoint->rdtsc = 20e9;
    checkpoint->unixTime = 30;

    long bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                                    sizeof(UncompressedEntry),
                                                    1,
                                                    false,
                                                    &compressedLogs);
    EXPECT_EQ(1, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(outputBuffer, encoder.getEncodedBytes());
    oFile.close();

    // Now let's attempt to parse it back and decompress it to decomp
    Decoder dc;
    ASSERT_TRUE(dc.open(testFile));
    FILE *outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    EXPECT_TRUE(dc.internalDecompressUnordered(outputFd));
    EXPECT_EQ(1, dc.logMsgsPrinted);
    fclose(outputFd);

    // Read it back and compare
    std::ifstream iFile;
    iFile.open(decomp);
    ASSERT_TRUE(iFile.good());

    const char* expectedLines[] = {
        "1969-12-31 16:00:20.000000001 testHelper/client.cc:20 NOTICE[1]: Simple log message with 0 parameters\r"
    };

    std::string iLine;
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Try iterative interface
    dc.open(testFile);

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    while (dc.getNextLogStatement(logMsg, outputFd));
    EXPECT_EQ(1, dc.logMsgsPrinted);
    EXPECT_EQ(1, dc.numBufferFragmentsRead);
    EXPECT_EQ(1, dc.numCheckpointsRead);
    fclose(outputFd);

    iFile.open(decomp);
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Try the ordered case
    dc.open(testFile);

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    dc.decompressTo(outputFd);
    EXPECT_EQ(1, dc.logMsgsPrinted);
    EXPECT_EQ(1, dc.numBufferFragmentsRead);
    EXPECT_EQ(1, dc.numCheckpointsRead);
    fclose(outputFd);

    iFile.open(decomp);
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    std::remove(testFile);
    std::remove(decomp);
}

TEST_F(LogTest, Decoder_getNextLogStatement) {
    // First we have to create a log file with encoder.
    const char *testFile = "/tmp/testFile";
    const char *decomp = "/tmp/testFile2";
    char inputBuffer[1000], buffer[1000];
    Encoder encoder(buffer, 1000, false, true);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint = (Checkpoint *) encoder.backing_buffer;
    checkpoint->cyclesPerSecond = 1e9;
    checkpoint->rdtsc = 0;
    checkpoint->unixTime = 1;

    char *writePos = inputBuffer;
    UncompressedEntry *ue = reinterpret_cast<UncompressedEntry *>(writePos);
    ue->timestamp = 10;
    ue->fmtId = integerParamId;
    ue->entrySize = sizeof(UncompressedEntry) + sizeof(int);
    writePos += ue->entrySize;
    *((int*)(ue->argData)) = 1;

    ue = reinterpret_cast<UncompressedEntry *>(writePos);
    ue->timestamp = 20;
    ue->fmtId = integerParamId;
    ue->entrySize = sizeof(UncompressedEntry) + sizeof(int);
    writePos += ue->entrySize;
    *((int*)(ue->argData)) = -2;

    ue = reinterpret_cast<UncompressedEntry *>(writePos);
    ue->timestamp = 30;
    ue->fmtId = uint64_tParamId;
    ue->entrySize = sizeof(UncompressedEntry) + sizeof(uint64_t);
    writePos += ue->entrySize;
    *((uint64_t*)(ue->argData)) = 3;

    ue = reinterpret_cast<UncompressedEntry *>(writePos);
    ue->timestamp = 40;
    ue->fmtId = doubleParamId;
    ue->entrySize = sizeof(UncompressedEntry) + sizeof(double);
    writePos += ue->entrySize;
    *((double*)(ue->argData)) = 4.0;

    // Note; this one is special since it has multiple args
    const char *strParam = "eight point oh";
    ue = reinterpret_cast<UncompressedEntry *>(writePos);
    ue->timestamp = 50;
    ue->fmtId = mixParamId;
    ue->entrySize = sizeof(UncompressedEntry) + sizeof(int) + sizeof(double) + sizeof(uint32_t) + strlen(strParam) + 1;
    writePos += sizeof(UncompressedEntry);

    *(reinterpret_cast<int*>(writePos)) = 5;
    writePos += sizeof(int);

    *(reinterpret_cast<double*>(writePos)) = 6.0;
    writePos += sizeof(double);

    *(reinterpret_cast<uint32_t*>(writePos)) = 7;
    writePos += sizeof(uint32_t);

    strcpy(writePos, strParam);
    writePos += strlen(strParam) + 1;

    // Finally, finish it off with a final double to make sure strings work
    ue = reinterpret_cast<UncompressedEntry *>(writePos);
    ue->timestamp = 60;
    ue->fmtId = doubleParamId;
    ue->entrySize = sizeof(UncompressedEntry) + sizeof(double);
    writePos += ue->entrySize;
    *((double*)(ue->argData)) = 9.0;

    uint64_t compressedLogs = 0;
    long bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           writePos - inputBuffer,
                                           1,
                                           false,
                                           &compressedLogs);
    EXPECT_EQ(6, compressedLogs);
    EXPECT_EQ(159, bytesRead);


    // Write it out and read it back in.
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(buffer, encoder.getEncodedBytes());
    oFile.close();

    std::string iLine;
    std::ifstream iFile;
    FILE *outputFd;

    const char* expectedLines[] = {
            "1969-12-31 16:00:01.000000010 testHelper/client.cc:28 NOTICE[1]: I have an integer 1\r",
            "1969-12-31 16:00:01.000000020 testHelper/client.cc:28 NOTICE[1]: I have an integer -2\r",
            "1969-12-31 16:00:01.000000030 testHelper/client.cc:29 NOTICE[1]: I have a uint64_t 3\r",
            "1969-12-31 16:00:01.000000040 testHelper/client.cc:30 NOTICE[1]: I have a double 4.000000\r",
            "1969-12-31 16:00:01.000000050 testHelper/client.cc:31 NOTICE[1]: I have a couple of things 5, 6.000000, 7, eight point oh\r",
            "1969-12-31 16:00:01.000000060 testHelper/client.cc:30 NOTICE[1]: I have a double 9.000000\r"
    };

    LogMessage logMsg;
    Decoder dc;

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    ASSERT_TRUE(dc.open(testFile));
    while (dc.getNextLogStatement(logMsg, outputFd));
    EXPECT_EQ(6, dc.logMsgsPrinted);
    EXPECT_EQ(1, dc.numBufferFragmentsRead);
    EXPECT_EQ(1, dc.numCheckpointsRead);
    fclose(outputFd);

    iFile.open(decomp);
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        if (iLine.size() >= 14)
            EXPECT_STREQ(line + 14, iLine.c_str() + 14);// +14 skips date + hour
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Now let's try accessing the fields one by one...
    dc.open(testFile);
    ASSERT_TRUE(dc.getNextLogStatement(logMsg));
    ASSERT_TRUE(logMsg.valid());
    EXPECT_EQ(integerParamId, logMsg.getLogId());
    EXPECT_EQ(10, logMsg.getTimestamp());
    EXPECT_EQ(1, logMsg.getNumArgs());
    EXPECT_EQ(1, logMsg.get<int>(0));

    ASSERT_TRUE(dc.getNextLogStatement(logMsg));
    ASSERT_TRUE(logMsg.valid());
    EXPECT_EQ(integerParamId, logMsg.getLogId());
    EXPECT_EQ(20, logMsg.getTimestamp());
    EXPECT_EQ(1, logMsg.getNumArgs());
    EXPECT_EQ(-2, logMsg.get<int>(0));

    ASSERT_TRUE(dc.getNextLogStatement(logMsg));
    ASSERT_TRUE(logMsg.valid());
    EXPECT_EQ(uint64_tParamId, logMsg.getLogId());
    EXPECT_EQ(30, logMsg.getTimestamp());
    EXPECT_EQ(1, logMsg.getNumArgs());
    EXPECT_EQ(3U, logMsg.get<uint64_t>(0));

    ASSERT_TRUE(dc.getNextLogStatement(logMsg));
    ASSERT_TRUE(logMsg.valid());
    EXPECT_EQ(doubleParamId, logMsg.getLogId());
    EXPECT_EQ(40, logMsg.getTimestamp());
    EXPECT_EQ(1, logMsg.getNumArgs());
    EXPECT_EQ(4.0, logMsg.get<double>(0));

    // LogMsg: "I have a couple of things 5, 6.000000, 7, eight point oh"
    ASSERT_TRUE(dc.getNextLogStatement(logMsg));
    ASSERT_TRUE(logMsg.valid());
    EXPECT_EQ(mixParamId, logMsg.getLogId());
    EXPECT_EQ(50, logMsg.getTimestamp());
    ASSERT_EQ(4, logMsg.getNumArgs());
    EXPECT_EQ(5U, logMsg.get<int>(0));
    EXPECT_EQ(6.0, logMsg.get<double>(1));
    EXPECT_EQ(7, logMsg.get<uint32_t>(2));
    EXPECT_STREQ(strParam, logMsg.get<const char*>(3));

    ASSERT_TRUE(dc.getNextLogStatement(logMsg));
    ASSERT_TRUE(logMsg.valid());
    EXPECT_EQ(doubleParamId, logMsg.getLogId());
    EXPECT_EQ(60, logMsg.getTimestamp());
    EXPECT_EQ(1, logMsg.getNumArgs());
    EXPECT_EQ(9.0, logMsg.get<double>(0));

    EXPECT_FALSE(dc.getNextLogStatement(logMsg));
    EXPECT_FALSE(logMsg.valid());

    std::remove(testFile);
    std::remove(decomp);
}

// Static helper functions to test when aggregation is run.
static int numInvocations = 0;

static void
countFn(const char *fmtString, ...) {
    ++numInvocations;
}

static void
resetCount() {
    numInvocations = 0;
}

static int
getCount() {
    return numInvocations;
}

TEST_F(LogTest, Decoder_internalDecompress_aggregationFn) {
    // First we have to create a log file with encoder.
    const char *testFile = "/tmp/testFile";
    const char *decomp = "/tmp/testFile2";
    char inputBuffer[1000], buffer[1000];
    Encoder encoder(buffer, 1000, false);

    // Hack to load fake Checkpoint values to get a consistent time output
    Checkpoint *checkpoint = (Checkpoint *) encoder.backing_buffer;
    checkpoint->cyclesPerSecond = 1e9;
    checkpoint->rdtsc = 0;
    checkpoint->unixTime = 1;

    UncompressedEntry *ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 90;
    ue->fmtId = noParamsId;
    ue->entrySize = sizeof(UncompressedEntry);

    ++ue;
    ue->timestamp = 105;
    ue->fmtId = stringParamId;
    ue->entrySize = 2 * sizeof(UncompressedEntry);

    // Okay, this is really starting to dig deep into the implementation of
    // how log messages are interpreted.... so if failures occur... yeah.
    char tmp[sizeof(UncompressedEntry)];
    memset(tmp, 'a', sizeof(UncompressedEntry));
    tmp[sizeof(UncompressedEntry) - 1] = '\0';
    memcpy(ue->argData, tmp, sizeof(tmp));

    ASSERT_LE(2, GeneratedFunctions::numLogIds);


    uint64_t compressedLogs = 0;
    long bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                           3 * sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);
    EXPECT_EQ(2, compressedLogs);
    EXPECT_EQ(3 * sizeof(UncompressedEntry), bytesRead);


    // Now let's swap to a different buffer and encoder two more entries
    // that intersplice between them in time.
    ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 93;
    ++ue;
    ue->timestamp = 96;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      3 * sizeof(UncompressedEntry),
                                      10,
                                      false,
                                      &compressedLogs);
    EXPECT_EQ(4, compressedLogs);
    EXPECT_EQ(3 * sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 100;
    ++ue;
    ue->timestamp = 111;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      3 * sizeof(UncompressedEntry),
                                      10,
                                      true,
                                      &compressedLogs);
    EXPECT_EQ(6, compressedLogs);
    EXPECT_EQ(3 * sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 145;
    ++ue;
    ue->timestamp = 156;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      3 * sizeof(UncompressedEntry),
                                      5,
                                      true,
                                      &compressedLogs);
    EXPECT_EQ(8, compressedLogs);
    EXPECT_EQ(3 * sizeof(UncompressedEntry), bytesRead);


    ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 118;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      sizeof(UncompressedEntry),
                                      10,
                                      false,
                                      &compressedLogs);
    EXPECT_EQ(9, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 91;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      sizeof(UncompressedEntry),
                                      11,
                                      false,
                                      &compressedLogs);
    EXPECT_EQ(10, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 135;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      sizeof(UncompressedEntry),
                                      12,
                                      true,
                                      &compressedLogs);
    EXPECT_EQ(11, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    ue = reinterpret_cast<UncompressedEntry *>(inputBuffer);
    ue->timestamp = 126;
    ue->fmtId = noParamsId;

    bytesRead = encoder.encodeLogMsgs(inputBuffer,
                                      sizeof(UncompressedEntry),
                                      7,
                                      true,
                                      &compressedLogs);
    EXPECT_EQ(12, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);

    /**
     * At this point, the file should look something like this
     * Checkpoint
     * BufferExtent 5
     *      LogMsg0 at time = 90  (order 0)
     *      LogMsg1 at time = 105 (order 5)
     *          'aaaaaaaaaa\0'
     * BufferExtent 10
     *      LogMsg0 at time = 93  (order 2)
     *      LogMsg1 at time = 96  (order 3)
     *          'aaaaaaaaaa\0'
     * BufferExtent 10 ====== newRound =====
     *      LogMsg0 at time = 100  (order 4)
     *      LogMsg1 at time = 111  (order 6)
     *          'aaaaaaaaaa\0'
     * BufferExtent 5 ===== newRound ======
     *      LogMsg0 at time = 145 (order 10)
     *      LogMsg1 at time = 156 (order 11)
     *          'aaaaaaaaaa\0'
     * BufferExtent 10
     *      LogMsg0 at time = 118 (order 7)
     * BuferExtent  11
     *      LogMsg0 at time = 91  (order 1)
     * BufferExtent 12 ===== newRound ======
     *      LogMsg0 at time = 135 (order 9)
     * BufferExtent 7 ===== newRound ======
     *      LogMsg0 at time = 126 (order 8)
     */

    // Write it out and read it back in.
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(buffer, encoder.getEncodedBytes());
    oFile.close();

    // Let's try to aggregate nothing
    {
        resetCount();
        Decoder dc;
        ASSERT_TRUE(dc.open(testFile));
        FILE *outputFd = fopen(decomp, "w");
        ASSERT_NE(nullptr, outputFd);
        EXPECT_TRUE(dc.internalDecompressUnordered(outputFd, -1, countFn));
        EXPECT_EQ(0U, getCount());
        EXPECT_EQ(12, dc.logMsgsPrinted);
        fclose(outputFd);
    }

    // Let's try to aggregate LogMsg0
    {
        resetCount();
        Decoder dc;
        ASSERT_TRUE(dc.open(testFile));
        FILE *outputFd = fopen(decomp, "w");
        ASSERT_NE(nullptr, outputFd);
        EXPECT_TRUE(dc.internalDecompressUnordered(outputFd,
                                                   noParamsId,
                                                   countFn));
        EXPECT_EQ(8U, getCount());
        EXPECT_EQ(12, dc.logMsgsPrinted);
        fclose(outputFd);
    }

    // Let's try to aggregate LogMsg1
    {
        resetCount();
        Decoder dc;
        ASSERT_TRUE(dc.open(testFile));
        FILE *outputFd = fopen(decomp, "w");
        ASSERT_NE(nullptr, outputFd);
        EXPECT_TRUE(dc.internalDecompressUnordered(outputFd,
                                                   stringParamId,
                                                   countFn));
        EXPECT_EQ(4U, getCount());
        EXPECT_EQ(12, dc.logMsgsPrinted);
        fclose(outputFd);
    }

    std::remove(testFile);
    std::remove(decomp);
}

static int compressHelper0TimesRun = 0;
static int compressHelper1TimesRun = 0;

static void
compressHelper0(int numNibbles, const ParamType*, char **in, char**out)
{
    ++compressHelper0TimesRun;
}

static void
compressHelper1(int numNibbles, const ParamType*, char **in, char**out)
{
    ++compressHelper1TimesRun;
}

template<typename T>
static T*
push(char* (&in)) {
    T *t = reinterpret_cast<T*>(in);
    in += sizeof(T);
    return t;
}

// Most of the logic is already extensively tested in the other encodeMsgs
// tests, so this will only test the new bits of code for cpp17
TEST_F(LogTest, encodeLogMsgs_cpp17) {
    char inBuffer[1024];
    char outBuffer[1024];

    char *in = inBuffer;
    char *out = outBuffer;

    uint64_t numEventsCompressed = 0;
    std::vector<StaticLogInfo> dictionary;
    NanoLogInternal::ParamType paramTypes[10];
    dictionary.emplace_back(&compressHelper0, "File", 123, 0, "Hello World", 0, 0, paramTypes);
    dictionary.emplace_back(&compressHelper1, "FileA", 99, 2, "Hello World %s", 0, 0, paramTypes);

    // Case 1: early break because we haven't persisted the dictionary entries
    UncompressedEntry *ue = push<UncompressedEntry>(in);
    ue->entrySize = sizeof(UncompressedEntry);
    ue->timestamp = 0;
    ue->fmtId = 10;

    UncompressedEntry *ue2 = push<UncompressedEntry>(in);
    ue2->entrySize = sizeof(UncompressedEntry);
    ue2->timestamp = 1;
    ue2->fmtId = 1;

    Encoder encoder(outBuffer, sizeof(outBuffer), true);

    // Case 1, not enough dictionary entries
    EXPECT_EQ(0, encoder.encodeMissDueToMetadata);
    EXPECT_EQ(0, encoder.consecutiveEncodeMissesDueToMetadata);

    in = inBuffer;
    char *encoderStartingPos = encoder.writePos;
    EXPECT_EQ(0, encoder.encodeLogMsgs(in, sizeof(UncompressedEntry), 0, false,
                                            dictionary, &numEventsCompressed));
    EXPECT_EQ(0, numEventsCompressed);
    EXPECT_EQ(encoderStartingPos + sizeof(BufferExtent), encoder.writePos);

    EXPECT_EQ(1, encoder.encodeMissDueToMetadata);
    EXPECT_EQ(1, encoder.consecutiveEncodeMissesDueToMetadata);

    // Case 1, part 2: Not enough dictionary entries a whole bunch of times.
    testing::internal::CaptureStderr();
    for (int i = 0; i < 1000; ++i){ // 1000 corresponds to magic number in code
        encoder.writePos = encoder.backing_buffer;
        EXPECT_EQ(0, encoder. encodeLogMsgs(in, sizeof(UncompressedEntry),
                                0, false, dictionary, &numEventsCompressed));
    }

    EXPECT_EQ(1001, encoder.encodeMissDueToMetadata);
    EXPECT_EQ(1001, encoder.consecutiveEncodeMissesDueToMetadata);

    std::string output = testing::internal::GetCapturedStderr().c_str();
    EXPECT_STREQ("NanoLog Error: Metadata missing for a dynamic log message "
                 "(id=10) during compression. If you are using Preprocessor "
                 "NanoLog, there is be a problem with your integration "
                 "(static logs detected=10).\r\n",
                 output.c_str());

    // Case 2: Normal Compression
    ue->fmtId = 0;
    in = inBuffer;
    encoderStartingPos = encoder.writePos;
    EXPECT_EQ(2*sizeof(UncompressedEntry),
                  encoder.encodeLogMsgs(in, 2*sizeof(UncompressedEntry), 0,
                                      false, dictionary, &numEventsCompressed));
    EXPECT_EQ(2, numEventsCompressed);
    EXPECT_EQ(encoderStartingPos
                + sizeof(BufferExtent)
                + 2*sizeof(CompressedEntry)
                + 4, // +4 for the 2x2 compacted timestamp + logId
                encoder.writePos);

    EXPECT_EQ(1, compressHelper0TimesRun);
    EXPECT_EQ(1, compressHelper1TimesRun);

    EXPECT_EQ(1001, encoder.encodeMissDueToMetadata);
    EXPECT_EQ(0, encoder.consecutiveEncodeMissesDueToMetadata);
}

TEST_F(LogTest, createMicroCode) {
    using namespace NanoLogInternal::Log;
    char backing_buffer[1024];
    char *microCode = backing_buffer;
    memset(backing_buffer, 'a', sizeof(backing_buffer));

    // Error Cases
    testing::internal::CaptureStderr();
    EXPECT_FALSE(Decoder::createMicroCode(&microCode, "%Ls", "file", 4, 2));
    EXPECT_EQ(backing_buffer, microCode);
    std::string output = testing::internal::GetCapturedStderr().c_str();
    EXPECT_STREQ("Attempt to decode format specifier failed: Ls\r\n"
                 "Error: Couldn't process this: %Ls\r\n", output.c_str());

    // No format specifiers
    microCode = backing_buffer;
    EXPECT_TRUE(Decoder::createMicroCode(&microCode, "Nothing", "file", 4, 0));

    EXPECT_EQ(sizeof(FormatMetadata)
                    + strlen("file") + 1
                    + sizeof(PrintFragment)
                    + strlen("Nothing") + 1,
            microCode - backing_buffer);

    microCode = backing_buffer;
    FormatMetadata *fm = push<FormatMetadata>(microCode);
    microCode += fm->filenameLength;
    PrintFragment *pf = push<PrintFragment>(microCode);

    EXPECT_STREQ("file", fm->filename);
    EXPECT_EQ(5, fm->filenameLength);
    EXPECT_EQ(4, fm->lineNumber);
    EXPECT_EQ(0, fm->logLevel);
    EXPECT_EQ(0, fm->numNibbles);
    EXPECT_EQ(1, fm->numPrintFragments);

    EXPECT_EQ(FormatType::NONE, pf->argType);
    EXPECT_STREQ("Nothing", pf->formatFragment);
    EXPECT_EQ(strlen("Nothing") + 1, pf->fragmentLength);
    EXPECT_FALSE(pf->hasDynamicPrecision);
    EXPECT_FALSE(pf->hasDynamicWidth);

    // A very complex one
    microCode = backing_buffer;
    const char *filename = "DatFile.txt";
    const char *formatString = "%% %*.4s %lf %Lf %4.*ls %*.*d blah";
    EXPECT_TRUE(Decoder::createMicroCode(&microCode,
                                         formatString,
                                         filename,
                                         1234,
                                         1));

    microCode = backing_buffer;
    fm = push<FormatMetadata>(microCode);
    microCode += fm->filenameLength;

    EXPECT_STREQ(filename, fm->filename);
    EXPECT_EQ(strlen(filename) + 1, fm->filenameLength);
    EXPECT_EQ(1234, fm->lineNumber);
    EXPECT_EQ(1, fm->logLevel);
    EXPECT_EQ(7, fm->numNibbles);
    EXPECT_EQ(5, fm->numPrintFragments);

    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::const_char_ptr_t, pf->argType);
    EXPECT_STREQ("%% %*.4s", pf->formatFragment);
    EXPECT_EQ(strlen("%% %*.4s") + 1, pf->fragmentLength);
    EXPECT_TRUE(pf->hasDynamicWidth);
    EXPECT_FALSE(pf->hasDynamicPrecision);

    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::double_t, pf->argType);
    EXPECT_STREQ(" %lf", pf->formatFragment);
    EXPECT_EQ(strlen(" %lf") + 1, pf->fragmentLength);
    EXPECT_FALSE(pf->hasDynamicWidth);
    EXPECT_FALSE(pf->hasDynamicPrecision);

    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::long_double_t, pf->argType);
    EXPECT_STREQ(" %Lf", pf->formatFragment);
    EXPECT_EQ(strlen(" %Lf") + 1, pf->fragmentLength);
    EXPECT_FALSE(pf->hasDynamicWidth);
    EXPECT_FALSE(pf->hasDynamicPrecision);

    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::const_wchar_t_ptr_t, pf->argType);
    EXPECT_STREQ(" %4.*ls", pf->formatFragment);
    EXPECT_EQ(strlen(" %4.*ls") + 1, pf->fragmentLength);
    EXPECT_FALSE(pf->hasDynamicWidth);
    EXPECT_TRUE(pf->hasDynamicPrecision);

    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::int_t, pf->argType);
    EXPECT_STREQ(" %*.*d blah", pf->formatFragment);
    EXPECT_EQ(strlen(" %*.*d blah") + 1, pf->fragmentLength);
    EXPECT_TRUE(pf->hasDynamicWidth);
    EXPECT_TRUE(pf->hasDynamicPrecision);
}

TEST_F(LogTest, createMicroCode_specifiersWithoutSpaces) {
    using namespace NanoLogInternal::Log;
    FormatMetadata *fm;
    PrintFragment *pf;
    char backing_buffer[1024];
    char *microCode = backing_buffer;
    memset(backing_buffer, 'a', sizeof(backing_buffer));

    microCode = backing_buffer;
    const char *filename = "DatFile.txt";
    const char *formatString = "%%%lf%Lf%*.*d";
    EXPECT_TRUE(Decoder::createMicroCode(&microCode,
                                         formatString,
                                         filename,
                                         1234,
                                         1));

    microCode = backing_buffer;
    fm = push<FormatMetadata>(microCode);
    microCode += fm->filenameLength;

    EXPECT_STREQ(filename, fm->filename);
    EXPECT_EQ(strlen(filename) + 1, fm->filenameLength);
    EXPECT_EQ(1234, fm->lineNumber);
    EXPECT_EQ(1, fm->logLevel);
    EXPECT_EQ(5, fm->numNibbles);
    EXPECT_EQ(3, fm->numPrintFragments);

    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::double_t, pf->argType);
    EXPECT_STREQ("%%%lf", pf->formatFragment);
    EXPECT_EQ(strlen("%%%lf") + 1, pf->fragmentLength);
    EXPECT_FALSE(pf->hasDynamicWidth);
    EXPECT_FALSE(pf->hasDynamicPrecision);

    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::long_double_t, pf->argType);
    EXPECT_STREQ("%Lf", pf->formatFragment);
    EXPECT_EQ(strlen("%Lf") + 1, pf->fragmentLength);
    EXPECT_FALSE(pf->hasDynamicWidth);
    EXPECT_FALSE(pf->hasDynamicPrecision);
    pf = push<PrintFragment>(microCode);
    microCode += pf->fragmentLength;

    EXPECT_EQ(FormatType::int_t, pf->argType);
    EXPECT_STREQ("%*.*d", pf->formatFragment);
    EXPECT_EQ(strlen("%*.*d") + 1, pf->fragmentLength);
    EXPECT_TRUE(pf->hasDynamicWidth);
    EXPECT_TRUE(pf->hasDynamicPrecision);
}

TEST_F(LogTest, readDictionaryFragment) {
    char testFile[] = "test.dic";
    char *buffer = static_cast<char*>(malloc(1024*1024));
    char *writePos = buffer;

    Decoder dc;

    DictionaryFragment *df = push<DictionaryFragment>(writePos);
    df->entryType = EntryType::LOG_MSGS_OR_DIC;
    df->totalMetadataEntries = 0;
    df->newMetadataBytes = 0;

    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(buffer, sizeof(DictionaryFragment) - 1);
    oFile.close();

    // Too few bytes
    FILE *fd = fopen(testFile, "rb");
    ASSERT_TRUE(fd);
    testing::internal::CaptureStderr();
    EXPECT_FALSE(dc.readDictionaryFragment(fd));
    EXPECT_STREQ("Could not read entire dictionary fragment header\r\n",
                 testing::internal::GetCapturedStderr().c_str());
    fclose(fd);
    std::remove(testFile);

    // Just enough, but header only
    oFile.open(testFile);
    oFile.write(buffer, sizeof(DictionaryFragment));
    oFile.close();

    fd = fopen(testFile, "rb");
    ASSERT_TRUE(fd);
    EXPECT_TRUE(dc.readDictionaryFragment(fd));
    fclose(fd);
    std::remove(testFile);

    // Header and incomplete Compressed Info
    writePos = buffer + sizeof(DictionaryFragment);
    CompressedLogInfo *cli = push<CompressedLogInfo>(writePos);
    char filename[] = "file.txt";
    char formatString[] = "blah blah %s\r\n";
    cli->severity = 2;
    cli->linenum = 124;
    cli->filenameLength = strlen(filename) + 1;
    cli->formatStringLength = strlen(formatString) + 1;

    memcpy(writePos, filename, cli->filenameLength);
    writePos += cli->filenameLength;
    memcpy(writePos, formatString, cli->formatStringLength);
    writePos += cli->formatStringLength;


    char filename2[] = "fileTwo.txt";
    char formatString2[] = "yup yup %d %d\r\n";
    cli = push<CompressedLogInfo>(writePos);
    cli->severity = 2;
    cli->linenum = 124;
    cli->filenameLength = strlen(filename2) + 1;
    cli->formatStringLength = strlen(formatString2) + 1;

    memcpy(writePos, filename2, cli->filenameLength);
    writePos += cli->filenameLength;
    memcpy(writePos, formatString2, cli->formatStringLength);
    writePos += cli->formatStringLength;

    df->newMetadataBytes = writePos - buffer;
    df->totalMetadataEntries = 2;

    oFile.open(testFile);
    oFile.write(buffer, sizeof(DictionaryFragment) + sizeof(CompressedLogInfo) - 1);
    oFile.close();

    fd = fopen(testFile, "rb");
    ASSERT_TRUE(fd);
    testing::internal::CaptureStderr();
    EXPECT_FALSE(dc.readDictionaryFragment(fd));
    EXPECT_STREQ("Could not read in log metadata\r\n",
                 testing::internal::GetCapturedStderr().c_str());
    fclose(fd);
    std::remove(testFile);

    // Header and complete CompressedInfo, but incomplete filenames
    oFile.open(testFile);
    oFile.write(buffer, sizeof(DictionaryFragment)
                            + sizeof(CompressedLogInfo)
                            + strlen(filename)
                            + strlen(formatString) - 1);
    oFile.close();

    fd = fopen(testFile, "rb");
    ASSERT_TRUE(fd);
    testing::internal::CaptureStderr();
    EXPECT_FALSE(dc.readDictionaryFragment(fd));
    EXPECT_STREQ("Could not read in a log's filename/format string\r\n",
                 testing::internal::GetCapturedStderr().c_str());
    fclose(fd);
    std::remove(testFile);

    // Finally, one that works okay
    oFile.open(testFile);
    oFile.write(buffer, writePos - buffer);
    oFile.close();

    fd = fopen(testFile, "rb");
    ASSERT_TRUE(fd);
    EXPECT_TRUE(dc.readDictionaryFragment(fd));
    ASSERT_EQ(2, dc.fmtId2fmtString.size());
    EXPECT_STREQ(formatString, dc.fmtId2fmtString.at(0).c_str());
    EXPECT_STREQ(formatString2, dc.fmtId2fmtString.at(1).c_str());

    EXPECT_EQ(2, dc.fmtId2metadata.size());

    // And then we duplicate the dictoinary and should end up with 4
    fd = fopen(testFile, "rb");
    ASSERT_TRUE(fd);
    EXPECT_TRUE(dc.readDictionaryFragment(fd));
    ASSERT_EQ(4, dc.fmtId2fmtString.size());
    EXPECT_STREQ(formatString, dc.fmtId2fmtString.at(0).c_str());
    EXPECT_STREQ(formatString2, dc.fmtId2fmtString.at(1).c_str());
    EXPECT_STREQ(formatString, dc.fmtId2fmtString.at(2).c_str());
    EXPECT_STREQ(formatString2, dc.fmtId2fmtString.at(3).c_str());

    ASSERT_EQ(4, dc.fmtId2metadata.size());

    fclose(fd);
    std::remove(testFile);
    free(buffer);
}


TEST_F(LogTest, LogMessage_constructor) {
    LogMessage la;

    EXPECT_EQ(nullptr, la.metadata);
    EXPECT_EQ(uint32_t(-1), la.logId);
    EXPECT_EQ(0, la.rdtsc);
    EXPECT_EQ(0, la.numArgs);
    EXPECT_EQ(10, la.totalCapacity);
    EXPECT_EQ(nullptr, la.rawArgsExtension);

    EXPECT_FALSE(la.valid());
}

TEST_F(LogTest, LogMessage_reserve) {
    LogMessage la;

    la.reserve(2);
    EXPECT_EQ(0, la.numArgs);
    EXPECT_EQ(10, la.totalCapacity);
    EXPECT_EQ(nullptr, la.rawArgsExtension);

    la.reserve(10);
    EXPECT_EQ(0, la.numArgs);
    EXPECT_EQ(10, la.totalCapacity);
    EXPECT_EQ(nullptr, la.rawArgsExtension);

    la.reserve(11);
    EXPECT_EQ(0, la.numArgs);
    EXPECT_EQ(20, la.totalCapacity);
    EXPECT_NE(nullptr, la.rawArgsExtension);
}

TEST_F(LogTest, LogMesssage_push_get) {
    LogMessage la;

    la.push((uint32_t) 5);
    la.push((void*) &la);
    la.push((double) 15.3);

    EXPECT_EQ(3, la.numArgs);
    EXPECT_EQ(5,    la.get<uint32_t>(0));
    EXPECT_EQ(&la,  la.get<void*>(1));
    EXPECT_EQ(15.3, la.get<double>(2));
}

TEST_F(LogTest, LogMessage_pushOverflow) {
    LogMessage la;

    for (int i = 0; i < 21; ++i)
        la.push(i);

    for (int i = 0; i < 21; ++i)
        EXPECT_EQ(i, la.get<int>(i));

    EXPECT_EQ(21, la.numArgs);
    EXPECT_EQ(40, la.totalCapacity);
    EXPECT_NE(nullptr, la.rawArgsExtension);
}

TEST_F(LogTest, LogMessage_longDoubles) {
    FormatMetadata fm;
    LogMessage la;

    la.reset(&fm);

    // Check that the long double doesn't overwrite data
    la.push<int>(1);
    long double arg = 0.2;
    la.push(arg);
    la.push<int>(2);

    EXPECT_TRUE(la.valid());
    EXPECT_EQ(1, la.get<int>(0));
    EXPECT_EQ(-1, la.get<int>(1));
    EXPECT_EQ(2, la.get<int>(2));

    // Check that we fail upon getting arg 2 as long double
    testing::internal::CaptureStderr();
    EXPECT_EQ(-1.0, la.get<long double>(1));
    std::string errorMsg = testing::internal::GetCapturedStderr();
    EXPECT_STREQ("**ERROR** Aggregating on Long Doubles is "
                 "currently unsupported\r\n", errorMsg.c_str());
}

TEST_F(LogTest, LogMessage_reset) {
    LogMessage la;

    la.push((uint32_t) 5);
    la.push((void*) &la);
    la.push((double) 15.3);

    ASSERT_EQ(3, la.numArgs);
    ASSERT_EQ(5,    la.get<uint32_t>(0));
    ASSERT_EQ(&la,  la.get<void*>(1));
    ASSERT_EQ(15.3, la.get<double>(2));

    la.reset(nullptr);
    EXPECT_FALSE(la.valid());
    EXPECT_EQ(0, la.numArgs);
    EXPECT_EQ(uint32_t(-1), la.getLogId());
    EXPECT_EQ(0, la.getTimestamp());
    EXPECT_EQ(10, la.totalCapacity);
    EXPECT_EQ(nullptr, la.rawArgsExtension);

    FormatMetadata fm;
    la.reset(&fm, 123, 1234);
    EXPECT_TRUE(la.valid());
    EXPECT_EQ(0, la.numArgs);
    EXPECT_EQ(123, la.getLogId());
    EXPECT_EQ(1234, la.getTimestamp());
    EXPECT_EQ(10, la.totalCapacity);
    EXPECT_EQ(nullptr, la.rawArgsExtension);

}
}; //namespace