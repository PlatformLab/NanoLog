#include <assert.h>
#include <cstdlib>
#include <stdio.h>

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

  PackerTest() {
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
  }

  // Objects declared here can be used by all tests in the test case for Foo.

    char buffer_space[10000];
    char *buffer = buffer_space;
};

TEST_F(PackerTest, packUnsignedIntegers)
{
    // Only test pack's ability to identify the min # of bytes required
    // and advances the buffer appropriately. Verification of the data
    // comes in the unpack test.
    char *buffer = buffer_space;
    EXPECT_EQ(0, pack(&buffer, uint8_t(0)));
    EXPECT_EQ(0, pack(&buffer, uint16_t(0)));
    EXPECT_EQ(0, pack(&buffer, uint32_t(0)));
    EXPECT_EQ(0, pack(&buffer, uint64_t(0)));
    EXPECT_EQ(buffer_space, buffer);

    EXPECT_EQ(1, pack(&buffer, uint8_t(1)));
    EXPECT_EQ(1, pack(&buffer, uint16_t(1)));
    EXPECT_EQ(1, pack(&buffer, uint32_t(1)));
    EXPECT_EQ(1, pack(&buffer, uint64_t(1)));
    EXPECT_EQ(buffer_space + 4, buffer);

    EXPECT_EQ(1, pack(&buffer, uint8_t(255)));
    EXPECT_EQ(1, pack(&buffer, uint16_t(255)));
    EXPECT_EQ(1, pack(&buffer, uint32_t(255)));
    EXPECT_EQ(1, pack(&buffer, uint64_t(255)));
    EXPECT_EQ(buffer_space + 8, buffer);

    EXPECT_EQ(2, pack(&buffer, uint16_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, uint32_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, uint64_t(1 << 8)));
    EXPECT_EQ(buffer_space + 14, buffer);

    EXPECT_EQ(2, pack(&buffer, uint16_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, uint32_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, uint64_t(1 << 16 - 1)));
    EXPECT_EQ(buffer_space + 20, buffer);

    EXPECT_EQ(3, pack(&buffer, uint32_t(1 << 16)));
    EXPECT_EQ(3, pack(&buffer, uint64_t(1 << 16)));
    EXPECT_EQ(buffer_space + 26, buffer);

    EXPECT_EQ(3, pack(&buffer, uint32_t(1 << 24 - 1)));
    EXPECT_EQ(3, pack(&buffer, uint64_t(1 << 24 - 1)));
    EXPECT_EQ(buffer_space + 32, buffer);

    EXPECT_EQ(4, pack(&buffer, uint32_t(1 << 24)));
    EXPECT_EQ(4, pack(&buffer, uint64_t(1 << 24)));
    EXPECT_EQ(buffer_space + 40, buffer);

    EXPECT_EQ(4, pack(&buffer, uint32_t(1ULL << 32 - 1)));
    EXPECT_EQ(4, pack(&buffer, uint64_t(1ULL << 32 - 1)));
    EXPECT_EQ(buffer_space + 48, buffer);

    EXPECT_EQ(5, pack(&buffer, uint64_t(1ULL << 32)));
    EXPECT_EQ(5, pack(&buffer, uint64_t(1ULL << 40 - 1)));
    EXPECT_EQ(buffer_space + 58, buffer);

    EXPECT_EQ(6, pack(&buffer, uint64_t(1ULL << 40)));
    EXPECT_EQ(6, pack(&buffer, uint64_t(1ULL << 48 - 1)));
    EXPECT_EQ(buffer_space + 70, buffer);
    EXPECT_EQ(7, pack(&buffer, uint64_t(1ULL << 48)));
    EXPECT_EQ(7, pack(&buffer, uint64_t(1ULL << 56 -1)));
    EXPECT_EQ(buffer_space + 84, buffer);
    EXPECT_EQ(8, pack(&buffer, uint64_t(1ULL << 56)));
    EXPECT_EQ(8, pack(&buffer, uint64_t(1ULL << 64 - 1)));
    EXPECT_EQ(buffer_space + 100, buffer);
}

