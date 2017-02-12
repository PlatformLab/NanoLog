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

import filecmp
import unittest
import os
from parser import *

from FunctionGenerator import *

class PreprocesorTestCase(unittest.TestCase):
    #def setUp(self):
    #    self.foo = UnitTests()
    #

    #def tearDown(self):
    #    self.foo.dispose()
    #    self.foo = None

    def test_extractCString(self):

        self.assertEqual("This is a simple line",
                         extractCString(""" "This is a simple line" """))

        string = " Here's a more complicated one\r\n on 2 lines"
        self.assertEqual(string, extractCString("\"%s\"" % string))

        string = " \\\"Escaped quotes!\\\" Noooo!!!"
        self.assertEqual(string, extractCString("\"%s\"" % string))

        string = "\\\\n Escaped new line"
        self.assertEqual(string, extractCString("\"%s\"" % string))

        string = "Multi line one \r\n that \n has \\\" an escaped quote"
        self.assertEqual(string, extractCString("\"%s\"" % string))

        string = "Format specifiers and special characters %s %d '#(*&(*79234'"
        self.assertEqual(string, extractCString("\"%s\"" % string))

        # Now work with C string concat, notice the style changed
        string = "    \" This is the real string \"\r\n \"and it's separated \""
        self.assertEqual(" This is the real string and it's separated ",
                         extractCString(string))

        string = "\r\n\r\n\r\n \"Weird\"\r\n\r\n "
        self.assertEqual("Weird", extractCString(string))

        string = """
"This is a string"

    " that's a bit" " more"
            " representative\r\n"

"""
        self.assertEqual(
            """This is a string that's a bit more representative\r\n""",
            extractCString(string))

        # Empty string
        string = "\"\""
        self.assertEqual("", extractCString(string))

        # Finally, error cases where the string is malformed
        string = "\""
        self.assertIsNone(extractCString(string))

        string = "\" One good string\" But another bad"
        self.assertIsNone(extractCString(string))

        string = "\" One\"   \" and a half good strings"
        self.assertIsNone(extractCString(string))

        string = " \"Test\" extraneous chars"
        self.assertIsNone(extractCString(string))

        string = "\r\n\r\n\r\n \"Extra quote there =>\"\r\n\r\n \""
        self.assertIsNone(extractCString(string))

    def test_parseArgumentStartingAt_quotes(self):
        lines = ["LOG((++i), \"\\\"Hello\\\"\")"]

        arg = parseArgumentStartingAt(lines, FilePosition(0, 4))
        self.assertEqual(Argument("(++i)",
                                FilePosition(0, 4), FilePosition(0, 9)),
                         arg)

        arg = parseArgumentStartingAt(lines, FilePosition(0, 10))
        self.assertEqual(Argument(' "\\"Hello\\""',
                                FilePosition(0, 10), FilePosition(0, 22)),
                         arg)

        # Misaligned quotes
        self.assertIsNone(parseArgumentStartingAt(["LOG(\"Test);"],
                                                  FilePosition(0, 4)))

        self.assertIsNone(parseArgumentStartingAt(["LOG(\"\"Test\");"],
                                                  FilePosition(0, 4)))

        # Escaped, but misaligned quote
        arg = parseArgumentStartingAt(["LOG(\\\"Test);"], FilePosition(0, 4))
        self.assertEqual("\\\"Test", arg.source)
        self.assertEqual(FilePosition(0, 10), arg.endPos)

    def test_parseArgumentStartingAt_brackets(self):
        startPos = FilePosition(0, 4)

        # Okay
        arg = parseArgumentStartingAt(["LOG({});"], startPos)
        self.assertEqual("{}", arg.source)

        arg = parseArgumentStartingAt(["LOG({((([[9]])))}, {});"], startPos)
        self.assertEqual("{((([[9]])))}", arg.source)

        # Malformed
        self.assertIsNone(parseArgumentStartingAt(["LOG({);"], startPos))
        self.assertIsNone(parseArgumentStartingAt(["LOG({{);"], startPos))
        self.assertIsNone(parseArgumentStartingAt(["LOG({}});"], startPos))
        self.assertIsNone(parseArgumentStartingAt(["LOG(});"], startPos))

        self.assertIsNone(parseArgumentStartingAt(["LOG(();"], startPos))
        self.assertIsNone(parseArgumentStartingAt(["LOG([]]);"], startPos))
        self.assertIsNone(parseArgumentStartingAt(["LOG(4"], startPos))

        # Okay because they're misaligned, but in quotes
        lines = ["LOG(Hello + \"{{{[[[[)]({\");"]
        arg = parseArgumentStartingAt(lines, FilePosition(0, 4))
        self.assertEqual('Hello + \"{{{[[[[)]({\"', arg.source)
        self.assertEqual(FilePosition(0, 25), arg.endPos)

    def test_parseLogStatement_nestedAndMultilined(self):
        lines = """NANO_LOG("Format \\\"string\\\" %d %d %s %0.2lf",
            calculate(a, b, c) * 22/3,
            arrayDereference[indexArray[(3+5)/var]],
{"constructor", {"nested", 4}}.getId( alpha ),
            0923.4918 * 22 - 1)
               ;
        """
        lines = lines.strip().split("\n")

        stmt = parseLogStatement(lines, FilePosition(0, 0))
        self.assertEqual(FilePosition(0, 8), stmt['openParenPos'])
        self.assertEqual(FilePosition(4, 30), stmt['closeParenPos'])
        self.assertEqual(FilePosition(5, 15), stmt['semiColonPos'])

        args = stmt['arguments']
        self.assertEqual(5, len(args))

        self.assertEqual("\"Format \\\"string\\\" %d %d %s %0.2lf\"",
                            args[0].source)
        self.assertEqual(FilePosition(0, 9), args[0].startPos)
        self.assertEqual(FilePosition(0, 44), args[0].endPos)

        self.assertEqual("            calculate(a, b, c) * 22/3",
                            args[1].source)
        self.assertEqual(FilePosition(0, 45), args[1].startPos)
        self.assertEqual(FilePosition(1, 37), args[1].endPos)

        self.assertEqual("            arrayDereference[indexArray[(3+5)/var]]",
                         args[2].source)
        self.assertEqual(FilePosition(1, 38), args[2].startPos)
        self.assertEqual(FilePosition(2, 51), args[2].endPos)

        self.assertEqual("{\"constructor\", {\"nested\", 4}}.getId( alpha )",
                         args[3].source)
        self.assertEqual(FilePosition(2, 52), args[3].startPos)
        self.assertEqual(FilePosition(3, 45), args[3].endPos)

        self.assertEqual("            0923.4918 * 22 - 1", args[4].source)
        self.assertEqual(FilePosition(3, 46), args[4].startPos)
        self.assertEqual(FilePosition(4, 30), args[4].endPos)

    def test_parseLogStatement_terribleFormatting(self):
        # Nested invocations and terrible formatting
        lines = """NANO_LOG("Format String %d %0.2lf %s", calculate(a, b),
                100.09/
                    variable[22]
        ,   "const string" "that's actually"
        "split"
        )       ;
        """

        lines = lines.strip().split("\n")

        stmt = parseLogStatement(lines, FilePosition(0, 0))
        self.assertEqual(FilePosition(0, 8), stmt['openParenPos'])
        self.assertEqual(FilePosition(5, 8), stmt['closeParenPos'])
        self.assertEqual(FilePosition(5, 16), stmt['semiColonPos'])

        args = stmt['arguments']
        self.assertEqual(4, len(args))

        self.assertEqual("\"Format String %d %0.2lf %s\"", args[0].source)
        self.assertEqual(FilePosition(0, 37), args[0].endPos)

        arg = parseArgumentStartingAt(lines, FilePosition(0, 33))
        self.assertEqual(" calculate(a, b)", args[1].source)
        self.assertEqual(FilePosition(0, 54), args[1].endPos)

        arg = parseArgumentStartingAt(lines, FilePosition(0, 50))
        self.assertEqual('100.09/                    variable[22]',
                            args[2].source.strip())
        self.assertEqual(FilePosition(3, 8), args[2].endPos)

        arg = parseArgumentStartingAt(lines, FilePosition(3, 9))
        self.assertEqual('"const string" "that\'s actually"        "split"',
                            args[3].source.strip())
        self.assertEqual(FilePosition(5, 8), args[3].endPos)

    def test_parseLogStatement_missingSemicolonOrParen(self):
        with self.assertRaisesRegexp(ValueError, "Expected ';'"):
            parseLogStatement(["NANO_LOG(\"1\") }"], FilePosition(0, 0))

        with self.assertRaisesRegexp(ValueError, "Expected ';'"):
            parseLogStatement(["NANO_LOG(\"1\")"], FilePosition(0, 0))

        with self.assertRaisesRegexp(ValueError, "Cannot find end"):
            parseLogStatement(["NANO_LOG(\"1\""], FilePosition(0, 0))

    def test_peekNextMeaningfulChar(self):
        lines = ["Hello ",
                 "    \t\t\n 5",
                 ""]

        peek = peekNextMeaningfulChar(lines, FilePosition(0, 0))
        self.assertEqual('H', peek[0])
        self.assertEqual(FilePosition(0, 0), peek[1])

        peek = peekNextMeaningfulChar(lines, FilePosition(0, 5))
        self.assertEqual('5', peek[0])
        self.assertEqual(FilePosition(1, 8), peek[1])

        # Invalid file position
        peek = peekNextMeaningfulChar(lines, FilePosition(1, 9))
        self.assertIsNone(peek)

        peek = peekNextMeaningfulChar(lines, FilePosition(5, 0))
        self.assertIsNone(peek)

    def test_processFile_noInjectionIfNoCode(self):
        src = """main() {}"""

        with open("test", "w") as testFile:
            testFile.write(src)

        processFile("test", "test.map")
        with open("testi", 'r') as iFile:
            self.assertEqual(src, iFile.read())

        os.remove("test")
        os.remove("testi")
        os.remove("test.map")

    ###### NOTE #######
    # parser.py:processFile is left for an integration test since it an entire
    # C++ source file and outputs generated code.
    ####################

