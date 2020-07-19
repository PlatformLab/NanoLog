/* Copyright (c) 2016-2018 Stanford University
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

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <fstream>

#include "TestUtil.h"
#include "Packer.h"

#include "gtest/gtest.h"

using namespace std;
using namespace BufferUtils;
namespace {

// The fixture for testing class Foo.
class PackerTest : public ::testing::Test {
    protected:
    // You can remove any or all of the following functions if its body
    // is empty.

    PackerTest()
        : testFile("packerTestFile.bin")
        , buffer_space()
        , buffer(buffer_space)
    {
        // You can do set-up work for each test here.
    }

    virtual ~PackerTest() {
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
        std::remove(testFile);
    }

    // Objects declared here can be used by all tests in the test case for Foo.

    const char *testFile;
    char buffer_space[10000];
    char *buffer = buffer_space;
};

TEST_F(PackerTest, packUnsignedIntegers)
{
    // Only test pack's ability to identify the min # of bytes required
    // and advances the buffer appropriately. Verification of the data
    // comes in the unpack test.
    char *buffer = buffer_space;
    EXPECT_EQ(1, pack(&buffer, uint8_t(0)));
    EXPECT_EQ(1, pack(&buffer, uint16_t(0)));
    EXPECT_EQ(1, pack(&buffer, uint32_t(0)));
    EXPECT_EQ(1, pack(&buffer, uint64_t(0)));
    EXPECT_EQ(buffer_space + 4, buffer);

    EXPECT_EQ(1, pack(&buffer, uint8_t(1)));
    EXPECT_EQ(1, pack(&buffer, uint16_t(1)));
    EXPECT_EQ(1, pack(&buffer, uint32_t(1)));
    EXPECT_EQ(1, pack(&buffer, uint64_t(1)));
    EXPECT_EQ(buffer_space + 8, buffer);

    EXPECT_EQ(1, pack(&buffer, uint8_t(255)));
    EXPECT_EQ(1, pack(&buffer, uint16_t(255)));
    EXPECT_EQ(1, pack(&buffer, uint32_t(255)));
    EXPECT_EQ(1, pack(&buffer, uint64_t(255)));
    EXPECT_EQ(buffer_space + 12, buffer);

    EXPECT_EQ(2, pack(&buffer, uint16_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, uint32_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, uint64_t(1 << 8)));
    EXPECT_EQ(buffer_space + 18, buffer);

    EXPECT_EQ(2, pack(&buffer, uint16_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, uint32_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, uint64_t(1 << 16 - 1)));
    EXPECT_EQ(buffer_space + 24, buffer);

    EXPECT_EQ(3, pack(&buffer, uint32_t(1 << 16)));
    EXPECT_EQ(3, pack(&buffer, uint64_t(1 << 16)));
    EXPECT_EQ(buffer_space + 30, buffer);

    EXPECT_EQ(3, pack(&buffer, uint32_t(1 << 24 - 1)));
    EXPECT_EQ(3, pack(&buffer, uint64_t(1 << 24 - 1)));
    EXPECT_EQ(buffer_space + 36, buffer);

    EXPECT_EQ(4, pack(&buffer, uint32_t(1 << 24)));
    EXPECT_EQ(4, pack(&buffer, uint64_t(1 << 24)));
    EXPECT_EQ(buffer_space + 44, buffer);

    EXPECT_EQ(4, pack(&buffer, uint32_t(1ULL << 32 - 1)));
    EXPECT_EQ(4, pack(&buffer, uint64_t(1ULL << 32 - 1)));
    EXPECT_EQ(buffer_space + 52, buffer);

    EXPECT_EQ(5, pack(&buffer, uint64_t(1ULL << 32)));
    EXPECT_EQ(5, pack(&buffer, uint64_t(1ULL << 40 - 1)));
    EXPECT_EQ(buffer_space + 62, buffer);

    EXPECT_EQ(6, pack(&buffer, uint64_t(1ULL << 40)));
    EXPECT_EQ(6, pack(&buffer, uint64_t(1ULL << 48 - 1)));
    EXPECT_EQ(buffer_space + 74, buffer);
    EXPECT_EQ(7, pack(&buffer, uint64_t(1ULL << 48)));
    EXPECT_EQ(7, pack(&buffer, uint64_t(1ULL << 56 -1)));
    EXPECT_EQ(buffer_space + 88, buffer);
    EXPECT_EQ(8, pack(&buffer, uint64_t(1ULL << 56)));
    EXPECT_EQ(8, pack(&buffer, uint64_t(1ULL << 64 - 1)));
    EXPECT_EQ(buffer_space + 104, buffer);
}

TEST_F(PackerTest, packSignedIntegers) {
    char *buffer = buffer_space;
    EXPECT_EQ(1, pack(&buffer, int8_t(0)));
    EXPECT_EQ(1, pack(&buffer, int16_t(0)));
    EXPECT_EQ(1, pack(&buffer, int32_t(0)));
    EXPECT_EQ(1, pack(&buffer, int64_t(0)));
    EXPECT_EQ(buffer_space + 4, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(1)));
    EXPECT_EQ(1, pack(&buffer, int16_t(1)));
    EXPECT_EQ(1, pack(&buffer, int32_t(1)));
    EXPECT_EQ(1, pack(&buffer, int64_t(1)));
    EXPECT_EQ(buffer_space + 8, buffer);

    EXPECT_EQ(1 + 8, pack(&buffer, int8_t(-1)));
    EXPECT_EQ(1 + 8, pack(&buffer, int16_t(-1)));
    EXPECT_EQ(1 + 8, pack(&buffer, int32_t(-1)));
    EXPECT_EQ(1 + 8, pack(&buffer, int64_t(-1)));
    EXPECT_EQ(buffer_space + 12, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(127)));
    EXPECT_EQ(1, pack(&buffer, int16_t(127)));
    EXPECT_EQ(1, pack(&buffer, int32_t(127)));
    EXPECT_EQ(1, pack(&buffer, int64_t(127)));
    EXPECT_EQ(buffer_space + 16, buffer);

    EXPECT_EQ(1 + 8, pack(&buffer, int8_t(128)));
    EXPECT_EQ(1, pack(&buffer, int16_t(128)));
    EXPECT_EQ(1, pack(&buffer, int32_t(128)));
    EXPECT_EQ(1, pack(&buffer, int64_t(128)));
    EXPECT_EQ(buffer_space + 20, buffer);

    EXPECT_EQ(1 + 8, pack(&buffer, int8_t(-128)));
    EXPECT_EQ(1 + 8, pack(&buffer, int16_t(-128)));
    EXPECT_EQ(1 + 8, pack(&buffer, int32_t(-128)));
    EXPECT_EQ(1 + 8, pack(&buffer, int64_t(-128)));
    EXPECT_EQ(buffer_space + 24, buffer);

    EXPECT_EQ(1 + 8, pack(&buffer, int8_t(255)));
    EXPECT_EQ(1, pack(&buffer, int16_t(255)));
    EXPECT_EQ(1, pack(&buffer, int32_t(255)));
    EXPECT_EQ(1, pack(&buffer, int64_t(255)));
    EXPECT_EQ(buffer_space + 28, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(-255)));
    EXPECT_EQ(1 + 8, pack(&buffer, int16_t(-255)));
    EXPECT_EQ(1 + 8, pack(&buffer, int32_t(-255)));
    EXPECT_EQ(1 + 8, pack(&buffer, int64_t(-255)));
    EXPECT_EQ(buffer_space + 32, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(1 << 8)));
    EXPECT_EQ(buffer_space + 33, buffer);
    EXPECT_EQ(2, pack(&buffer, int16_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, int32_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, int64_t(1 << 8)));
    EXPECT_EQ(buffer_space + 39, buffer);

    EXPECT_EQ(1, pack(&buffer, -int8_t(1 << 8)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int16_t(1 << 8)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int32_t(1 << 8)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int64_t(1 << 8)));
    EXPECT_EQ(buffer_space + 46, buffer);

    EXPECT_EQ(2 + 8, pack(&buffer, int16_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, int32_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, int64_t(1 << 16 - 1)));
    EXPECT_EQ(buffer_space + 52, buffer);

    EXPECT_EQ(2 + 8, pack(&buffer, -int32_t(1 << 16 - 1)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int64_t(1 << 16 - 1)));
    EXPECT_EQ(buffer_space + 56, buffer);

    EXPECT_EQ(3, pack(&buffer, int32_t(1 << 16)));
    EXPECT_EQ(3, pack(&buffer, int64_t(1 << 16)));
    EXPECT_EQ(buffer_space + 62, buffer);

    EXPECT_EQ(3 + 8, pack(&buffer, -int32_t(1 << 16)));
    EXPECT_EQ(3 + 8, pack(&buffer, -int64_t(1 << 16)));
    EXPECT_EQ(buffer_space + 68, buffer);

    EXPECT_EQ(3, pack(&buffer, int32_t(1 << 24 - 1)));
    EXPECT_EQ(3, pack(&buffer, int64_t(1 << 24 - 1)));
    EXPECT_EQ(buffer_space + 74, buffer);

    EXPECT_EQ(4, pack(&buffer, int32_t(1 << 24)));
    EXPECT_EQ(4, pack(&buffer, uint64_t(1 << 24)));
    EXPECT_EQ(buffer_space + 82, buffer);

    EXPECT_EQ(4, pack(&buffer, -int32_t(1 << 24)));
    EXPECT_EQ(4 + 8, pack(&buffer, -int64_t(1 << 24)));
    EXPECT_EQ(buffer_space + 90, buffer);

    EXPECT_EQ(4, pack(&buffer, int32_t(1LL << 32 - 1)));
    EXPECT_EQ(4, pack(&buffer, int64_t(1LL << 32 - 1)));
    EXPECT_EQ(buffer_space + 98, buffer);

    EXPECT_EQ(4 + 8, pack(&buffer, -int64_t(1ULL << 32 - 1)));
    EXPECT_EQ(buffer_space + 102, buffer);

    EXPECT_EQ(5, pack(&buffer, int64_t(1ULL << 32)));
    EXPECT_EQ(5, pack(&buffer, int64_t(1ULL << 40 - 1)));
    EXPECT_EQ(6, pack(&buffer, int64_t(1ULL << 40)));
    EXPECT_EQ(6, pack(&buffer, int64_t(1ULL << 48 - 1)));
    EXPECT_EQ(7, pack(&buffer, int64_t(1ULL << 48)));
    EXPECT_EQ(7, pack(&buffer, int64_t(1ULL << 56 -1)));
    EXPECT_EQ(8, pack(&buffer, int64_t(1ULL << 56)));
    EXPECT_EQ(8, pack(&buffer, int64_t(1ULL << 64 - 1)));
    EXPECT_EQ(buffer_space + 102 + 52, buffer);

    EXPECT_EQ(5 + 8, pack(&buffer, -int64_t(1ULL << 32)));
    EXPECT_EQ(5 + 8, pack(&buffer, -int64_t(1ULL << 40 - 1)));
    EXPECT_EQ(6 + 8, pack(&buffer, -int64_t(1ULL << 40)));
    EXPECT_EQ(6 + 8, pack(&buffer, -int64_t(1ULL << 48 - 1)));
    EXPECT_EQ(7 + 8, pack(&buffer, -int64_t(1ULL << 48)));
    EXPECT_EQ(7 + 8, pack(&buffer, -int64_t(1ULL << 56 -1)));
    EXPECT_EQ(8, pack(&buffer, -int64_t(1ULL << 56)));
    EXPECT_EQ(buffer_space + 102 + 52 + 44, buffer);
}

TEST_F(PackerTest, unpack_int) {
    // The majority of tests were performed in the pack test. This one will
    // now test the integration and only spot check

    char *buffer = buffer_space;
    ASSERT_EQ(1, pack(&buffer, int(0)));
    ASSERT_EQ(1, pack(&buffer, int(1)));
    ASSERT_EQ(1 + 8, pack(&buffer, int(-1)));
    ASSERT_EQ(1, pack(&buffer, int(255)));
    ASSERT_EQ(2, pack(&buffer, int(256)));

    ASSERT_EQ(3, pack(&buffer, int((1<<24) - 1)));
    ASSERT_EQ(4, pack(&buffer, -int(1<<24)));
    ASSERT_EQ(4, pack(&buffer, int(1<<25)));
    ASSERT_EQ(4, pack(&buffer, -int(1<<25)));
    ASSERT_EQ(buffer_space + 21, buffer);

//    // Write the buffer to a file and read it back
//    ofstream oFile;
//    oFile.open(testFile);
//    oFile.write(buffer_space, buffer - buffer_space);
//    oFile.close();
//
//    ifstream in;
//    in.open(testFile);
    const char *readPtr = buffer_space;

    EXPECT_EQ(0, unpack<int>(&readPtr, 1));

    EXPECT_EQ(1, unpack<int>(&readPtr, 1));
    EXPECT_EQ(-1, unpack<int>(&readPtr, 1 + 8));
    EXPECT_EQ(255, unpack<int>(&readPtr, 1));
    EXPECT_EQ(256, unpack<int>(&readPtr, 2));

    EXPECT_EQ(int((1<<24) - 1), unpack<int>(&readPtr, 3));
    EXPECT_EQ(-int(1<<24), unpack<int>(&readPtr, 4));
    EXPECT_EQ(int(1<<25), unpack<int>(&readPtr, 4));
    EXPECT_EQ(-int(1<<25), unpack<int>(&readPtr, 4));
    EXPECT_EQ(buffer_space + 21, readPtr);

//    std::remove(testFile);
}

TEST_F(PackerTest, unpack_pointer) {
    void *ptr0 = (void*)(0x1234567890123456);
    void *ptr1 = (void*)(0xf23456789012);

    char *buffer = buffer_space;
    EXPECT_EQ(8U, pack(&buffer, ptr0));
    EXPECT_EQ(6U, pack(&buffer, ptr1));
    EXPECT_EQ(buffer_space + 14, buffer);

    const char *readPtr = buffer_space;

    EXPECT_EQ(ptr0, unpack<void*>(&readPtr, 8));
    EXPECT_EQ(ptr1, unpack<void*>(&readPtr, 6));
    EXPECT_EQ(buffer_space + 14, readPtr);
}

TEST_F(PackerTest, unpack_uint64_t) {
    uint64_t u0 = (uint64_t)(0x1234567890123456);
    uint64_t u1 = (uint64_t)(0xf23456789012);

    char *buffer = buffer_space;
    EXPECT_EQ(8U, pack(&buffer, u0));
    EXPECT_EQ(6U, pack(&buffer, u1));
    ASSERT_EQ(buffer_space + 14, buffer);

    const char *readPtr = buffer_space;

    EXPECT_EQ(u0, unpack<uint64_t>(&readPtr, 8));
    EXPECT_EQ(u1, unpack<uint64_t>(&readPtr, 6));
    EXPECT_EQ(buffer_space + 14, readPtr);
}

TEST_F(PackerTest, unpack_ptrdiff) {
    char *ptr0 = (char*)(0x1234567890123456);
    char *ptr1 = (char*)(0x1234567890123450);
    char *ptr2 = (char*)(0xf234567890123456);

    std::ptrdiff_t smallDiff1 = ptr0 - ptr1;
    std::ptrdiff_t smallDiff2 = ptr1 - ptr0;

    std::ptrdiff_t bigDiff1 = ptr0 - ptr2;
    std::ptrdiff_t bigDiff2 = ptr2 - ptr0;

    char *buffer = buffer_space;
    EXPECT_EQ(1U, pack(&buffer, smallDiff1));
    EXPECT_EQ(1 + 8, pack(&buffer, smallDiff2));
    EXPECT_EQ(8, pack(&buffer, bigDiff1));
    EXPECT_EQ(8, pack(&buffer, bigDiff2));
    ASSERT_EQ(buffer_space + 18, buffer);

    const char *readPtr = buffer_space;

    EXPECT_EQ(smallDiff1, unpack<std::ptrdiff_t>(&readPtr, 1U));
    EXPECT_EQ(smallDiff2, unpack<std::ptrdiff_t>(&readPtr, 1 + 8));
    EXPECT_EQ(bigDiff1, unpack<std::ptrdiff_t>(&readPtr, 8));
    EXPECT_EQ(bigDiff2, unpack<std::ptrdiff_t>(&readPtr, 8));
    EXPECT_EQ(buffer_space + 18, readPtr);
}

TEST_F(PackerTest, packFloating) {
    char *buffer = buffer_space;
    EXPECT_EQ(8, pack(&buffer, (double)(0.0)));
    EXPECT_EQ(4, pack(&buffer, (float)0.0));
    EXPECT_EQ(buffer_space + 12, buffer);

    EXPECT_EQ(8, pack(&buffer, (double)(0.1)));
    EXPECT_EQ(4, pack(&buffer, (float)(0.1)));
    EXPECT_EQ(buffer_space + 24, buffer);
}

TEST_F(PackerTest, unpack_floating) {
    char *buffer = buffer_space;
    ASSERT_EQ(8, pack(&buffer, (double)(0.0)));
    ASSERT_EQ(4, pack(&buffer, (float)0.0));

    ASSERT_EQ(8, pack(&buffer, (double)(0.1)));
    ASSERT_EQ(4, pack(&buffer, (float)(0.1)));
    ASSERT_EQ(buffer_space + 24, buffer);

    const char *readPtr = buffer_space;

    EXPECT_EQ(0.0, unpack<double>(&readPtr, 8));
    EXPECT_EQ(0.0, unpack<float>(&readPtr, 4));
    EXPECT_EQ(buffer_space + sizeof(float) + sizeof(double), readPtr);

    EXPECT_EQ(0.1, unpack<double>(&readPtr, 8));
    EXPECT_EQ(0.1f, unpack<float>(&readPtr, 4));
    EXPECT_EQ(buffer_space + 2*(sizeof(float) + sizeof(double)), readPtr);
}

TEST_F(PackerTest, getSizeOfPackedValues) {
    BufferUtils::TwoNibbles nibbles[1000];
    char *buffer = buffer_space;

    int nibbleCntr = 0;
    // Try special types
    nibbles[nibbleCntr++/2].first = pack(&buffer, (float)(0.1));
    EXPECT_EQ(4, getSizeOfPackedValues(nibbles, nibbleCntr));

    nibbles[nibbleCntr++/2].second = pack(&buffer, (double)(0.1));
    EXPECT_EQ(12, getSizeOfPackedValues(nibbles, nibbleCntr));

    // Try unsigned numbers
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(0));
    EXPECT_EQ(13, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1<<8));
    EXPECT_EQ(15, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(1<<16));
    EXPECT_EQ(18, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1<<24));
    EXPECT_EQ(22, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(1UL<<32));
    EXPECT_EQ(27, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1UL<<40));
    EXPECT_EQ(33, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(1UL<<48));
    EXPECT_EQ(40, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1UL<<56));
    EXPECT_EQ(48, getSizeOfPackedValues(nibbles, nibbleCntr));

    // Try signed numbers
    nibbles[nibbleCntr++/2].first  = pack(&buffer, int64_t(-1));
    EXPECT_EQ(49, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, int64_t(-(1<<8)));
    EXPECT_EQ(51, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, int64_t(-(1<<16)));
    EXPECT_EQ(54, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, int64_t(-(1<<24)));
    EXPECT_EQ(58, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, int64_t(-(1LL<<32)));
    EXPECT_EQ(63, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, int64_t(-(1LL<<40)));
    EXPECT_EQ(69, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, int64_t(-(1LL<<48)));
    EXPECT_EQ(76, getSizeOfPackedValues(nibbles, nibbleCntr));
    nibbles[nibbleCntr++/2].second = pack(&buffer, int64_t(-(1LL<<56)));
    EXPECT_EQ(84, getSizeOfPackedValues(nibbles, nibbleCntr));
}

TEST_F(PackerTest, nibbler) {
    BufferUtils::TwoNibbles nibbles[1000];
    char backing_buffer[1024];
    char *buffer = backing_buffer;

    // Start packing some data
    int nibbleCntr = 0;
    // Try special types
    nibbles[nibbleCntr++/2].first = pack(&buffer, (float)(0.1));
    nibbles[nibbleCntr++/2].second = pack(&buffer, (double)(0.2));

    // Try unsigned numbers
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(0));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1<<8));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(1<<16));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1<<24));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(1UL<<32));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1UL<<40));
    nibbles[nibbleCntr++/2].first  = pack(&buffer, uint64_t(1UL<<48));
    nibbles[nibbleCntr++/2].second = pack(&buffer, uint64_t(1UL<<56));

    // Try signed numbers
    nibbles[nibbleCntr++/2].first   = pack(&buffer, int64_t(-(1<<8)));
    nibbles[nibbleCntr++/2].second  = pack(&buffer, int64_t(-(1<<16)));
    nibbles[nibbleCntr++/2].first   = pack(&buffer, int64_t(-(1<<24)));
    nibbles[nibbleCntr++/2].second  = pack(&buffer, int64_t(-(1LL<<32)));
    nibbles[nibbleCntr++/2].first   = pack(&buffer, int64_t(-(1LL<<40)));
    nibbles[nibbleCntr++/2].second  = pack(&buffer, int64_t(-(1LL<<48)));
    nibbles[nibbleCntr++/2].first   = pack(&buffer, int64_t(-(1LL<<56)));

    // Now let's squash the nibbles and pack()-ed values into one stream.
    uint32_t packedBytes = buffer - backing_buffer;
    uint32_t nibbleBytes = (nibbleCntr+1)/2;

    memmove(backing_buffer + nibbleBytes, backing_buffer, packedBytes);
    memcpy(backing_buffer, nibbles, nibbleBytes);

    Nibbler nb(backing_buffer, nibbleCntr);
    EXPECT_EQ(nibbleBytes, nb.currPackedValue - backing_buffer);
    EXPECT_EQ(packedBytes, nb.getEndOfPackedArguments() - nb.currPackedValue);

    EXPECT_EQ((float)0.1, nb.getNext<float>());
    EXPECT_EQ((double)0.2, nb.getNext<double>());

    EXPECT_EQ(0, nb.getNext<uint64_t>());
    EXPECT_EQ(1<<8, nb.getNext<uint64_t>());
    EXPECT_EQ(1<<16, nb.getNext<uint64_t>());
    EXPECT_EQ(1<<24, nb.getNext<uint64_t>());
    EXPECT_EQ(1UL<<32, nb.getNext<uint64_t>());
    EXPECT_EQ(1UL<<40, nb.getNext<uint64_t>());
    EXPECT_EQ(1UL<<48, nb.getNext<uint64_t>());
    EXPECT_EQ(1UL<<56, nb.getNext<uint64_t>());

    EXPECT_EQ(int64_t(-(1<<8)), nb.getNext<int64_t>());
    EXPECT_EQ(int64_t(-(1<<16)), nb.getNext<int64_t>());
    EXPECT_EQ(int64_t(-(1<<24)), nb.getNext<int64_t>());
    EXPECT_EQ(int64_t(-(1LL<<32)), nb.getNext<int64_t>());
    EXPECT_EQ(int64_t(-(1LL<<40)), nb.getNext<int64_t>());
    EXPECT_EQ(int64_t(-(1LL<<48)), nb.getNext<int64_t>());
    EXPECT_EQ(int64_t(-(1LL<<56)), nb.getNext<int64_t>());

}

TEST_F(PackerTest, nibbler_assert) {
    BufferUtils::TwoNibbles nibbles[1000];
    char backing_buffer[1024];
    char *buffer = backing_buffer;

    // Start packing some data
    int nibbleCntr = 0;
    nibbles[nibbleCntr++/2].first = pack(&buffer, (float)(0.1));
    nibbles[nibbleCntr++/2].second = pack(&buffer, (double)(0.2));

    // Now let's squash the nibbles and pack()-ed values into one stream.
    uint32_t packedBytes = buffer - backing_buffer;
    uint32_t nibbleBytes = (nibbleCntr+1)/2;

    memmove(backing_buffer + nibbleBytes, backing_buffer, packedBytes);
    memcpy(backing_buffer, nibbles, nibbleBytes);

    Nibbler nb(backing_buffer, nibbleCntr);

    // Consume all the data and prepare for death
    EXPECT_EQ((float)0.1, nb.getNext<float>());
    EXPECT_EQ((double)0.2, nb.getNext<double>());
    EXPECT_DEATH(nb.getNext<int>(), "");
}

}  // namespace
