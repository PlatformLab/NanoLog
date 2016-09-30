#!/usr/bin/env python2
#encoding: UTF-8

import re
from collections import namedtuple

# This class encapsulates a source file. It keeps track of every line
# and their symbolic mapping back to the original file and assists with the
# injection of arbitrary code by automagically generating line markers and
# preserving spaces

Line = namedtuple('Line', ['is_meta', 'raw_line', 'filename', 'lineno'])
FilePos = namedTuple('FilePos', ['lineIndex', 'lineOffet'])


class SourceFile(object):

    def __init__(self, input_file):
        self.lines = []
        self.hasInjectedCodeBefore = False

        lineno = 0
        filename = ""
        flags = ""
        raw_lines = open(input_file).readlines()
        for line in raw_lines:
            lines.append(line)

            # Denotes a line marker which is in the format of
            #     '# lineNum "filename" flags'
            lineMarker = re.match('^# (\d+) \"([^\"]+)\"(.*)', line, re.L)
            if lineMarker:
                lineno = lineMarker.group(1)
                filename = lineMarker.group(2)
                flags = lineMarker.group(3)

                lines.append(Line(True, line, None, None))

            else:
                lineno += 1
                lines.append(Line(False, line, filename, lineno))

    def serialize(self, output_filename):
        with open(output_filename, 'w') as output:
            for line in self.lines:
                output.write(line)
            output.close()



    # Dummy atm
    def injectCode(self, code):
        lines.append(code)

