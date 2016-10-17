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

ENTRY_HEADER = "BufferUtils::RecordEntry"
NIBBLE_OBJ = "BufferUtils::Nibble"

RECORD_HEADER_FN = "BufferUtils::recordMetadata"
RECORD_PRIMITIVE_FN = "BufferUtils::recordPrimitive"

ALLOC_FN = "PerfUtils::FastLogger::alloc"
FINISH_ALLOC_FN = "PerfUtils::FastLogger::finishAlloc"

PACK_FN = "BufferUtils::pack"
UNPACK_FN = "BufferUtils::unpack"



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
    def __init__(self, mappingFile=None):
        self.mappingFile = mappingFile
        self.fmtStr2Id = { }

        # The mapping file maintains various mappings related to the format
        # string (such as its unique identifier and compression functions).
        # This is encoded in a JSON format so that it can easily be serailized
        # and de-serialized for partial compilations.

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

        self.fmtStr2Id = {}
        self.unusedIds = []
        self.uniqueArgs2Cnt = {}
        self.argLists2Cnt = {}

        if mappingFile and os.path.isfile(mappingFile):
            with open(mappingFile) as json_file:
                loaded = json.load(json_file)

                self.fmtId2Code = loaded.get("fmtId2Code", [])
                self.unusedIds = loaded.get("unusedIds", [])
                self.fmtStr2Id = loaded.get("fmtStr2Id", {})
                self.argLists2Cnt = loaded.get("argLists2Cnt", {})

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

    def outputCompilationFiles(self, filename="BufferStuffer.h"):
        with open(filename, 'w') as oFile:
            oFile.write("#ifndef BUFFER_STUFFER\n")
            oFile.write("#define BUFFER_STUFFER\n\n")

            oFile.write("#include \"Packer.h\"\n")
            oFile.write("#include \"FastLogger.h\"\n\n")

            oFile.write("#include <fstream>     // for decompression\n")
            oFile.write("#include <string>\n\n")

            # output the record functions in its own namspace for
            # debugging purposes
            oFile.write("// Record code (for debugging)\n\n")
            oFile.write("namespace {\n")
            for fmtId, code in enumerate(self.fmtId2Code[1:]):
                oFile.write(code["recordFnDef"])
                oFile.write("\n")
                oFile.write(code["compressFnDef"])
                oFile.write("\n")
                oFile.write(code["decompressFnDef"])
                oFile.write("\n")
            oFile.write("} // end empty namespace\n\n")

            numFns = len(self.fmtId2Code)

#            # Print out the compressor and decompressor functions
#            oFile.write("// Compression Code\n")
#            for fmtId, code in enumerate(self.fmtId2Code[1:]):
#                oFile.write(code["compressFnDef"])
#                oFile.write("\n")
#
#            oFile.write("// Decompression Code\n")
#            for fmtId, code in enumerate(self.fmtId2Code[1:]):
#                oFile.write(code["decompressFnDef"])
#                oFile.write("\n")

            # Output arrays that we can address into
            # Note that the first element is nullptr since fmtId=0 is invalid
            compressFnNameArray = ["\tnullptr"]
            decompressFnNameArray = ["\tnullptr"]

            for i in range(1, len(self.fmtId2Code)):
                compressFnNameArray.append("\tcompressArgs%d" % i)
                decompressFnNameArray.append("\tdecompressPrintArg%d" % i)

            oFile.write("static void (*compressFnArray[%d])(%s *re, char** out)"
                            " {\n%s\n};\n\n" %
                            (numFns, ENTRY_HEADER, ",\n".join(compressFnNameArray)))

            oFile.write("static void (*decompressAndPrintFnArray[%d])(std::ifstream &in) "
                            "{\n%s\n};\n" %
                                (numFns, ",\n".join(decompressFnNameArray)))

            # Output a list of the fmtStrings so we can refer to them
            allFmtStrings = []
            for i in range(1, len(self.fmtId2Code)):
                allFmtStrings.append("\t\"%s\"" % self.fmtId2Code[i]["fmtString"])

            oFile.write("// Format Id to original Format String\n")
            oFile.write("static const char* fmtId2Str[%d] = {\n" % len(allFmtStrings))
            oFile.write(",\n".join(allFmtStrings))
            oFile.write("\n};\n\n");

            oFile.write("\n\n#endif /* BUFFER_STUFFER */\n")


    def clearLogFunctionsForCompilationUnit(self, filename=None):
        if not filename:
            return

        for fmtId, code in self.fmtId2Code.iteritems():
            if code["compilationUnit"] == filename:
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

        #TODO(syang0) Add in __attribute__((printf...)) and the fmtString.
        recordDeclaration = "void %s(%s)" % (genRecordName(fmtId), parameterDeclarationString)
        recordInvocation = "%s(%s)" % (genRecordName(fmtId), invocationArgumentsString)

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

        recordCode += "\t%s *re = %s(maxSizeOfArgs);\n\n" % (ENTRY_HEADER, ALLOC_FN)
        # recordCode += "\tprintf(\"I am at %d; buffer is %%p\\n\", re);\n" % fmtId
        recordCode += "\tif (re == nullptr)\n"
        recordCode += "\t\treturn;\n\n"

        recordCode += "\t%s(re, %d, maxSizeOfArgs, %d);\n" %  (RECORD_HEADER_FN, fmtId, nibbleByteSizes)
        # recordCode += "\tre->fmtId = %d;" % fmtId
        # recordCode += "\tre->timestamp = Cycles::rdtsc();\n"
        # recordCode += "\tre->entrySize = maxSizeOfArgs + sizeof(%s);" % ENTRY_HEADER
        # recordCode += "\tre->argMetadataBytes = %d;" % nibbleByteSizes

        # For debug only...
