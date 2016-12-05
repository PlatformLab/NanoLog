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

# This script encapsulates all the logic to parse printf-like format strings
# and generate C++ code/files that perform the record/compression/decompression
# routines for those log messages in the Fast Logger system.

import sys
import json
import os.path

# Various globals mapping symbolic names to the object/function names in
# the supporting C++ library. This is done so that changes in namespaces don't
# result in large sweeping changes of this file.
RECORD_ENTRY = "BufferUtils::RecordEntry"
NIBBLE_OBJ = "BufferUtils::TwoNibbles"

RECORD_HEADER_FN = "BufferUtils::recordMetadata"
RECORD_PRIMITIVE_FN = "BufferUtils::recordPrimitive"

ALLOC_FN = "PerfUtils::FastLogger::reserveAlloc"
FINISH_ALLOC_FN = "PerfUtils::FastLogger::finishAlloc"

PACK_FN = "BufferUtils::pack"
UNPACK_FN = "BufferUtils::unpack"

# This class assigns unique identifiers to unique printf-like format strings,
# generates C++ code to record/compress/decompress the printf-like statements
# in the FastLogger system, and maintains these mappings between multiple
# invocations of the preprocessor system.
#
# This class is intended to be used in two stages. In the first stage, the
# preprocessor component shall identify all the log statements in the user
# files and pass them to the FunctionGenerator, which will attempt to collapse
# similar log statements and return code needed to record the dynamic
# information of the log statement for the preprocessor to inject. The first
# stage can be done piece-meal since the FunctionGenerator is able to persist
# its state between invocations. This is useful for both parallel* (*not
# supported yet) and partial re-builds.
#
# In the second stage, after all the user files are processed and compiled,
# this script can be invoked as a stand-alone python file to output the
# additional C++ code required for the compression and decompression of
# the dynamic information. This stage should be the last step before
# compiling the runtime library and linking it with the user code.
class FunctionGenerator(object):

    # Constructor for FunctionGenerator. An optional parameter can be provided
    # to restore the state of the previous FunctionGenerator.
    def __init__(self, mappingFile=None):

        # Every format string that is passed to the FunctionGenerator is
        # assigned an integer id. The assignments are dense and the same
        # format string (by value) will result in the same id.
        self.fmtStr2Id = { }

        # List of format integer ids that become deallocated due to the format
        # string no longer existing in the C++ source files.
        self.unusedIds = []

        # Map of format id to the various metadata associated with with
        # the original format string. Note that it's prepopulated
        # for format id 0 both because it's invalid and to demonstrate
        # the structure of the metadata.

        # TODO(syang0) This should be split into data.json and index the
        # functions by argument types.
        self.fmtId2Code = [
            {
                "fmtString"        : "INVALID",
                "filename"         : "INVALID.cc",
                "linenum"          : "-1",
                "compilationUnit"  : "INVALID.cc",
                "recordFnDef"      : "invalidRecord(int arg0) { ... }",
                "compressFnDef"    : "invalidCompress(...) { ....}",
                "decompressFnDef"  : "invalidDecompress(...) { ... }"
            }
        ]

        # Debug data structure that keeps track of the number of non-unique
        # parameter combinations.
        self.argLists2Cnt = {}

        # Load the previous mappings
        if mappingFile and os.path.isfile(mappingFile):
            with open(mappingFile) as json_file:
                loaded = json.load(json_file)

                self.fmtId2Code = loaded.get("fmtId2Code", [])
                self.unusedIds = loaded.get("unusedIds", [])
                self.fmtStr2Id = loaded.get("fmtStr2Id", {})
                self.argLists2Cnt = loaded.get("argLists2Cnt", {})

    # Output the internal state of the FunctionGenerator to a JSON file that
    # can later be used in the constructor to reconstitute a new
    # FunctionGenerator
    #
    # \param filename       - file to persist the state to
    def outputMappingFile(self, filename):
        with open(filename, 'w') as json_file:
            outputJSON = {
                "fmtId2Code":self.fmtId2Code,
                "unusedIds":self.unusedIds,
                "fmtStr2Id":self.fmtStr2Id,
                "argLists2Cnt":self.argLists2Cnt
            }

            json_file.write(json.dumps(outputJSON, sort_keys=True,
                                            indent=4, separators=(',', ': ')))

    # Output the C++ header needed by the runtime library to perform the
    # compression and decompression routines. The file shall contain the
    # following data structures/code:
    #       - A function array mapping format id's to compression functions
    #       - A function array mapping format id's to decompression functions
    #       - The supporting compression/decompression functions
    #       - The record function that should have been injected (for debug)
    #       - A char* array mapping format id's to format strings (for debug)
    #
    # \param filename - The C++ file to emit
    def outputCompilationFiles(self, filename="BufferStuffer.h"):
        with open(filename, 'w') as oFile:
            oFile.write("#ifndef BUFFER_STUFFER\n")
            oFile.write("#define BUFFER_STUFFER\n\n")

            oFile.write("#include \"FastLogger.h\"\n")
            oFile.write("#include \"Packer.h\"\n\n")

            oFile.write("#include <fstream>     // for decompression\n")
            oFile.write("#include <string>\n\n")

            # Some of the functions/variables output below are for debugging
            # purposes only (i.e. they're not used in their current form), so
            # we squash all the gcc complaints about unused functions.
            oFile.write("#pragma GCC diagnostic push\n")
            oFile.write("#pragma GCC diagnostic ignored \"-Wunused-function\"\n")
            oFile.write("#pragma GCC diagnostic ignored \"-Wunused-variable\"\n\n")

            # Output the record/compress/decompress functions in a separate
            # namespace
            oFile.write("// Record code in an empty namespace(for debugging)\n")
            oFile.write("namespace {\n")
            for fmtId, code in enumerate(self.fmtId2Code[1:]):
                # There seems to be a g++ bug with #pragma disabling Wunused-function
                # that requires an inline infront of the recordFn to supress it.
                oFile.write("inline " + code["recordFnDef"] + "\n")

                oFile.write(code["compressFnDef"] + "\n")
                oFile.write(code["decompressFnDef"] + "\n")
            oFile.write("} // end empty namespace\n\n")

            numFns = len(self.fmtId2Code)

            # Output arrays that we can address into
            # Note that the first element is nullptr since fmtId=0 is invalid
            compressFnNameArray = ["\tnullptr"]
            decompressFnNameArray = ["\tnullptr"]

            for i in range(1, len(self.fmtId2Code)):
                compressFnNameArray.append("\tcompressArgs%d" % i)
                decompressFnNameArray.append("\tdecompressPrintArg%d" % i)

            oFile.write("ssize_t (*compressFnArray[%d])(%s *re, char* out)"
                            " {\n%s\n};\n\n" % (numFns, RECORD_ENTRY, ",\n".
                                                    join(compressFnNameArray)))

            oFile.write("void (*decompressAndPrintFnArray[%d])("
                        "std::ifstream &in) {\n%s\n};\n\n" %
                                (numFns, ",\n".join(decompressFnNameArray)))

            # Output a list of the fmtStrings for debugging purposes
            allFmtStrings = []
            for i in range(1, len(self.fmtId2Code)):
                allFmtStrings.append("\t\"%s\"" %
                                                self.fmtId2Code[i]["fmtString"])

            oFile.write("// Format Id to original Format String\n")
            oFile.write("const char* fmtId2Str[%d] = {\n" \
                                                        % len(allFmtStrings))
            oFile.write(",\n".join(allFmtStrings))
            oFile.write("\n};\n\n")

            oFile.write("// Pop -Wunused-function\n")
            oFile.write("#pragma GCC diagnostic pop")

            oFile.write("\n\n#endif /* BUFFER_STUFFER */\n")

    # Given a compilation unit via filename, remove all the mappings
    # related to that file. This is useful for partial recompilations
    # where log statements may have shifted/been deleted/modified, so it's
    # better to start from a clean plate for each compilation unit.
    #
    # \param compilationUnit - filename of compilation unit to clear out
    def clearLogFunctionsForCompilationUnit(self, compilationUnit=None):
        if not compilationUnit:
            return

        for fmtId, code in self.fmtId2Code.iteritems():
            if code["compilationUnit"] == compilationUnit:
                self.fmtStr2Id.pop(value["fmtString"], None)
                self.fmtId2Code[fmtId] = None
                self.unusedIds.append(fmtId)

    # Given a compilation unit via filename, return all the record functions
    # that were generated for that file.
    #
    # \param compilationUnit - filename of compilation unit
    def getRecordFunctionDefinitionsFor(self, compilationUnit):
        recordFns = []

        for code in self.fmtId2Code:
            if code["compilationUnit"] == compilationUnit:
                recordFns.append(code["recordFnDef"])

        return recordFns

    # Given the format string and the arguments to a log statement, generate
    # the code required to record the dynamic information in the
    # FastLogging system.
    #
    # Note that this function will only return the record function declaration
    # and invocation. The defintion for a compilationUnit can be gotten via
    # getRecordFunctionDefinitionsFor(compilationUnit)
    #
    # \param fmtString - C++ printf-like format string for the log message
    #                       (note "%n"  is not supported)
    # \param compilationUnit - cc file being preprocessed/compiled
    # \param filename   - the original location of the log statement before
    #                       it was inlined into the compilationUnit by the
    #                       C++ pre-processor
    # \param linenum    - The line in the compilationUnit where the log
    #                       statement was found
    #
    # \return - tuple of the record function declaration and invocation.
    def generateLogFunctions(self, fmtString, compilationName ="default",
                                filename="unknownFile", linenum=-1):

        argTypes = parseTypesInFmtString(fmtString)
        parameterDeclarations = ["%s arg%d" % (type, idx)
                                    for idx, type in enumerate(argTypes)]
        parameterDeclarationString = ", ".join(parameterDeclarations)

        # TODO(syang0) currently there is a bug here whereby different
        # compilation units with the same format string will conflict.
        # This is not fixed at the moment since we may switch to an
        # alternate scheme of tracking log statements whereby the record
        # functions are shared and the fmtId is extern-ed and will
        # unique identify compilation unit + line number.

        # Check to see if code has been generated before
        prevCodeExists = False
        if fmtString in self.fmtStr2Id:
            fmtId = self.fmtStr2Id[fmtString]
            prevCodeExists = True
        elif self.unusedIds:
            fmtId = self.usedIds.pop()
            self.fmtStr2Id[fmtString] = fmtId
        else:
            fmtId = len(self.fmtStr2Id) + 1
            self.fmtStr2Id[fmtString] = fmtId

        recordFnName = generateFunctionNameFromFmtId(fmtId)
        if parameterDeclarationString:
            recordDeclaration = "void %s(const char* fmtStr, %s)" % (
                                    recordFnName, parameterDeclarationString)
        else:
            recordDeclaration = "void %s(const char* fmtStr)" % recordFnName
        recordInvocation = "%s" % (recordFnName)

        if parameterDeclarationString in self.argLists2Cnt:
            self.argLists2Cnt[parameterDeclarationString] += 1
        else:
            self.argLists2Cnt[parameterDeclarationString] = 1

        if prevCodeExists:
            return (recordDeclaration, recordInvocation)

        ###
        # Generate Record function
        ###

        # Create Parameter/Argument Lists
        primitiveArgIds = [idx for idx, type in enumerate(argTypes)
                                                    if not isStringType(type)]
        stringArgIds = [idx for idx, type in enumerate(argTypes)
                                                        if isStringType(type)]

        strlenDeclarations = ["str%dLen = strlen(arg%d) + 1" %
                                            (idx, idx) for idx in stringArgIds]
        strlenVariables = ["str%dLen" % (idx) for idx in stringArgIds]

        primitiveSizes = ["sizeof(arg%d)" % idx for idx in primitiveArgIds]

        # Create more usable strings for each list
        strlenDeclarationsString = ", ".join(strlenDeclarations)
        primitiveSizeSumString = " + ".join(primitiveSizes) \
                                                    if primitiveSizes else "0"
        strlenSumString = " + ".join(strlenVariables) \
                                                    if strlenVariables else "0"

        # Bytes needed to store the primitive byte lengths
        nibbleByteSizes = (len(primitiveArgIds) + 1)/2

        # Start Generating the record code

        if strlenDeclarationsString:
            strlenDeclarationsString = "\tsize_t %s;\n" % strlenDeclarationsString

        recordPrimitivesString = "".join(["\t%s(buffer, arg%d);\n" % \
                (RECORD_PRIMITIVE_FN, idx) for idx in primitiveArgIds])


        recordCode = \