TEST_F(PackerTest, packSignedIntegers) {
    char *buffer = buffer_space;
    EXPECT_EQ(0, pack(&buffer, int8_t(0)));
    EXPECT_EQ(0, pack(&buffer, int16_t(0)));
    EXPECT_EQ(0, pack(&buffer, int32_t(0)));
    EXPECT_EQ(0, pack(&buffer, int64_t(0)));
    EXPECT_EQ(buffer_space + 0, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(1)));
    EXPECT_EQ(1, pack(&buffer, int16_t(1)));
    EXPECT_EQ(1, pack(&buffer, int32_t(1)));
    EXPECT_EQ(1, pack(&buffer, int64_t(1)));
    EXPECT_EQ(buffer_space + 4, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(-1)));
    EXPECT_EQ(1 + 8, pack(&buffer, int16_t(-1)));
    EXPECT_EQ(1 + 8, pack(&buffer, int32_t(-1)));
    EXPECT_EQ(1 + 8, pack(&buffer, int64_t(-1)));
    EXPECT_EQ(buffer_space + 8, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(127)));
    EXPECT_EQ(1, pack(&buffer, int16_t(127)));
    EXPECT_EQ(1, pack(&buffer, int32_t(127)));
    EXPECT_EQ(1, pack(&buffer, int64_t(127)));
    EXPECT_EQ(buffer_space + 12, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(128)));
    EXPECT_EQ(1, pack(&buffer, int16_t(128)));
    EXPECT_EQ(1, pack(&buffer, int32_t(128)));
    EXPECT_EQ(1, pack(&buffer, int64_t(128)));
    EXPECT_EQ(buffer_space + 16, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(-128)));
    EXPECT_EQ(1 + 8, pack(&buffer, int16_t(-128)));
    EXPECT_EQ(1 + 8, pack(&buffer, int32_t(-128)));
    EXPECT_EQ(1 + 8, pack(&buffer, int64_t(-128)));
    EXPECT_EQ(buffer_space + 20, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(255)));
    EXPECT_EQ(1, pack(&buffer, int16_t(255)));
    EXPECT_EQ(1, pack(&buffer, int32_t(255)));
    EXPECT_EQ(1, pack(&buffer, int64_t(255)));
    EXPECT_EQ(buffer_space + 24, buffer);

    EXPECT_EQ(1, pack(&buffer, int8_t(-255)));
    EXPECT_EQ(1 + 8, pack(&buffer, int16_t(-255)));
    EXPECT_EQ(1 + 8, pack(&buffer, int32_t(-255)));
    EXPECT_EQ(1 + 8, pack(&buffer, int64_t(-255)));
    EXPECT_EQ(buffer_space + 28, buffer);

    EXPECT_EQ(0, pack(&buffer, int8_t(1 << 8)));
    EXPECT_EQ(buffer_space + 28, buffer);
    EXPECT_EQ(2, pack(&buffer, int16_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, int32_t(1 << 8)));
    EXPECT_EQ(2, pack(&buffer, int64_t(1 << 8)));
    EXPECT_EQ(buffer_space + 34, buffer);

    EXPECT_EQ(0, pack(&buffer, -int8_t(1 << 8)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int16_t(1 << 8)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int32_t(1 << 8)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int64_t(1 << 8)));
    EXPECT_EQ(buffer_space + 40, buffer);

    EXPECT_EQ(2, pack(&buffer, int16_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, int32_t(1 << 16 - 1)));
    EXPECT_EQ(2, pack(&buffer, int64_t(1 << 16 - 1)));
    EXPECT_EQ(buffer_space + 46, buffer);

    EXPECT_EQ(2 + 8, pack(&buffer, -int32_t(1 << 16 - 1)));
    EXPECT_EQ(2 + 8, pack(&buffer, -int64_t(1 << 16 - 1)));
    EXPECT_EQ(buffer_space + 50, buffer);

    EXPECT_EQ(3, pack(&buffer, int32_t(1 << 16)));
    EXPECT_EQ(3, pack(&buffer, int64_t(1 << 16)));
    EXPECT_EQ(buffer_space + 56, buffer);

    EXPECT_EQ(3 + 8, pack(&buffer, -int32_t(1 << 16)));
    EXPECT_EQ(3 + 8, pack(&buffer, -int64_t(1 << 16)));
    EXPECT_EQ(buffer_space + 62, buffer);

    EXPECT_EQ(3, pack(&buffer, int32_t(1 << 24 - 1)));
    EXPECT_EQ(3, pack(&buffer, int64_t(1 << 24 - 1)));
    EXPECT_EQ(buffer_space + 68, buffer);

    EXPECT_EQ(4, pack(&buffer, int32_t(1 << 24)));
    EXPECT_EQ(4, pack(&buffer, uint64_t(1 << 24)));
    EXPECT_EQ(buffer_space + 76, buffer);

    EXPECT_EQ(4, pack(&buffer, -int32_t(1 << 24)));
    EXPECT_EQ(4 + 8, pack(&buffer, -int64_t(1 << 24)));
    EXPECT_EQ(buffer_space + 84, buffer);

    EXPECT_EQ(4, pack(&buffer, int32_t(1LL << 32 - 1)));
    EXPECT_EQ(4, pack(&buffer, int64_t(1LL << 32 - 1)));
    EXPECT_EQ(buffer_space + 92, buffer);

    EXPECT_EQ(4 + 8, pack(&buffer, -int64_t(1ULL << 32 - 1)));
    EXPECT_EQ(buffer_space + 96, buffer);

    EXPECT_EQ(5, pack(&buffer, int64_t(1ULL << 32)));
    EXPECT_EQ(5, pack(&buffer, int64_t(1ULL << 40 - 1)));
    EXPECT_EQ(6, pack(&buffer, int64_t(1ULL << 40)));
    EXPECT_EQ(6, pack(&buffer, int64_t(1ULL << 48 - 1)));
    EXPECT_EQ(7, pack(&buffer, int64_t(1ULL << 48)));
    EXPECT_EQ(7, pack(&buffer, int64_t(1ULL << 56 -1)));
    EXPECT_EQ(8, pack(&buffer, int64_t(1ULL << 56)));
    EXPECT_EQ(8, pack(&buffer, int64_t(1ULL << 64 - 1)));
    EXPECT_EQ(buffer_space + 96 + 52, buffer);

    EXPECT_EQ(5 + 8, pack(&buffer, -int64_t(1ULL << 32)));
    EXPECT_EQ(5 + 8, pack(&buffer, -int64_t(1ULL << 40 - 1)));
    EXPECT_EQ(6 + 8, pack(&buffer, -int64_t(1ULL << 40)));
    EXPECT_EQ(6 + 8, pack(&buffer, -int64_t(1ULL << 48 - 1)));
    EXPECT_EQ(7 + 8, pack(&buffer, -int64_t(1ULL << 48)));
    EXPECT_EQ(7 + 8, pack(&buffer, -int64_t(1ULL << 56 -1)));
    EXPECT_EQ(8, pack(&buffer, -int64_t(1ULL << 56)));
    EXPECT_EQ(buffer_space + 96 + 52 + 44, buffer);
}

