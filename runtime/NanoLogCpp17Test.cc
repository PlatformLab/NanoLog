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

TEST_F(NanoLogCpp17Test, getParamInfo_constexpr) {
    constexpr ParamType ret1 = getParamInfo("Hello World %*.*s asdf", 1);
    EXPECT_EQ(ParamType::DYNAMIC_PRECISION, ret1);

    constexpr ParamType ret2 = getParamInfo("Hello World %*.*s asdf", 2);
    EXPECT_EQ(ParamType::STRING_WITH_DYNAMIC_PRECISION, ret2);

    constexpr ParamType ret3 = getParamInfo("Hello World %*.*s asdf", 3);
    EXPECT_EQ(ParamType::INVALID, ret3);
}

TEST_F(NanoLogCpp17Test, getParamInfo) {
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

    // Test escaped parameters
    EXPECT_EQ(INVALID, getParamInfo("~S!@#$^&*()_+1234567890qwertyu\n"
                                    "iopasdfghjkl;zxcv  bnm,\\\\\\\\r\\\\n\n"
                                    "%%ud \\%%lf osdif<>\":L:];"));

    // Test all the types (ported from python unit tests)
    const char test_jzt[] = "%10.12jd %9ji %0*.*ju %jo %jx %jX %zu %zd %tu %td";
    EXPECT_EQ(NON_STRING, getParamInfo(test_jzt, 0));
    EXPECT_EQ(NON_STRING, getParamInfo(test_jzt, 1));
    EXPECT_EQ(DYNAMIC_WIDTH, getParamInfo(test_jzt, 2));
    EXPECT_EQ(DYNAMIC_PRECISION, getParamInfo(test_jzt, 3));
    for (int i = 4; i <= 11; ++i)
        EXPECT_EQ(NON_STRING, getParamInfo(test_jzt, i));
    EXPECT_EQ(INVALID, getParamInfo(test_jzt, 12));

    const char doubles[] = "%12.0f %12.3F %e %55.3E %-10.5g %G %a %A";
    for (int i = 0; i <= 7; ++i)
        EXPECT_EQ(NON_STRING, getParamInfo(doubles, i));
    EXPECT_EQ(INVALID, getParamInfo(doubles, 8));

    const char longDoubles[] = "%12.0Lf %12.3LF %Le %55.3LE %-10.5Lg %LG %La%LA";
    for (int i = 0; i <= 7; ++i)
        EXPECT_EQ(NON_STRING, getParamInfo(longDoubles, i));
    EXPECT_EQ(INVALID, getParamInfo(longDoubles, 8));

    const char basicInts[] = "%d %i %u %o %x %X %c %p";
    for (int i = 0; i <= 7; ++i)
        EXPECT_EQ(NON_STRING, getParamInfo(basicInts, i));
    EXPECT_EQ(INVALID, getParamInfo(basicInts, 8));

    const char lengthMods[] = "%hhd %hd %ld %lld %jd %zd %09.2td";
    for (int i = 0; i <= 6; ++i)
        EXPECT_EQ(NON_STRING, getParamInfo(lengthMods, i));
    EXPECT_EQ(INVALID, getParamInfo(lengthMods, 7));

    // Test all the types
    EXPECT_EQ(INVALID, getParamInfo("Hello World!"));

    EXPECT_EQ(NON_STRING, getParamInfo("%hhd %hhi", 0));
    EXPECT_EQ(NON_STRING, getParamInfo("%hhd %hhi", 1));
    EXPECT_EQ(INVALID, getParamInfo("%hhd %hhi", 2));

    // Unsupported variations of %n
    EXPECT_ANY_THROW(getParamInfo("%hhn"));
    EXPECT_ANY_THROW(getParamInfo("%jn"));
    EXPECT_ANY_THROW(getParamInfo("%zn"));
    EXPECT_ANY_THROW(getParamInfo("%#0t4.02n"));

    // invalid specifier
    EXPECT_ANY_THROW(getParamInfo("%hhj"));
}

