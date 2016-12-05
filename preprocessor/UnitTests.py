# Copyright (c) 2016 Stanford University
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

import unittest
import os
from parser import *

from FunctionGenerator import *


class  PreprocesorTestCase(unittest.TestCase):
    #def setUp(self):
    #    self.foo = UnitTests()
    #

    #def tearDown(self):
    #    self.foo.dispose()
    #    self.foo = None

    def test_parseArgumentStartingAt_quotes(self):
        lines = ["LOG((++i), \"Hello\")"]

        arg = parseArgumentStartingAt(lines, FilePosition(0, 4))
        self.assertEqual(arg, Argument("(++i)",
                                        FilePosition(0, 4), FilePosition(0, 9)))

        arg = parseArgumentStartingAt(lines, FilePosition(0, 10))
        self.assertEqual(arg, Argument(" \"Hello\"",
                                      FilePosition(0, 10), FilePosition(0, 18)))


    def test_markAndSeparateOnSemicolon_basic(self):
        lines = ["LOG(Test, 5);++i; ++i"]
        self.assertTrue(
                markAndSeparateOnSemicolon(lines, (0, 0), "test.txt", 55))

        # Important things are that it only splits the first semicolon
        # and the offset of the next statement matches the previous
        self.assertEqual(lines, ['LOG(Test, 5);\r\n',
                                 '# 55 "test.txt"\r\n',
                                 '             ++i; ++i']);

        # lastly, test multiple lines with offset configuration
        lines = [   "Unimportant();\n\n",
                    "Skipped() ;    NotSkipped(); ImportantStuff();\r\n",
                    "YoloSwag();\r\n"
                    ]
        self.assertTrue(
                markAndSeparateOnSemicolon(lines, (1, 17), "test.txt", 55))
        self.assertEqual(lines,
                        ['Unimportant();\n\n',
                         'Skipped() ;    NotSkipped();\r\n',
                         '# 55 "test.txt"\r\n',
                         '                             ImportantStuff();\r\n',
                         'YoloSwag();\r\n'
                         ])

    def test_markAndSeparateOnSemicolon_noSplits(self):
        # Unchanged due to missing semicolon on specified line
        lines = ["blah balh no Semi\r\n"
                    "Semi;\r\n"]
        self.assertFalse(
                markAndSeparateOnSemicolon(lines, (0, 0), "test.txt", 56))
        self.assertEqual(lines, ["blah balh no Semi\r\n"
                                 "Semi;\r\n"])

        # Unimportant characters found after semicolon so no splits
        lines = ["blah blah blah;     \t\r\n  "]
        self.assertFalse(
                markAndSeparateOnSemicolon(lines, (0, 0), "test.txt", 56))
        self.assertEquals(lines, ["blah blah blah;     \t\r\n  "])

    def test_parseTypesInFmtString_noReplacements(self):
        fmtString =  """~S!@#$^&*()_+1234567890qwertyu
                            iopasdfghjkl;zxcv  bnm,\\\\r\\n
                            %%ud \%lf osdif<>":L:];
                    """

        # No replacements should be performed becuase all % are escaped
        self.assertEqual(parseTypesInFmtString(fmtString), [])

        fmtString = ""
        self.assertEqual(parseTypesInFmtString(fmtString), [])

        fmtString = "Hello"
        self.assertEqual(parseTypesInFmtString(fmtString), [])

        fmtString = "\% %%"
        self.assertEqual(parseTypesInFmtString(fmtString), [])

    def test_parseTypesInFmtString_charTypes(self):
        self.assertEqual(parseTypesInFmtString("%hhd %hhi"),
                            ["signed char", "signed char"])
        self.assertEqual(parseTypesInFmtString(" %d"),
                            ["int"])

        with self.assertRaises(SystemExit):
            parseTypesInFmtString("%hhn")

        with self.assertRaises(AssertionError):
            parseTypesInFmtString("%hhj")

    def test_parseTypesInFmtString_jzt(self):
        self.assertEqual(parseTypesInFmtString("%jd %ji"),
                            ["intmax_t", "intmax_t"])

        self.assertEqual(parseTypesInFmtString("%ju %jo %jx %jX"),
                            ["uintmax_t", "uintmax_t", "uintmax_t", "uintmax_t"])

        self.assertEqual(parseTypesInFmtString("%zu %zd %tu %td"),
                            ["size_t", "size_t", 'ptrdiff_t', "ptrdiff_t"])

        self.assertEqual(parseTypesInFmtString("%jn %zn zn %tn"),
                            ["intmax_t*", "size_t*", "ptrdiff_t*"])

        # Unexpected characters!
        with self.assertRaises(AssertionError):
            parseTypesInFmtString("%z\r\n")

        with self.assertRaises(AssertionError):
            parseTypesInFmtString("%j\r\n")

        with self.assertRaises(AssertionError):
            parseTypesInFmtString("%t\r\n")

    def test_parseTypesInFmtString_doubleTypes(self):
        self.assertEqual(parseTypesInFmtString("%12.0f %12.3F %e %55.3E %-10.5g %G %a %A"),
                ["double", "double", "double", "double",
                "double", "double", "double", "double" ])

        self.assertEqual(parseTypesInFmtString("%12.0Lf %12.3LF %Le %55.3LE %-10.5Lg %LG %La %LA"),
                ["long double", "long double", "long double", "long double",
                "long double", "long double", "long double", "long double"])

        # Check that random modifiers don't change the type
        self.assertEqual(parseTypesInFmtString("%lf %llf"),["double", "double"])

        # Check for errors
        with self.assertRaises(AssertionError):
            parseTypesInFmtString("%Lu")

    def test_parseTypesInFmtString_basicIntegerTypes(self):
        self.assertEqual(parseTypesInFmtString("%d %i"), ["int", "int"])
        self.assertEqual(parseTypesInFmtString("%u %o"),
                            ["unsigned int", "unsigned int"])
        self.assertEqual(parseTypesInFmtString("%x %X"),
                            ["unsigned int", "unsigned int"])

        self.assertEqual(parseTypesInFmtString("%c %s %p"),
                            ["int", "const char*", "void*"])

        with self.assertRaises(SystemExit):
            parseTypesInFmtString("%n")

    def test_parseTypesInFmtString_cspn(self):
        self.assertEqual(parseTypesInFmtString("%c %s %p"),
            ["int", "const char*", "void*"])

        self.assertEqual(parseTypesInFmtString("%ls %lc"),
            ["const wchar_t*", "wint_t"])

        with self.assertRaises(SystemExit):
            parseTypesInFmtString("%n")

    def test_lengthModifiers(self):
        self.assertEqual(parseTypesInFmtString("%hhd %hd %ld %lld %jd %zd %td"),
                            ["signed char", "short int", "long int",
                            "long long int", "intmax_t", "size_t", "ptrdiff_t"])

        self.assertEqual(parseTypesInFmtString("%hhu %hu %lu %llu %ju %zu %tu"),
                                ["unsigned char", "short unsigned int",
                                 "long unsigned int", 'long long unsigned int',
                                 "uintmax_t", "size_t", "ptrdiff_t"])

        with self.assertRaises(SystemExit):
            self.assertEqual(
                    parseTypesInFmtString("%hhn %hn %ln %lln %jn %zn %tn"),
                                ["signed char*", "short int*", "long int*",
                                 "long long int*", "intmax_t*", "size_t*",
                                 "ptrdiff_t*"])