"""
%s {
    %s;
    size_t allocSize = %s + %s + sizeof(%s);
    %s *re = reinterpret_cast<%s*>(%s(allocSize));

    if (re == nullptr)
        return;

    re->fmtId = %d;
    re->timestamp = PerfUtils::Cycles::rdtsc();
    re->entrySize = static_cast<uint32_t>(allocSize);
    re->argMetaBytes = %d;

    char *buffer = re->argData;

    // Record the primitives
    %s;

    // Make the entry visible
    %s(allocSize);
}
""" %  (recordDeclaration,
        strlenDeclarationsString,
        primitiveSizeSumString, strlenSumString, RECORD_ENTRY,
        RECORD_ENTRY, RECORD_ENTRY, ALLOC_FN,
        fmtId,
        nibbleByteSizes,
        recordPrimitivesString,
        FINISH_ALLOC_FN)


        ###
        # Generate compression
        ###

        # Generate code to compress the arguments from a RecordEntry to
        # an output array. Note that the record code should have
        # handled the metadata, so we don't have to worry about that here
        compressionCode = \
"""
inline ssize_t
compressArgs%d(%s *re, char* out) {
    char *originalOutPtr = out;
    %s *nib = reinterpret_cast<%s*>(out);
    out += %d;

    char *args = re->argData;

""" % ( fmtId, RECORD_ENTRY,
        NIBBLE_OBJ, NIBBLE_OBJ,
        nibbleByteSizes)


        # compressionCode = "inline void\ncompressArgs%d(%s *re, char** out) {\n"\
        #                                                 % (fmtId, RECORD_ENTRY)

        #TODO(syang0) Pointers should have a different compression algorithm.
        # it should take the difference between the last pointer recorded
        # and this one so that it's even shorter.

        # # Step 1: Allocate the nibbles to store the primitives
        # if nibbleByteSizes:
        #     compressionCode += "\t%s *nib = reinterpret_cast<%s*>(*out);\n" %\
        #                                                 (NIBBLE_OBJ, NIBBLE_OBJ)
        #     compressionCode += "\t*out += %d;\n" % nibbleByteSizes

        # Step 2a: Read back all the primitives
        for idx in primitiveArgIds:
            type = argTypes[idx]
            compressionCode += "\t%s arg%d = *reinterpret_cast<%s*>(args); " \
                               "args += sizeof(%s);\n" % (type, idx, type, type)

        # Step 2b: pack all the primitives
        compressionCode += "\n"
        for i, idx in enumerate(primitiveArgIds):
            mem = "first" if (i%2 == 0) else "second"
            arrIndex = i/2
            compressionCode += "\tnib[%d].%s = 0x07 & static_cast<uint8_t>(%s(&out, arg%d));\n" % (arrIndex, mem, PACK_FN, idx)

        # Step 3: Figure out size of strings and just memcpy it
        if stringArgIds:
            compressionCode += "\n\tsize_t stringBytes = re->entrySize - (%s) - " \
                        "sizeof(%s);\n" % (primitiveSizeSumString, RECORD_ENTRY)
            compressionCode += "\tmemcpy(out, args, stringBytes);\n" \
                            "\targs += stringBytes;\n\tout += stringBytes;\n"

        compressionCode += "\treturn out - originalOutPtr;\n"
        compressionCode += "}\n"

        ###
        # Generate Decompression
        ###
        decompressionCode = "inline void\ndecompressPrintArg%d" \
                                            "(std::ifstream &in) {\n" % fmtId

        # Step 1: Read back all the nibbles
        decompressionCode += "\t%s nib[%d];\n" % (NIBBLE_OBJ, nibbleByteSizes)
        decompressionCode += "\tin.read(reinterpret_cast<char*>(&nib)," \
                                                " %d);\n\n" % nibbleByteSizes

        # Step 2: Read back all the primitives
        for i, idx in enumerate(primitiveArgIds):
            type = argTypes[idx]
            mem = "first" if (i%2 == 0) else "second"
            arrIndex = i/2
            decompressionCode += "\t%s arg%d = %s<%s>(in, nib[%d].%s);\n" \
                                % (type, idx, UNPACK_FN, type, arrIndex, mem)

        # Step 3: Find all da strings
        decompressionCode += "\n"
        for idx in stringArgIds:
            type = argTypes[idx]
            strType = "std::wstring" if "w_char" in type else "std::string"

            decompressionCode += "\t%s arg%d_str;\n" % (strType, idx)
            decompressionCode += "\tstd::getline(in, arg%d_str, \'\\0\');\n" \
                                        % (idx)
            decompressionCode += "\t%s arg%d = arg%d_str.c_str();\n" \
                                        % (type, idx, idx)

        decompressionCode += "\n\tconst char *fmtString = \"%s\";\n" % fmtString
        decompressionCode += "\tconst char *filename = \"%s\";\n" % filename
        decompressionCode += "\tconst int linenum = %d;\n" % linenum

        # Step 4: Final Printout
        if argTypes:
            decompressionCode += "\n\tprintf(\"%s\", %s);\n" % (
                fmtString,
                ", ".join(
                    ["arg%d" % idx for idx, type in enumerate(argTypes)]))
        else:
            decompressionCode += "\n\tprintf(\"%s\");\n" % fmtString

        decompressionCode += "}\n"


        # All the code has been generated,  save them in our data structure
        code = {
            "fmtString"         : fmtString,
            "filename"          : filename,
            "linenum"           : linenum,
            "compilationUnit"   : compilationName,
            "recordFnDef"       : recordCode,
            "compressFnDef"     : compressionCode,
            "decompressFnDef"   : decompressionCode
        }

        if fmtId == len(self.fmtId2Code):
            self.fmtId2Code.append(code)
        else:
            assert(fmtId < len(self.fmtId2Code))
            self.fmtId2Code[fmtId] = code

        return (recordDeclaration, recordInvocation)


