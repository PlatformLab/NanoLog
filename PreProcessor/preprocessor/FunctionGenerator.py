# This class encapsulates all the logic to manage/generate a C++ file that
# encodes the compression/decompression routines for specific log messages.
# The generated file shall have the following mappings of Fmt Id to....
#
#   * char id2FmtString[][]
#               the original fmt string
#   * void *(compressFunctions[])(char* in, char* out);
#               the compression function takes in an input buffer and compress to an output
#   * void *(decompressFunctions[])(char* in);
#               decompression function that takes an input buffer and prints
#

import sys
import json
import os.path

ENTRY_HEADER = "PerfUtils::FastLogger::RecordMetadata"
RECORD_HEADER_FN = "PerfUtils::FastLogger::recordMetadata"
RECORD_PRIMITIVE_FN = "PerfUtils::FastLogger::recordPrimitive"


def genRecordName(fmtId):
    return "__syang0__fl__%d" % fmtId;

def isStringType(typeStr):
    return -1 != typeStr.find("char*") or -1 != typeStr.find("wchar_t*");

# Returns a list of C++ types
def parseTypesInFmtString(fmtString):
    types = []

    # These are the set of characters that can serve as specifiers
    signedSet = 'di'
    unsignedSet = 'uoxX'
    floatSet = 'fFeEgGaA'
    integerSet = signedSet + unsignedSet
    terminalSet = signedSet + unsignedSet + floatSet + 'cspn'

    offset = 0
    typeSoFar = ""
    parsingSpecifier = False
    while (offset < len(fmtString)):
        c = fmtString[offset]

        if c == "%":
            # Trickiness here is that double percent (%%) means print the %
            parsingSpecifier = not parsingSpecifier
        elif c == "\\":
            # Skip next character
            offset += 1

        # Follows http://www.cplusplus.com/reference/cstdio/printf/ (9/7/16)
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


