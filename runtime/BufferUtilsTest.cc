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

#include "NanoLog.h"

namespace {

using namespace PerfUtils;

// The fixture for testing class Foo.
class BufferUtilsTest : public ::testing::Test {
 protected:

  BufferUtilsTest()
  {
  }

  virtual ~BufferUtilsTest() {
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

TEST_F(BufferUtilsTest, strnlen) {
    const char* str1 = "This is a string\0 ended";
    const char* str2 = "This is another string";
    const wchar_t* str3 = L"This is a wide string";

    EXPECT_EQ(16U, BufferUtils::strnlen(str1));
    EXPECT_EQ(1U, BufferUtils::strnlen(str1, 1));
    EXPECT_EQ(0U, BufferUtils::strnlen(str1, 0));
    EXPECT_EQ(16U, BufferUtils::strnlen(str1, 100000));

    EXPECT_EQ(22U, BufferUtils::strnlen(str2));
    EXPECT_EQ(1U, BufferUtils::strnlen(str2, 1));
    EXPECT_EQ(0U, BufferUtils::strnlen(str2, 0));
    EXPECT_EQ(22U, BufferUtils::strnlen(str2, 100000));

    EXPECT_EQ(21U, BufferUtils::strnlen(str3));
    EXPECT_EQ(1U, BufferUtils::strnlen(str3, 1));
    EXPECT_EQ(0U, BufferUtils::strnlen(str3, 0));
    EXPECT_EQ(21U, BufferUtils::strnlen(str3, 100000));
}

}; //namespace