class FunctionGeneratorTestCase(unittest.TestCase):

    def test_generateLogFunctions_empty(self):
        self.maxDiff = None
        fg = FunctionGenerator("Input file")

        ret = fg.generateLogFunctions("Empty Print", "mar.cc", "mar.cc", 293)

        expectedFnName = generateFunctionNameFromFmtId(1);
        expectedResult = ("void " + expectedFnName + "(const char* fmtStr)",
                                expectedFnName)
        self.assertEqual(expectedResult, ret)

        self.assertEqual(1, fg.fmtStr2Id["Empty Print"])
        code = fg.fmtId2Code[1];

        self.assertEqual("Empty Print", code["fmtString"])
        self.assertEqual("mar.cc", code["filename"])
        self.assertEqual(293, code["linenum"])


        expectedRecordCode = \
"""void __syang0__fl__1(const char* fmtStr) {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::reserveAlloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 1, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}
"""
        expectedCompressCode = \
"""inline void
compressArgs1(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}
"""
        expectedDecompressCode = \
"""inline void
decompressPrintArg1(std::ifstream &in) {
\tBufferUtils::Nibble nib[0];
\tin.read(reinterpret_cast<char*>(&nib), 0);



\tconst char *fmtString = "Empty Print";
\tconst char *filename = "mar.cc";
\tconst int linenum = 293;

\tprintf(fmtString);
}
"""
        self.assertMultiLineEqual(expectedRecordCode, code["recordFnDef"])
        self.assertMultiLineEqual(expectedCompressCode, code["compressFnDef"])
        self.assertMultiLineEqual(expectedDecompressCode, code["decompressFnDef"])

    def test_generateLogFunctions(self):
        self.maxDiff = None
        fg = FunctionGenerator("Input file")
        args = [
                Argument("someVariable", FilePosition(0,0), FilePosition(1,0)),
                Argument("\"staticString\"", FilePosition(1,0), FilePosition(2,0)),
                Argument("0.1lf\t\n", FilePosition(3,0), FilePosition(4,0)),
                Argument("stringVar", FilePosition(4,0), FilePosition(5,0))
            ]

        fmtStr = "Hello World! %u %s %lf %s"
        ret = fg.generateLogFunctions(fmtStr, "testFile.cc", "testFile.cc", 100)
        expectedFnName = generateFunctionNameFromFmtId(1);
        expectedResult = ("void " + expectedFnName + "(const char* fmtStr, "
                            "unsigned int arg0, const char* arg1, double arg2, "
                            "const char* arg3)",
                            expectedFnName)

        self.assertEqual(expectedResult, ret)

        self.assertEqual(1, fg.fmtStr2Id[fmtStr])
        code = fg.fmtId2Code[1];

        self.assertEqual(fmtStr, code["fmtString"])
        self.assertEqual("testFile.cc", code["filename"])
        self.assertEqual(100, code["linenum"])

        # Now check the generated functions
        expectedRecordCode = \
