#! /bin/bash

######
# Measures the tail performance of NanoLog when delay is added in between
# log statements. This is intended to mimic a more realistic application
# where one expect a small amount of computation be introduced between
# log statements.
######

# Number of BENCH_OP operations to execute per size of staging buffer
COUNT=1000000000

# Number of delays to inject inbetween log statements.
# Each delay is injected via an rdtsc call, which incurs around 6ns of delay.
# [MIN_DELAYS, MAX_DELAYS] will be injected.
DELAYS="0 2 10 20 30"

# NANO_LOG statements to test the performance. Any new log statements added
# to this array will be tested with all staging buffer sizes.
declare -A BENCH_OPS
BENCH_OPS[staticString]="NANO_LOG(NOTICE, \"A\");"
BENCH_OPS[complexFormat]="NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"

# NanoLog Configuration: indicates how much time (in microseconds)
# to sleep if the background thread files no additional log statements to
# compress (note: due to linux kernel delays, the minimum value is actually
# 50µs when set to 1).
POLL_INTERVALS="0 1"



for POLL_INTERVAL in $POLL_INTERVALS
do
  for BENCH_OP_KEY in "${!BENCH_OPS[@]}";
  do
    BENCH_OP="${BENCH_OPS[$BENCH_OP_KEY]}"
    DELAY_OP="PerfUtils::Cycles::rdtsc();"

    for DELAY in $DELAYS;
    do
      OP="$BENCH_OP"
      for ((i=0; i < DELAY; ++i));
      do
        OP="$DELAY_OP $OP"
      done

      python genConfig.py --pollInterval=${POLL_INTERVAL} --iterations=${COUNT} --benchOp="$OP"
      ./run_bench.sh "varyingDelays_p${POLL_INTERVAL}_${BENCH_OP_KEY}_d${DELAY}"
      echo "Sleeping for 5 secs to wait for the SSD..."
      sync && sleep 5

    done # Delays
  done # BENCH_OP_KEY
done # POLL_INTERVAL

echo "# Restoring ../runtime/Config.h"
git checkout ../runtime/Config.h
echo "# Done"
date