TEST_F(PackerTest, unpack_int) {
    // The majority of tests were performed in the pack test. This one will
    // now test the integration and only spot check

    char *buffer = buffer_space;
    ASSERT_EQ(0, pack(&buffer, int(0)));
    ASSERT_EQ(1, pack(&buffer, int(1)));
    ASSERT_EQ(1 + 8, pack(&buffer, int(-1)));
    ASSERT_EQ(1, pack(&buffer, int(255)));
    ASSERT_EQ(2, pack(&buffer, int(256)));

    ASSERT_EQ(3, pack(&buffer, int((1<<24) - 1)));
    ASSERT_EQ(4, pack(&buffer, -int(1<<24)));
    ASSERT_EQ(4, pack(&buffer, int(1<<25)));
    ASSERT_EQ(4, pack(&buffer, -int(1<<25)));
    ASSERT_EQ(buffer_space + 20, buffer);

    buffer = buffer_space;
    EXPECT_EQ(0, unpack<int>(&buffer, 0));
    EXPECT_EQ(buffer, buffer_space);

    EXPECT_EQ(1, unpack<int>(&buffer, 1));
    EXPECT_EQ(-1, unpack<int>(&buffer, 1 + 8));
    EXPECT_EQ(255, unpack<int>(&buffer, 1));
    EXPECT_EQ(256, unpack<int>(&buffer, 2));

    EXPECT_EQ(int((1<<24) - 1), unpack<int>(&buffer, 3));
    EXPECT_EQ(-int(1<<24), unpack<int>(&buffer, 4));
    EXPECT_EQ(int(1<<25), unpack<int>(&buffer, 4));
    EXPECT_EQ(-int(1<<25), unpack<int>(&buffer, 4));
    ASSERT_EQ(buffer_space + 20, buffer);
}