"""void %s(const char* fmtStr, unsigned int arg0, const char* arg1, double arg2, const char* arg3) {
	int str1Len = strlen(arg1) + 1, str3Len = strlen(arg3) + 1;
	int maxSizeOfArgs = sizeof(arg0) + sizeof(arg2) + str1Len + str3Len;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::reserveAlloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 1, maxSizeOfArgs, 1);
	char *buffer = re->argData;

	BufferUtils::recordPrimitive(buffer, arg0);
	BufferUtils::recordPrimitive(buffer, arg2);

	memcpy(buffer, arg1, str1Len); buffer += str1Len;
	memcpy(buffer, arg3, str3Len); buffer += str3Len;
	PerfUtils::FastLogger::finishAlloc(re);
}
""" % expectedFnName

        expectedCompressCode = \
"""inline void
compressArgs1(BufferUtils::RecordEntry *re, char** out) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(*out);
	*out += 1;
	char* args = re->argData;
	unsigned int arg0 = *reinterpret_cast<unsigned int*>(args); args += sizeof(unsigned int);
	double arg2 = *reinterpret_cast<double*>(args); args += sizeof(double);

	nib[0].first = BufferUtils::pack(out, arg0);
	nib[0].second = BufferUtils::pack(out, arg2);

	int stringBytes = re->entrySize - (sizeof(arg0) + sizeof(arg2)) - sizeof(BufferUtils::RecordEntry);
	memcpy(*out, args, stringBytes);
	args += stringBytes;
	*out += stringBytes;
}
"""
        expectedDecompressCode = \
