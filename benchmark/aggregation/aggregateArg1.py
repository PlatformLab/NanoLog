#!/usr/bin/python

import sys
import re

def main(argv):
  regex = re.compile('.*# (\d+)')
  min = None
  max = None
  total = 0
  count = 0

  with open(argv[0]) as iFile:
    for line in iFile:
      match = regex.match(line)
      if (match):
        number = int(match.group(1))
        if not min or min > number:
          min = number

        if not max or max < number:
          max = number

        total += number
        count += 1

    print "min: %d" % min
    print "max: %d" % max
    print "avg: %d" % (total/count)
    print "count: %d" % count
    print "total: %d" % total

if __name__ == "__main__":
   main(sys.argv[1:])