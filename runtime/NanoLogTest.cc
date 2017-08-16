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

#include "gtest/gtest.h"

#include "TestUtil.h"

#include "RuntimeLogger.h"

namespace {
using namespace NanoLogInternal;
using namespace PerfUtils;

// The fixture for testing class Foo.
class NanoLogTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.
  uint32_t bufferSize;
  uint32_t halfSize;
  RuntimeLogger::StagingBuffer *sb;

  NanoLogTest()
    : bufferSize(NanoLogConfig::STAGING_BUFFER_SIZE)
    , halfSize(bufferSize/2)
    , sb(new RuntimeLogger::StagingBuffer(0))
  {
      static_assert(1024 <= NanoLogConfig::STAGING_BUFFER_SIZE,
                                "Test requires at least 1KB of buffer space");
  }

  virtual ~NanoLogTest() {
    if (sb) {
        // Since the tests screw with internal state, it's best to
        // reset them before exiting
        sb->producerPos = sb->consumerPos = sb->storage;
        sb->minFreeSpace = 0;
        delete sb;
    }

    sb = nullptr;
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

TEST_F(NanoLogTest, StagingBuffer_reserveProducerSpace)
{
    EXPECT_EQ(sb->minFreeSpace, bufferSize);
    EXPECT_EQ(sb->storage, sb->reserveProducerSpace(100));
    EXPECT_EQ(sb->minFreeSpace, bufferSize);

    // Mimic running out of minFreeSpace
    sb->minFreeSpace = 0;
    EXPECT_EQ(sb->storage, sb->reserveProducerSpace(100));
    EXPECT_EQ(bufferSize, sb->minFreeSpace);

    // Roll over tests are done in reserveSpaceInternal
}


TEST_F(NanoLogTest, StagingBuffer_reserveSpaceInternal)
{
    // Case 1: Empty buffer
    EXPECT_EQ(bufferSize, sb->minFreeSpace);
    EXPECT_EQ(sb->storage, sb->reserveSpaceInternal(100));

    // Case 2: Empty buffer but minFreeSpace is out of date
    sb->minFreeSpace = 0;
    EXPECT_EQ(sb->storage, sb->reserveSpaceInternal(100));
    EXPECT_EQ(bufferSize, sb->minFreeSpace);

    // Case 3: Buffer is about half utilized
    sb->minFreeSpace = 0;
    sb->consumerPos = sb->storage + halfSize;
    EXPECT_EQ(sb->storage, sb->reserveSpaceInternal(100));
    EXPECT_EQ(halfSize, sb->minFreeSpace);

    // Case 4: Out of space in the middle
    sb->minFreeSpace = 0;
    sb->producerPos = sb->storage + halfSize - 1;
    EXPECT_EQ(nullptr, sb->reserveSpaceInternal(100, false));
    EXPECT_EQ(1U, sb->minFreeSpace);

    // Case 5: Exact space in the middle (should fail)
    sb->minFreeSpace = 0;
    sb->producerPos = sb->consumerPos - 100;
    EXPECT_EQ(nullptr, sb->reserveSpaceInternal(100, false));
    EXPECT_EQ(100U, sb->minFreeSpace);

    // Case 6: Just over exact space
    sb->minFreeSpace = 0;
    sb->producerPos = sb->consumerPos - 101;
    EXPECT_EQ(sb->producerPos, sb->reserveSpaceInternal(100, false));
    EXPECT_EQ(101U, sb->minFreeSpace);

    // Case 7: Need roll over, but still not just enough space
    sb->minFreeSpace = 0;
    sb->producerPos = sb->storage + bufferSize - 100;
    sb->consumerPos = sb->storage + 100;
    EXPECT_EQ(nullptr, sb->reserveSpaceInternal(100, false));
    EXPECT_EQ(sb->storage, sb->producerPos);
    EXPECT_EQ(100U, sb->minFreeSpace);

    // Case 8: Need roll over, enough space
    sb->minFreeSpace = 0;
    sb->producerPos = sb->storage + bufferSize - 100;
    sb->consumerPos = sb->storage + 101;
    EXPECT_EQ(sb->storage, sb->reserveSpaceInternal(100, false));
    EXPECT_EQ(sb->storage, sb->producerPos);
    EXPECT_EQ(101U, sb->minFreeSpace);
}

TEST_F(NanoLogTest, StagingBuffer_reserveSpaceInternal_rollover_prevention)
{
    // Setup the situation where the consumer is at position 0 and the producer
    // needs to roll over. When this situation occurs, the producer should
    // NOT roll over, otherwise the two positions overlap, which we have defined
    // to be a completely empty buffer, but that is not the case in actuality

    sb->minFreeSpace = 0;
    sb->consumerPos = sb->storage;
    sb->producerPos = sb->storage + bufferSize;

    // Because of where our testing exit block is, we need to execute
    // reserveSpaceInternal 2x to get the looping behavior
    EXPECT_EQ(nullptr, sb->reserveSpaceInternal(1, false));
    EXPECT_EQ(nullptr, sb->reserveSpaceInternal(1, false));
}

TEST_F(NanoLogTest, StagingBuffer_finishReservation) {
    EXPECT_EQ(sb->storage, sb->producerPos);
    EXPECT_EQ(bufferSize, sb->minFreeSpace);

    sb->finishReservation(100);
    EXPECT_EQ(bufferSize, sb->minFreeSpace + 100);
    EXPECT_EQ(sb->storage + 100, sb->producerPos);

    sb->finishReservation(102);
    EXPECT_EQ(bufferSize, sb->minFreeSpace + 202);
    EXPECT_EQ(sb->storage + 202, sb->producerPos);
}

TEST_F(NanoLogTest, StagingBuffer_finishReservation_asserts) {

    // Case 1a: Ran out of space and didn't reserve (Artificial)
    EXPECT_EQ(bufferSize, sb->minFreeSpace);
    sb->minFreeSpace = 10;
    EXPECT_DEATH(sb->finishReservation(100), "nbytes < minFreeSpace");
    sb->minFreeSpace = bufferSize;

    // Case 1b: Ran out of space and didn't reserve (through API)

    // Get to half
    sb->reserveSpaceInternal(halfSize);
    sb->finishReservation(halfSize);
    sb->consume(halfSize);

    // Allocate and free the two other halves
    sb->reserveSpaceInternal(halfSize - 1);
    sb->finishReservation(halfSize - 1);
    sb->reserveSpaceInternal(halfSize - 1);
    sb->finishReservation(halfSize - 1);

    // This should free the space for us to alloc
    sb->consume(halfSize - 1);
    EXPECT_DEATH(sb->finishReservation(100), "nbytes < minFreeSpace");

    // This should finish it off.
    sb->reserveSpaceInternal(100);
    EXPECT_NO_FATAL_FAILURE(sb->finishReservation(100));

    // Case 2: Don't fall of the end (indicates bug in library code)
    sb->minFreeSpace = 2*bufferSize;
    EXPECT_DEATH(sb->finishReservation(bufferSize), "BUFFER_SIZE");

    // Case 3: The producer somehow passes the consumer location (library bug)
    sb->producerPos = sb->storage + halfSize - 50;
    sb->consumerPos = sb->storage + halfSize + 100;
    sb->reserveSpaceInternal(100);
}

TEST_F(NanoLogTest, StagingBuffer_peek) {
    uint64_t bytesAvailable = -1;

    // Case 1: Empty Buffer
    sb->peek(&bytesAvailable);
    EXPECT_EQ(0U, bytesAvailable);

    // Case 2: There's stuff (via API);
    sb->reserveProducerSpace(1000);
    sb->finishReservation(1000);
    sb->reserveProducerSpace(150);

    sb->peek(&bytesAvailable);
    EXPECT_EQ(1000U, bytesAvailable);

    sb->finishReservation(150);
    sb->peek(&bytesAvailable);
    EXPECT_EQ(1150U, bytesAvailable);

    sb->consume(1150U);

    // Case 3: Roll over, need double peeks.
    delete sb;
    sb = new RuntimeLogger::StagingBuffer(1);

    sb->reserveProducerSpace(bufferSize - 100);
    sb->finishReservation(bufferSize - 100);
    sb->peek(&bytesAvailable);
    EXPECT_EQ(bufferSize - 100, bytesAvailable);
    sb->consume(halfSize + 10);

    sb->reserveProducerSpace(halfSize);
    sb->finishReservation(halfSize);

    // At this point the internal buffer should look something like this
    //    halfSize bytes to consume
    //    10 bytes free space
    //    halfSize - 10 - 100 bytes to consume
    //    100 bytes to skip (unrecorded space)

    sb->peek(&bytesAvailable);
    EXPECT_EQ(halfSize - 110, bytesAvailable);
    sb->consume(bytesAvailable - 1);

    sb->peek(&bytesAvailable);
    EXPECT_EQ(1U, bytesAvailable);
    sb->consume(1);

    sb->peek(&bytesAvailable);
    EXPECT_EQ(halfSize, bytesAvailable);
    sb->consume(halfSize);

    // At this point we should have no data.
    sb->peek(&bytesAvailable);
    EXPECT_EQ(0U, bytesAvailable);

    // Put a bit more to finish it off.
    sb->reserveProducerSpace(10);
    sb->finishReservation(10);
    sb->peek(&bytesAvailable);
    EXPECT_EQ(10U, bytesAvailable);
}
}; //namespace