"""inline void
decompressPrintArg1(std::ifstream &in) {
	BufferUtils::Nibble nib[1];
	in.read(reinterpret_cast<char*>(&nib), 1);

	unsigned int arg0 = BufferUtils::unpack<unsigned int>(in, nib[0].first);
	double arg2 = BufferUtils::unpack<double>(in, nib[0].second);

	std::string arg1_str;
	std::getline(in, arg1_str, '\\0');
	const char* arg1 = arg1_str.c_str();
	std::string arg3_str;
	std::getline(in, arg3_str, '\\0');
	const char* arg3 = arg3_str.c_str();

	const char *fmtString = "Hello World! %u %s %lf %s";
	const char *filename = "testFile.cc";
	const int linenum = 100;

	printf(fmtString, arg0, arg1, arg2, arg3);
}
"""
        self.assertMultiLineEqual(expectedRecordCode, code["recordFnDef"])
        self.assertMultiLineEqual(expectedCompressCode, code["compressFnDef"])
        self.assertMultiLineEqual(expectedDecompressCode, code["decompressFnDef"])

    def test_generateLogFunctions_combinationAndOverwrite(self):
        fg = FunctionGenerator("Input file")

        # TODO(syang0) Currently we do not differenciate on files, only strings.
        # In the future, please add multi-file support that way the correct
        # filename and line number are saved

        # Original
        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)

        # Different log
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 293)

        # Same log + file, different location
        fg.generateLogFunctions("A", "mar.cc",  "mar.cc", 200)

        # smae log, diff file + location
        fg.generateLogFunctions("A", "s.cc", 100)

        self.assertEqual(2, len(fg.fmtStr2Id))
        self.assertEqual(3, len(fg.fmtId2Code))

        self.assertEqual(1 , fg.fmtStr2Id["A"])
        self.assertEqual(2 , fg.fmtStr2Id["B"])
        self.assertEqual(0, len(fg.unusedIds))

        self.assertEqual("A", fg.fmtId2Code[1]["fmtString"])
        self.assertEqual("B", fg.fmtId2Code[2]["fmtString"])

    def test_getRecordFunctionDefinitionsFor(self):
        self.maxDiff = None

        emptyRec = \
"""void __syang0__fl__%d(const char* fmtStr) {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::reserveAlloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, %d, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}
"""
        fg = FunctionGenerator()

        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("C", "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D", "s.cc", "s.cc", 100)

        self.assertEqual(3, len(fg.getRecordFunctionDefinitionsFor("mar.cc")))
        self.assertEqual(1, len(fg.getRecordFunctionDefinitionsFor("s.cc")))
        self.assertEqual(0, len(fg.getRecordFunctionDefinitionsFor("asdf.cc")))

        funcs = fg.getRecordFunctionDefinitionsFor("mar.cc");\
        self.assertMultiLineEqual(emptyRec % (1, 1), funcs[0])
        self.assertMultiLineEqual(emptyRec % (2, 2), funcs[1])
        self.assertMultiLineEqual(emptyRec % (3, 3), funcs[2])

        self.assertMultiLineEqual(emptyRec % (4, 4),
                                fg.getRecordFunctionDefinitionsFor("s.cc")[0])

    # TODO(syang0) this test only makes sense if implment diffenciating on
    # different files
