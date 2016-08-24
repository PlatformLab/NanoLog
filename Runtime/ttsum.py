#!/usr/bin/env python

# Copyright (c) 2014 Stanford University
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

"""
This program reads a log file and generates summary information for
time trace information in the files.
"""

from __future__ import division, print_function
from glob import glob
from optparse import OptionParser
import math
import os
import re
import string
import sys

# This variable collects all the times for all events, individually. It is
# a dictionary that maps from key names to a list containing all of the
# intervals for that event name (each interval is the elapsed time between
# the most recent previous event and this event).
eventIntervals = {}

# This variable collects information for all events relative to a given
# starting event (the --from command line option).
#
# relativeEvents:
#     dictionary (event name => OccurrenceList)
#
# The same event may sometimes happen multiple times for single occurrence
# of the starting event. An OccurrenceList is a list, where the nth entry
# describes all the events that occurred after n prior occurrences of the
# event.
# OccurrenceList:
#    list (OccurrenceInfo)
#
# OccurrenceInfo:
#    dictionary (
#        times: list()        One entry for each event: elapsed ns between
#                             the starting event and this event
#        intervals: list()    One entry for each event: elapsed ns between
#                             immediately preceding event and this event
#    )

relativeEvents = {}

# This variable contains a count of the number of times each event has
# occurred since the last time the starting event occurred.

eventCount = {}

# This variable holds the names of events in the order in which they originally
# occurred. It is used for breaking ties when sorting by trace value relative
# to some starting event, especially when we are looking at a CacheTrace
# instead of a TimeTrace.
eventNames = []

def scan(f, startingEvent):
    """
    Scan the log file given by 'f' (handle for an open file) and collect
    information from time trace records as described by the arguments.
    If 'startingEvent' isn't None, it specifies an event indicating the
    beginning of a related sequence of event; times are accumulated for all
    other events, relative to the most recent occurrence of the starting event.
    """

    foundStart = False
    startTime = 0.0
    lastTime = -1.0
    for line in f:
        match = re.match(' *([0-9.]+) ns \(\+ *([0-9.]+) ns\): (.*)', line)
        if not match:
            match = re.match(' *([0-9.]+) misses \(\+ *([0-9.]+) misses\): (.*)', line)
            if not match:
                continue
        thisEventTime = float(match.group(1))
        thisEventInterval = float(match.group(2))
        thisEvent = match.group(3)
        if (thisEventTime < lastTime):
            print('Time went backwards at the following line:\n%s' % (line))
        lastTime = thisEventTime
        if (thisEventTime != 0.0) :
            if not thisEvent in eventIntervals:
                eventIntervals[thisEvent] = []
            eventIntervals[thisEvent].append(thisEventInterval)
            # print('%s %s %s' % (thisEventTime, thisEventInterval, thisEvent))
        if thisEvent not in eventNames:
            eventNames.append(thisEvent)
        if startingEvent:
            if string.find(thisEvent, startingEvent) >= 0:
                # Reset variables to indicate that we are starting a new
                # sequence of events from the starting event.
                startTime = thisEventTime
                foundStart = True
                eventCount = {}

            if not foundStart:
                continue

            # If we get here, it means that we have found an event that
            # is not the starting event, and startTime indicates the time of
            # the starting event. First, see how many times this event has
            # occurred since the last occurrence of the starting event.
            relativeTime = thisEventTime - startTime
            # print('%.1f %.1f %s' % (relativeTime, thisEventInterval, thisEvent))
            if thisEvent in eventCount:
                count = eventCount[thisEvent] + 1
            else:
                count = 1
            eventCount[thisEvent] = count
            # print("Count for '%s': %d" % (thisEvent, count))
            if not thisEvent in relativeEvents:
                relativeEvents[thisEvent] = []
            occurrences = relativeEvents[thisEvent]
            while len(occurrences) < count:
                occurrences.append({'times': [], 'intervals': []})
            occurrences[count-1]['times'].append(relativeTime)
            occurrences[count-1]['intervals'].append(thisEventInterval)



