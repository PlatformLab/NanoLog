#####
# Decompression tests for 6 different logs
# In the paper as decompresionCosts
####
SUDO_POWER="$(sudo -v 2>&1)"
if [[ ! -z "$SUDO_POWER" ]]; then
    echo "You need sudo priviledges. Add this line to /etc/sudoers"
    echo "$(whoami)  ALL=(ALL:ALL) NOPASSWD: ALL"
    exit 1
fi

THREADS_MAX=4096
# Even power of 2
ITTRS=8388608
declare -A BENCH_OPS
BENCH_OPS["staticString"]="NANO_LOG(NOTICE, \"Starting backup replica garbage collector thread\");"
BENCH_OPS["stringConcat"]="NANO_LOG(NOTICE, \"Opened session with coordinator at %s\", \"basic+udp:host=192.168.1.140,port=12246\");"
BENCH_OPS["singleInteger"]="NANO_LOG(NOTICE, \"Backup storage speeds (min): %d MB/s read\", 181);"
BENCH_OPS["twoIntegers"]="NANO_LOG(NOTICE, \"buffer has consumed %lu bytes of extra storage, current allocation: %lu bytes\", 1032024, 1016544);"
BENCH_OPS["singleDouble"]="NANO_LOG(NOTICE, \"Using tombstone ratio balancer with ratio = %0.6lf\", 0.400000);"
BENCH_OPS["complexFormat"]="NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"

BENCH_OPS_ORDERING="staticString stringConcat singleInteger twoIntegers singleDouble complexFormat"

LOG_FILE="results/$(date +%Y%m%d%H%M%S)_decompressionCosts.txt"
mkdir -p results

echo "# Running decompress on $(hostname) over ${ITTRS} iterations of the following messages:" |& tee -a $LOG_FILE

for OP_KEY in "${!BENCH_OPS[@]}"
do
    printf "# %-15s = %s\r\n" "$OP_KEY" "${BENCH_OPS[$OP_KEY]}" |& tee -a $LOG_FILE
done


echo "" |& tee -a $LOG_FILE
echo "# All results are in millions of operations per second." |& tee -a $LOG_FILE

printf "\r\n# Legend (numbers are threads)\r\n" |& tee -a $LOG_FILE
printf "# BenchOp " |& tee -a $LOG_FILE

for (( threads = 1; threads <= $THREADS_MAX; threads*=4 )); do
    printf " & $threads" |& tee -a $LOG_FILE
done
echo "" |& tee -a $LOG_FILE

SORTED_LOG_FILE="./sorted.tmp"
STATS_LOG_FILE="./stats.tmp"

printf "\r\n# Unsorted Output\r\n" |& tee -a $LOG_FILE
printf "\r\n# Sorted Output\r\n" > $SORTED_LOG_FILE
printf "\r\n\r\n\r\n#Random Statistics\r\n" > $STATS_LOG_FILE


#####
# Output Goal
#
# Threads OP1 OP2 OP3 ...
#####

for OP_KEY in $BENCH_OPS_ORDERING
    do

    BENCH_OP=${BENCH_OPS[$OP_KEY]}
    echo -n "$OP_KEY" >> $LOG_FILE
    echo -n "$OP_KEY" >> $SORTED_LOG_FILE

    for (( threads = 1; threads <= $THREADS_MAX; threads*=4 )); do
        ((itterations = $ITTRS/$threads))

        python genConfig.py --iterations=${itterations} --benchOp="$BENCH_OP" --threads=${threads} > /dev/null
        ./run_bench.sh decompSetup > /dev/null
        sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'

        # First one is warmup
        UNSORTED_TIME=$((/usr/bin/time -f " %e " ./decompressor decompressUnordered /tmp/logFile > /tmp/decomp2) 2>&1 )
        UNSORTED_TIME=$((/usr/bin/time -f " %e " ./decompressor decompressUnordered /tmp/logFile > /tmp/decomp2) 2>&1 )
        SORTED_TIME=$((/usr/bin/time -f " %e " ./decompressor decompress /tmp/logFile > /tmp/decomp) 2>&1 )

        UNSORTED_THROUGHPUT=$(echo "scale=2; $ITTRS/(1000000*${UNSORTED_TIME})" | bc)
        SORTED_THROUGHPUT=$(echo "scale=2;   $ITTRS/(1000000*${SORTED_TIME})" | bc)

        printf "At $threads threads, $OP_KEY had unsorted/sorted throughputs of "
        printf " & 0%s" $UNSORTED_THROUGHPUT |& tee -a $LOG_FILE
        printf " / "
        printf " & 0%s" $SORTED_THROUGHPUT |& tee -a $SORTED_LOG_FILE
        echo ""

        # (/usr/bin/time -f " %e " ./decompressor decompress /tmp/logFile > /tmp/decomp) |& tee -a $SORTED_LOG_FILE
        # (/usr/bin/time -f " %e " ./decompressor decompressUnordered /tmp/logFile > /tmp/decomp2) |& tee -a $LOG_FILE

        if ((threads==$THREADS_MAX)); then
            echo "# Input/output/sorted output size of $BENCH_OP was $(stat -c "%s" /tmp/logFile) / $(stat -c "%s" /tmp/decomp) / $(stat -c "%s" /tmp/decomp2) bytes" |& tee -a $STATS_LOG_FILE
        fi

        rm -f /tmp/decomp /tmp/decomp2 /tmp/logFile
    done

    echo "" |& tee -a $LOG_FILE
    echo "" |& tee -a $SORTED_LOG_FILE
done

cat $SORTED_LOG_FILE >> $LOG_FILE
cat $STATS_LOG_FILE >> $LOG_FILE

rm -f $SORTED_LOG_FILE $STATS_LOG_FILE