# Helper function to deterministically generate a managled record function
# name given its assigned format id
#
# \param fmtId - Format id of the log message
#
# \return - record function name.
def generateFunctionNameFromFmtId(fmtId):
    return "__syang0__fl__%d" % fmtId

# Given a C++ printf-like format string, identify all the C++ types that
# correspond to the format specifiers in the format string.
#
# Note that the "%n" specifier is not supported in the FastLogger system and
# will cause the following function to abort in failure.
#
# \param fmtString  - Printf-like format string such as "number=%d, float=%0.2f"
#
# \return           - A list of C++ types as strings (e.x. ["int", "char*", ...]
def parseTypesInFmtString(fmtString):
    # This function follows the standard according to the cplusplus reference
    # http://www.cplusplus.com/reference/cstdio/printf/ (9/7/16)

    # These are the set of characters that can serve as specifiers
    signedSet = 'di'
    unsignedSet = 'uoxX'
    floatSet = 'fFeEgGaA'
    integerSet = signedSet + unsignedSet

    # A set of characters that will end a "%" format specifier.
    terminalSet = signedSet + unsignedSet + floatSet + 'cspn'

    types = []
    offset = 0
    typeSoFar = ""
    parsingSpecifier = False
    while (offset < len(fmtString)):
        c = fmtString[offset]

        if c == "%":
            # A double percent (%%) means print just the %
            parsingSpecifier = not parsingSpecifier
        elif c == "\\":
            # Skip next character since it's escaped
            offset += 1

        elif parsingSpecifier:
            # hh is a special case since it can output a char instead of an int
            if c == 'h':
                assert(typeSoFar == "")
                if fmtString[offset + 1] == 'h':
                    offset += 2
                    t = fmtString[offset]

                    if (t in signedSet):
                        typeSoFar = "signed char "
                    elif (t in unsignedSet):
                        typeSoFar = "unsigned char "
                    else:
                        assert (t == 'n')
                        sys.exit("Error: \"%n\" print specifier not supported")
                        typeSoFar = "signed const char* "
                else:
                    typeSoFar = "short "
            elif c == "l":
                typeSoFar = typeSoFar + "long "

            # This set restricts what can come next, so we special case it here
            elif c == 'j' or c == 'z' or c == 't':
                offset += 1
                t = fmtString[offset]

                assert(typeSoFar == "")
                if c == 'j':
                    if t in unsignedSet:
                        typeSoFar = "uintmax_t"
                    else:
                        typeSoFar = "intmax_t"
                elif c == 'z':
                    typeSoFar = "size_t"
                else:
                    typeSoFar = "ptrdiff_t"

                assert(t in integerSet or t in "n")

                if t == 'n':
                    typeSoFar = typeSoFar + "*"

                typeSoFar = typeSoFar + " "

            # Specifies a long double (16-bytes)
            elif c == 'L':
                offset += 1
                assert(fmtString[offset] in floatSet)
                typeSoFar = "long double "

            # Standard set of terminals
            elif c in signedSet:
               typeSoFar = typeSoFar + "int "

            elif c in unsignedSet:
                typeSoFar = typeSoFar + "unsigned int "

            elif c in floatSet:
                # Apparently all floats are doubles unless prepended by 'L',
                # which I already handled above
                typeSoFar = "double "
            elif c == 'c':
                if typeSoFar == "":
                    typeSoFar = "int "
                else:
                    assert(typeSoFar == "long ")
                    typeSoFar = "wint_t "
            elif c == 's':
                if typeSoFar == "":
                    typeSoFar = "const char* "
                else:
                    assert(typeSoFar == "long ")
                    typeSoFar = "const wchar_t* "
            elif c == 'p':
                typeSoFar = "void* "
            elif c == 'n':
                sys.exit("Error: \"%n\" print specifier not supported")
                typeSoFar = typeSoFar + "int* "

        # Check based on offset since the cases above may advance the offset
        if parsingSpecifier and fmtString[offset] in terminalSet:
            assert(typeSoFar != "")
            types.append(typeSoFar.strip())

            typeSoFar = ""
            parsingSpecifier = False

        offset = offset + 1
    return types

# Given a C++ type (such as 'int') as identified by parseTypesInFmtString,
# determine whether that type is a string or not.
#
# \param typeStr - Whether a type is a string or not in C/C++ land
def isStringType(typeStr):
    return -1 != typeStr.find("char*") or -1 != typeStr.find("wchar_t*");