# Parse command line options
parser = OptionParser(description=
        'Read one or more log files and summarize the time trace information '
        'present in the file(s) as specified by the arguments.',
        usage='%prog [options] file file ...',
        conflict_handler='resolve')
parser.add_option('-f', '--from', type='string', dest='startEvent',
    help='measure times for other events relative to FROM; FROM contains a '
            'substring of an event')

(options, files) = parser.parse_args()
if len(files) == 0:
    print("No log files given")
    sys.exit(1)
for name in files:
    scan(open(name), options.startEvent)

# Print information about all events, unless --from was specified.
if not options.startEvent:
    # Do this in 2 passes. First, generate a string describing each
    # event; then sort the list of messages and print.

    # Each entry in the following variable will contain a list with
    # 2 elements: time to use for sorting, and string to print.
    outputInfo = []

    # Compute the length of the longest event name.
    nameLength = 0;
    for event in eventIntervals.keys():
        nameLength = max(nameLength, len(event))

    # Each iteration through the following loop processes one event name
    for event in eventIntervals.keys():
        intervals = eventIntervals[event]
        intervals.sort()
        medianTime = intervals[len(intervals)//2]
        message = '%-*s  %8.1f %8.1f %8.1f %8.1f %7d' % (nameLength,
            event, medianTime, intervals[0], intervals[-1],
            sum(intervals)/len(intervals), len(intervals))
        outputInfo.append([medianTime, message, event])

    # Pass 2: sort in order of median interval length, then print.
    outputInfo.sort(key=lambda item: item[0], reverse=True)
    print('%-*s    Median      Min      Max  Average   Count' % (nameLength,
            "Event"))
    print('%s---------------------------------------------' %
            ('-' * nameLength))
    for message in outputInfo:
        print(message[1])

# Print output for the --from option. First, process each event occurrence,
# then sort them by elapsed time from the starting event.
if options.startEvent:
    # Each entry in the following variable will contain a list with
    # 3 elements: time to use for sorting, and string to print, and a event
    # name for secondary sorting.
    outputInfo = []

    # Compute the length of the longest event name.
    nameLength = 0;
    for event in relativeEvents.keys():
        occurrences = relativeEvents[event]
        thisLength = len(event)
        if len(occurrences) > 1:
            thisLength += len(' (#%d)' % (len(occurrences)))
        nameLength = max(nameLength, thisLength)

    # Each iteration through the following loop processes one event name
    for event in relativeEvents.keys():
        occurrences = relativeEvents[event]

        # Each iteration through the following loop processes the nth
        # occurrences of this event.
        for i in range(len(occurrences)):
            eventName = event
            if i != 0:
                eventName = '%s (#%d)' % (event, i+1)
            times = occurrences[i]['times']
            intervals = occurrences[i]['intervals']
            times.sort()
            medianTime = times[len(times)//2]
            intervals.sort()
            message = '%-*s  %8.1f %8.1f %8.1f %8.1f %8.1f %7d' % (nameLength,
                eventName, medianTime, times[0], times[-1],
                sum(times)/len(times), intervals[len(intervals)//2],
                len(times))
            outputInfo.append([medianTime, message, event])

    # Rotate eventNames so that the starting event appears first in the list,
    # and we can sort other events based on the index in eventNames.
    startIndex = 0
    for index, eventName in enumerate(eventNames):
        if options.startEvent in eventName:
            startIndex = index
            break
    eventNames = eventNames[startIndex:] + eventNames[:startIndex]

    # This function compares events in a TimeTrace or CacheTrace first by the
    # median value relative to the strarting event, and then by the order they
    # appeared in the trace, relative to the starting event.
    def eventComparator(x,y):
        if x[0] < y[0]: return -1
        if x[0] > y[0]: return 1
        if eventNames.index(x[2]) < eventNames.index(y[2]):
            return -1
        if eventNames.index(x[2]) > eventNames.index(y[2]):
            return 1
        return 0

    outputInfo.sort(cmp=eventComparator)
    print('%-*s    Median      Min      Max  Average    Delta   Count' % (nameLength,
            "Event"))
    print('%s------------------------------------------------------' %
            ('-' * nameLength))
    for message in outputInfo:
        print(message[1])
