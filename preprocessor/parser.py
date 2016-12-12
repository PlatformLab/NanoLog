#! /usr/bin/python

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

"""Fast Logger Preprocessor

Usage:
    parser.py [-h] --map=MAP [--output=OUTPUT] [FILES...]

Options:
  -h --help             Show this help messages

  --mapping=MAP         File for persisting the state of this script
                        between invocations [default: mapping.map]

  --output=OUTPUT       Optional output destination for the generated
                        C++ header that shall be used with the FastLogger
                        Runtime system [default: BufferStuffer.h]

  FILES                 GNU-preprocessed C/C++ files to process. The processed
                        files will be outputted with an "i" extension
                        (ex test.i -> test.ii)

 This script is 1 of a 3 part system that enables fast, sub-millisecond logging.
 This component takes in GNU-preprocessed C/C++ files (FILES), identifies all
 the log statements and replaces them with more efficient logging code that
 interfaces with the FastLogger Runtime component. The Runtime would then
 output a file that can be decompressed by the Decompressor application.

 This script is to be used in two stages. In the first stage, all the user
 GNU-preprocessed C/C++ files as passed in for processing. An mapping
 file should be provided to persist the state of the script between invocations
 (useful for parallel and partial rebuilds). In the second stage, when all the
 user files are processed, the script can be invoked again with the -c option
 and the mapping file from the previous stage to generate the final files
 to be used in the FastLogger Runtime and the Decompressor application.
"""

from docopt import docopt
from collections import namedtuple

from FunctionGenerator import *

####
# Below are configuration parameters to be toggled by the library implementer
####

# Log function to search for within the C++ files and perform the replacement
# and stripping of format strings.
LOG_FUNCTION = "FAST_LOG"

# Marks which argument of the record function is the static format string
# and assumes the arguments come after this point.
FORMAT_ARG_NUM = 0

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
# and start/end file positions.
Argument = namedtuple('Argument', ['source', 'startPos', 'endPos'])


# Given a C/C++ style string in source code, attempt to parse it back as a
# regular string. The source passed in can be multi-line (due to C string
# concatenation), but should not contain any extraneous characters outside the
# quotations (such as commas separating invocation parameters).
#
# \param source - source string to parse
# \return       - contents of the C/C++ string as a python string. None if
#                 the lines did not encode a C/C++ style string.
#
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

  return returnString

# Attempt to extract a single argument in a C++ function invocation given
# the argument's start position (immediately after left parenthesis or comma)
# within a file.
#
# \param lines          - all lines of the file
# \param startPosition  - FilePosition denoting the start of the argument
#
# \return an Argument named tuple
#
def parseArgumentStartingAt(lines, startPos):

  # The algorithm uses the heuristic of assuming that the argument ends
  # when it finds a terminating character (either a comma or right parenthesis)
  # in a position where the relative parenthesis/curlyBraces/bracket depth is 0.
  # The latter constraint prevents false positives where function calls are used
  # to generate the parameter (i.e. log("number is %d", calculate(a, b)))
  parenDepth = 0
  curlyDepth = 0
  bracketDepth = 0
  inQuotes = False
  argSrcStr = ""

  lineNum = startPos.lineNum
  offset = startPos.offset

  while True:
    prevWasEscape = False
    line = lines[lineNum]

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
        return Argument(argSrcStr[:-1], startPos, endPos);

    # Couldn't find it on this line, must be on the next
    offset = 0
    lineNum = lineNum + 1


# Given the starting position of a LOG_FUNCTION, attempt to identify
# all the syntactic components of the LOG_FUNCTION (such as arguments and
# ending semicolon) and their positions in the file
#
# \param lines          - all the lines of the file
# \param startPosition  - tuple containing the line number and offset where
#                         the LOG_FUNCTION starts
#
# \return a dictionary with the following values:
#         'startPos'        - FilePosition of the LOG_FUNCTION
#         'openParenPos'    - FilePosition of the first ( after LOG_FUNCTION
#         'closeParenPos'   - FilePosition of the closing )
#         'semiColonPos'    - FilePosition of the function's semicolon
#         'arguments'       - List of Arguments for the LOG_FUNCTION
#
def parseLogStatement(lines, startPosition):
  lineNum, offset = startPosition
  assert lines[lineNum].find(LOG_FUNCTION, offset) == offset

  # Find the left parenthesis after the LOG_FUNCTION identifier
  offset += len(LOG_FUNCTION)
  while(lines[lineNum].find("(", offset) == -1):
    lineNum = lineNum + 1
    offset = 0

  offset = lines[lineNum].find("(", offset)
  openParenPos = FilePosition(lineNum, offset)

  # Identify all the argument start and end positions
  args = []
  while lines[lineNum][offset] != ")":
    offset = offset + 1
    startPos = FilePosition(lineNum, offset)
    arg = parseArgumentStartingAt(lines, startPos)
    args.append(arg)

    lineNum, offset = arg.endPos

  closeParenPos = FilePosition(lineNum, offset)

  # To finish this off, find the closing semicolon:
  char, pos = peekNextMeaningfulChar(lines, FilePosition(lineNum, offset + 1))
  assert (char == ";")

  logStatement = {
      'startPos': startPosition,
      'openParenPos': openParenPos,
      'closeParenPos': closeParenPos,
      'semiColonPos': pos,
      'arguments': args,
  }

  return logStatement

