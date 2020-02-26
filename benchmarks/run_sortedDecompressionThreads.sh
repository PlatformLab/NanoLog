#! /bin/bash -e

###
# Effect of threading and sorted decompression cost.
# In theory, the decompression should take longer with increasing threads
# (but same nubmer of log messages)
# In the paper as sortedDecompressionThreads
###

SUDO_POWER="$(sudo -v 2>&1)"
if [[ ! -z "$SUDO_POWER" ]]; then
    echo "You need sudo priviledges. Add this line to /etc/sudoers"
    echo "$(whoami)  ALL=(ALL:ALL) NOPASSWD: ALL"
    exit 1
fi

# Even power of 2
ITTRS=16777216
# ITTRS=262144
LOG_FILE="results/$(date +%Y%m%d%H%M%S)_sortedDecompressionThreads.txt"
mkdir -p results

# This run is for gathering metadata
python genConfig.py --iterations=${ITTRS} --threads=1
./run_bench.sh "sortedDecompSetup1" > /dev/null
(/usr/bin/time -f "%e" ./decompressor decompress /tmp/logFile > /tmp/decomp)
printf "# Cost of decompressing a compact log file with increasing threads (i.e. interleaving)\r\n" |& tee -a $LOG_FILE
printf "# Raw Log Size $(ls -lah /tmp/logFile)\r\n" |& tee -a $LOG_FILE
printf "# Decompressed Size $(ls -lah /tmp/decomp)\r\n" |& tee -a $LOG_FILE
printf "# Sample log message: $(head -n 1 /tmp/decomp)\r\n\r\n" |& tee -a $LOG_FILE

printf "# Threads | Time (secs)\r\n" |& tee -a $LOG_FILE

TMP_SORTED="/tmp/$(date +%Y%m%d%H%M%S)_1.txt"
TMP_UNSORTED="/tmp/$(date +%Y%m%d%H%M%S)_2.txt"
TMP_SORTED_NULL="/tmp/$(date +%Y%m%d%H%M%S)_3.txt"
TMP_UNSORTED_NULL="/tmp/$(date +%Y%m%d%H%M%S)_4.txt"

for ((threads=1; threads<=4096; threads*=2))
# for ((threads=1; threads<=1; threads*=2))
do
    ((itterations = $ITTRS/$threads))
    python genConfig.py --iterations=${itterations} --threads=${threads}
    ./run_bench.sh "sortedDecompSetup${threads}" > /dev/null

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompress /tmp/logFile > /tmp/decomp; } 2>&1 )
    printf "${threads}    ${RESULT}\r\n" |& tee -a $TMP_SORTED
    rm -f /tmp/decomp

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompressUnordered /tmp/logFile > /tmp/decomp; } 2>&1 )
    printf "${threads}    ${RESULT}\r\n" |& tee -a $TMP_UNSORTED
    rm -f /tmp/decomp

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompress /tmp/logFile > /dev/null; } 2>&1 )
    printf "${threads}    ${RESULT}\r\n" |& tee -a $TMP_SORTED_NULL

    sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'
    RESULT=$( { /usr/bin/time -f "%e"  ./decompressor decompressUnordered /tmp/logFile > /dev/null; } 2>&1 )
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