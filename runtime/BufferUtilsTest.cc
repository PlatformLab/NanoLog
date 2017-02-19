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

#include <fstream>
#include <iostream>
#include <iosfwd>

#include "gtest/gtest.h"

#include "TestUtil.h"

#include "NanoLog.h"
#include "BufferUtils.h"

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

using namespace BufferUtils;

TEST_F(BufferUtilsTest, compressMetadata)
{
    char buffer[100];
    char *pos = buffer;

    BufferUtils::UncompressedLogEntry re;
    re.fmtId = 100;
    re.timestamp = 1000000000;

    size_t cmpSize = compressMetadata(&re, &pos, 0);
    EXPECT_EQ(6U, cmpSize);
    EXPECT_EQ(6U, pos - buffer);
}

TEST_F(BufferUtilsTest, compressMetadata_negativeDeltas)
{
    char buffer[100];
    char *pos = buffer;

    BufferUtils::UncompressedLogEntry re;
    re.fmtId = 100;
    re.timestamp = 100;

    size_t cmpSize = compressMetadata(&re, &pos, 1000);
    EXPECT_EQ(4U, cmpSize);
    EXPECT_EQ(4U, pos - buffer);

    re.fmtId = 5000000;
    re.timestamp = 90;
    cmpSize = compressMetadata(&re, &pos, 100);
    EXPECT_EQ(5U, cmpSize);
    EXPECT_EQ(9U, pos - buffer);
}

TEST_F(BufferUtilsTest, compressMetadata_end2end)
{
    char buffer[100];
    char *pos = buffer;
    size_t cmpSize;
    UncompressedLogEntry re;
    DecompressedMetadata dm;

    re.fmtId = 1000;
    re.timestamp = 10000000000000L;
    cmpSize = compressMetadata(&re, &pos, 0);
    EXPECT_EQ(9U, cmpSize);
    EXPECT_EQ(9U, pos - buffer);

    re.fmtId = 10000;
    re.timestamp = 10000;
    cmpSize = compressMetadata(&re, &pos, 10000000000000L);
    EXPECT_EQ(9U, cmpSize);
    EXPECT_EQ(18U, pos - buffer);

    re.fmtId = 1;
    re.timestamp = 100000;
    cmpSize = compressMetadata(&re, &pos, 10000);
    EXPECT_EQ(5U, cmpSize);
    EXPECT_EQ(23U, pos - buffer);

    re.fmtId = 1;
    re.timestamp = 100001;
    cmpSize = compressMetadata(&re, &pos, 100000);
    EXPECT_EQ(3U, cmpSize);
    EXPECT_EQ(26U, pos - buffer);

    cmpSize = compressMetadata(&re, &pos, 100001);
    EXPECT_EQ(3U, cmpSize);
    EXPECT_EQ(29U, pos - buffer);

    // Write the data to a file.
    std::ofstream oFile;
    oFile.open("testLog.dat");
    ASSERT_TRUE(oFile.good());
    oFile.write(buffer, pos - buffer);
    ASSERT_TRUE(oFile.good());
    oFile.close();

    // Read it back
    std::ifstream iFile;
    iFile.open("testLog.dat");
    ASSERT_TRUE(iFile.good());

    dm = decompressMetadata(iFile, 0U);
    EXPECT_EQ(1000, dm.fmtId);
    EXPECT_EQ(10000000000000L, dm.timestamp);

    dm = decompressMetadata(iFile, 10000000000000L);
    EXPECT_EQ(10000, dm.fmtId);
    EXPECT_EQ(10000, dm.timestamp);

    dm = decompressMetadata(iFile, 10000);
    EXPECT_EQ(1, dm.fmtId);
    EXPECT_EQ(100000, dm.timestamp);

    dm = decompressMetadata(iFile, 100000);
    EXPECT_EQ(1, dm.fmtId);
    EXPECT_EQ(100001, dm.timestamp);

    dm = decompressMetadata(iFile, 100001);
    EXPECT_EQ(1, dm.fmtId);
    EXPECT_EQ(100001, dm.timestamp);

    iFile.close();
    std::remove("testLog.dat");
}

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