# Given the lines to a file and a FilePosition, find the next semicolon on
# that line and if there is code after the semicolon, split the line while
# preserving line spacing for the latter split, and insert a C preprocessor
# directive to indicate that the next line is actually a part of
# the previous one.
#
# Example:
#       Input = "functionA(); functionB();"
#       Output = "functionA();
#                 # 10 "filename.cc"
#                             functionB();"
#
# \param lines         - lines of the file in a list
# \param startPosition - FilePosition of where to start looking for the next ";"
# \param filename      - filename for the preprocessor directive
# \param srcLine       - source line number for the preprocessor directive
#
# \return              - True if there was content after the ";" false otherwise
#
def markAndSeparateOnSemicolon(lines, startPosition, filename, srcLine):
    lineNum, offset = startPosition

    line = lines[lineNum]
    semiColon = line.find(";", offset)
    if semiColon == -1:
        return False

    cursor = semiColon + 1
    while cursor < len(line):
        c = line[cursor]
        if c >= "!" and c <= "~" and c != ";":
            # Something important after semi-colon found! Split the string!
            lines[lineNum] = line[:semiColon + 1] + "\r\n"
            newLine = " "*(semiColon + 1) + line[semiColon + 1:]
            lines.insert(lineNum + 1, newLine)

            marker = "# %d \"%s\"\r\n" % (srcLine, filename)
            lines.insert(lineNum + 1, marker)
            return True
        cursor = cursor + 1
    return False

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
            if isprintable(c) and not c.isspace():
                return (c, FilePosition(lineNum, offset))
            offset = offset + 1
        offset = 0
        lineNum = lineNum + 1

    return None