class FunctionGeneratorTestCase(unittest.TestCase):
    def test_parseTypesInFmtString_noReplacements(self):
        fmtString = """~S!@#$^&*()_+1234567890qwertyu
                              iopasdfghjkl;zxcv  bnm,\\\\r\\n
                              %%ud \%lf osdif<>":L:];
                      """

        # No replacements should be performed because all % are escaped
        self.assertEqual(parseTypesInFmtString(fmtString), [])

        fmtString = ""
        self.assertEqual(parseTypesInFmtString(fmtString), [])

        fmtString = "Hello"
        self.assertEqual(parseTypesInFmtString(fmtString), [])

        fmtString = "\% %%ud"
        self.assertEqual(parseTypesInFmtString(fmtString), [])

        # Invalid types
        fmtString = "%S %qosiwieud"
        with self.assertRaisesRegexp(ValueError,
                                     "Unrecognized Format Specifier"):
            parseTypesInFmtString(fmtString)

    def test_parseTypesInFmtString_escapes(self):
        # Tricky
        fmtString = "\\%s %%p %%%s \\\\%s"
        self.assertEqual(parseTypesInFmtString(fmtString),
                         ["const char*", "const char*"])

    def test_parseTypesInFmtString_charTypes(self):
        self.assertEqual(parseTypesInFmtString("%hhd %hhi"),
                         ["signed char", "signed char"])
        self.assertEqual(parseTypesInFmtString(" %d"),
                         ["int"])

        with self.assertRaisesRegexp(ValueError, "not supported"):
            parseTypesInFmtString("%hhn")

        with self.assertRaisesRegexp(ValueError, "Unrecognized Format"):
            parseTypesInFmtString("%hhj")

    def test_parseTypesInFmtString_jzt(self):
        self.assertEqual(parseTypesInFmtString("%jd %ji"),
                         ["intmax_t", "intmax_t"])

        self.assertEqual(parseTypesInFmtString("%ju %jo %jx %jX"),
                         ["uintmax_t", "uintmax_t", "uintmax_t", "uintmax_t"])

        self.assertEqual(parseTypesInFmtString("%zu %zd %tu %td"),
                         ["size_t", "size_t", 'ptrdiff_t', "ptrdiff_t"])

        with self.assertRaisesRegexp(ValueError, "specifier not supported"):
            parseTypesInFmtString("%jn %zn zn %tn")

        # Unexpected characters!
        with self.assertRaisesRegexp(ValueError,
                                     "Unrecognized Format Specifier: \"%z"):
            parseTypesInFmtString("%z\r\n")

        with self.assertRaisesRegexp(ValueError,
                                     "Unrecognized Format Specifier: \"%j"):
            parseTypesInFmtString("%j\r\n")

        with self.assertRaisesRegexp(ValueError,
                                     "Unrecognized Format Specifier: \"%t"):
            parseTypesInFmtString("%t\r\n")

    def test_parseTypesInFmtString_doubleTypes(self):
        self.assertEqual(parseTypesInFmtString(
            "%12.0f %12.3F %e %55.3E %-10.5g %G %a %A"),
            ["double", "double", "double", "double",
             "double", "double", "double", "double"])

        self.assertEqual(parseTypesInFmtString(
            "%12.0Lf %12.3LF %Le %55.3LE %-10.5Lg %LG %La %LA"),
            ["long double", "long double", "long double", "long double",
             "long double", "long double", "long double", "long double"])

        # Check that random modifiers don't change the type
        self.assertEqual(parseTypesInFmtString("%lf %llf"),
                         ["double", "double"])

        # Check for errors
        with self.assertRaisesRegexp(ValueError, "Invalid arguments for"):
            parseTypesInFmtString("%Lu")

    def test_parseTypesInFmtString_basicIntegerTypes(self):
        self.assertEqual(parseTypesInFmtString("%d %i"), ["int", "int"])
        self.assertEqual(parseTypesInFmtString("%u %o"),
                         ["unsigned int", "unsigned int"])
        self.assertEqual(parseTypesInFmtString("%x %X"),
                         ["unsigned int", "unsigned int"])

        self.assertEqual(parseTypesInFmtString("%c %s %p"),
                         ["int", "const char*", "void*"])

        with self.assertRaisesRegexp(ValueError, "specifier not supported"):
            parseTypesInFmtString("%n")

    def test_parseTypesInFmtString_cspn(self):
        self.assertEqual(parseTypesInFmtString("%c %s %p"),
                         ["int", "const char*", "void*"])

        self.assertEqual(parseTypesInFmtString("%ls %lc"),
                         ["const wchar_t*", "wint_t"])

        with self.assertRaisesRegexp(ValueError, "not supported"):
            parseTypesInFmtString("%n")

    def test_lengthModifiers(self):
        self.assertEqual(parseTypesInFmtString("%hhd %hd %ld %lld %jd %zd %td"),
                         ["signed char", "short int", "long int",
                          "long long int", "intmax_t", "size_t", "ptrdiff_t"])

        self.assertEqual(parseTypesInFmtString("%hhu %hu %lu %llu %ju %zu %tu"),
                         ["unsigned char", "unsigned short int",
                          "unsigned long int", 'unsigned long long int',
                          "uintmax_t", "size_t", "ptrdiff_t"])

        with self.assertRaisesRegexp(ValueError, "specifier not supported"):
            parseTypesInFmtString("%hhn %hn %ln %lln %jn %zn %tn")

    def test_generateLogFunctions_empty(self):
        self.maxDiff = None
        fg = FunctionGenerator()

        ret = fg.generateLogFunctions("Empty Print", "gar.cc", "mar.cc", 293)

        logId = generateLogIdStr("Empty Print", "mar.cc", 293)
        expectedFnName = "__syang0__fl" + logId
        expectedResult = ("void " + expectedFnName + "(const char* fmtStr )",
                                expectedFnName)
        self.assertEqual(expectedResult, ret)

        code = fg.logId2Code[logId]
        self.assertEqual("Empty Print", code['fmtString'])
        self.assertEqual("mar.cc", code["filename"])
        self.assertEqual(293, code["linenum"])
        self.assertEqual("gar.cc", code["compilationUnit"])

        # Note: I used to test for the generated code here, but it doesn't
        # really make sense since it's checked in the integration code and in
        # another test case later on.

    def test_generateLogFunctions(self):
        self.maxDiff = None
        fg = FunctionGenerator()

        fmtStr = "Hello World! %u %s %lf %s"
        ret = fg.generateLogFunctions(fmtStr, "testFile.cc", "testFile.cc", 100)
        logId = generateLogIdStr(fmtStr, "testFile.cc", 100)
        expectedFnName = "__syang0__fl" + logId

        expectedResult = ("void " + expectedFnName + "(const char* fmtStr , "
                            "unsigned int arg0, const char* arg1, double arg2, "
                            "const char* arg3)",
                            expectedFnName)

        self.assertEqual(expectedResult, ret)

        self.assertEqual(2, len(fg.logId2Code))
        code = fg.logId2Code[logId]

        self.assertEqual(fmtStr, code["fmtString"])
        self.assertEqual("testFile.cc", code["filename"])
        self.assertEqual(100, code["linenum"])

        # Note: I used to test for the generated code here, but it doesn't
        # really make sense since it's checked in the integration code and in
        # another test case later on.

    def test_generateLogFunctions_combinationAndOverwrite(self):
        fg = FunctionGenerator()

        # TODO(syang0) Currently we do not differenciate on files, only strings.
        # In the future, please add multi-file support that way the correct
        # filename and line number are saved

        # Original
        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)

        # Different log
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 293)

        # Same log + file, different location
        fg.generateLogFunctions("A", "mar.cc",  "mar.cc", 200)

        # same log, diff file + location
        fg.generateLogFunctions("A", "s.cc", "s.cc", 100)

        # same log, diff compilation unit, but same originating file
        fg.generateLogFunctions("A", "s.cc", "mar.cc", 293)

        ids = fg.logId2Code.keys()
        self.assertEqual(['__A__mar46cc__293__',
                          '__A__s46cc__100__',
                          '__B__mar46cc__293__',
                          '__A__mar46cc__200__',
                          '__INVALID__INVALID__INVALID__'], ids)

    def test_getRecordFunctionDefinitionsFor(self):
        self.maxDiff = None

        emptyRec = \