TEST_F(NanoLogCpp17Test, analyzeFormatString) {
    constexpr std::array<ParamType, 10> testArray = analyzeFormatString<10>(
                              "Hello %*.*d %%%s %*.*s %10.500s %10.500d %+#.s");

    EXPECT_EQ(ParamType::DYNAMIC_WIDTH, testArray.at(0));
    EXPECT_EQ(ParamType::DYNAMIC_PRECISION, testArray.at(1));
    EXPECT_EQ(ParamType::NON_STRING, testArray.at(2));
    EXPECT_EQ(ParamType::STRING_WITH_NO_PRECISION, testArray.at(3));
    EXPECT_EQ(ParamType::DYNAMIC_WIDTH, testArray.at(4));
    EXPECT_EQ(ParamType::DYNAMIC_PRECISION, testArray.at(5));
    EXPECT_EQ(ParamType::STRING_WITH_DYNAMIC_PRECISION, testArray.at(6));
    EXPECT_EQ(ParamType(500), testArray.at(7));
    EXPECT_EQ(ParamType::NON_STRING, testArray.at(8));
    EXPECT_EQ(ParamType(0), testArray.at(9));
}

TEST_F(NanoLogCpp17Test, analyzeFormatString_empty) {
    constexpr std::array<ParamType, 0> testArray = analyzeFormatString<0>("a");
    EXPECT_EQ(0U, testArray.size());
}

TEST_F(NanoLogCpp17Test, countFmtParams) {
    EXPECT_EQ(10, countFmtParams("d %*.*d %%%s %*.*s %10.500s %10.500d %+#.s"));
    EXPECT_EQ(0, countFmtParams("alsdjaklsjflsajfkdasjl%%%%f"));
    EXPECT_EQ(0, countFmtParams(""));
}

TEST_F(NanoLogCpp17Test, getNumNibblesNeeded) {
    EXPECT_EQ(6, getNumNibblesNeeded(
            "d %*.*d %%%s %*.*s %10.500s %10.500d %+#.s"));
    EXPECT_EQ(0, getNumNibblesNeeded(""));
    EXPECT_EQ(0, getNumNibblesNeeded("asldkfjaslkfjasfd"));
    EXPECT_EQ(0, getNumNibblesNeeded("%s"));
    EXPECT_EQ(1, getNumNibblesNeeded("%d"));
}

TEST_F(NanoLogCpp17Test, store_argument) {
    // There are a lot of types out there, so we only try a few...
    char backingBuffer[10*1024];
    char *buffer = backingBuffer;

    int randomNumber = 5;
    store_argument(&buffer, 'a', NON_STRING, -1);
    store_argument(&buffer, int(5), NON_STRING, -1);
    store_argument(&buffer, int(-5), NON_STRING, -1);
    store_argument(&buffer, uint16_t(2), NON_STRING, -1);
    store_argument(&buffer, uint32_t(-1), NON_STRING, -1);
    store_argument(&buffer, int64_t(1LL<<48), NON_STRING, -1);
    store_argument(&buffer, 0.333333333333f, NON_STRING, -1);
    store_argument(&buffer, 0.6666666666666, NON_STRING, -1);
    store_argument(&buffer, &randomNumber, NON_STRING, -1);
    store_argument(&buffer, 0.99999999L, NON_STRING, -1);
    store_argument(&buffer, 'b', NON_STRING, -1);

    ASSERT_EQ(sizeof('a')
                + sizeof(int)
                  + sizeof(int)
                    + sizeof(uint16_t)
                      + sizeof(uint32_t)
                        + sizeof(int64_t)
                          + sizeof(float)
                            + sizeof(double)
                              + sizeof(void*)
                                + sizeof(long double)
                                  + sizeof('b')
                , buffer - backingBuffer);

    buffer = backingBuffer;
    EXPECT_EQ('a', *buffer); buffer += sizeof('a');

    EXPECT_EQ(5, *reinterpret_cast<int*>(buffer));
    buffer += sizeof(int);

    EXPECT_EQ(-5, *reinterpret_cast<int*>(buffer));
    buffer += sizeof(int);

    EXPECT_EQ(2, *reinterpret_cast<uint16_t*>(buffer));
    buffer += sizeof(uint16_t);

    EXPECT_EQ(uint32_t(-1), *reinterpret_cast<uint32_t*>(buffer));
    buffer += sizeof(uint32_t);

    EXPECT_EQ(281474976710656, *reinterpret_cast<int64_t*>(buffer));
    buffer += sizeof(int64_t);

    EXPECT_FLOAT_EQ(0.333333333333f, *reinterpret_cast<float*>(buffer));
    buffer += sizeof(float);

    EXPECT_DOUBLE_EQ(0.6666666666666, *reinterpret_cast<double*>(buffer));
    buffer += sizeof(double);

    EXPECT_EQ(&randomNumber, *reinterpret_cast<void**>(buffer));
    buffer += sizeof(void*);

    EXPECT_DOUBLE_EQ(0.99999999, *reinterpret_cast<long double*>(buffer));
    buffer += sizeof(long double);

    EXPECT_EQ('b', *buffer);
    buffer += sizeof('b');
}

