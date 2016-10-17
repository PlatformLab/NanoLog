# To change this license header, choose License Headers in Project Properties.
# To change this template file, choose Tools | Templates
# and open the template in the editor.

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
                            FileRange(FilePosition(0, 4), FilePosition(0, 9))))

        arg = parseArgumentStartingAt(lines, FilePosition(0, 10))
        self.assertEqual(arg, Argument(" \"Hello\"",
                            FileRange(FilePosition(0, 10), FilePosition(0, 18))))


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
        args = []

        ret = fg.generateLogFunctions("Empty Print", args, "mar.cc", "mar.cc", 293)

        expectedFnName = genRecordName(1);
        expectedResult = ("void " + expectedFnName + "()", expectedFnName +"()")
        self.assertEqual(expectedResult, ret)

        self.assertEqual(1, fg.fmtStr2Id["Empty Print"])
        code = fg.fmtId2Code[1];

        self.assertEqual("Empty Print", code["fmtString"])
        self.assertEqual("mar.cc", code["filename"])
        self.assertEqual(293, code["linenum"])


        expectedRecordCode = \
"""void __syang0__fl__1() {
\tint maxSizeOfArgs = 0 + 0;
\tint reqSize = sizeof(PerfUtils::FastLogger::RecordMetadata) + maxSizeOfArgs;
\tint maxSizeOfCompressedArgs = maxSizeOfArgs + 0;
\tchar *buffer = PerfUtils::FastLogger::alloc(reqSize);

\tif (buffer == nullptr)
\t\treturn;

\tPerfUtils::FastLogger::recordMetadata(buffer, 1, maxSizeOfCompressedArgs);
}
"""
        expectedCompressCode = \
"""inline void
compressArgs1(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {

}
"""
        expectedDecompressCode = \