TEST_F(PackerTest, unpack_pointer) {
    void *ptr0 = (void*)(0x1234567890123456);
    void *ptr1 = (void*)(0xf23456789012);

    char *buffer = buffer_space;
    EXPECT_EQ(8U, pack(&buffer, ptr0));
    EXPECT_EQ(6U, pack(&buffer, ptr1));
    EXPECT_EQ(buffer_space + 14, buffer);

    buffer = buffer_space;
    EXPECT_EQ(ptr0, unpackPointer<void*>(&buffer, 8));
    EXPECT_EQ(ptr1, unpackPointer<void*>(&buffer, 6));
    EXPECT_EQ(buffer_space + 14, buffer);
}

TEST_F(PackerTest, unpack_uint64_t) {
    uint64_t u0 = (uint64_t)(0x1234567890123456);
    uint64_t u1 = (uint64_t)(0xf23456789012);

    char *buffer = buffer_space;
    EXPECT_EQ(8U, pack(&buffer, u0));
    EXPECT_EQ(6U, pack(&buffer, u1));
    ASSERT_EQ(buffer_space + 14, buffer);

    buffer = buffer_space;
    EXPECT_EQ(u0, unpack<uint64_t>(&buffer, 8));
    EXPECT_EQ(u1, unpack<uint64_t>(&buffer, 6));
    EXPECT_EQ(buffer_space + 14, buffer);
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
}

TEST_F(PackerTest, packFloating) {
    char *buffer = buffer_space;
    EXPECT_EQ(0, pack(&buffer, (double)(0.0)));
    EXPECT_EQ(0, pack(&buffer, (float)0.0));
    EXPECT_EQ(buffer_space, buffer);

    EXPECT_EQ(8, pack(&buffer, (double)(0.1)));
    EXPECT_EQ(4, pack(&buffer, (float)(0.1)));
    EXPECT_EQ(buffer_space + 12, buffer);
}

TEST_F(PackerTest, unpack_floating) {
    char *buffer = buffer_space;
    ASSERT_EQ(0, pack(&buffer, (double)(0.0)));
    ASSERT_EQ(0, pack(&buffer, (float)0.0));

    ASSERT_EQ(8, pack(&buffer, (double)(0.1)));
    ASSERT_EQ(4, pack(&buffer, (float)(0.1)));
    ASSERT_EQ(buffer_space + 12, buffer);

    buffer = buffer_space;
    EXPECT_EQ(0.0, unpack<double>(&buffer, 0));
    EXPECT_EQ(0.0, unpack<float>(&buffer, 0));

    EXPECT_EQ(0.1, unpack<double>(&buffer, 8));
    EXPECT_EQ(0.1f, unpack<float>(&buffer, 4));
    ASSERT_EQ(buffer_space + 12, buffer);
}

}  // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
