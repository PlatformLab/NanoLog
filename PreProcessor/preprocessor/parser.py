#! /usr/bin/python

"""Fast Logger Preprocessor

Usage:
    parser.py [-h] [-m MAP] FILES ...
    parser.py -c MAP [-o OUTPUT]

Options:
  -h --help         Show this help messages
  -m MAP            Filename for the previously generated mappings. This option
                    will both read in the file (if it exists) and overwrite
                    it with new contents
  -c MAP            Convert the previously generated mapping into a compilable
                    C++ file
  -o OUTPUT         Optional output name for the compilable C++ output
                    [default: BufferStuffer.h]


This script is 1 part of 3 part system that enables fast, submillisecond logging.
 This component takes in pre-processed C++ files, extracts the static format
 string used in a specified log message and replaces it with an identifier unique
 to every instance of that static string. This script will then output the modified,
 pre-processed C++ files (appended with a .i extension) and a JSON mapping file in the
 following format:
 {
   "mappings":[
     "string0",
     "string1",
     ...
   ],
   "metadata":{
     totalMappings": <integer>
   }
 }

The resultant C++ files are intended to be compiled with the runtime environment
 (the 2nd part of the system) which will compress the log messages and the mapping
 file is intended to be used with a Dmangler that interprets the compressed log files.

To support partial recompilation, this script also takes in an optional mapping file
 which will be used to preserve prior mappings.

"""
from docopt import docopt

import json
from collections import namedtuple
from FunctionGenerator import *

# Simple structure to identify a position within a file via a line number
# and an offset on that line
FilePosition = namedtuple('FilePosition', ['lineNum', 'offset'])

# A simple tuple that contains two FilePosition tuples to denote the start
# and end of a range within a file
FileRange = namedtuple('FileRange', ['begin', 'end'])

# Encapsulates an argument that has been printed
Argument = namedtuple('Argument', ['source', 'fileRange'])


# Log function to search for within the C++ files and perform the replacement
# and stripping of format strings.
# LOG_FUNCTION = "TimeTrace::record"
LOG_FUNCTION = "RAMCLOUD_LOG"

# Marks which argument of the record function is the static format string
FORMAT_ARG_NUM = 1

# Once a valid LOG_FUNCTION is found and the format string removed, replace
# the log function with this function instead
# LOG_FUNCTION_REPLACEMENT = "TimeTrace::rec_p"
LOG_FUNCTION_REPLACEMENT = "RAMCLOUD_NOP"

# If there's an error of some sort, such as not being able to identify the
# format string, the parser will instead replace it with this result.
# LOG_FUNCTION_NOT_CONST_FMT = "TimeTrace::record"
LOG_FUNCTION_NOT_CONST_FMT = "SLOW_LOG"


# Set of files that are to be ignored by the pre-procesor
ignored_files = set([
    "folder/lib.h"
])


# Initialization function that checks that the replacement functions are shorter
# than the original log function and pads to the length of the original log function.
# This is required to preserve spacing between the pre-processed and post-processed
# C++ files so that G++ errors are easier to read.
def init():
  global LOG_FUNCTION, LOG_FUNCTION_NOT_CONST_FMT, LOG_FUNCTION_REPLACEMENT
  assert len(LOG_FUNCTION) >= len(LOG_FUNCTION_NOT_CONST_FMT)
  while len(LOG_FUNCTION_NOT_CONST_FMT) < len(LOG_FUNCTION):
    LOG_FUNCTION_NOT_CONST_FMT = " " + LOG_FUNCTION_NOT_CONST_FMT

  assert len(LOG_FUNCTION) >= len(LOG_FUNCTION_REPLACEMENT)
  while len(LOG_FUNCTION_REPLACEMENT) < len(LOG_FUNCTION):
    LOG_FUNCTION_REPLACEMENT = " " + LOG_FUNCTION_REPLACEMENT

# Given a single line string, attempt the replace the contents between
# beginOffset and endOffset with the string replacement. If the range
# is longer than the replacement string, it will be padded with spaces
# and if the range is shorter than the replacement string, the replacement
# will fail and false is returned.
#
# \param line - single line to attempt replacement on
# \param beginOffset - beginning offset in the line to attempt the replacement
# \param endOffset - ending offset in tthe line to attempt to replacement
#
# \return - true if the replacement was sucessful

def attemptReplace(line, beginOffset, endOffset, replacement):
  replaced = True
  if endOffset - beginOffset <  len(replacement):
    replaced = False
    replacement = ""
  newLine = line[:beginOffset] + replacement.ljust(endOffset - beginOffset) + line[endOffset:]
  return (newLine , replaced)


# Attempt to replace the lines specified in argRange with the replacementString
# while ensuring that the overall character length of argRange is preserved.
#
# \param lines - the lines in a file
# \param argRange - FileRange specifying the beginning and end of the
#                   replacement range
# \return - true if the replacement was sucessful

