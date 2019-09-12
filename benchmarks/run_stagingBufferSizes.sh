#! /bin/bash

######
# Measure the performance of NanoLog when varying the Staging Buffer size and
# sleep intervals with repeated invocations of NANO_LOG statements.
# This benchmark shows up in the NanoLog Paper and dissertation
# as "stagingBufferSizes."
######

# NANO_LOG statements to test the performance. Any new log statements added
# to this array will be tested with all staging buffer sizes.
declare -A BENCH_OPS
BENCH_OPS[complexFormat]="NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"
BENCH_OPS[staticString]="NANO_LOG(NOTICE, \"A\");"

# Number of BENCH_OP operations to execute per size of staging buffer
COUNT=1000000000

# NanoLog Configuration: Specifies the mimum and maximium sizes of the staging
# buffer in powers of two. All values inbetween will be tested as well.
MIN_EXP=12
MAX_EXP=23

# NanoLog Configuration: indicates how much time (in microseconds)
# to sleep if the background thread files no additional log statements to
# compress (note: due to linux kernel delays, the minimum value is actually
# 50µs when set to 1).
POLL_INTERVALS="0 1"

# NanoLog Configuration:
# Indicates what the "Release Threshold" or how many bytes maximum the
# the background thread should process between releasing the space back to the
# logging thread. This is indicated in a power of 2, and the 'c'
# refers to an internal loop variable that indicates the current size of the
# current staging buffer (in a power of 2).
for RELEASE_EXP in c-1 11;
do
  for POLL_INTERVAL in $POLL_INTERVALS
  do
    for BENCH_OP_KEY in "${!BENCH_OPS[@]}";
    do
      BENCH_OP="${BENCH_OPS[$BENCH_OP_KEY]}"

      for ((c=$MAX_EXP; c>=$MIN_EXP; c--))
      do
      ((THRESHHOLD=${RELEASE_EXP})) # Evaluates the value held within
      python genConfig.py --releaseThresholdExp=$THRESHHOLD --pollInterval=${POLL_INTERVAL} --stagingBufferExp=${c} --iterations=${COUNT} --benchOp="$BENCH_OP"
      ./run_bench.sh "stagingBuffer_${BENCH_OP_KEY}_pow${c}_r${THRESHHOLD}_p${POLL_INTERVAL}"
      echo "Sleeping for 5 secs to wait for the SSD..."
      sync && sleep 5

      done # for c
    done # BENCH_OP_KEY
  done # POLL_INTERVAL
done # RELEASE_EXP
