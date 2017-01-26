#! /bin/bash

mkdir results &> /dev/null
#####################################################################
## This script encodes how to recreate all the NanoLog benchmarks.
## It's mostly commented out and should serve as a reference.
######################################################################

######
# Different StagingBuffer sizes
# In the paper as stagingBufferSizes...
######
./run_stagingBufferSizes.sh

#####
# StagingBuffer Release Threshold
# In the paper as releaseThreshold
#####
./run_releaseThreshold.sh

################################################################################
# This tests what happens when have a variable number of log messages in the system.
# In the paper as uniqueLogs
################################################################################
TOTAL_LOGS="100000000"
NUM_LOG_MSGS="1 2 3 4 5 10 25 50 100 500 1000"
# LOG_FILE="results/$(date +%Y%m%d%H%M%S)_uniqueLogs.meta"
# ls --block-size=1024 -la ./benchmark | cut -d" " -f 5
# echo "# Throughput(Mop/s) | Operations | Time | Threads | Compaction | OutputFile | Binary Size" |& tee -a $LOG_FILE

for NUM in $NUM_LOG_MSGS
do
  LOG_STRING=""
  for ((i=1; i <= $NUM; i++))
  do
    printf -v LOG_STRING "${LOG_STRING} \\\\\n NANO_LOG(NOTICE, \"Log message # ${i}\");"
  done

  ((ITTRS=${TOTAL_LOGS}/${NUM}))
  python genConfig.py --iterations=${ITTRS} --benchOp="${LOG_STRING}"
  ./run_bench.sh "UniqueLogs${NUM}"

  sync
  echo "Sleeping for 5 seconds to let the SSD rest..."
  sleep 5
done


#####################
# Tests the affect of threading. In a perfect world, the throughput should go
# down linearly with the number of threads (since they're all computing for
# scarce IO bandwidth)
# In the paper as threadCount
#####################
./run_threadCount.sh


# ################################################################################
# # Tests the benefits we get by increasing the application delay.
# # Spoiler: It's already maximally good on our 250MB/s SSDs with a 10ns delay
# # It equals kernel jitter.
# ################################################################################
# for ((delay=0; delay<=10; delay++))
# do
#   printf -v LOG_OP "NANO_LOG(NOTICE, \"aEw6ppfz3QMmDXBm91v10TxzCWdTaWUUX9ta0Fihl86Ta9nlFN123456\");"
#   for ((i=0; i<delay; i++))
#   do
#     LOG_OP="${LOG_OP} function(${i});"
#   done
#   python genConfig.py --stagingBufferExp=20 --outputBufferExp=24 --iterations=100000000 --threads=1 --benchOp="${LOG_OP}"
#   ./run_bench.sh "withDelay${i}0ns"

#   echo "Sleeping for 30 seconds to let the SSD rest..."
#   sleep 30
# done


# ################################################################################
# # Tests what happens when we increase the number of arguments in the format is
# # both hard and easy to comapct.
# ################################################################################
# ARG_VALS="2000000000 1"
# for ARG_VAL in ${ARG_VALS}
# do
#   for ((args=0; args<=5; args++))
#   do
#     SPECIFIERS=""
#     ARGS=""
#     for ((i=0; i <args; i++))
#     do
#       SPECIFIERS="${SPECIFIERS}, %%d"
#       ARGS="${ARGS}, ${ARG_VAL}"
#     done
#     printf -v LOG_OP "NANO_LOG(NOTICE, \"aEw6ppfz3QMmDXBm91v10TxzCWdTaWUUX9ta0Fihl86Ta9nlFN123456${SPECIFIERS}\"${ARGS});"

#     python genConfig.py --iterations=100000000 --threads=1 --benchOp="${LOG_OP}"
#     ./run_bench.sh "${args}_argsOfValue_${ARG_VAL}"

#     echo "Sleeping for 30 seconds to let the SSD rest..."
#     sleep 30
#   done
# done

###################################
# Tests the aggregation vs. other system when the interesting log message isn't the only one
# In the paper as aggregationComparison
###################################
./run_aggregation.sh