def clearAndAttemptReplace(lines, argRange, replacementString):
  begin, end = argRange
  # One liner
  if begin.lineNum == end.lineNum:
    newLine, replaced = attemptReplace(lines[begin.lineNum], begin.offset, end.offset, replacementString)
    lines[begin.lineNum] = newLine
  else:
    # Attempt to put the replacement on the first line, if it fits
    newLine, replaced = attemptReplace(lines[begin.lineNum], begin.offset, len(lines[begin.lineNum]) - 1, replacementString)
    lines[begin.lineNum] = newLine

    # If it doesn't fit, attempt to replace it on the last line
    newLine, replaced = attemptReplace(lines[end.lineNum], 0, end.offset, "" if replaced else replacementString)
    lines[end.lineNum] = newLine

    # If it doesn't fit on the first or the second, then try all the in between.
    for li in range(begin.lineNum + 1, end.lineNum):
      lines[li], replaced = attemptReplace(lines[li], 0, len(lines[li]) - 1, "" if replaced else replacementString)

  return replaced

# Given a set of lines from a C++ program, attempt to parse the lines a single
# static string. These lines should meet the C++ standard of strings and
# should not contain any extraneous characters outside of the string (such as
# commas). The expect input is along the lines of \"blah blah\"
#
# \param srcStr - source source string to parse
#
# \return - argument as a string
def extractCString(srcStr):
  returnString = ""
  isInQuotes = False
  prevWasEscape = False

  for line in srcStr.splitlines(True):
    for c in line:
      if c == "\"" and not prevWasEscape:
        isInQuotes = not isInQuotes
      elif isInQuotes:
        returnString += c
        prevWasEscape = c == "\\"
      else:
        if not (c == " " or c == "\r" or c == "\n"):
          return None

  return returnString

# Attempt to find the end of an argument given its start position. The start
# position should be either immediately after the first left parenthesis or
# after the previous argument's comma and the end position will be the next
# comma or the ending right parenthesis.
#
# \param lines          - all lines of the file
# \param startPosition  - FilePositition denoting the start of the argument
#
# \return - a FilePosition denoting the end of the parameter
def parseArgumentStartingAt(lines, startPos):

  # The algorithm uses the heuristic of assuming that the argument ends
  # when it finds a terminating character (either a comma or right parenthesis)
  # in a position where the relative parenthesis/curlyBraces/bracket depth is 0.
  # The latter constraint prevents false positives where function calls are used
  # to generate the parameter (i.e. log("number is %d", calculate(a, b)))

# TODO(syang0) Wait... this heuristic can fail if the argument is a string and
# contains a stray left or right bracket.... Shoot.

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
        argRange = FileRange(startPos, endPos)
        return Argument(argSrcStr[:-1], argRange);

    # Couldn't find it on this line, must be on the next
    offset = 0
    lineNum = lineNum + 1



# Find the start and end positions of the arguments of the LOG_FUNCTION
# starting at the startLineNum, startOffset
#
# \param lines          - all the lines of the file
# \param startPosition  - tuple containing the line number and offset where
#                         the LOG_FUNCTION starts
#
# \return a list of Argument tuples marking the beginning and ends of
#         each of the arguments and its original source
def parseLogStatement(lines, startPosition):
  lineNum, offset = startPosition;
  assert lines[lineNum].find(LOG_FUNCTION, offset) == offset

  logStatement = {'startPos': startPosition}

  # Find the left parenthesis after the LOG_FUNCTION identifier
  offset += len(LOG_FUNCTION_NOT_CONST_FMT);
  while(lines[lineNum].find("(", offset) == -1):
    lineNum = lineNum + 1
    offset = 0

  offset = lines[lineNum].find("(", offset)
  logStatement['openParen'] = FilePosition(lineNum, offset)

  # Identify all the argument start and end positions
  args = []
  while lines[lineNum][offset] != ")":
    offset = offset + 1
    startPos = FilePosition(lineNum, offset);
    arg = parseArgumentStartingAt(lines, startPos);
    args.append(arg)

    lineNum, offset = arg.fileRange.end

  logStatement['closeParen'] = FilePosition(lineNum, offset);
  logStatement['args'] = args;

  # To finish this off, find the closing semicolon:
  char, pos = peekNextMeaningfulChar(lines, FilePosition(lineNum, offset + 1))
  assert (char == ";")
  logStatement['semiColonPos'] = pos

  return logStatement


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



# Takes the result returned by parseLogMessage and pretty prints it as a string
def printLogMsgArgs(lines, listOfArgs):
    argNum = 0
    for arg in listOfArgs:
        source, argRange = arg
        print "Arg %d: %s" % (argNum, source)
        argNum = argNum + 1

    print ""

def isprintable(s, codec='utf8'):
    try: s.decode(codec)
    except UnicodeDecodeError: return False
    else: return True