#    def test_generateLogFunctions_clearLogFunctionsForFile(self):
#        "goodbyte"


    def test_outputMappingFile(self):
        fg = FunctionGenerator()

        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 294)
        fg.generateLogFunctions("C", "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D %d", "s.cc", "s.cc", 100)

        fg.unusedIds.append(10)

        # Test serialization and deserialization
        fg.outputMappingFile("test.json");
        fg2 = FunctionGenerator("test.json")

        self.assertEqual(fg.fmtId2Code, fg2.fmtId2Code)
        self.assertEqual(fg.unusedIds, fg2.unusedIds)
        self.assertEqual(fg.fmtStr2Id, fg2.fmtStr2Id)
        self.assertEqual(fg.argLists2Cnt, fg2.argLists2Cnt)

        os.remove("test.json")

    def test_loadPartialyMappingFile(self):
        mapping = {
            "unusedIds":[1, 2, 3, 4, 5]
        }

        with open("test.json", 'w') as json_file:
            json_file.write(json.dumps(mapping, sort_keys=True, indent=4,
                                                        separators=(',', ': ')))

        fg = FunctionGenerator("test.json")
        self.assertEqual([], fg.fmtId2Code)
        self.assertEqual([1, 2, 3, 4, 5], fg.unusedIds)
        self.assertEqual({}, fg.fmtStr2Id)
        self.assertEqual({}, fg.argLists2Cnt)

        os.remove("test.json")

    def test_outputCompilationFiles(self):
        self.maxDiff = None
        fg = FunctionGenerator()

        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 294)
        fg.generateLogFunctions("C", "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D %d", "s.cc", "s.cc", 100)

        fg.unusedIds.append(10)

        expectedContents = """#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include "FastLogger.h"
#include "Packer.h"

#include <fstream>     // for decompression
#include <string>

// Record code in an empty namespace(for debugging)
namespace {
void __syang0__fl__1(const char* fmtStr) {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::reserveAlloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 1, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

inline void
compressArgs1(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
decompressPrintArg1(std::ifstream &in) {
	BufferUtils::Nibble nib[0];
	in.read(reinterpret_cast<char*>(&nib), 0);



	const char *fmtString = "A";
	const char *filename = "mar.cc";
	const int linenum = 293;

	printf(fmtString);
}

void __syang0__fl__2(const char* fmtStr) {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::reserveAlloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 2, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

inline void
compressArgs2(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
decompressPrintArg2(std::ifstream &in) {
	BufferUtils::Nibble nib[0];
	in.read(reinterpret_cast<char*>(&nib), 0);



	const char *fmtString = "B";
	const char *filename = "mar.cc";
	const int linenum = 294;

	printf(fmtString);
}

void __syang0__fl__3(const char* fmtStr) {
	int maxSizeOfArgs = 0 + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::reserveAlloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 3, maxSizeOfArgs, 0);
	char *buffer = re->argData;
	PerfUtils::FastLogger::finishAlloc(re);
}

inline void
compressArgs3(BufferUtils::RecordEntry *re, char** out) {
	char* args = re->argData;

}

inline void
decompressPrintArg3(std::ifstream &in) {
	BufferUtils::Nibble nib[0];
	in.read(reinterpret_cast<char*>(&nib), 0);



	const char *fmtString = "C";
	const char *filename = "mar.cc";
	const int linenum = 200;

	printf(fmtString);
}

void __syang0__fl__4(const char* fmtStr, int arg0) {
	int maxSizeOfArgs = sizeof(arg0) + 0;
	BufferUtils::RecordEntry *re = PerfUtils::FastLogger::reserveAlloc(maxSizeOfArgs);

	if (re == nullptr)
		return;

	BufferUtils::recordMetadata(re, 4, maxSizeOfArgs, 1);
	char *buffer = re->argData;

	BufferUtils::recordPrimitive(buffer, arg0);

	PerfUtils::FastLogger::finishAlloc(re);
}

inline void
compressArgs4(BufferUtils::RecordEntry *re, char** out) {
	BufferUtils::Nibble *nib = reinterpret_cast<BufferUtils::Nibble*>(*out);
	*out += 1;
	char* args = re->argData;
	int arg0 = *reinterpret_cast<int*>(args); args += sizeof(int);

	nib[0].first = BufferUtils::pack(out, arg0);
}

inline void
decompressPrintArg4(std::ifstream &in) {
	BufferUtils::Nibble nib[1];
	in.read(reinterpret_cast<char*>(&nib), 1);

	int arg0 = BufferUtils::unpack<int>(in, nib[0].first);


	const char *fmtString = "D %d";
	const char *filename = "s.cc";
	const int linenum = 100;

	printf(fmtString, arg0);
}

} // end empty namespace

static void (*compressFnArray[5])(BufferUtils::RecordEntry *re, char** out) {
	nullptr,
	compressArgs1,
	compressArgs2,
	compressArgs3,
	compressArgs4
};

static void (*decompressAndPrintFnArray[5])(std::ifstream &in) {
	nullptr,
	decompressPrintArg1,
	decompressPrintArg2,
	decompressPrintArg3,
	decompressPrintArg4
};

// Format Id to original Format String
static const char* fmtId2Str[4] = {
	"A",
	"B",
	"C",
	"D %d"
};

#endif /* BUFFER_STUFFER */
"""

        fg.outputCompilationFiles("test.h")
        with open("test.h", 'r') as headerFile:
            contents = headerFile.read();
            self.assertMultiLineEqual(expectedContents, contents)

        os.remove("test.h")

if __name__ == '__main__':
    unittest.main()

