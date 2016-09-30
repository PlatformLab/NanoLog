#include "gtest/gtest.h"
#include "FastLogger.h"

namespace {

using namespace PerfUtils;

// The fixture for testing class Foo.
class FastLoggerTest : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.

  FastLoggerTest() {
    // You can do set-up work for each test here.
  }

  virtual ~FastLoggerTest() {
    // You can do clean-up work that doesn't throw exceptions here.
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

TEST_F(FastLoggerTest, StagingBuffer_alloc)
{
    static_assert(1024 <= PerfUtils::FastLogger::StagingBuffer::BUFFER_SIZE,
            "Test requires at least 1KB of buffer space");
    PerfUtils::FastLogger::StagingBuffer *sb =
                                    new PerfUtils::FastLogger::StagingBuffer();
    char *endOfBuffer =
                sb->storage + PerfUtils::FastLogger::StagingBuffer::BUFFER_SIZE;

    // basic - Case 1: Printer is caught up/slightly behind
    EXPECT_EQ(sb->storage, sb->alloc(100));
    EXPECT_EQ(sb->storage + 100U, sb->recordPointer);

    EXPECT_EQ(sb->storage + 100U, sb->alloc(250));
    EXPECT_EQ(sb->storage + 350U, sb->recordPointer);

    // Case 2: Printer Caught up + Wrap around required
    sb->cachedReadPointer = sb->recordPointer = endOfBuffer - 10;
    EXPECT_EQ(sb->storage, sb->alloc(100));
    EXPECT_EQ(sb->storage + 100U, sb->recordPointer);

    // Case 3: Printer is far-far behind and a wrap around fails
    sb->cachedReadPointer = sb->storage + 10;
    sb->readPointer = sb->cachedReadPointer;
    sb->recordPointer = endOfBuffer - 10;
    EXPECT_EQ(NULL, sb->alloc(100));
    EXPECT_EQ(endOfBuffer - 10, sb->recordPointer);


    // case 4: Printer was far behind, caught up a bit, but is still behind
    // with wrap around
    sb->cachedReadPointer = sb->storage + 10;
    sb->readPointer = sb->storage + 100;
    sb->recordPointer = endOfBuffer - 10;
    EXPECT_EQ(NULL, sb->alloc(100));
    EXPECT_EQ(sb->storage + 100, sb->cachedReadPointer);
    EXPECT_EQ(sb->storage + 100, sb->readPointer);
    EXPECT_EQ(endOfBuffer - 10, sb->recordPointer);

    // case 5: Printer was far behind but now has caught up, w/ wrap around
    sb->cachedReadPointer = sb->storage + 10;
    sb->readPointer = sb->storage + 101;
    sb->recordPointer = endOfBuffer - 10;
    EXPECT_EQ(sb->storage, sb->alloc(100));
    EXPECT_EQ(sb->storage + 101, sb->cachedReadPointer);
    EXPECT_EQ(sb->storage + 101, sb->readPointer);
    EXPECT_EQ(sb->storage + 100, sb->recordPointer);

    // Exact allocation fails (cannot overlap record/printer pointers)
    EXPECT_EQ(NULL, sb->alloc(1));

    // Case 6: Printer is behind, but there is still space ahead
    sb->readPointer = sb->cachedReadPointer = sb->storage + 101;
    sb->recordPointer = sb->storage;
    EXPECT_EQ(sb->storage, sb->alloc(100));
    EXPECT_EQ(sb->storage + 101, sb->cachedReadPointer);
    EXPECT_EQ(sb->storage + 101, sb->readPointer);

    // Case 7: Printer is behind, but there is exact space ahead (fail)
    sb->readPointer = sb->cachedReadPointer = sb->storage + 100;
    sb->recordPointer = sb->storage;
    EXPECT_EQ(NULL, sb->alloc(100));
    EXPECT_EQ(sb->storage + 100, sb->cachedReadPointer);
    EXPECT_EQ(sb->storage + 100, sb->readPointer);

    // Case 8: Printer is behind, but caught up just enough
    sb->cachedReadPointer = sb->storage + 100;
    sb->readPointer = sb->storage + 101;
    sb->recordPointer = sb->storage;
    EXPECT_EQ(sb->storage, sb->alloc(100));
    EXPECT_EQ(sb->storage + 101, sb->cachedReadPointer);
    EXPECT_EQ(sb->storage + 101, sb->readPointer);

    // The following two cases (9+10) at this point are just repeating a
    // subset of the original few to test for recursion

    // Case 9: Printer had wrapped around and there's enough space
    sb->cachedReadPointer = endOfBuffer - 50;
    sb->readPointer = sb->storage + 101;
    sb->recordPointer = endOfBuffer - 100;
    EXPECT_EQ(sb->storage, sb->alloc(100));
    EXPECT_EQ(sb->cachedReadPointer, sb->storage + 101);

    // Case 10: printer pointer had wrapped around and there's not enough space
    sb->cachedReadPointer = endOfBuffer - 50;
    sb->readPointer = sb->storage + 100;
    sb->recordPointer = endOfBuffer - 100;
    EXPECT_EQ(NULL, sb->alloc(100));
    EXPECT_EQ(sb->cachedReadPointer, sb->readPointer);

    delete sb;
}

inline void
reset() {
    if (FastLogger::compressor != NULL) {
        delete FastLogger::compressor;
        FastLogger::compressor = NULL;
    }

    bool found = false;
    for (FastLogger::StagingBuffer *buff : FastLogger::threadBuffers) {
        if (buff == FastLogger::stagingBuffer)
            found = true;
        delete buff;
    }

    if (!found && FastLogger::stagingBuffer != NULL)
        delete FastLogger::stagingBuffer;

    FastLogger::stagingBuffer = NULL;
}

TEST_F(FastLoggerTest, alloc) {
    // If some other test has alloc-ed space, free it.
    reset();

    ASSERT_EQ(NULL, FastLogger::compressor);
    ASSERT_EQ(NULL, FastLogger::stagingBuffer);

    char *buff1 = FastLogger::alloc(100);
    void *null = NULL;

    EXPECT_NE(null, buff1);
    EXPECT_NE(null, FastLogger::compressor);
    EXPECT_NE(null, FastLogger::stagingBuffer);

    LogCompressor *print = FastLogger::compressor;
    FastLogger::StagingBuffer *stage = FastLogger::stagingBuffer;

    char *buff2 = FastLogger::alloc(100);
    EXPECT_EQ(print, FastLogger::compressor);
    EXPECT_EQ(stage, FastLogger::stagingBuffer);

    EXPECT_EQ(buff1 + 100, buff2);

    reset();
}

} //namespace