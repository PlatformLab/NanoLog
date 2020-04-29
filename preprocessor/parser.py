#! /usr/bin/python

# Copyright (c) 2016-2017 Stanford University
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

"""NanoLog Preprocessor
This script is the first component of a three part system (NanoLog) that
enables fast, sub-microsecond error logging. The key to the NanoLog's speed
is the extraction of static information from the log statements at compile time
and the injection of code to log only the dynamic information at runtime into
the user's sources. This injected code then interacts with the NanoLog
Runtime to output a succinct, compressed log which can be later transformed
into a human-readable format by the NanoLog Decompressor.

This script performs the first step of log statement extraction and code
injection and is intended to be used in to phases. In the first phase, all
user sources are to be preprocessed by the GNU C/C++ preprocessor and then
passed into this script in mapOutput mode. The outputs of this phase are a
version of the user's source with injected code that can be directly compiled
with g++ and an intermediate map file to be used in the second stage.

In the second stage, all the intermediate map files (there should be one
per compiled user source) should be passed into this script in combinedOutput
mode. This will aggregate all the log metadata in the map files and produce
a supporting C++ header file used in the Runtime and Decompression components.

Usage:
    parser.py [-h] --mapOutput=MAP PREPROCESSED_SRC
    parser.py [-h] --combinedOutput=HEADER [MAP_FILES...]

Options:
  -h --help             Show this help messages

  --mapOutput=MAP       Output destination for the intermediate metadata map
                        file to be used in the combinedOutput mode. There should
                        be one map file per preprocessed_src

  PREPROCESSED_SRC      GNU-preprocessed C/C++ file to process. The processed
                        files will be outputted with an extra "i" extension
                        (ex test.i ->test.ii), will contain injected code,
                        and can be compiled directly with g++

  --combinedOutput=HEADER
                        Output destination for the final C++ header file that
                        aggregates all the map files for use with the
                        other NanoLog components [default:BufferStuffer.h]

  MAP_FILES             List of map files to combine into the final header;
                        There should be one map file per preprocessed source
"""

from docopt import docopt
from collections import namedtuple
import sys

from FunctionGenerator import *

####
# Below are configuration parameters to be toggled by the library implementer
####

# Log function to search for within the C++ files and perform the replacement
# and stripping of format strings.
LOG_FUNCTION = "NANO_LOG"

# A special C++ line at the end of NanoLog.h that marks where the parser
# can start injecting inline function definitions. The key to it being at the
# end of NanoLog.h is that it ensures all required #includes have been
# included by this point in the file.
INJECTION_MARKER = \
    "static const int __internal_dummy_variable_marker_for_code_injection = 0;"

# Since header files are in-lined after the GNU preprocessing step, library
# files can be unintentionally processed. This list is a set of files that
# should be ignored
ignored_files = set([
])

####
# End library implementer parameters
####

# Simple structure to identify a position within a file via a line number
# and an offset on that line
FilePosition = namedtuple('FilePosition', ['lineNum', 'offset'])

# Encapsulates a function invocation's argument as the original source text
# and start/end FilePositions.
Argument = namedtuple('Argument', ['source', 'startPos', 'endPos'])

# Given a C/C++ style string in source code (i.e. in quotes), attempt to parse
# it back as a regular string. The source passed in can be multi-line (due to C
# string concatenation), but should not contain any extraneous characters
# outside the quotations (such as commas separating invocation parameters).
#
# \param source
#         C/C++ style string to extract
#
# \return
#         contents of the C/C++ string as a python string. None if
#         the lines did not encode a C/C++ style string.
def extractCString(source):
  returnString = ""
  isInQuotes = False
  prevWasEscape = False

  for line in source.splitlines(True):
    for c in line:
      if c == "\"" and not prevWasEscape:
        isInQuotes = not isInQuotes
      elif isInQuotes:
        returnString += c
        prevWasEscape = c == "\\"
      else:
        if not (c.isspace()):
          return None

  if isInQuotes:
    return None

  return returnString

