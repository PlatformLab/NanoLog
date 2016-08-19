#! /usr/bin/python

"""Usage: parser.py [-h] [-m MAP] FILES ...

Options:
  -h --help         show this
  -m MAP            Previously generated mapping file for partial compilation


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

import sys
import json
from collections import namedtuple

# Simple structure to identify a position within a file via a line number
# and an offset on that line
FilePosition = namedtuple('FilePosition', ['lineNum', 'offset'])

# A simple tuple that contains two FilePosition tuples to denote the start
# and end of a range within a file
FileRange = namedtuple('FileRange', ['begin', 'end'])


# Log function to search for within the C++ files and perform the replacement
# and stripping of format strings.
# LOG_FUNCTION = "TimeTrace::record"
LOG_FUNCTION = "RAMCLOUD_LOG"

# Marks which argument of the record function is the static format string
FORMAT_ARG_NUM = 1

# Once a valid LOG_FUNCTION is found and the format string removed, replace
# the log function with this function instead
# LOG_FUNCTION_REPLACEMENT = "TimeTrace::rec_p"
LOG_FUNCTION_REPLACEMENT = "FAST_LOG"

# If there's an error of some sort, such as not being able to identify the
# format string, the parser will instead replace it with this result.
# LOG_FUNCTION_NOT_CONST_FMT = "TimeTrace::record"
LOG_FUNCTION_NOT_CONST_FMT = "SLOW_LOG"


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
# \param lines - lines to parse
#
# \return - argument as a string
def parseLinesAsString(lines):
  returnString = ""
  isInQuotes = False
  prevWasEscape = False

  for line in lines:
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
# Parse the argument contained in lines as specified by argRange, attempt to
# parse the range as a C++ string. Multi-line strings are supported, however,
# the argument should conform to the C++ standard for strings and should not
# contain any extraneous characters outside the quotes (such as commas).
#
# \param lines - lines in the file
# \param argRange - The FileRange that specifies the beginning and
#                   end of the argument
#
# \return - the parsed string
def parseArgumentAsString(lines, argRange):
  begin, end = argRange

  # If it's a one liner
  if begin.lineNum == end.lineNum:
    oneLiner = lines[begin.lineNum][begin.offset:end.offset]
    return parseLinesAsString(oneLiner)

  # Mutli Line
  else:
    firstLine = lines[begin.lineNum][begin.offset:]
    lastLine = lines[end.lineNum][:end.offset]

    linesToParse = [firstLine]
    for li in range(begin.lineNum + 1, end.lineNum):
      linesToParse.append(lines[li])
    linesToParse.append(lastLine)
    return parseLinesAsString(linesToParse)


# Attempt to find the end of an argument given its start position. The start
# position should be either immediately after the first left parenthesis or
# after the previous argument's comma and the end position will be the next
# comma or the ending right parenthesis.
#
# \param lines          - all lines of the file
# \param startPosition  - FilePositition denoting the start of the argument
#
# \return - a FilePosition denoting the end of the parameter
def findEndOfArgument(lines, startLineNum, startOffset):

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

  lineNum = startLineNum
  offset = startOffset

  while True:
    prevWasEscape = False
    line = lines[lineNum]

    for i in range(offset, len(line)):
      c = line[i]
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
        return FilePosition(lineNum, i)

    # Couldn't find it on this line, must be on the next
    offset = 0
    lineNum += 1


# Find the start and end positions of the arguments of the LOG_FUNCTION
# starting at the startLineNum, startOffset
#
# \param lines          - all the lines of the file
# \param startPosition  - tuple containing the line number and offset where
#                         the LOG_FUNCTION starts
#
# \return a list of FileRange tuples marking the beginning and ends of
#         each of the arguments
def findArguments(lines, startPosition):
  lineNum, offset = startPosition;
  assert lines[lineNum].find(LOG_FUNCTION, offset) == offset

  # Find the left parenthesis after the LOG_FUNCTION identifier
  offset += len(LOG_FUNCTION_NOT_CONST_FMT);
  while(lines[lineNum].find("(", offset) == -1):
    lineNum = lineNum + 1
    offset = 0

  # Identify all the argument start and end positions
  returnArgs = []
  while lines[lineNum][offset] != ")":
    offset = offset + 1
    startPos = FilePosition(lineNum, offset);
    endPos = findEndOfArgument(lines, lineNum, offset)
    returnArgs.append(FileRange(startPos, endPos))

    lineNum, offset = endPos

  return returnArgs

# Takes the result returned by parseLogMessage and pretty prints it as a string
def printLogMsgArgs(lines, listOfArgs):
  returnString = ""

  for arg in listOfArgs:
    returnString += "\nArg:\t"
    if arg[0] == arg[2]:
      returnString += lines[arg[0]][arg[1]:arg[3]]
    else:
      returnString += lines[arg[0]][arg[1]:]
      for li in range(arg[0]+1, arg[2]):
        returnString += lines[li]
      returnString += lines[arg[2]][:arg[3]]

  return returnString

def parseFile(mappingFile, inputFiles):
  # Mapping of String -> ID
  uniqStrings = {}

  # Restores the previous mapping, if it exists
  if mappingFile:
    with open(mappingFile) as json_file:
      prevMappings = json.load(json_file)["mappings"];
      for string in prevMappings:
        uniqStrings[string] = len(uniqStrings)

  with open("mappings.map", 'w') as mapOutput:
    for inputFile in inputFiles:
      outputFile = inputFile + "i"
      with open(inputFile) as f, open(outputFile, 'w') as output:
        lines = f.readlines()

        # Scan for instances of the LOG_FUNCTION using a simple heuristic,
        # which is to search for the LOG_FUNCTION outside of quotes. This
        # works because at this point, the file should already be pre-processed
        # so all the comments have been stripped any any #DEFINE's have been
        # resolved.
        for li in range(len(lines)):
          prevWasEscape = False
          inQuotes = False

          line = lines[li]
          for i in range(len(line)):
            c = line[i]

            # If escape, we don't really care about the next char
            if c == "\\" or prevWasEscape:
              prevWasEscape = not prevWasEscape
              continue

            if c == "\"":
              inQuotes = not inQuotes

            # If we match the first character, cheat a little and scan forward
            if c == LOG_FUNCTION[0] and not inQuotes:
              found = True
              for ii in range(len(LOG_FUNCTION)):
                if line[i + ii] != LOG_FUNCTION[ii]:
                  found = False
                  break

              if found:
                startPosition = FilePosition(li, i)
                listOfArgs = findArguments(lines, startPosition)
                fmtArg = listOfArgs[FORMAT_ARG_NUM]
                fmtString = parseArgumentAsString(lines, fmtArg)

                if fmtString == None:
                  print("Non-constant String detected: %s" % lines[li][i:])
                  # lines[li] = line[:i] + line[i:].replace(
                  #            LOG_FUNCTION, LOG_FUNCTION_NOT_CONST_FMT, 1)

                  # if we encounter a slow log, just leave the code in place.

                else:
                  id = uniqStrings.get(fmtString, None)
                  if id == None:
                    id = len(uniqStrings)
                    uniqStrings[fmtString] = id

                  # Replace line with fast print
                  line = line[:i] + line[i:].replace(LOG_FUNCTION,
                                                LOG_FUNCTION_REPLACEMENT, 1)
                  lines[li] = line

                  # Replace fmt string with id
                  clearAndAttemptReplace(lines, fmtArg, "%d" % id)



        # Output all the lines
        for line in lines:
          output.write(line)

        output.close()

    # Output map file in a json format
    metadata = {'totalMappings':len(uniqStrings)}
    id2string = {}

    for string in uniqStrings:
      id2string[uniqStrings[string]] = string

    mappingsAsAList = []
    for i in range(0, len(uniqStrings)):
      mappingsAsAList.append(id2string[i])

    mapOutput.write(json.dumps({'mappings':mappingsAsAList, 'metadata':metadata},
                                sort_keys=True, indent=4, separators=(',', ': ')))
    mapOutput.close()

# Now we attempt to identify all the valid RAMCLOUD_LOG lines
# We note that
# a) RAMCLOUD_LOG should not be contained by quotes "" and should have ()
# b) Arguments are separated by comments when quotes (non-escaped), brackets,
#    and parentehsis are at the same level


if __name__ == "__main__":
  arguments = docopt(__doc__, version='LogStripper v1.0')

  init()
  parseFile(arguments['-m'], arguments['FILES'])