TEST_F(NanoLogCpp17Test, store_argument_stringTypes) {
    char backingBuffer[1024];
    char *buffer = backingBuffer;

    const char *str1  = "String Onneee";
    char *str2 = const_cast<char*>("String Two ");
    const char str3[] = "String Three";
    char str4[] = "String Four";

    const wchar_t *wstr1  = L"Wide String Onneee";
    wchar_t *wstr2 = const_cast<wchar_t*>(L"Wide String Two ");
    const wchar_t wstr3[] = L"Wide String Three";
    wchar_t wstr4[] = L"Wide String Four";


    // Normal Storage
    store_argument(&buffer, str1, STRING_WITH_NO_PRECISION, strlen(str1));
    EXPECT_EQ(sizeof(uint32_t) + strlen(str1), buffer - backingBuffer);
    EXPECT_EQ(strlen(str1), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(str1, backingBuffer + sizeof(uint32_t), strlen(str1)));
    buffer = backingBuffer;

    store_argument(&buffer, str2, STRING_WITH_NO_PRECISION, strlen(str2));
    EXPECT_EQ(sizeof(uint32_t) + strlen(str2), buffer - backingBuffer);
    EXPECT_EQ(strlen(str2), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(str2, backingBuffer + sizeof(uint32_t), strlen(str2)));
    buffer = backingBuffer;

    store_argument(&buffer, str3, STRING_WITH_NO_PRECISION, strlen(str3));
    EXPECT_EQ(sizeof(uint32_t) + strlen(str3), buffer - backingBuffer);
    EXPECT_EQ(strlen(str3), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(str3, backingBuffer + sizeof(uint32_t), strlen(str3)));
    buffer = backingBuffer;

    store_argument(&buffer, str4, STRING_WITH_NO_PRECISION, strlen(str4));
    EXPECT_EQ(sizeof(uint32_t) + strlen(str4), buffer - backingBuffer);
    EXPECT_EQ(strlen(str4), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(str4, backingBuffer + sizeof(uint32_t), strlen(str4)));
    buffer = backingBuffer;

    store_argument(&buffer, wstr1, STRING_WITH_NO_PRECISION, wcslen(wstr1));
    EXPECT_EQ(sizeof(uint32_t) + wcslen(wstr1), buffer - backingBuffer);
    EXPECT_EQ(wcslen(wstr1), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(wstr1, backingBuffer + sizeof(uint32_t), wcslen(wstr1)));
    buffer = backingBuffer;

    store_argument(&buffer, wstr2, STRING_WITH_NO_PRECISION, wcslen(wstr2));
    EXPECT_EQ(sizeof(uint32_t) + wcslen(wstr2), buffer - backingBuffer);
    EXPECT_EQ(wcslen(wstr2), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(wstr2, backingBuffer + sizeof(uint32_t), wcslen(wstr2)));
    buffer = backingBuffer;

    store_argument(&buffer, wstr3, STRING_WITH_NO_PRECISION, wcslen(wstr3));
    EXPECT_EQ(sizeof(uint32_t) + wcslen(wstr3), buffer - backingBuffer);
    EXPECT_EQ(wcslen(wstr3), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(wstr3, backingBuffer + sizeof(uint32_t), wcslen(wstr3)));
    buffer = backingBuffer;

    store_argument(&buffer, wstr4, STRING_WITH_NO_PRECISION, wcslen(wstr4));
    EXPECT_EQ(sizeof(uint32_t) + wcslen(wstr4), buffer - backingBuffer);
    EXPECT_EQ(wcslen(wstr4), *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(wstr4, backingBuffer + sizeof(uint32_t), wcslen(wstr4)));
    buffer = backingBuffer;

    // Try a truncated store as well
    store_argument(&buffer, str1, STRING, 4);
    EXPECT_EQ(sizeof(uint32_t) + 4, buffer - backingBuffer);
    EXPECT_EQ(4, *reinterpret_cast<uint32_t*>(backingBuffer));
    EXPECT_EQ(0U, memcmp(str1, backingBuffer + sizeof(uint32_t), 4));
    buffer = backingBuffer;

    // Finally what if it's just a pointer we want?
    store_argument(&buffer, str1, NON_STRING, 4);
    EXPECT_EQ(sizeof(void*), buffer - backingBuffer);
    buffer = backingBuffer;
    EXPECT_EQ((void*)str1, *reinterpret_cast<void**>(buffer));
}