#####
# Throughput tests for 6 different logs
# In the paper as systemComparison
####
./run_systemComparison.sh

#####
# Decompression tests for 6 different logs
# In the paper as decompresionCosts
####
./run_decompressionCosts.sh


######
# Unloaded Latency tests for 6 different logs
# In the paper as ...
#####
./run_unloaded_latency.sh


###
# Effect of threading and sorted decompression cost.
# In theory, the decompression should take longer with increasing threads
# (but same nubmer of log messages)
# In the paper as sortedDecompressionThreads
###
if [ "$(sudo whoami)" != "root" ]; then
    echo "You need sudo priviledges. Add this line to /etc/sudoers"
    echo "$(whoami)  ALL=(ALL:ALL) NOPASSWD: ALL"
    exit 1
fi

# Even power of 2
ITTRS=16777216
LOG_FILE="results/$(date +%Y%m%d%H%M%S)_sortedDecompressionThreads.txt"
mkdir -p results

# This run is for gathering metadata
python genConfig.py --iterations=${ITTRS} --threads=1
./run_bench.sh "sortedDecompSetup1" > /dev/null
(/usr/bin/time -f "%e" ./decompressor decompressSort /tmp/logFile > /tmp/decomp)
printf "# Cost of decompressing a compact log file with increasing threads (i.e. interleaving)\r\n" |& tee -a $LOG_FILE
printf "# Raw Log Size $(ls -lah /tmp/logFile)\r\n" |& tee -a $LOG_FILE
printf "# Decompressed Size $(ls -lah /tmp/decomp)\r\n" |& tee -a $LOG_FILE
printf "# Sample log message: $(head -n 1 /tmp/decomp)\r\n\r\n" |& tee -a $LOG_FILE

printf "# Threads | Time (secs)\r\n" |& tee -a $LOG_FILE

TMP_SORTED="/tmp/$(date +%Y%m%d%H%M%S)_1.txt"
TMP_UNSORTED="/tmp/$(date +%Y%m%d%H%M%S)_2.txt"
TMP_SORTED_NULL="/tmp/$(date +%Y%m%d%H%M%S)_3.txt"
TMP_UNSORTED_NULL="/tmp/$(date +%Y%m%d%H%M%S)_4.txt"

for ((threads=1; threads<=1024; threads*=2))
do
    ((itterations = $ITTRS/$threads))
    python genConfig.py --iterations=${itterations} --threads=${threads}
    ./run_bench.sh "sortedDecompSetup${threads}" > /dev/null

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompressSort /tmp/logFile > /tmp/decomp; } 2>&1 )
    printf "${threads}    ${RESULT}\r\n" |& tee -a $TMP_SORTED
    rm -f /tmp/decomp

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompress /tmp/logFile > /tmp/decomp; } 2>&1 )
    printf "${threads}    ${RESULT}\r\n" |& tee -a $TMP_UNSORTED
    rm -f /tmp/decomp

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompressSort /tmp/logFile > /dev/null; } 2>&1 )
    printf "${threads}    ${RESULT}\r\n" |& tee -a $TMP_SORTED_NULL

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompress /tmp/logFile > /dev/null; } 2>&1 )
    printf "${threads}    ${RESULT}\r\n" |& tee -a $TMP_UNSORTED_NULL
done

clear

printf "# Sorted to file\r\n" |& tee -a $LOG_FILE
cat $TMP_SORTED |& tee -a $LOG_FILE

printf "\r\n# Unsorted to file\r\n" |& tee -a $LOG_FILE
cat $TMP_UNSORTED |& tee -a $LOG_FILE

printf "\r\n# Sorted to /dev/null\r\n" |& tee -a $LOG_FILE
cat $TMP_SORTED_NULL |& tee -a $LOG_FILE

printf "\r\n# Unsorted to /dev/null\r\n" |& tee -a $LOG_FILE
cat $TMP_UNSORTED_NULL |& tee -a $LOG_FILE

rm -f $TMP_SORTED $TMP_UNSORTED $TMP_SORTED_NULL $TMP_UNSORTED_NULL