class FunctionGenerator(object):
    # input_file is a previously generated function file
    def __init__(self, mappingFile=None, outputHeader="BufferStuffer.h"):
        self.mappingFile = mappingFile
        self.outputHeader = outputHeader
        self.fmtStr2Id = { }

        # The mapping file maintains various mappings related to the format
        # string (such as its unique identifier and compression functions).
        # This is encoded in a JSON format so that it can easily be serailized
        # and de-serialized for partial compilations.
        self.fmtId2Code = [
            {
                "fmtString"        : "INVALID",
                "filename"         : "INVALID.cc",
                "linenum"          : "-1",
                "compilationUnit"  : "INVALID.cc",
                "recordFnDef"      : "invalidRecord(int arg0) { ... }",
                "compressFnDef"    : "invalidCompress(...)",
                "decompressFnDef"  : "invalidDecompress(...)"
            }
        ]

        self.fmtStr2Id = {}
        self.unusedIds = []

        if mappingFile and os.path.isfile(mappingFile):
            with open(mappingFile) as json_file:
                loaded = json.load(json_file)

                self.fmtId2Code = loaded["fmtId2Code"]
                self.unusedIds = loaded["unusedIds"]
                self.fmtStr2Id = loaded["fmtStr2Id"]

    def clearLogFunctionsForFile(self, filename=None):
        if not filename:
            return

        for fmtId, code in self.fmtId2Code.iteritems():
            if code["filename"] == filename:
                self.fmtStr2Id.pop(value["fmtString"], None)
                self.fmtId2Code[fmtId] = None
                self.unusedIds.append(fmtId)

    def getRecordFunctionDefinitionsFor(self, compilationUnit):
        recordFns = []

        # TODO(syang0) we could use a hint system to speed this up in the normal
        for code in self.fmtId2Code:
            if code["compilationUnit"] == compilationUnit:
                recordFns.append(code["recordFnDef"])

        return recordFns

    # Returns a tuple (delcaration, invocation). Stashes the definitions
    # Must call serialize() at the end of file
    #TODO(syang0) should differenctiate on file name
    def generateLogFunctions(self, fmtString, args, compilationName ="default", filename="unknownFile", linenum=-1):

        invocationArgumentsString = ", ".join([arg.source for arg in args]);

        argTypes = parseTypesInFmtString(fmtString)
        parameterDeclarations = ["%s arg%d" % (type, idx) for idx, type in enumerate(argTypes)]
        parameterDeclarationString = ", ".join(parameterDeclarations)

        # Check to see if code has been generated before
        prevCodeExists = False;
        if fmtString in self.fmtStr2Id:
            fmtId = self.fmtStr2Id[fmtString]
            prevCodeExists = True
        elif self.unusedIds:
            fmtId = self.usedIds.pop()
            self.fmtStr2Id[fmtString] = fmtId
        else:
            fmtId = len(self.fmtStr2Id) + 1
            self.fmtStr2Id[fmtString] = fmtId

        recordDeclaration = "void %s(%s)" % (genRecordName(fmtId), parameterDeclarationString)
        recordInvocation = "%s(%s)" % (genRecordName(fmtId), invocationArgumentsString)

        if prevCodeExists:
            return (recordDeclaration, recordInvocation)

        ###
        # Generate Record function
        ###
        # Create Parameter/Argument Lists
        primitiveArgIds = [idx for idx, type in enumerate(argTypes) if not isStringType(type)]
        stringArgIds = [idx for idx, type in enumerate(argTypes) if isStringType(type)]

        strlenDeclarations = ["str%dLen = strlen(arg%d) + 1" % (idx, idx) for idx in stringArgIds]
        strlenVariables = ["str%dLen" % (idx) for idx in stringArgIds]

        primitiveSizes = ["sizeof(arg%d)" % idx for idx in primitiveArgIds]

        # Create more usable strings for each list
        strlenDeclarationsString = ", ".join(strlenDeclarations)
        primitiveSizeSumString = " + ".join(primitiveSizes) if primitiveSizes else "0"
        strlenSumString = " + ".join(strlenVariables) if strlenVariables else "0"

        # Bytes needed to store the primitive byte lengths
        nibbleByteSizes = (len(primitiveArgIds) + 1)/2

        # Start Generating Codes
        recordCode = recordDeclaration + " {\n" # start function

        if strlenDeclarationsString:
            recordCode += "\tint %s;\n" % strlenDeclarationsString

        recordCode += "\tint maxSizeOfArgs = %s + %s;\n" % (primitiveSizeSumString, strlenSumString)
        recordCode += "\tint reqSize = sizeof(%s) + maxSizeOfArgs;\n" % ENTRY_HEADER
        recordCode += "\tint maxSizeOfCompressedArgs = maxSizeOfArgs + %d;\n" % nibbleByteSizes

        recordCode += "\tchar *buffer = PerfUtils::FastLogger::alloc(reqSize);\n\n"
        recordCode += "\tif (buffer == nullptr)\n"
        recordCode += "\t\treturn;\n\n"

        recordCode += "\t%s(buffer, %d, maxSizeOfCompressedArgs);\n" %  (RECORD_HEADER_FN, fmtId)

        recordCode += "\n" if primitiveArgIds else ""
        recordCode += "".join(["\t%s(buffer, arg%d);\n" % (RECORD_PRIMITIVE_FN, idx) for idx in primitiveArgIds])

        recordCode += "\n" if primitiveArgIds else ""
        recordCode += "".join(["\tmemcpy(buffer, arg%d, str%dLen); buffer += str%dLen;\n" % (idx, idx, idx) for idx in stringArgIds])

        recordCode += "}\n"   # end function


        ###
        # Generate compression
        ###

        # At this point in the code, the printer should have already read in the metadata and compressed it, thus
        # we only compress arguments at this point

        # At this point, the C++ code should have already read in and compressed the metadata, thus we only compress
        # the arguments at this point.
        compressionCode =  "inline void\ncompressArgs%d(char* &in, char* &out, uint32_t maxSizeOfCompressedArgs) {\n" % fmtId

        #TODO(syang0) Should do pointer compression a bit differently than the primitives.

        # Step 1: Allocate the nibbles to store the primitives
        if nibbleByteSizes:
            compressionCode += "\tPerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(out);\n"
            compressionCode += "\tout += %d;\n" % nibbleByteSizes

        # Step 2a: Read back all the primitives
        for idx in primitiveArgIds:
            type = argTypes[idx]
            compressionCode += "\t%s arg%d = *reinterpret_cast<%s*>(in); in += sizeof(%s);\n" % (type, idx, type, type)

        # Step 2b: pack all the primitives
        compressionCode += "\n"
        for i, idx in enumerate(primitiveArgIds):
            mem = "first" if (i%2 == 0) else "second"
            arrIndex = i/2
            compressionCode += "\tnib[%d].%s = PerfUtils::pack(out, arg%d);\n" % (arrIndex, mem, idx)

        # Step 3: Figure out size of strings and just memcpy it
        if stringArgIds:
            compressionCode += "\n\tint stringBytes = maxSizeOfCompressedArgs - (%s) - %d;\n" % (primitiveSizeSumString, nibbleByteSizes)
            compressionCode += "\tmemcpy(out, in, stringBytes);\n\tin += stringBytes;\n\tout += stringBytes;\n"

        compressionCode += "}\n"

        ###
        # Generate Decompression
        ###
        decompressionCode = "inline void\ndecompressPrintArg%d(char* &in) {\n" % fmtId

        # Step 1: Read back all the nibbles
        decompressionCode += "\tPerfUtils::FastLogger::Nibble *nib = reinterpret_cast<PerfUtils::FastLogger::Nibble*>(in);\n"
        decompressionCode += "\tin += %d;\n" % nibbleByteSizes

        # Step 2: Read back all the primitives
        decompressionCode += "\n"
        for i, idx in enumerate(primitiveArgIds):
            type = argTypes[idx]
            mem = "first" if (i%2 == 0) else "second"
            arrIndex = i/2
            decompressionCode += "\t%s arg%d = PerfUtils::unpack<%s>(out, nib[%d].%s);\n" % (type, idx, type, arrIndex, mem)

        # Step 3: Find all da strings
        decompressionCode += "\n"
        for idx in stringArgIds:
            type = argTypes[idx]
            decompressionCode += "\t%s arg%d = in; in += strlen(arg%d) + 1;\n" % (type, idx, idx)

        # Step 3: Integrate our static knowledge
        decompressionCode += "\n\tconst char *fmtString = \"%s\";\n" % "Test"
        decompressionCode += "\tconst char *filename = \"%s\";\n" % filename
        decompressionCode += "\tint linenum = %d;\n" % linenum

        # Step 4: Final Printout
        if argTypes:
            decompressionCode += "\n\tprintf(fmtString, %s);\n" % ", ".join(["arg%d" % idx for idx, type in enumerate(argTypes)])
        else:
            decompressionCode += "\n\tprintf(fmtString);\n"

        decompressionCode += "}\n"


        # All the code has been generated, now stuff them into our datastructures
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


#    def serializeToFile(self, output_file):
#        # put to file