# Starting at filePos, find the next valid character that is syntatically
# important for the C/C++ program.
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

    # If we reach here, there's an error.


def parseFile(mappingFile, inputFiles):
    fg = FunctionGenerator(mappingFile)

    for inputFile in inputFiles:
      outputFile = inputFile + "i"

      filenameStr = inputFile
      ppLineNum = 0
      with open(inputFile) as f, open(outputFile, 'w') as output:
        lines = f.readlines()

        # Scan for instances of the LOG_FUNCTION using a simple heuristic,
        # which is to search for the LOG_FUNCTION outside of quotes. This
        # works because at this point, the file should already be pre-processed
        # so all the comments have been stripped any any #DEFINE's have been
        # resolved.
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

          # Keep track of of the preprocessor line nums so that we
          # can put in our own line markers as we edit the file.
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
                  # current one
                  ppLineNum = int(float(lineNumStr)) - 1

                  # +2 to skip the space and the first "
                  i = i + 2
                  filenameStr = ""
                  while (line[i] != '\"'):
                      filenameStr = filenameStr + line[i]
                      i = i + 1

                  if not firstFilename:
                      firstFilename = filenameStr

                  i = i + 1;
                  flags = line[i:].strip()

          if filenameStr in ignored_files:
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
            #       a part of a longer identifer name)
            #  (c) the next syntatical character after log function is a (
              found = True
              for ii in range(len(LOG_FUNCTION)):
                if line[i + ii] != LOG_FUNCTION[ii]:
                  found = False
                  break

              if found:
                filePosAfter = FilePosition(li, i + len(LOG_FUNCTION));
                # Valid identifier characters are [a-zA-Z_][a-zA-Z0-9_]*
                if lastChar.isalnum() or lastChar == '_':
#                  print "At line %d (%s), failed due to last char being '%c'" % (ppLineNum, line.strip(), lastChar)
                  found = False

              if found:
                mChar, mPos = peekNextMeaningfulChar(lines, filePosAfter)
                if mChar != "(":
                  found = False
#                  print "at line %d (%s), failed due to next printableChar being '%c'" % (ppLineNum, line.strip(), mChar)

              if found:
                logStatement = parseLogStatement(lines, (li, i))


                if markAndSeparateOnSemicolon(lines, logStatement['semiColonPos'], filenameStr, ppLineNum):
                    line = lines[li]

                fmtArg = logStatement['args'][FORMAT_ARG_NUM]
                fmtString = extractCString(fmtArg.source)
                if fmtString == None:
                  print("Non-constant String detected: %s" % lines[li][i:])
                  # lines[li] = line[:i] + line[i:].replace(
                  #            LOG_FUNCTION, LOG_FUNCTION_NOT_CONST_FMT, 1)

                  # if we encounter a slow log, just leave the code in place.

                else:

                  # Replace line with nop function
                  line = line[:i] + line[i:].replace(LOG_FUNCTION,
                                                LOG_FUNCTION_REPLACEMENT, 1)
                  lines[li] = line


                  # Code Injection time!

                  # Clip the front of the args so that we get everything after
                  # format string
                  args = logStatement['args'][FORMAT_ARG_NUM+1:]


                  (recordDecl, recordInvoke) = fg.generateLogFunctions(
                        fmtString, args, firstFilename, filenameStr, ppLineNum)

                  codeToInject = [
                    recordDecl + ";\n",
                    recordInvoke + ";\n"
                  ]

                  insertPos = logStatement['semiColonPos'].lineNum + 1
                  lines = lines[:insertPos]  + codeToInject + lines[insertPos:]

            lastChar = c

        # Last step, retrieve the generated code and insert it at the end
        if firstFilename != filenameStr:
            print "Error: Stephen Yang made the wrong assumption about EOF"

        lines.append("# 1 \"generatedCode.h\" 3\n")
        recFns = fg.getRecordFunctionDefinitionsFor(firstFilename)
        recFns = "\n".join(recFns);

        lines += recFns

        # Output all the lines
        for line in lines:
          output.write(line)

        output.close()

    fg.outputMappingFile(mappingFile)

#    mapOutput.write(json.dumps({'mappings':mappingsAsAList, 'metadata':metadata},
#                                sort_keys=True, indent=4, separators=(',', ': ')))
#    mapOutput.close()


# Now we attempt to identify all the valid RAMCLOUD_LOG lines
# We note that
# a) RAMCLOUD_LOG should not be contained by quotes "" and should have ()
# b) Arguments are separated by comments when quotes (non-escaped), brackets,
#    and parentehsis are at the same level


if __name__ == "__main__":
  arguments = docopt(__doc__, version='LogStripper v1.0')

  init()
  if arguments['-c']:
      fg = FunctionGenerator(arguments['-c'])
      fg.outputCompilationFiles(arguments['-o'])
  else:
    parseFile(arguments['-m'], arguments['FILES'])