TEST_F(NanoLogCpp17Test, store_arguments) {
    char backing_buffer[1024];
    char *buffer = backing_buffer;

    constexpr std::array<ParamType, 5> testArray = analyzeFormatString<5>(
            "Hello %s %p %*.*s");
    size_t stringSizes[5];

    // Do nothing
    store_arguments(testArray, stringSizes, &buffer);
    EXPECT_EQ(backing_buffer, buffer);
    buffer = backing_buffer;

    // Store one int
    store_arguments(testArray, stringSizes, &buffer, int(5));
    EXPECT_EQ(sizeof(int), buffer - backing_buffer);
    buffer = backing_buffer;
    EXPECT_EQ(5, *reinterpret_cast<int*>(buffer));

    // Store one string
    stringSizes[0] = 10; // we will truncate
    store_arguments(testArray, stringSizes, &buffer, "hablabamos en espanol");
    EXPECT_EQ(sizeof(uint32_t) + 10, buffer - backing_buffer);
    buffer = backing_buffer;
    EXPECT_EQ(10, *reinterpret_cast<uint32_t*>(buffer));
    EXPECT_EQ(0, memcmp(backing_buffer + sizeof(int32_t),
                                    "hablabamos en espanol", 10));

    // Try a full on store
    const char *pointer = "John Ousterhout";

    buffer = backing_buffer;
    stringSizes[0] = strlen("Stephen Yang");
    stringSizes[4] = strlen("Seo Jin Park");
    store_arguments(testArray, stringSizes, &buffer,
            "Stephen Yang",
            pointer,
            5,
            10,
            "Seo Jin Park"
    );

    // Parity check that all the correct store_argument's have been invoked
    EXPECT_EQ(sizeof(uint32_t) + stringSizes[0]
                     + sizeof(void*)
                     + sizeof(int)
                     + sizeof(int)
                     + sizeof(uint32_t) + strlen("Seo Jin Park"),
                 buffer - backing_buffer);
}

template<int N>
constexpr static int
staticStrlen(const char (&)[N]) {
    return N;
}

template<int N>
constexpr static int
staticStrlen(const wchar_t (&)[N]) {
    return N;
}

