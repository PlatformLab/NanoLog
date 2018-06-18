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
#include "NanoLogCpp17.h"

namespace {
using namespace NanoLogInternal;
using namespace PerfUtils;

// The fixture for testing class Foo.
class NanoLogCpp17Test : public ::testing::Test {
 protected:
  // You can remove any or all of the following functions if its body
  // is empty.
  uint32_t bufferSize;
  uint32_t halfSize;
  RuntimeLogger::StagingBuffer *sb;

    NanoLogCpp17Test()
    : bufferSize(NanoLogConfig::STAGING_BUFFER_SIZE)
    , halfSize(bufferSize/2)
    , sb(new RuntimeLogger::StagingBuffer(0))
  {
      static_assert(1024 <= NanoLogConfig::STAGING_BUFFER_SIZE,
                                "Test requires at least 1KB of buffer space");
  }

  virtual ~NanoLogCpp17Test() {
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

TEST_F(NanoLogCpp17Test, getParamInfo) {
    // Check the constexprness
    constexpr ParamType ret1 = getParamInfo("Hello World %*.*s asdf", 1);
    EXPECT_EQ(ParamType::DYNAMIC_PRECISION, ret1);

    constexpr ParamType ret2 = getParamInfo("Hello World %*.*s asdf", 2);
    EXPECT_EQ(ParamType::STRING_WITH_DYNAMIC_PRECISION, ret2);

    constexpr ParamType ret3 = getParamInfo("Hello World %*.*s asdf", 3);
    EXPECT_EQ(ParamType::INVALID, ret3);

    // Regular testing
    const char testString[] = "Hello %*.*d %%%s %*.*s %10.500s %10.500d %+#.s";
    EXPECT_EQ(ParamType::DYNAMIC_WIDTH, getParamInfo(testString, 0));
    EXPECT_EQ(ParamType::DYNAMIC_PRECISION, getParamInfo(testString, 1));
    EXPECT_EQ(ParamType::NON_STRING, getParamInfo(testString, 2));
    EXPECT_EQ(ParamType::STRING_WITH_NO_PRECISION, getParamInfo(testString, 3));
    EXPECT_EQ(ParamType::DYNAMIC_WIDTH, getParamInfo(testString, 4));
    EXPECT_EQ(ParamType::DYNAMIC_PRECISION, getParamInfo(testString, 5));
    EXPECT_EQ(ParamType::STRING_WITH_DYNAMIC_PRECISION, getParamInfo(testString, 6));
    EXPECT_EQ(ParamType(500), getParamInfo(testString, 7));
    EXPECT_EQ(ParamType::NON_STRING, getParamInfo(testString, 8));
    EXPECT_EQ(ParamType(0), getParamInfo(testString, 9));
    EXPECT_EQ(ParamType::INVALID, getParamInfo(testString, 10));
    EXPECT_EQ(ParamType::INVALID, getParamInfo(testString, 11));
}

TEST_F(NanoLogCpp17Test, countFmtParams) {
    EXPECT_EQ(countFmtParams2("How about a double? %lf"),
                countFmtParams("How about a double? %lf"));
}

}; //namespace