# Given a list of C/C++ source files that have been preprocessed by the GNU
# preprocessor with the -E option, identify all the FastLogger log statements
# and inject code in place of the statements to interface with the FastLogger
# runtime system. The processed files will be outputted as <filename>i
# (ex: test.i -> test.ii)
#
# \param FunctionGenerator used to generate interface code and maintain mappings
# \param inputFiles - list of preprocessed C/C++ files to process
#
def parseFiles(functionGenerator, inputFiles):
  for inputFile in inputFiles:
    outputFile = inputFile + "i"

    # Logical location in a file based on GNU Preprocessor directives
    ppFileName = inputFile
    ppLineNum = 0

    with open(inputFile) as f, open(outputFile, 'w') as output:
      lines = f.readlines()

      # Scan for instances of the LOG_FUNCTION using a simple heuristic,
      # which is to search for the LOG_FUNCTION outside of quotes. This
      # works because at this point, the file should already be pre-processed
      # by the C preprocessor so all the comments have been stripped and
      # all #DEFINE's have been resolved.
      lastChar = '\0'
      li = -1

      firstFilename = None
      while True:
        li = li + 1
        if li >= len(lines):
            break

        prevWasEscape = False
        inQuotes = False

        line = lines[li]

        # Keep track of of the preprocessor line number so that we can
        # put in our own line markers as we inject code into the file.
        ppLineNum = ppLineNum + 1

        # Parse special preprocessor directives that start with a #
        if line[0] == '#':
            # This denotes that it is a line marker
            if line[1] == ' ':
                i = 2
                lineNumStr = ""
                while(line[i] != ' '):
                    lineNumStr = lineNumStr + line[i]
                    i = i + 1

                # -1 since the line num describes the line after it, not the
                # current one, so we decrement it here before looping
                ppLineNum = int(float(lineNumStr)) - 1

                # +2 to skip the space and the first "
                i = i + 2
                ppFileName = ""
                while (line[i] != '\"'):
                    ppFileName = ppFileName + line[i]
                    i = i + 1

                if not firstFilename:
                    firstFilename = ppFileName
                    fg.clearLogFunctionsForCompilationUnit(firstFilename)

                i = i + 1
                flags = line[i:].strip()
                continue

        if ppFileName in ignored_files:
            continue

        # Holds log messages we will find along the way when parsing the
        # following line.
        i = -1
        while True:
          i = i + 1
          if i >= len(line):
              break

          c = line[i]

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
              if line[i + ii] != LOG_FUNCTION[ii]:
                found = False
                break

            if not found:
              continue

            filePosAfter = FilePosition(li, i + len(LOG_FUNCTION))
            # Valid identifier characters are [a-zA-Z_][a-zA-Z0-9_]*
            if lastChar.isalnum() or lastChar == '_':
              continue

            mChar, mPos = peekNextMeaningfulChar(lines, filePosAfter)
            if mChar != "(":
              continue

            # Okay at this point we are pretty sure we have a genuine
            # log statement, so parse it and start modifying the code!
            logStatement = parseLogStatement(lines, (li, i))
            fmtArg = logStatement['arguments'][FORMAT_ARG_NUM]
            args = logStatement['arguments'][FORMAT_ARG_NUM+1:]
            fmtString = extractCString(fmtArg.source)

            if fmtString == None:
              print("Non-constant String detected in %s line %d: %s" %
                            (ppFileName, ppLineNum, lines[li][i:]))
              assert(fmtString)
              # TODO(syang0) Have better error reporting

            (recordDecl, recordFn) = functionGenerator.generateLogFunctions(
                                                    fmtString, firstFilename,
                                                    ppFileName, ppLineNum)


            # Now we're ready to inject the code. What's going to happen is
            # that the original LOG_FUNCTION will be ripped out and in its
            # place, a function declaration and invocation for the record
            # logic will be inserted. It will look something like this:
            #
            #    input: "++i; LOG("Test, %d", 5); ++i;"
            #    output: "i++;
            #             # 1 "injectedCode.fake"
            #             { __syang0__fl__(const char * fmtId, int arg0)
            #             __syang0__fl__(
            #             # 10 "original.cc"
            #                     "Test, %d", 5); }
            #             # 10 "original.cc"
            #                                     ++i;"
            #
            # Note that we try to preserve spacing and use line preprocessor
            # directives wherever we can so that if the compiler reports
            # errors, then the erorrs can be consistent with the user's view
            # of the source file.

            # First we separate the code that comes after the log statement's
            # semicolon into its own line with the appropriate preprocessor
            # directive to mark where it originally came from.
            semiColonPPLineNum = \
                        ppLineNum + (logStatement['semiColonPos'].lineNum - li)
            if markAndSeparateOnSemicolon(lines, logStatement['semiColonPos'],
                                            ppFileName, semiColonPPLineNum):
                  line = lines[li]

            # Next, we're going to open a new curly brace in the injected code,
            # so we add a closing curly brace right after the semi-colon
            insertPos = logStatement['semiColonPos']
            lines[insertPos.lineNum] = \
                lines[insertPos.lineNum][:insertPos.offset + 1] + \
                "}\r\n" + \
                lines[insertPos.lineNum][insertPos.offset + 1:]

            # Lastly, we get rid of the original LOG_FUNCTION and replace
            # it with our generated definition and invocation in a new
            # scope (which the previous curly brace closes off)
            restOfLine = lines[li][(i + len(LOG_FUNCTION)):].rjust(len(line))
            lines[li] = lines[li][:i] + "\r\n"

            #TODO(syang0) turns out we can't __attribute__ non-var_args types...
            # we'll have to find another way to check for type errors...
            printfAttr = " __attribute__ ((format (printf, %d, %d))) " % \
                                        (FORMAT_ARG_NUM + 1, FORMAT_ARG_NUM + 2)

            # Note that this code has an open brace {
            codeToInject = [
              "# %d \"%s\"\r\n" % (1, "injectedCode.fake"),
              "{ " + recordDecl + ";\r\n",
              recordFn + "\r\n",
              "# %d \"%s\"\r\n" % (ppLineNum, ppFileName),
              restOfLine
            ]

            lines = lines[:li + 1] + codeToInject + lines[li + 1:]

          lastChar = c

      # Last step, retrieve the generated code and insert it at the end
      if firstFilename != ppFileName:
        print "Error: Expected preprocessed file to end in the compilation " \
              "unit %s but found %s instead" % (firstFilename, ppFileName)

      lines.append("\r\n\r\n# 1 \"generatedCode.h\" 3\n")
      recFns = functionGenerator.getRecordFunctionDefinitionsFor(firstFilename)
      recFns = "\r\n".join(recFns)
      lines += recFns

      # Output all the lines
      for line in lines:
        output.write(line)

      output.close()

if __name__ == "__main__":
  arguments = docopt(__doc__, version='LogStripper v1.0')

  fg = FunctionGenerator(arguments['--map'])
  if arguments['FILES']:
    parseFiles(fg, arguments['FILES'])

  if arguments['--output']:
    fg.outputCompilationFiles(arguments['--output'])

  if arguments['--map']:
    fg.outputMappingFile(arguments['--map'])