TEST_F(NanoLogCpp17Test, getArgSize) {
    size_t stringSize = 0;
    uint64_t previousPrecision = 0;
    EXPECT_EQ(sizeof(uint16_t), getArgSize(NON_STRING,
                                            previousPrecision,
                                            stringSize,
                                            (uint16_t(5))));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(sizeof(uint16_t), getArgSize(DYNAMIC_PRECISION,
                                          previousPrecision,
                                          stringSize,
                                          (uint16_t(5))));
    EXPECT_EQ(5U, previousPrecision);

    void *ptr = &stringSize;
    EXPECT_EQ(sizeof(void*), getArgSize(DYNAMIC_PRECISION,
                                        previousPrecision,
                                        stringSize,
                                        ptr));

    previousPrecision = 0;
    // First check that we trigger the right function for char*'s
    const char *str1 = "Hey now";
    const char str2[] = "you're a rockstar";
    char *str3 = const_cast<char*>("get your game on");
    char str4[] = "go. play.";

    size_t len = strlen(str1);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                        previousPrecision,
                                        stringSize,
                                        str1));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    len = strlen(str2);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                      previousPrecision,
                                      stringSize,
                                      str2));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    len = strlen(str3);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                      previousPrecision,
                                      stringSize,
                                      str3));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    len = strlen(str4);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                      previousPrecision,
                                      stringSize,
                                      str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    // Now for a given char*, we go through all the possible if's
    stringSize = 0;
    previousPrecision = 0;
    EXPECT_EQ(sizeof(void*), getArgSize(NON_STRING,
                                        previousPrecision,
                                        stringSize,
                                        str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(0, stringSize);

    // String with static precision, but precision is shorter.
    len = staticStrlen(str4);
    static_assert(4 < staticStrlen(str4), "str4 has to be longer than 4 chars");
    EXPECT_EQ(4 + sizeof(uint32_t), getArgSize(ParamType(4),
                                                 previousPrecision,
                                                 stringSize,
                                                 str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(4, stringSize);

    // String with static precision, but precision is is longer.
    len = strlen(str4);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(ParamType(9999),
                                                 previousPrecision,
                                                 stringSize,
                                                 str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);


    // String with dynamic precision, but precision is shorter.
    len = staticStrlen(str4);
    previousPrecision = 4;
    static_assert(4 < staticStrlen(str4), "str4 has to be longer than 4 chars");
    EXPECT_EQ(4 + sizeof(uint32_t), getArgSize(STRING_WITH_DYNAMIC_PRECISION,
                                               previousPrecision,
                                               stringSize,
                                               str4));
    EXPECT_EQ(4, stringSize);

    // String with dynamic precision, but precision is is longer.
    len = strlen(str4);
    previousPrecision = 9999;
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_DYNAMIC_PRECISION,
                                                 previousPrecision,
                                                 stringSize,
                                                 str4));
    EXPECT_EQ(len, stringSize);
}

TEST_F(NanoLogCpp17Test, getArgSize_wchar_t) {
    size_t stringSize = 0;
    uint64_t previousPrecision = 0;

    const wchar_t *str1 = L"Hey now";
    const wchar_t str2[] = L"you're a rockstar";
    wchar_t *str3 = const_cast<wchar_t*>(L"get your game on");
    wchar_t str4[] = L"go. play.";

    size_t len = wcslen(str1)*sizeof(wchar_t);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                                 previousPrecision,
                                                 stringSize,
                                                 str1));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    len = wcslen(str2)*sizeof(wchar_t);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                                 previousPrecision,
                                                 stringSize,
                                                 str2));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    len = wcslen(str3)*sizeof(wchar_t);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                                 previousPrecision,
                                                 stringSize,
                                                 str3));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    len = wcslen(str4)*sizeof(wchar_t);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_NO_PRECISION,
                                                 previousPrecision,
                                                 stringSize,
                                                 str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);

    // Now for a given char*, we go through all the possible if's
    stringSize = 0;
    previousPrecision = 0;
    EXPECT_EQ(sizeof(void*), getArgSize(NON_STRING,
                                        previousPrecision,
                                        stringSize,
                                        str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(0, stringSize);

    // String with static precision, but precision is shorter.
    len = staticStrlen(str4)*sizeof(wchar_t);
    static_assert(4 < staticStrlen(str4), "str4 has to be longer than 4 chars");
    EXPECT_EQ(4*sizeof(wchar_t) + sizeof(uint32_t), getArgSize(ParamType(4),
                                                           previousPrecision,
                                                           stringSize,
                                                           str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(4*sizeof(wchar_t), stringSize);

    // String with static precision, but precision is is longer.
    len = wcslen(str4)*sizeof(wchar_t);
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(ParamType(9999),
                                                 previousPrecision,
                                                 stringSize,
                                                 str4));
    EXPECT_EQ(0U, previousPrecision);
    EXPECT_EQ(len, stringSize);


    // String with dynamic precision, but precision is shorter.
    len = staticStrlen(str4)*sizeof(wchar_t);
    previousPrecision = 4;
    static_assert(4 < staticStrlen(str4), "str4 has to be longer than 4 chars");
    EXPECT_EQ(4*sizeof(wchar_t) + sizeof(uint32_t), getArgSize(
                                                STRING_WITH_DYNAMIC_PRECISION,
                                                previousPrecision,
                                                stringSize,
                                                str4));
    EXPECT_EQ(4*sizeof(wchar_t), stringSize);

    // String with dynamic precision, but precision is is longer.
    len = wcslen(str4)*sizeof(wchar_t);
    previousPrecision = 9999;
    EXPECT_EQ(len + sizeof(uint32_t), getArgSize(STRING_WITH_DYNAMIC_PRECISION,
                                                 previousPrecision,
                                                 stringSize,
                                                 str4));
    EXPECT_EQ(len, stringSize);
}

TEST_F(NanoLogCpp17Test, getArgSizes) {
    const std::array<ParamType, 0> noArgs = {};
    uint64_t previousPrecision = 0;
    size_t stringArgSizes[10] = {};

    // Do nothing
    EXPECT_EQ(0U, getArgSizes(noArgs, previousPrecision, stringArgSizes));

    // One int
    const std::array<ParamType, 1> singleInt = {NON_STRING};
    EXPECT_EQ(sizeof(int),
                getArgSizes(singleInt, previousPrecision, stringArgSizes, 99));

    // One specifier and a string pointer (%p)
    const char *randomStr = "blah blah";
    const std::array<ParamType, 2> secondTest = {DYNAMIC_PRECISION, NON_STRING};
    EXPECT_EQ(sizeof(int) + sizeof(void*),
                getArgSizes(secondTest, previousPrecision, stringArgSizes,
                                                                10, randomStr));
    EXPECT_EQ(10, previousPrecision);
    EXPECT_EQ(0U, stringArgSizes[0]);
    EXPECT_EQ(0U, stringArgSizes[1]);
    EXPECT_EQ(0U, stringArgSizes[2]);

    const std::array<ParamType, 2> thirdTest = {DYNAMIC_PRECISION,
                                                STRING_WITH_DYNAMIC_PRECISION};
    EXPECT_EQ(sizeof(int) + sizeof(uint32_t) + strlen(randomStr),
              getArgSizes(thirdTest, previousPrecision, stringArgSizes,
                                                                10, randomStr));
    EXPECT_EQ(10, previousPrecision);
    EXPECT_EQ(0U, stringArgSizes[0]);
    EXPECT_EQ(strlen(randomStr), stringArgSizes[1]);
    EXPECT_EQ(0U, stringArgSizes[2]);
    stringArgSizes[1] = 0; // reset

    // A few things
    constexpr std::array<ParamType, 5> finalArray = analyzeFormatString<5>(
            "Hello %s %p %*.*s");
    EXPECT_EQ(sizeof(uint32_t) + strlen("Stephen Yang")
                + sizeof(void*)
                + sizeof(int)
                + sizeof(int)
                + sizeof(uint32_t) + 10,
                    getArgSizes(finalArray, previousPrecision, stringArgSizes,
                                                     "Stephen Yang",
                                                     randomStr,
                                                     5,
                                                     10,
                                                     "Seo Jin Park"));
}

TEST_F(NanoLogCpp17Test, compressSingle) {
    BufferUtils::TwoNibbles nibbles[10] {};
    ParamType type = ParamType::INVALID;

    char inBuffer[1024];
    char outBuffer[1024];
    char scratchBuffer[1024];

    char *in = inBuffer;
    char *out = outBuffer;
    char *scratch = scratchBuffer;
    int nibbleCnt = 0;

    // Load a string
    char aString[] = "blah blah bla?";
    uint32_t aStringLength = strlen(aString);
    *reinterpret_cast<uint32_t*>(in) = aStringLength;
    in += sizeof(uint32_t);
    memcpy(in, aString, aStringLength);

    // Want to compress string, but we're in no strings mode (i.e. skip)
    in = inBuffer;
    compressSingle<char*>(nibbles, &nibbleCnt,
                            ParamType::STRING, false,
                            &in, &out);
    EXPECT_EQ(0, nibbles[0].first);
    EXPECT_EQ(0, nibbles[0].second);
    EXPECT_EQ(0, nibbleCnt);
    EXPECT_EQ(outBuffer, out);
    EXPECT_EQ(inBuffer + sizeof(uint32_t) + aStringLength, in);

    // Want to compress string, and we are in string mode
    in = inBuffer;
    out = outBuffer;

    compressSingle<char*>(nibbles, &nibbleCnt,
                          ParamType::STRING, true,
                          &in, &out);
    EXPECT_EQ(0, nibbles[0].first);
    EXPECT_EQ(0, nibbles[0].second);
    EXPECT_EQ(0, nibbleCnt);
    EXPECT_EQ(outBuffer + aStringLength + 1, out); // +1 for NULL character
    EXPECT_EQ(inBuffer + sizeof(uint32_t) + aStringLength, in);
    EXPECT_STREQ(aString, outBuffer);

    // Want to compress wide string, but not in strings mode
    in = inBuffer; out = outBuffer;
    wchar_t wString[] = L"Blah blah blaaaaah?";
    uint32_t wStringBytes = wcslen(wString)*sizeof(wchar_t);
    *reinterpret_cast<uint32_t*>(in) = wStringBytes;
    in += sizeof(uint32_t);
    memcpy(in, wString, wStringBytes);

    in = inBuffer; out = outBuffer;
    compressSingle<wchar_t>(nibbles, &nibbleCnt,
                                ParamType::STRING_WITH_NO_PRECISION, false,
                                &in, &out);

    EXPECT_EQ(0, nibbles[0].first);
    EXPECT_EQ(0, nibbles[0].second);
    EXPECT_EQ(0, nibbleCnt);
    EXPECT_EQ(outBuffer, out);
    EXPECT_EQ(inBuffer + sizeof(uint32_t) + wStringBytes, in);

    // Want to compress wide string, and in string mode
    in = inBuffer; out = outBuffer;
    compressSingle<wchar_t>(nibbles, &nibbleCnt,
                            ParamType::STRING_WITH_NO_PRECISION, true,
                            &in, &out);

    EXPECT_EQ(0, nibbles[0].first);
    EXPECT_EQ(0, nibbles[0].second);
    EXPECT_EQ(0, nibbleCnt);
    EXPECT_EQ(outBuffer + wStringBytes + sizeof(wchar_t), out);
    EXPECT_EQ(inBuffer + sizeof(uint32_t) + wStringBytes, in);
    EXPECT_EQ(0, memcmp(outBuffer, wString, wStringBytes));


    // Want to compress a uint64_t, and not in string mode
    in = inBuffer; out = outBuffer;
    uint64_t aNumber = 256;
    *reinterpret_cast<uint64_t*>(in) = aNumber;
    in += sizeof(uint64_t);

    in = inBuffer;
    compressSingle<uint64_t>(nibbles, &nibbleCnt,
                             ParamType::NON_STRING, false,
                             &in, &out);
    EXPECT_EQ(1, nibbleCnt);
    EXPECT_EQ(BufferUtils::pack(&scratch, aNumber), nibbles[0].first);
    EXPECT_EQ(0, nibbles[0].second);
    EXPECT_EQ(sizeof(uint64_t) + inBuffer, in);
    EXPECT_EQ(scratch - scratchBuffer, out - outBuffer);

    // Want to compress it again
    in = inBuffer;
    compressSingle<uint64_t>(nibbles, &nibbleCnt,
                             ParamType::NON_STRING, false,
                             &in, &out);
    EXPECT_EQ(2, nibbleCnt);
    EXPECT_EQ(BufferUtils::pack(&scratch, aNumber), nibbles[0].second);
    EXPECT_EQ(0, nibbles[1].first);
    EXPECT_EQ(sizeof(uint64_t) + inBuffer, in);
    // SUBTLE POINT, we BufferUtils::pack(...) the same number we do in
    // compressSingle to get the same offsets
    EXPECT_EQ(scratch - scratchBuffer, out - outBuffer);

    // Want to compress a uint64_t, and in string mode. So in should be
    // incremented, but not out.
    in = inBuffer;
    compressSingle<uint64_t>(nibbles, &nibbleCnt,
                             ParamType::NON_STRING, true,
                             &in, &out);
    EXPECT_EQ(2, nibbleCnt);
    EXPECT_EQ(sizeof(uint64_t) + inBuffer, in);
    EXPECT_EQ(scratch - scratchBuffer, out - outBuffer);
}

TEST_F(NanoLogCpp17Test, compress_internal) {
    BufferUtils::TwoNibbles nibbles[10];
    const ParamType isArgString[] = {NON_STRING,
                                     STRING_WITH_NO_PRECISION,
                                     STRING,
                                     NON_STRING};
    char inBuffer[1024];
    char outBuffer[1024];
    char scratchBuffer[1024];

    char *in = inBuffer;
    char *out = outBuffer;
    char *scratch = scratchBuffer;

    // Empty, do nothing
    compress_internal<>(nibbles, 0,
                        isArgString, false,
                        0, &in, &out);


    // Setup
    char aString[] = "Blah blah";
    wchar_t wString[] = L"bleh";

    uint32_t aStringBytes = strlen(aString);
    uint32_t wStringBytes = wcslen(wString)*sizeof(wchar_t);

    *reinterpret_cast<int*>(in) = -2;
    in += sizeof(int);

    *reinterpret_cast<uint32_t*>(in) = aStringBytes;
    in += sizeof(uint32_t);
    memcpy(in, aString, aStringBytes);
    in += aStringBytes;

    *reinterpret_cast<uint32_t*>(in) = wStringBytes;
    in += sizeof(uint32_t);
    memcpy(in, wString, wStringBytes);
    in += wStringBytes;

    *reinterpret_cast<uint16_t*>(in) = 99;
    in += sizeof(uint16_t);

    char *endOfIn = in;

    // Compress the every non-string
    in = inBuffer;
    compress_internal<int, char*, wchar_t*, uint16_t>(
                            nibbles, 0, isArgString, false, 0, &in, &out);
    EXPECT_EQ(endOfIn, in);
    EXPECT_EQ(BufferUtils::pack(&scratch, (int)(-2)), nibbles[0].first);
    EXPECT_EQ(BufferUtils::pack(&scratch, uint16_t(99)), nibbles[0].second);
    EXPECT_EQ(0, nibbles[1].first);

    ASSERT_EQ(scratch - scratchBuffer, out - outBuffer);
    EXPECT_EQ(0, memcmp(scratchBuffer, outBuffer, scratch - scratchBuffer));

    // "compress" every string
    in = inBuffer; out = outBuffer;
    nibbles[0].first = nibbles[0].second = 0;

    compress_internal<int, char*, wchar_t*, uint16_t>(
                                  nibbles, 0, isArgString, true, 0, &in, &out);
    EXPECT_EQ(endOfIn, in);
    EXPECT_EQ(0, nibbles[0].first);
    EXPECT_EQ(0, nibbles[0].second);
    EXPECT_EQ(0, nibbles[1].first);

    EXPECT_EQ(aStringBytes + 1 + wStringBytes + sizeof(wchar_t),
                                                               out - outBuffer);

    out = outBuffer;

    // First string
    EXPECT_EQ(0, memcmp(out, aString, aStringBytes));
    out += aStringBytes;
    EXPECT_EQ(0, *out);
    ++out;

    // Second string
    EXPECT_EQ(0, memcmp(out, wString, wStringBytes));
    out += wStringBytes;
    EXPECT_EQ(0, *out); ++out;
    EXPECT_EQ(0, *out); ++out;
    EXPECT_EQ(0, *out); ++out;
    EXPECT_EQ(0, *out); ++out;
}

TEST_F(NanoLogCpp17Test, compress) {
    const ParamType isArgString[] = {NON_STRING,
                                     STRING_WITH_NO_PRECISION,
                                     STRING,
                                     NON_STRING};
    char inBuffer[1024];
    char outBuffer[1024];
    char scratchBuffer[1024];

    char *in = inBuffer;
    char *out = outBuffer;
    char *scratch = scratchBuffer;

    // Empty, do nothing
    compress<>(0, isArgString, &in, &out);

    // Setup
    char aString[] = "Blah blah";
    wchar_t wString[] = L"bleh";

    uint32_t aStringBytes = strlen(aString);
    uint32_t wStringBytes = wcslen(wString)*sizeof(wchar_t);

    *reinterpret_cast<int*>(in) = -2;
    in += sizeof(int);

    *reinterpret_cast<uint32_t*>(in) = aStringBytes;
    in += sizeof(uint32_t);
    memcpy(in, aString, aStringBytes);
    in += aStringBytes;

    *reinterpret_cast<uint32_t*>(in) = wStringBytes;
    in += sizeof(uint32_t);
    memcpy(in, wString, wStringBytes);
    in += wStringBytes;

    *reinterpret_cast<uint16_t*>(in) = 99;
    in += sizeof(uint16_t);

    char *endOfIn = in;

    BufferUtils::TwoNibbles expectedNibble;
    expectedNibble.first = BufferUtils::pack(&scratch, (int)(-2));
    expectedNibble.second = BufferUtils::pack(&scratch, uint16_t(99));

    // Compress everything!
    in = inBuffer; out = outBuffer;
    compress<int, char*, wchar_t*, uint16_t>(2, isArgString, &in, &out);
    EXPECT_EQ(endOfIn, in);
    ASSERT_EQ(sizeof(BufferUtils::TwoNibbles)
              + scratch - scratchBuffer
              + aStringBytes + 1
              + wStringBytes + sizeof(wchar_t),
              out - outBuffer);

    // Check the nibbles
    out = outBuffer;
    auto *nibbles = reinterpret_cast<BufferUtils::TwoNibbles*>(out);
    out += sizeof(BufferUtils::TwoNibbles);

    EXPECT_EQ(expectedNibble.first, nibbles[0].first);
    EXPECT_EQ(expectedNibble.second, nibbles[0].second);

    // Check the compacted stuff
    EXPECT_EQ(0, memcmp(scratchBuffer, out, scratch - scratchBuffer));
    out += scratch - scratchBuffer;

    // Check the strings that follow
    EXPECT_STREQ(aString, out);
    out += aStringBytes + 1;

    EXPECT_EQ(0, memcmp(wString, out, wStringBytes));
    out += wStringBytes;

    // Make sure the terminal to wchar_t is right
    EXPECT_EQ(0, *out); ++out;
    EXPECT_EQ(0, *out); ++out;
    EXPECT_EQ(0, *out); ++out;
    EXPECT_EQ(0, *out); ++out;
}

}; //namespace