"""inline void
decompressPrintArg1(char* &in) {
\tPerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(in);
\tin += 0;



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
                Argument("someVariable", FileRange(FilePosition(0,0), FilePosition(1,0))),
                Argument("\"staticString\"", FileRange(FilePosition(1,0), FilePosition(2,0))),
                Argument("0.1lf\t\n", FileRange(FilePosition(3,0), FilePosition(4,0))),
                Argument("stringVar", FileRange(FilePosition(4,0), FilePosition(5,0)))
            ]

        fmtStr = "Hello World! %u %s %lf %s"
        ret = fg.generateLogFunctions(fmtStr, args, "testFile.cc", "testFile.cc", 100)
        expectedFnName = genRecordName(1);
        expectedResult = ("void " + expectedFnName +
            "(unsigned int arg0, const char* arg1, double arg2, const char* arg3)",
            expectedFnName +"(someVariable, \"staticString\", 0.1lf\t\n, stringVar)")

        self.assertEqual(expectedResult, ret)

        self.assertEqual(1, fg.fmtStr2Id[fmtStr])
        code = fg.fmtId2Code[1];

        self.assertEqual(fmtStr, code["fmtString"])
        self.assertEqual("testFile.cc", code["filename"])
        self.assertEqual(100, code["linenum"])

        # Now check the generated functions
        expectedRecordCode = \
"""void %s(unsigned int arg0, const char* arg1, double arg2, const char* arg3) {
	int str1Len = strlen(arg1) + 1, str3Len = strlen(arg3) + 1;
	int maxSizeOfArgs = sizeof(arg0) + sizeof(arg2) + str1Len + str3Len;
	int reqSize = sizeof(PerfUtils::FastLogger::RecordMetadata) + maxSizeOfArgs;
	int maxSizeOfCompressedArgs = maxSizeOfArgs + 1;
	char *buffer = PerfUtils::FastLogger::alloc(reqSize);

	if (buffer == nullptr)
		return;

	PerfUtils::FastLogger::recordMetadata(buffer, 1, maxSizeOfCompressedArgs);

	PerfUtils::FastLogger::recordPrimitive(buffer, arg0);
	PerfUtils::FastLogger::recordPrimitive(buffer, arg2);

	memcpy(buffer, arg1, str1Len); buffer += str1Len;
	memcpy(buffer, arg3, str3Len); buffer += str3Len;
}
""" % expectedFnName

        expectedCompressCode = \
"""inline void
compressArgs1(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {
	PerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(out);
	out += 1;
	unsigned int arg0 = *reinterpret_cast<unsigned int*>(in); in += sizeof(unsigned int);
	double arg2 = *reinterpret_cast<double*>(in); in += sizeof(double);

	nib[0].first = PerfUtils::pack(out, arg0);
	nib[0].second = PerfUtils::pack(out, arg2);

	int stringBytes = maxSizeOfCompressedArgs - (sizeof(arg0) + sizeof(arg2)) - 1;
	memcpy(out, in, stringBytes);
	in += stringBytes;
	out += stringBytes;
}
"""
        expectedDecompressCode = \
"""inline void
decompressPrintArg1(char* &in) {
	PerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(in);
	in += 1;

	unsigned int arg0 = PerfUtils::unpack<unsigned int>(out, nib[0].first);
	double arg2 = PerfUtils::unpack<double>(out, nib[0].second);

	const char* arg1 = in; in += strlen(arg1) + 1;
	const char* arg3 = in; in += strlen(arg3) + 1;

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
        args = []

        # TODO(syang0) Currently we do not differenciate on files, only strings.
        # In the future, please add multi-file support that way the correct
        # filename and line number are saved

        # Original
        fg.generateLogFunctions("A", args, "mar.cc", "mar.cc", 293)

        # Different log
        fg.generateLogFunctions("B", args, "mar.cc", "mar.cc", 293)

        # Same log + file, different location
        fg.generateLogFunctions("A", args, "mar.cc",  "mar.cc", 200)

        # smae log, diff file + location
        fg.generateLogFunctions("A", args, "s.cc", 100)

        self.assertEqual(2, len(fg.fmtStr2Id))
        self.assertEqual(3, len(fg.fmtId2Code))

        self.assertEqual(1 , fg.fmtStr2Id["A"])
        self.assertEqual(2 , fg.fmtStr2Id["B"])
        self.assertEqual(0, len(fg.unusedIds))

        self.assertEqual("A", fg.fmtId2Code[1]["fmtString"])
        self.assertEqual("B", fg.fmtId2Code[2]["fmtString"])

    def test_getRecordFunctionDefinitionsFor(self):

        emptyRec = \
"""void __syang0__fl__%d() {
	int maxSizeOfArgs = 0 + 0;
	int reqSize = sizeof(PerfUtils::FastLogger::RecordMetadata) + maxSizeOfArgs;
	int maxSizeOfCompressedArgs = maxSizeOfArgs + 0;
	char *buffer = PerfUtils::FastLogger::alloc(reqSize);

	if (buffer == nullptr)
		return;

	PerfUtils::FastLogger::recordMetadata(buffer, %d, maxSizeOfCompressedArgs);
}
"""
        fg = FunctionGenerator()
        args = []

        fg.generateLogFunctions("A", args, "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", args, "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("C", args, "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D", args, "s.cc", "s.cc", 100)

        self.assertEqual(3, len(fg.getRecordFunctionDefinitionsFor("mar.cc")))
        self.assertEqual(1, len(fg.getRecordFunctionDefinitionsFor("s.cc")))
        self.assertEqual(0, len(fg.getRecordFunctionDefinitionsFor("asdf.cc")))

        funcs = fg.getRecordFunctionDefinitionsFor("mar.cc");
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
        args = []
        args1 = [
            Argument("someVariable", FileRange(FilePosition(0,0), FilePosition(1,0)))
        ]

        fg.generateLogFunctions("A", args, "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", args, "mar.cc", "mar.cc", 294)
        fg.generateLogFunctions("C", args, "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D %d", args1, "s.cc", "s.cc", 100)

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
        args = []
        args1 = [
            Argument("someVariable", FileRange(FilePosition(0,0), FilePosition(1,0)))
        ]

        fg.generateLogFunctions("A", args, "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", args, "mar.cc", "mar.cc", 294)
        fg.generateLogFunctions("C", args, "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D %d", args1, "s.cc", "s.cc", 100)

        fg.unusedIds.append(10)

        expectedContents = """#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include "FastLogger.h"

// Compression Code
inline void
compressArgs1(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {

}

inline void
compressArgs2(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {

}

inline void
compressArgs3(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {

}

inline void
compressArgs4(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {
	PerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(out);
	out += 1;
	int arg0 = *reinterpret_cast<int*>(in); in += sizeof(int);

	nib[0].first = PerfUtils::pack(out, arg0);
}

// Decompression Code
inline void
decompressPrintArg1(char* &in) {
	PerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(in);
	in += 0;



	const char *fmtString = "A";
	const char *filename = "mar.cc";
	const int linenum = 293;

	printf(fmtString);
}

inline void
decompressPrintArg2(char* &in) {
	PerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(in);
	in += 0;



	const char *fmtString = "B";
	const char *filename = "mar.cc";
	const int linenum = 294;

	printf(fmtString);
}

inline void
decompressPrintArg3(char* &in) {
	PerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(in);
	in += 0;



	const char *fmtString = "C";
	const char *filename = "mar.cc";
	const int linenum = 200;

	printf(fmtString);
}

inline void
decompressPrintArg4(char* &in) {
	PerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(in);
	in += 1;

	int arg0 = PerfUtils::unpack<int>(out, nib[0].first);


	const char *fmtString = "D %d";
	const char *filename = "s.cc";
	const int linenum = 100;

	printf(fmtString, arg0);
}

void (*compressFnArray[4])(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {
	compressArgs1,
	compressArgs2,
	compressArgs3,
	compressArgs4
};

void (*decompressAndPrintFnArray[4])(char* &in) {
	decompressPrintArg1,
	decompressPrintArg2,
	decompressPrintArg3,
	decompressPrintArg4
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