"""
inline void __syang0__fl{logId}(const char* fmtStr ) {{
    extern const uint32_t __fmtId{logId};

    ;
    size_t allocSize =   sizeof(BufferUtils::UncompressedLogEntry);
    BufferUtils::UncompressedLogEntry *re = reinterpret_cast<BufferUtils::UncompressedLogEntry*>(NanoLog::__internal_reserveAlloc(allocSize));

    re->fmtId = __fmtId{logId};
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);

    char *buffer = re->argData;

    // Record the non-string arguments
    %s

    // Record the strings (if any) at the end of the entry
    %s

    // Make the entry visible
    NanoLog::__internal_finishAlloc(allocSize);
}}
""" % ("", "")
        fg = FunctionGenerator()

        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("C", "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D", "s.cc", "mar.cc", 100)

        self.assertEqual(3, len(fg.getRecordFunctionDefinitionsFor("mar.cc")))
        self.assertEqual(1, len(fg.getRecordFunctionDefinitionsFor("s.cc")))
        self.assertEqual(0, len(fg.getRecordFunctionDefinitionsFor("asdf.cc")))

        funcs = fg.getRecordFunctionDefinitionsFor("mar.cc")

        logId = generateLogIdStr("A", "mar.cc", 293)
        self.assertMultiLineEqual(emptyRec.format(logId=logId), funcs[0])

        logId = generateLogIdStr("C", "mar.cc", 200)
        self.assertMultiLineEqual(emptyRec.format(logId=logId), funcs[1])

        logId = generateLogIdStr("B", "mar.cc", 293)
        self.assertMultiLineEqual(emptyRec.format(logId=logId), funcs[2])

        logId = generateLogIdStr("D", "mar.cc", 100)
        self.assertMultiLineEqual(emptyRec.format(logId=logId),
                                fg.getRecordFunctionDefinitionsFor("s.cc")[0])

    def test_outputMappingFile(self):
        fg = FunctionGenerator()

        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 294)
        fg.generateLogFunctions("C", "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D %d", "s.cc", "s.cc", 100)

        # Test serialization and deserialization
        fg.outputMappingFile("test.json")
        with open("test.json") as dataFile:
            data = json.load(dataFile)
            self.assertEqual(fg.argLists2Cnt, data.get('argLists2Cnt'))
            self.assertEqual(fg.logId2Code, data.get('logId2Code'))

        os.remove("test.json")


    def test_outputMappingFile_withFolders(self):
        fg = FunctionGenerator()

        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 294)
        fg.generateLogFunctions("C", "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D %d", "s.cc", "s.cc", 100)

        # Test what happens when the directory does not exist
        try:
            os.remove("testFolder/test.json")
            os.removedirs("testFolder")
        except:
            pass

        fg.outputMappingFile("testFolder/test.json")
        with open("testFolder/test.json") as dataFile:
            data = json.load(dataFile)
            self.assertEqual(fg.argLists2Cnt, data.get('argLists2Cnt'))
            self.assertEqual(fg.logId2Code, data.get('logId2Code'))


        os.remove("testFolder/test.json")
        os.removedirs("testFolder")

    def test_outputCompilationFiles(self):
        self.maxDiff = None
        fg = FunctionGenerator()

        fg.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg.generateLogFunctions("B", "mar.cc", "mar.cc", 294)
        fg.generateLogFunctions("C", "mar.cc", "mar.cc", 200)
        fg.generateLogFunctions("D %d", "s.cc", "s.cc", 100)

        fg.outputMappingFile("map1.map")

        # Also test the merging
        fg2 = FunctionGenerator()
        fg2.generateLogFunctions("A", "mar.cc", "mar.cc", 293)
        fg2.generateLogFunctions("A", "mar.cc", "mar.h", 1)
        fg2.generateLogFunctions("E", "del.cc", "del.cc", 199)

        fg2.outputMappingFile("map2.map")

        # Merge the two map files
        FunctionGenerator.outputCompilationFiles("test.h",
                                                 ["map1.map", "map2.map"])

        self.assertTrue(filecmp.cmp("test.h",
                                "unitTestData/test_outputCompilationFiles.h"))

        os.remove("map1.map")
        os.remove("map2.map")
        os.remove("test.h")

if __name__ == '__main__':
    unittest.main()

