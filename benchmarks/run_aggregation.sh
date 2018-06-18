#! /bin/bash

###################################
# Tests the aggregation vs. other system when the interesting log message isn't the only one
# In the paper as aggregationComparison
###################################

SUDO_POWER="$(sudo -v 2>&1)"
if [[ ! -z "$SUDO_POWER" ]]; then
    echo "You need sudo priviledges. Add this line to /etc/sudoers"
    echo "$(whoami)  ALL=(ALL:ALL) NOPASSWD: ALL"
    exit 1
fi

TOTAL_LOG_MSGS=100000000
UNRELATED_MSGS_ARRAY="0 1 9 99"

# Goal should be to keep these the same length
TARGET_MSG="Hello World # "
UNRELATED="UnrelatedLog #"

LOG_FILE="results/$(date +%Y%m%d%H%M%S)_aggregationComparison.txt"
DEBUG_LOG_FILE="results/$(date +%Y%m%d%H%M%S)_aggregationComparison_Debug.txt"

# This is to add a small delay between the log messages so that they're
# spaced out more realistically. They affect decompressor performance by
# allowing the decompressor thread to insert more buffer extents
# (and thus overlap more compute with I/O)
DELAY_CMD=";PerfUtils::Cycles::rdtsc();"

for UNRELATED_MSGS in $UNRELATED_MSGS_ARRAY
do
  # Create the log messages
  BENCH_OP="static int cnt = 0; NANO_LOG(NOTICE, \"${TARGET_MSG}%d\", ++cnt); ${DELAY_CMD}"
  for ((i=0; i < $UNRELATED_MSGS; ++i))
  do
    BENCH_OP="${BENCH_OP} NANO_LOG(NOTICE, \"${UNRELATED}%d\", ++cnt); ${DELAY_CMD}"
  done

  # Create the file
  python genConfig.py --benchOp="$BENCH_OP" --iterations=$(( $TOTAL_LOG_MSGS / ($UNRELATED_MSGS + 1) ))
  ./run_bench.sh "aggregationWith${UNRELATED_MSGS}UnrelatedMsgsSetup" > /dev/null

  (( PERCENTAGE=(100/(1 + $UNRELATED_MSGS) ) ))
  echo "# Aggregating over ${PERCENTAGE}% of the log file" |& tee -a $LOG_FILE
  echo "# Time (seconds) | Max Memory | Avg Memory | Percentage | System" |& tee -a $LOG_FILE

  sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
  (/usr/bin/time --format="%e %M %K ${PERCENTAGE} NanoLog Decompress" ./decompressor decompress /tmp/logFile > /tmp/decomp) |& tee -a $LOG_FILE

  echo "# NanoLog Compact Log File size is $(ls -lah /tmp/logFile)" >> $DEBUG_LOG_FILE
  echo "# NanoLog Inflated Log File size is $(ls -lah /tmp/decomp)" >> $DEBUG_LOG_FILE
  echo "Sample NanoLog decompressed output" >> $DEBUG_LOG_FILE
  tail -n10 /tmp/decomp >> $DEBUG_LOG_FILE
  printf "\r\nNanoLog Aggregator output\r\n" >> $DEBUG_LOG_FILE

  sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
  LOG_ID=$(./decompressor find "${TARGET_MSG}" | grep "Benchmark\.cc" | cut -d "|" -f 1)
  (/usr/bin/time --format="%e %M %K ${PERCENTAGE} NanoLog Aggregation" ./decompressor minMaxMean /tmp/logFile ${LOG_ID} >> $DEBUG_LOG_FILE) |& tee -a $LOG_FILE

  pushd aggregation &> /dev/null
  make > /dev/null

  # printf "\r\n== C++ Single Read Compact ==\r\n" >> ../$DEBUG_LOG_FILE
  # sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
  # (/usr/bin/time --format="%e %M %K ${PERCENTAGE} C++ Single Read Compact" ./simpleRead /tmp/logFile >> ../$DEBUG_LOG_FILE) |& tee -a ../$LOG_FILE

  printf "\r\n== C++ Single Read Full ==\r\n" >> ../$DEBUG_LOG_FILE
  sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
  (/usr/bin/time --format="%e %M %K ${PERCENTAGE} C++ Single Read Full" ./simpleRead /tmp/decomp >> ../$DEBUG_LOG_FILE) |& tee -a ../$LOG_FILE

  printf "\r\n== C++ Aggregator output ==\r\n" >> ../$DEBUG_LOG_FILE
  sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
  (/usr/bin/time --format="%e %M %K ${PERCENTAGE} C++ Aggregation" ./aggregate "${TARGET_MSG}" /tmp/decomp >> ../$DEBUG_LOG_FILE) |& tee -a ../$LOG_FILE

  make clean &> /dev/null
  popd >/dev/null

  printf "\r\n== Awk Aggregation output ==\r\n"  >> $DEBUG_LOG_FILE
  sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
  (/usr/bin/time --format="%e %M %K ${PERCENTAGE} Awk Aggregation" awk '{
                if ($0 ~ /.*'"${TARGET_MSG}"'.*/) {
                if(min==""){min=max=$8};
                if($8>max) {max=$8};
                if($8< min) {min=$8};
                total+=$8; count+=1;
              }
            } END {print "Target: '"${TARGET_MSG}"'";
                    print "mean =", total/count;
                    print "minimum =", min;
                    print "maximum =", max;
                    print "total =", total;
                    print "count =", count;}' /tmp/decomp >> $DEBUG_LOG_FILE) |& tee -a $LOG_FILE

  printf "\r\n== Python Aggregator output ==\r\n" >> $DEBUG_LOG_FILE
  sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
  (/usr/bin/time --format="%e %M %K ${PERCENTAGE} Python Aggregation" python aggregation/aggregateArg1.py /tmp/decomp >> $DEBUG_LOG_FILE) |& tee -a $LOG_FILE
  echo "" |& tee -a $LOG_FILE
done

echo "" |& tee -a $LOG_FILE
printf "# NanoLog Compact Log File size is \r\n$(ls -lah /tmp/logFile)\r\n" |& tee -a $DEBUG_LOG_FILE
printf "# NanoLog Inflated Log File size is \r\n$(ls -lah /tmp/decomp)\r\n" |& tee -a $DEBUG_LOG_FILE

rm -f /tmp/logFile /tmp/decomp