# Attempt to extract a single argument in a C++ function invocation given
# the argument's start position (immediately after left parenthesis or comma)
# within a file.
#
# \param lines
#         all lines of the file
#
# \param startPosition
#         FilePosition denoting the start of the argument
#
# \return
#         an Argument namedtuple. None if it was unable to find the argument
#
def parseArgumentStartingAt(lines, startPos):

  # The algorithm uses the heuristic of assuming that the argument ends
  # when it finds a terminating character (either a comma or right parenthesis)
  # in a position where the relative parenthesis/curly braces/bracket depth is 0
  # The latter constraint prevents false positives where function calls are used
  # to generate the parameter (i.e. log("number is %d", calculate(a, b)))
  parenDepth = 0
  curlyDepth = 0
  bracketDepth = 0
  inQuotes = False
  argSrcStr = ""

  offset = startPos.offset
  for lineNum in range(startPos.lineNum, len(lines)):
    line = lines[lineNum]

    prevWasEscape = False
    for i in range(offset, len(line)):
      c = line[i]
      argSrcStr = argSrcStr + c

      # If it's an escape, we don't care what comes after it
      if c == "\\" or prevWasEscape:
        prevWasEscape = not prevWasEscape
        continue

      # Start counting depths
      if c == "\"":
        inQuotes = not inQuotes

      # Don't count curlies and parenthesis when in quotes
      if inQuotes:
        continue

      if c == "{":
        curlyDepth = curlyDepth + 1
      elif c == "}":
        curlyDepth = curlyDepth - 1
      elif c == "(":
        parenDepth = parenDepth + 1
      elif c == ")" and parenDepth > 0:
        parenDepth = parenDepth - 1
      elif c == "[":
        bracketDepth = bracketDepth + 1
      elif c == "]":
        bracketDepth = bracketDepth - 1
      elif (c == "," or c == ")") and curlyDepth == 0 \
              and parenDepth == 0 and bracketDepth == 0:
        # found it!
        endPos = FilePosition(lineNum, i)
        return Argument(argSrcStr[:-1], startPos, endPos)

    # Couldn't find it on this line, must be on the next
    offset = 0

  return None


# Given the starting position of a LOG_FUNCTION, attempt to identify
# all the syntactic components of the LOG_FUNCTION (such as arguments and
# ending semicolon) and their positions in the file
#
# \param lines
#             all the lines of the file
# \param startPosition
#             tuple containing the line number and offset where
#             the LOG_FUNCTION starts within lines
#
# \return a dictionary with the following values:
#         'startPos'        - FilePosition of the LOG_FUNCTION
#         'openParenPos'    - FilePosition of the first ( after LOG_FUNCTION
#         'closeParenPos'   - FilePosition of the closing )
#         'semiColonPos'    - FilePosition of the function's semicolon
#         'arguments'       - List of Arguments for the LOG_FUNCTION
#
# \throws ValueError
#         When parts of the LOG_FUNCTION cannot be found
#
def parseLogStatement(lines, startPosition):
  lineNum, offset = startPosition
  assert lines[lineNum].find(LOG_FUNCTION, offset) == offset

  # Find the left parenthesis after the LOG_FUNCTION identifier
  offset += len(LOG_FUNCTION)
  char, openParenPos = peekNextMeaningfulChar(lines, FilePosition(lineNum, offset))
  lineNum, offset = openParenPos

  # This is an assert instead of a ValueError since the caller should ensure
  # this is a valid start to a function invocation before calling us.
  assert(char == "(")

  # Identify all the argument start and end positions
  args = []
  while lines[lineNum][offset] != ")":
    offset = offset + 1
    startPos = FilePosition(lineNum, offset)
    arg = parseArgumentStartingAt(lines, startPos)
    if not arg:
      raise ValueError("Cannot find end of NANO_LOG invocation",
                       lines[startPosition[0]:startPosition[0]+5])
    args.append(arg)
    lineNum, offset = arg.endPos

  closeParenPos = FilePosition(lineNum, offset)

  # To finish this off, find the closing semicolon
  semiColonPeek =  peekNextMeaningfulChar(lines, FilePosition(lineNum, offset + 1))
  if not semiColonPeek:
    raise ValueError("Expected ';' after NANO_LOG statement",
                     lines[startPosition[0]:closeParenPos.lineNum])

  char, pos = semiColonPeek
  if (char != ";"):
    raise ValueError("Expected ';' after NANO_LOG statement",
                   lines[startPosition[0]:pos[0]])

  logStatement = {
      'startPos': startPosition,
      'openParenPos': openParenPos,
      'closeParenPos': closeParenPos,
      'semiColonPos': pos,
      'arguments': args,
  }

  return logStatement

