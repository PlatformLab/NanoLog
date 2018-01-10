/* Copyright (c) 2016-2017 Stanford University
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
#include <stdio.h>
#include <vector>
#include <sstream>

#include "gtest/gtest.h"

#include "TestUtil.h"

#include "RuntimeLogger.h"
#include "Packer.h"
#include "Log.h"
#include "GeneratedCode.h"


extern int __fmtId__Simple32log32message32with32032parameters__testHelper47client46cc__19__;
extern int __fmtId__This32is32a32string3237s__testHelper47client46cc__20__;

namespace {

using namespace PerfUtils;
using namespace NanoLogInternal;

// The fixture for testing class Foo.
class LogTest : public ::testing::Test {
 protected:
    // Note, if the tests ever fail to compile due to these symbols not being
    // found, it's most likely that someone updated the testHelper/main.cc file.
    // In this case, check testHelper/GeneratedCode.cc for updated values and
    // change the declarations above and uses below to match.
    int noParamsId = __fmtId__Simple32log32message32with32032parameters__testHelper47client46cc__19__;
    int stringParamId = __fmtId__This32is32a32string3237s__testHelper47client46cc__20__;
  LogTest()
  {
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
};

using namespace Log;

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
    header->entryType = EntryType::LOG_MSG;
    (++header)->entryType = EntryType::BUFFER_EXTENT;
    (++header)->entryType = EntryType::CHECKPOINT;
    (++header)->entryType = EntryType::INVALID;

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(buffer));
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

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(in));
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

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 0U, dLogId, dTimestamp));
    EXPECT_EQ(1000, dLogId);
    EXPECT_EQ(10000000000000L, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 10000000000000L, dLogId,
                                                                   dTimestamp));
    EXPECT_EQ(10000, dLogId);
    EXPECT_EQ(10000, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 10000, dLogId, dTimestamp));
    EXPECT_EQ(1, dLogId);
    EXPECT_EQ(100000, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 100000, dLogId, dTimestamp));
    EXPECT_EQ(1, dLogId);
    EXPECT_EQ(100001, dTimestamp);

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPtr));
    ASSERT_TRUE(decompressLogHeader(&readPtr, 100001, dLogId, dTimestamp));
    EXPECT_EQ(1, dLogId);
    EXPECT_EQ(100001, dTimestamp);

    EXPECT_EQ(EntryType::INVALID, peekEntryType(readPtr));
    EXPECT_EQ(29U, readPtr - backing_buffer);
}

TEST_F(LogTest, insertCheckpoint) {
    char backing_buffer[100];
    char *writePos = backing_buffer;
    char *endOfBuffer = backing_buffer + 100;

    // out of space
    EXPECT_FALSE(insertCheckpoint(&writePos, backing_buffer));
    EXPECT_EQ(writePos, backing_buffer);

    // Not out of space
    EXPECT_TRUE(insertCheckpoint(&writePos, endOfBuffer));
    EXPECT_EQ(sizeof(Checkpoint), writePos - backing_buffer);
    EXPECT_EQ(EntryType::CHECKPOINT, peekEntryType(backing_buffer));

    //Out of space at the very end
    writePos = endOfBuffer - sizeof(Checkpoint) + 1;
    EXPECT_FALSE(insertCheckpoint(&writePos, endOfBuffer));
}

TEST_F(LogTest, insertCheckpoint_end2end) {
    Checkpoint cp1, cp2, cp3, cp4;
    char backing_buffer[100];
    char *writePos = backing_buffer;
    char *endOfBuffer = backing_buffer + 100;
    const char *testFile = "/tmp/testFile";

    // Write the buffer to a file and read it back
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(backing_buffer, 0);
    oFile.close();

    FILE *in = fopen(testFile, "r");
    ASSERT_NE(nullptr, in);
    ASSERT_FALSE(readCheckpoint(cp1, in));

    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer));
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer));
    ASSERT_TRUE(insertCheckpoint(&writePos, endOfBuffer));

    // Write the buffer to a file and read it back
    fclose(in);
    oFile.open(testFile);
    oFile.write(backing_buffer, writePos - backing_buffer);
    oFile.close();

    in = fopen(testFile, "r");
    ASSERT_NE(nullptr, in);

    ASSERT_TRUE(readCheckpoint(cp1, in));
    ASSERT_TRUE(readCheckpoint(cp2, in));
    ASSERT_TRUE(readCheckpoint(cp3, in));
    ASSERT_FALSE(readCheckpoint(cp4, in));
}

TEST_F(LogTest, encoder_constructor) {
    char buffer[100];

    Encoder encoder(buffer, 100, false);

    // Check that a checkpoint was inserted
    EXPECT_EQ(sizeof(Checkpoint), encoder.writePos - encoder.backing_buffer);
    EXPECT_EQ(buffer, encoder.backing_buffer);
    EXPECT_EQ(100, encoder.endOfBuffer - encoder.backing_buffer);
    EXPECT_EQ(EntryType::CHECKPOINT, peekEntryType(buffer));

    // Check that a checkpoint was not inserted
    memset(buffer, 0, 100);
    Encoder e(buffer, 100, true);
    EXPECT_EQ(0U, e.writePos - e.backing_buffer);
    EXPECT_EQ(buffer, e.backing_buffer);
    EXPECT_EQ(100, e.endOfBuffer - e.backing_buffer);
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

    uint32_t bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           true,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);
    EXPECT_EQ(101UL, e.lastTimestamp);

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
    readPos += sizeof(Checkpoint);

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
    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPos));
    decompressLogHeader(&readPos, 0, fmtId, ts);

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPos));
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
    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPos));

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

    EXPECT_EQ(EntryType::LOG_MSG, peekEntryType(readPos));

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
    char inputBuffer[100], outputBuffer1[100];

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
    Encoder e(outputBuffer1, 100);

    uint32_t bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           true,
                                           &compressedLogs);

    EXPECT_EQ(1 + 1, compressedLogs);
    EXPECT_EQ(sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);

    // Rough check of what's in the buffer
    EXPECT_EQ(e.writePos - e.backing_buffer, sizeof(Checkpoint)
                                                + sizeof(BufferExtent)
                                                + sizeof(CompressedEntry) + 2);
    const char *readPos = e.backing_buffer + sizeof(Checkpoint);
    const BufferExtent *be = reinterpret_cast<const BufferExtent*>(readPos);
    EXPECT_EQ(sizeof(BufferExtent) + 3U, be->length);

    // Now let's try one more out of space whereby there's not enough space
    // to encode the buffer extent.
    compressedLogs = 1;
    uint32_t bufferSize = sizeof(Checkpoint) + sizeof(BufferExtent) - 1;
    Encoder e2(outputBuffer1, bufferSize);
    bytesRead = e2.encodeLogMsgs(inputBuffer,
                                    3*sizeof(UncompressedEntry),
                                    100,
                                    true,
                                    &compressedLogs);
    EXPECT_EQ(0U, bytesRead);
    EXPECT_EQ(1U, compressedLogs);
    EXPECT_NE(100U, e2.lastBufferIdEncoded);
    EXPECT_EQ(sizeof(Checkpoint), e2.getEncodedBytes());

    // One last attempt whereby we have enough space to encode the buffer
    // extent but nothing else.
    ue = reinterpret_cast<UncompressedEntry*>(inputBuffer);
    ue->timestamp = 100;
    ue->fmtId = stringParamId;
    ue->entrySize = 2*sizeof(UncompressedEntry);
    compressedLogs = 1;

    bufferSize = sizeof(Checkpoint) + sizeof(BufferExtent) + sizeof(uint32_t);
    Encoder e3(outputBuffer1, bufferSize);
    bytesRead = e3.encodeLogMsgs(inputBuffer,
                                    3*sizeof(UncompressedEntry),
                                    1,
                                    true,
                                    &compressedLogs);
    EXPECT_EQ(0U, bytesRead);
    EXPECT_EQ(1U, compressedLogs);
    EXPECT_EQ(1U, e3.lastBufferIdEncoded);
    EXPECT_EQ(sizeof(Checkpoint) + sizeof(BufferExtent), e3.getEncodedBytes());

    be = reinterpret_cast<const BufferExtent*>(e3.backing_buffer
                                                        + sizeof(Checkpoint));
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

    uint32_t bytesRead = e.encodeLogMsgs(inputBuffer,
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
    EXPECT_EQ(0U, encoder.lastTimestamp);


    // Let's try another one whereby id is larger than what can fit internally
    char *secondWritePos = encoder.writePos;
    ASSERT_TRUE(encoder.encodeBufferExtentStart(111111111, true));
    ++be;

    EXPECT_EQ(EntryType::BUFFER_EXTENT, be->entryType);
    EXPECT_FALSE(be->isShort);
    EXPECT_TRUE(be->wrapAround);

    EXPECT_EQ(&(be->length), encoder.currentExtentSize);
    EXPECT_EQ(111111111, encoder.lastBufferIdEncoded);
    EXPECT_EQ(0U, encoder.lastTimestamp);

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
    EXPECT_EQ(0U, encoder.lastTimestamp);
}

TEST_F(LogTest, swapBuffer) {
    char buffer1[1000], buffer2[100];
    Encoder encoder(buffer1, 1000, false);

    char *outBuffer;
    size_t outLength;
    size_t outSize;

    encoder.swapBuffer(buffer2, 100, &outBuffer, &outLength, &outSize);
    EXPECT_EQ(buffer1, outBuffer);
    EXPECT_EQ(sizeof(Checkpoint), outLength);
    EXPECT_EQ(1000, outSize);

    EXPECT_EQ(buffer2, encoder.backing_buffer);
    EXPECT_EQ(buffer2, encoder.writePos);
    EXPECT_EQ(buffer2 + 100, encoder.endOfBuffer);
    EXPECT_EQ(uint32_t(-1), encoder.lastBufferIdEncoded);
    EXPECT_EQ(nullptr, encoder.currentExtentSize);
    EXPECT_EQ(0U, encoder.lastTimestamp);
}

TEST_F(LogTest, Decoder_open) {
    char buffer[1000];
    const char *testFile = "/tmp/testFile";
    Decoder dc;

    // Open an invalid file
    EXPECT_FALSE(dc.open("/dev/null"));
    EXPECT_EQ(nullptr, dc.filename);
    EXPECT_EQ(0U, dc.inputFd);


    // Write back an empty file and try to open it.
    std::ofstream oFile;
    oFile.open(testFile);
    oFile.write(buffer, 0);
    oFile.close();

    EXPECT_FALSE(dc.open(testFile));
    EXPECT_EQ(nullptr, dc.filename);
    EXPECT_EQ(0U, dc.inputFd);

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
    EXPECT_NE(nullptr, dc.filename);

    std::remove(testFile);
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
    EXPECT_EQ(nullptr, dc->filename);
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
    uint32_t bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);
    EXPECT_EQ(101UL, e.lastTimestamp);

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
    uint32_t bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);
    EXPECT_EQ(101UL, e.lastTimestamp);

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
    uint32_t bytesRead = e.encodeLogMsgs(inputBuffer,
                                           3*sizeof(UncompressedEntry),
                                           5,
                                           false,
                                           &compressedLogs);

    EXPECT_EQ(1 + 2, compressedLogs);
    EXPECT_EQ(3*sizeof(UncompressedEntry), bytesRead);
    EXPECT_EQ(5U, e.lastBufferIdEncoded);
    EXPECT_EQ(101UL, e.lastTimestamp);

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
    uint64_t lastTimestamp = 0;
    Checkpoint checkpoint;
    checkpoint.cyclesPerSecond = 1;
    long aggregationFilterId = stringParamId;
    numAggregationsRun = 0;
    EXPECT_TRUE(bf->decompressNextLogStatement(NULL,
                                                logMsgsPrinted,
                                                lastTimestamp,
                                                checkpoint,
                                                aggregationFilterId,
                                                &aggregation));

    EXPECT_EQ(0, numAggregationsRun);
    EXPECT_FALSE(bf->decompressNextLogStatement(NULL,
                                                logMsgsPrinted,
                                                lastTimestamp,
                                                checkpoint,
                                                aggregationFilterId,
                                                &aggregation));
    EXPECT_EQ(1, numAggregationsRun);

    // There should be no mores
    EXPECT_FALSE(bf->decompressNextLogStatement(NULL,
                                                logMsgsPrinted,
                                                lastTimestamp,
                                                checkpoint,
                                                aggregationFilterId,
                                                &aggregation));
    EXPECT_EQ(1, numAggregationsRun);
    EXPECT_FALSE(bf->hasMoreLogs);
    EXPECT_EQ(2U, logMsgsPrinted);
    EXPECT_EQ(101UL, lastTimestamp);

    fclose(in);
    delete bf;
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
    uint32_t bytesRead = encoder.encodeLogMsgs(inputBuffer,
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
    ASSERT_TRUE(dc.open(testFile));
    FILE *outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    uint64_t msgsPrinted;
    EXPECT_TRUE(dc.internalDecompressUnordered(outputFd, -1, &msgsPrinted));
    EXPECT_EQ(12, msgsPrinted);
    fclose(outputFd);

    // Read it back and compare
    std::ifstream iFile;
    iFile.open(decomp);
    ASSERT_TRUE(iFile.good());

    const char* unorderedLines[] = {
        "1969-12-31 16:00:01.000000090 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000105 testHelper/client.cc:20 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000093 testHelper/client.cc:19 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000096 testHelper/client.cc:20 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000100 testHelper/client.cc:19 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000111 testHelper/client.cc:20 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000145 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000156 testHelper/client.cc:20 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000118 testHelper/client.cc:19 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000091 testHelper/client.cc:19 NOTICE[11]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000135 testHelper/client.cc:19 NOTICE[12]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000126 testHelper/client.cc:19 NOTICE[7]: Simple log message with 0 parameters\r",
        "\r",
        "\r",
        "# Decompression Complete after printing 12 log messages\r"
    };

    std::string iLine;
    for (const char *line : unorderedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        EXPECT_STREQ(line, iLine.c_str());
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Try the unordered case
    dc.open(testFile);
    msgsPrinted = 0;

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    dc.internalDecompressOrdered(outputFd, -1, &msgsPrinted);
    EXPECT_EQ(12, msgsPrinted);
    fclose(outputFd);

    const char* orderedLines[] = {
        "1969-12-31 16:00:01.000000090 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000091 testHelper/client.cc:19 NOTICE[11]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000093 testHelper/client.cc:19 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000096 testHelper/client.cc:20 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000100 testHelper/client.cc:19 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000105 testHelper/client.cc:20 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000111 testHelper/client.cc:20 NOTICE[10]: This is a string aaaaaaaaaaaaaaa\r",
        "1969-12-31 16:00:01.000000118 testHelper/client.cc:19 NOTICE[10]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000126 testHelper/client.cc:19 NOTICE[7]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000135 testHelper/client.cc:19 NOTICE[12]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000145 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000156 testHelper/client.cc:20 NOTICE[5]: This is a string aaaaaaaaaaaaaaa\r",
        "\r",
        "\r",
        "# Decompression Complete after printing 12 log messages\r"
    };

    iFile.open(decomp);
    for (const char *line : orderedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        EXPECT_STREQ(line, iLine.c_str());
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

    uint32_t bytesRead = encoder.encodeLogMsgs(inputBuffer,
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
    uint64_t msgsPrinted = 0;
    EXPECT_TRUE(dc.internalDecompressUnordered(outputFd, -1, &msgsPrinted));
    EXPECT_EQ(10, msgsPrinted);
    fclose(outputFd);

    // Read it back and compare
    std::ifstream iFile;
    iFile.open(decomp);
    ASSERT_TRUE(iFile.good());

    const char* expectedLines[] = {
        "1969-12-31 16:00:01.000000000 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000001 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000002 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000003 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000004 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "\r",
        "# New execution started\r",
        "\r",
        "# New execution started\r",
        "1969-12-31 16:00:01.000000000 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000001 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000002 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000003 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "1969-12-31 16:00:01.000000004 testHelper/client.cc:19 NOTICE[5]: Simple log message with 0 parameters\r",
        "\r",
        "\r",
        "# Decompression Complete after printing 10 log messages\r"
    };

    std::string iLine;
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        EXPECT_STREQ(line, iLine.c_str());
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    // Try the unordered case
    dc.open(testFile);
    msgsPrinted = 0;

    outputFd = fopen(decomp, "w");
    ASSERT_NE(nullptr, outputFd);
    dc.internalDecompressOrdered(outputFd, -1, &msgsPrinted);
    EXPECT_EQ(10, msgsPrinted);
    fclose(outputFd);

    iFile.open(decomp);
    for (const char *line : expectedLines) {
        ASSERT_TRUE(iFile.good());
        std::getline(iFile, iLine);
        EXPECT_STREQ(line, iLine.c_str());
    }
    EXPECT_FALSE(iFile.eof());
    iFile.close();

    std::remove(testFile);
    std::remove(decomp);
}
}; //namespace