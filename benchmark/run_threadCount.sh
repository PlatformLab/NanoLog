#! /bin/bash

#####################
# Tests the affect of threading. In a perfect world, the throughput should go
# down linearly with the number of threads (since they're all computing for
# scarce IO bandwidth)
# In the paper as threadCount
#####################
# Even Power of 2 means easily divisible into even powers of two threads
TOTAL_ITERATIONS=134217728

TMP_FULL="/tmp/$(date +%Y%m%d%H%M%S)_1.txt"
TMP_NO_OUTPUT="/tmp/$(date +%Y%m%d%H%M%S)_2.txt"
TMP_NO_COMPACT_NO_OUTPUT="/tmp/$(date +%Y%m%d%H%M%S)_3.txt"
TMP_NO_COMPACT="/tmp/$(date +%Y%m%d%H%M%S)_4.txt"

LOG_FILE="results/$(date +%Y%m%d%H%M%S)_threadCount.txt"


printf "# Logging a static string ${TOTAL_ITERATIONS} times\r\n" |& tee -a $LOG_FILE
echo "# Note: Segmentation Faults for './decompressor rcdfTime' is"
echo "# benign when compaction is turned off"

for ((threads=1; threads<=16; threads += 1))
do
  ((ITTRS=TOTAL_ITERATIONS/${threads}))

  python genConfig.py --iterations=${ITTRS} --threads=${threads}
  ./run_bench.sh "threadCount${threads}" > /dev/null
  tail -n 1 results/$(ls results | grep threadCount${threads} | tail -n 1)/threadCount${threads}.log |& tee -a $TMP_FULL
  sync; sleep 5

  python genConfig.py --iterations=${ITTRS} --threads=${threads} --disableOutput
  ./run_bench.sh "threadCount${threads}" > /dev/null
  tail -n 1 results/$(ls results | grep threadCount${threads} | tail -n 1)/threadCount${threads}.log |& tee -a $TMP_NO_OUTPUT
  sync; sleep 5

  python genConfig.py --iterations=${ITTRS} --threads=${threads} --disableOutput --disableCompaction
  ./run_bench.sh "threadCount${threads}" > /dev/null
  tail -n 1 results/$(ls results | grep threadCount${threads} | tail -n 1)/threadCount${threads}.log |& tee -a $TMP_NO_COMPACT_NO_OUTPUT
  sync; sleep 5

  python genConfig.py --iterations=${ITTRS} --threads=${threads} --disableCompaction
  ./run_bench.sh "threadCount${threads}" > /dev/null
  tail -n 1 results/$(ls results | grep threadCount${threads} | tail -n 1)/threadCount${threads}.log |& tee -a $TMP_NO_COMPACT
  sync; sleep 5
done

clear

# Grab the legend
tail -n 4 results/$(ls results | grep threadCount${threads} | tail -n 1)/threadCount${threads}.log | head -n 1 |& tee -a $LOG_FILE

# Put together everything else
printf "# Full Output\r\n" |& tee -a $LOG_FILE
cat $TMP_FULL |& tee -a $LOG_FILE

printf "\r\n# No Output\r\n" |& tee -a $LOG_FILE
cat $TMP_NO_OUTPUT |& tee -a $LOG_FILE

printf "\r\n# No Compaction\r\n" |& tee -a $LOG_FILE
cat $TMP_NO_COMPACT |& tee -a $LOG_FILE

printf "\r\n# No Compaction and No Output\r\n" |& tee -a $LOG_FILE
cat $TMP_NO_COMPACT_NO_OUTPUT |& tee -a $LOG_FILE

rm -f $TMP_FULL $TMP_NO_OUTPUT $TMP_NO_COMPACT_NO_OUTPUT $TMP_NO_COMPACT

