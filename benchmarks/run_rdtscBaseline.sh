#! /bin/bash

#####
# This benchmark attempts to measure the variation in thread performance over
# time. It does this by taking 1 billion time points back-to-back using either
# rdtsc or high_resolution_clock and reports the inverse cdf of the time between
# operations.
#####

# Number of consecutive time stamples to take
COUNT=1000000000

DIR="results/$(date +%Y%m%d%H%M%S)_rdtscBaseline"
mkdir -p $DIR

make -j4 simpleTimeRecorder > /dev/null
./simpleTimeRecorder rdtsc $COUNT > ${DIR}/rdtsc.rcdf
./simpleTimeRecorder high_resolution_clock $COUNT > ${DIR}/high_resolution_clock.rcdf

