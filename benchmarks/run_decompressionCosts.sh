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
declare -a BENCH_OPS=(
                "NANO_LOG(NOTICE, \"Starting backup replica garbage collector thread\");"
                "NANO_LOG(NOTICE, \"Opened session with coordinator at %s\", \"basic+udp:host=192.168.1.140,port=12246\");"
                "NANO_LOG(NOTICE, \"Backup storage speeds (min): %d MB/s read\", 181);"
                "NANO_LOG(NOTICE, \"buffer has consumed %lu bytes of extra storage, current allocation: %lu bytes\", 1032024, 1016544);"
                "NANO_LOG(NOTICE, \"Using tombstone ratio balancer with ratio = %0.6lf\", 0.400000);"
                "NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"
                )

LOG_FILE="results/$(date +%Y%m%d%H%M%S)_decompressionCosts.txt"
mkdir -p results

ITTRS=100000000
echo "# Running Unsorted decompress on $(hostname) over ${ITTRS} log messages" |& tee -a $LOG_FILE

for BENCH_OP in "${BENCH_OPS[@]}"
do
        python genConfig.py --iterations=${ITTRS} --benchOp="$BENCH_OP"
        ./run_bench.sh decompSetup > /dev/null
        sync; sudo sh -c 'echo 1 > /proc/sys/vm/drop_caches'

        echo "# Running Sorted Decompress on $BENCH_OP" |& tee -a $LOG_FILE
        echo "# Log file input size" |& tee -a $LOG_FILE
        echo "# $(ls -lah /tmp/logFile)" |& tee -a $LOG_FILE
        (/usr/bin/time -f "%e" ./decompressor decompress /tmp/logFile > /tmp/decomp) |& tee -a $LOG_FILE
        rm -f /tmp/decomp
done