# Helper function to peekNextMeaningfulCharacter that determines whether
# a character is a printable character (like a-z) vs. a control code
#
# \param c - character to test
# \param codec - character type (optional)
#
# \return - true if printable, false if not
def isprintable(c, codec='utf8'):
  try: c.decode(codec)
  except UnicodeDecodeError: return False
  else: return True

# Given a start FilePosition, find the next valid character that is
# syntactically important for the C/C++ program and return both the character
# and FilePosition of that character.
#
# \param lines      - lines in the file
# \param filePos    - FilePosition of where to start looking
# \return           - a (character, FilePosition) Tuple; None if no such
#                       character exists (i.e. EOF)
#
def peekNextMeaningfulChar(lines, filePos):
  lineNum, offset = filePos

  while lineNum < len(lines):
    line = lines[lineNum]
    while offset < len(line):
      c = line[offset]
      if not c.isspace():
        return (c, FilePosition(lineNum, offset))
      offset = offset + 1
    offset = 0
    lineNum = lineNum + 1

  return None

# Given a C/C++ source file that have been preprocessed by the GNU
# preprocessor with the -E option, identify all the NanoLog log statements
# and inject code in place of the statements to interface with the NanoLog
# runtime system. The processed files will be outputted as <filename>i
# (ex: test.i -> test.ii)
#
# \param functionGenerator
#           FunctionGenerator used to generate interface code
#           and maintain mappings
#
# \param inputFiles
#           list of g++ preprocessed C/C++ files to analyze
#
def processFile(inputFile, mapOutputFilename):
  functionGenerator = FunctionGenerator()
  directiveRegex = re.compile("^# (\d+) \"(.*)\"(.*)")

  with open(inputFile) as f, open(inputFile + "i", 'w') as output:
    try:
      lines = f.readlines()
      lineIndex = -1

      lastChar = '\0'

      # Logical location in a file based on GNU Preprocessor directives
      ppFileName = inputFile
      ppLineNum = 0

      # Notes the first filename referenced by the pre-processor directives
      # which should be the name of the file being compiled.
      firstFilename = None

      # Marks at which line the preprocessor can start safely injecting
      # generated, inlined code. A value of None indicates that the NanoLog
      # header was not #include-d yet
      inlineCodeInjectionLineIndex = None

      # Scan through the lines of the file parsing the preprocessor directives,
      # identfying log statements, and replacing them with generated code.
      while lineIndex < len(lines) - 1:
        lineIndex = lineIndex + 1
        line = lines[lineIndex]

        # Keep track of of the preprocessor line number so that we can
        # put in our own line markers as we inject code into the file
        # and report errors. This line number should correspond to the
        # actual user source line number.
        ppLineNum = ppLineNum + 1

        # Parse special preprocessor directives that follows the format
        # '# lineNumber "filename" flags'
        if line[0] == "#":
          directive = directiveRegex.match(line)
          if directive:
              # -1 since the line num describes the line after it, not the
              # current one, so we decrement it here before looping
              ppLineNum = int(float(directive.group(1))) - 1

              ppFileName = directive.group(2)
              if not firstFilename:
                  firstFilename = ppFileName

              flags = directive.group(3).strip()
              continue

        if INJECTION_MARKER in line:
            inlineCodeInjectionLineIndex = lineIndex
            continue

        if ppFileName in ignored_files:
            continue

        # Scan for instances of the LOG_FUNCTION using a simple heuristic,
        # which is to search for the LOG_FUNCTION outside of quotes. This
        # works because at this point, the file should already be pre-processed
        # by the C/C++ preprocessor so all the comments have been stripped and
        # all #define's have been resolved.
        prevWasEscape = False
        inQuotes = False
        charOffset = -1

        # Optimization: Make sure line has LOG_FUNCTION before doing more work
        if LOG_FUNCTION not in line:
          continue

        while charOffset < len(line) - 1:
          charOffset = charOffset + 1
          c = line[charOffset]

          # If escape, we don't really care about the next char
          if c == "\\" or prevWasEscape:
            prevWasEscape = not prevWasEscape
            lastChar = c
            continue

          if c == "\"":
            inQuotes = not inQuotes

          # If we match the first character, cheat a little and scan forward
          if c == LOG_FUNCTION[0] and not inQuotes:
            # Check if we've found the log function via the following heuristics
            #  (a) the next n-1 characters spell out the rest of LOG_FUNCTION
            #  (b) the previous character was not an alpha numeric (i.e. not
            #       a part of a longer identifier name)
            #  (c) the next syntactical character after log function is a (
            found = True
            for ii in range(len(LOG_FUNCTION)):
              if line[charOffset + ii] != LOG_FUNCTION[ii]:
                found = False
                break

            if not found:
              continue

            # Valid identifier characters are [a-zA-Z_][a-zA-Z0-9_]*
            if lastChar.isalnum() or lastChar == '_':
              continue

            # Check that it's a function invocation via the existence of (
            filePosAfter = FilePosition(lineIndex, charOffset + len(LOG_FUNCTION))
            mChar, mPos = peekNextMeaningfulChar(lines, filePosAfter)
            if mChar != "(":
              continue

            # Okay at this point we are pretty sure we have a genuine
            # log statement, parse it and start modifying the code!
            logStatement = parseLogStatement(lines, (lineIndex, charOffset))
            lastLogStatementLine = logStatement['semiColonPos'].lineNum

            if len(logStatement['arguments']) < 2:
              raise ValueError("NANO_LOG statement expects at least 2 arguments"
                                 ": a LogLevel and a literal format string",
                                 lines[lineIndex:lastLogStatementLine + 1])

            # We expect the log invocation to have the following format:
            # LOG_FN(LogLevel, FormatString, ...), hence the magic indexes
            logLevel = logStatement['arguments'][0].source
            fmtArg = logStatement['arguments'][1]
            fmtString = extractCString(fmtArg.source)

            # At this point, we should check that NanoLog was #include-d
            # and that the format string was a static string
            if not inlineCodeInjectionLineIndex:
              raise ValueError("NANO_LOG statement occurred before "
                                "#include-ing the NanoLog header!",
                               lines[lineIndex:lastLogStatementLine + 1])

            if not fmtString:
              raise ValueError("NANO_LOG statement expects a literal format "
                               "string for its second argument",
                                lines[lineIndex:lastLogStatementLine + 1])

            # Invoke the FunctionGenerator and if it throws a ValueError,
            # tack on an extra argument to print out the log function itself
            try:
              (recordDecl, recordFn) = functionGenerator.generateLogFunctions(
                                            logLevel, fmtString, firstFilename,
                                            ppFileName, ppLineNum)
            except ValueError as e:
              raise ValueError(e.args[0],
                               lines[lineIndex:lastLogStatementLine + 1])

            # Now we're ready to inject the code. What's going to happen is
            # that the original LOG_FUNCTION will be ripped out and in its
            # place, a function invocation for the record logic will be
            # inserted. It will look something like this:
            #
            #    input: "++i; LOG("Test, %d", 5); ++i;"
            #    output: "i++;
            #             # 1 "injectedCode.fake"
            #             {
            #             __syang0__fl__(
            #             # 10 "original.cc"
            #                     "Test, %d", 5); }
            #             # 10 "original.cc"
            #                                     ++i;"
            #
            # Note that we try to preserve spacing and use line preprocessor
            # directives wherever we can so that if the compiler reports
            # errors, then the errors can be consistent with the user's view
            # of the source file.

            # First we separate the code that comes after the log statement's
            # semicolon onto its own line while preserving the line spacing
            # and symbolic reference to the original source file
            #
            # Example:
            #       "functionA(); functionB();"
            # becomes
            #       "functionA();
            #       # 10 "filename.cc"
            #                     functionB();"
            #
            # Note that we're working from back to front so that our line
            # indices don't shift as we insert new lines.
            scLineNum, scOffset = logStatement['semiColonPos']
            scLine = lines[scLineNum]

            # Extrapolate the symbolic line number
            scPPLineNum = ppLineNum + (scLineNum - lineIndex)

            # Split the line
            scHeadLine = scLine[:scOffset + 1] + "\r\n"
            scMarker = "# %d \"%s\"\r\n" % (scPPLineNum, ppFileName)
            scTailLine = " "*(scOffset + 1) + scLine[scOffset + 1:]

            lines[scLineNum] = scHeadLine
            lines.insert(scLineNum + 1, scMarker)
            lines.insert(scLineNum + 2, scTailLine)

            # update the line we're working with in case the we split it above
            if scLineNum == lineIndex:
              line = lines[lineIndex]

            # Next, we're going to replace the LOG_FUNCTION string from the
            # first line with our generated function's name and insert
            # the appropriate preprocessor directives to mark the boundaries
            #
            # Example:
            #       "A(); LOG("Hello!);"
            # Becomes
            #       "A();
            #        # 10 "injectedCode.fake"
            #        { GENERATED_FUNC_NAME
            #        # 10 "filename.cc"
            #                ("Hello!");
            #        }
            #        # 10 "filename.cc"

            # Close off the new scope
            lines.insert(scLineNum + 1, "}\r\n")

            offsetAfterLogFn = (charOffset + len(LOG_FUNCTION))
            headOfLine = line[:charOffset]
            tailOfLine = line[offsetAfterLogFn:].rjust(len(line))

            lines[lineIndex] = \
                          headOfLine \
                            + "\r\n# %d \"injectedCode.fake\"\r\n" % ppLineNum \
                            + "{ " + recordFn \
                            + "\r\n# %d \"%s\"\r\n" % (ppLineNum, ppFileName) \
                            + tailOfLine

          lastChar = c
    except ValueError as e:
        print("\r\n%s:%d: Error - %s\r\n\r\n%s\r\n" % (
            ppFileName, ppLineNum, e.args[0], "".join(e.args[1])))
        sys.exit(1)

    # Last step, retrieve the generated code and insert it at the end
    recFns = functionGenerator.getRecordFunctionDefinitionsFor(firstFilename)
    codeToInject = "\r\n\r\n# 1 \"generatedCode.h\" 3\r\n" \
                              + "\r\n".join(recFns)

    if recFns:
      # Assert is okay here since this should have been caught the first time
      # we found NANO_LOG without a #include
      assert inlineCodeInjectionLineIndex
      lines.insert(inlineCodeInjectionLineIndex + 1, codeToInject)

    # Output all the lines
    for line in lines:
      output.write(line)

    output.close()
    functionGenerator.outputMappingFile(mapOutputFilename)

if __name__ == "__main__":
  arguments = docopt(__doc__, version='NanoLog Preprocesor v1.0')

  if arguments['--mapOutput']:
    processFile(inputFile=arguments['PREPROCESSED_SRC'],
                mapOutputFilename=arguments['--mapOutput'])
  else:
    FunctionGenerator.outputCompilationFiles(
                                  outputFileName=arguments['--combinedOutput'],
                                  inputFiles=arguments['MAP_FILES'])