#        recordCode += "\tprintf(\"Recording for fmtId=%%d which is %%s\\n\\n\", %d, \"%s\");\n" % (fmtId, fmtString)

        recordCode += "\tchar *buffer = re->argData;\n"
        recordCode += "\n" if primitiveArgIds else ""
        recordCode += "".join(["\t%s(buffer, arg%d);\n" % (RECORD_PRIMITIVE_FN, idx) for idx in primitiveArgIds])

        recordCode += "\n" if primitiveArgIds else ""
        recordCode += "".join(["\tmemcpy(buffer, arg%d, str%dLen); buffer += str%dLen;\n" % (idx, idx, idx) for idx in stringArgIds])

        recordCode += "\t%s(re);\n" % FINISH_ALLOC_FN
        recordCode += "}\n"   # end function


        ###
        # Generate compression
        ###

        # At this point in the code, the printer should have already read in the metadata and compressed it, thus
        # we only compress arguments at this point

        # At this point, the C++ code should have already read in and compressed the metadata, thus we only compress
        # the arguments at this point.
        compressionCode =  "inline void\ncompressArgs%d(%s *re, char** out) {\n" % (fmtId, ENTRY_HEADER)

        #TODO(syang0) Should do pointer compression a bit differently than the primitives.

        # Step 1: Allocate the nibbles to store the primitives
        if nibbleByteSizes:
            compressionCode += "\t%s *nib = reinterpret_cast<%s*>(*out);\n" % (NIBBLE_OBJ, NIBBLE_OBJ)
            compressionCode += "\t*out += %d;\n" % nibbleByteSizes

        # Step 2a: Read back all the primitives

        compressionCode += "\tchar* args = re->argData;\n"
        for idx in primitiveArgIds:
            type = argTypes[idx]
            compressionCode += "\t%s arg%d = *reinterpret_cast<%s*>(args); args += sizeof(%s);\n" % (type, idx, type, type)

        # Step 2b: pack all the primitives
        compressionCode += "\n"
        for i, idx in enumerate(primitiveArgIds):
            mem = "first" if (i%2 == 0) else "second"
            arrIndex = i/2
            compressionCode += "\tnib[%d].%s = %s(out, arg%d);\n" % (arrIndex, mem, PACK_FN, idx)

        # Step 3: Figure out size of strings and just memcpy it
        if stringArgIds:
            compressionCode += "\n\tint stringBytes = re->entrySize - (%s) - sizeof(%s);\n" % (primitiveSizeSumString, ENTRY_HEADER)
            compressionCode += "\tmemcpy(*out, args, stringBytes);\n\targs += stringBytes;\n\t*out += stringBytes;\n"

        compressionCode += "}\n"

        ###
        # Generate Decompression
        ###
#        decompressionCode = "inline void\ndecompressPrintArg%d(std::ifstream &in, char *buffer, uint32_t bufferSize) {\n" % fmtId

        # Step 1: Read back all the nibbles
#        decompressionCode += "\tin.read(buffer, %d); buffer += %d;\n" % (nibbleBytes, nibbleBytes)
#        decompressionCode += "\t%s *nib = reinterpret_cast<%s*>(in);\n" %(NIBBLE_OBJ, NIBBLE_OBJ)



        decompressionCode = "inline void\ndecompressPrintArg%d(std::ifstream &in) {\n" % fmtId

        # Step 1: Read back all the nibbles
        decompressionCode += "\t%s nib[%d];\n" % (NIBBLE_OBJ, nibbleByteSizes)
        decompressionCode += "\tin.read(reinterpret_cast<char*>(&nib), %d);\n\n" % nibbleByteSizes

        # Step 2: Read back all the primitives
        for i, idx in enumerate(primitiveArgIds):
            type = argTypes[idx]
            mem = "first" if (i%2 == 0) else "second"
            arrIndex = i/2
            decompressionCode += "\t%s arg%d = %s<%s>(in, nib[%d].%s);\n" % (type, idx, UNPACK_FN, type, arrIndex, mem)

        # Step 3: Find all da strings
        decompressionCode += "\n"
        for idx in stringArgIds:
            type = argTypes[idx]
            strType = "std::wstring" if "w_char" in type else "std::string"

            decompressionCode += "\t%s arg%d_str;\n" % (strType, idx)
            decompressionCode += "\tstd::getline(in, arg%d_str, \'\\0\');\n" % (idx)
            decompressionCode += "\t%s arg%d = arg%d_str.c_str();\n" % (type, idx, idx)

#TODO(syang0) I had some trouble with mapping the pack/unpack function names due
# to name space changes... i wonder if I should just do substitutions and the
# JSON saves sorta the %s type and we can subsitution on output.
        # Step 3: Integrate our static knowledge
        decompressionCode += "\n\tconst char *fmtString = \"%s\";\n" % fmtString
        decompressionCode += "\tconst char *filename = \"%s\";\n" % filename
        decompressionCode += "\tconst int linenum = %d;\n" % linenum

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

