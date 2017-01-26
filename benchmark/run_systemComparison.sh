#! /bin/bash

#####
# Throughput tests for 6 different logs
# In the paper as systemComparison
####
declare -a BENCH_OPS=(
                "NANO_LOG(NOTICE, \"Starting backup replica garbage collector thread\");"
                "NANO_LOG(NOTICE, \"Opened session with coordinator at %s\", \"basic+udp:host=192.168.1.140,port=12246\");"
                "NANO_LOG(NOTICE, \"Backup storage speeds (min): %d MB/s read\", 181); NANO_LOG(NOTICE, \"Backup storage speeds (min): %d MB/s read\", 202);"
                "NANO_LOG(NOTICE, \"buffer has consumed %lu bytes of extra storage, current allocation: %lu bytes\", 1032024, 1016544);"
                "NANO_LOG(NOTICE, \"Using tombstone ratio balancer with ratio = %0.6lf\", 0.400000);"
                "NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"
                )

# declare -a OPTIONS=(
#                 "--disableOutput --disableCompaction"
#                 "--disableOutput"
#                 ""
#                 )
declare -a OPTIONS=(
                ""
                )
ITTERATIONS_ALL="100000000 1000000"

for itterations in $ITTERATIONS_ALL
do
    LOG_DIR="results/$(date +%Y%m%d%H%M%S)"
    LOG_FILE="${LOG_DIR}_throughput6Logs.txt"
    LOG_FILE_DEBUG="${LOG_DIR}_throughput6Logs_debug.txt"

    mkdir -p $LOG_DIR
    touch $LOG_FILE
    touch $LOG_FILE_DEBUG

    echo "# Throughput(Mop/s)  Operations Time Threads Compaction OutputFile BenchOp\r\n" > $LOG_FILE
    for OPTION in "${OPTIONS[@]}"
    do
        echo "# Options ${OPTION}" | tee -a $LOG_FILE
        for ((THREADS=1; THREADS<=16; THREADS=THREADS+=1))
        do
            for BENCH_OP in "${BENCH_OPS[@]}"
            do
                echo "Running Threads=${THREADS} Options=${OPTION} Operation=${BENCH_OP}"
                ((its = $itterations/$THREADS))
                python genConfig.py --iterations=${its} --benchOp="$BENCH_OP" --threads="$THREADS" $OPTION
                make clean-all      > /dev/null
                make -j10           > /dev/null
                ./benchmark        >> ${LOG_FILE_DEBUG}
                tail -n1 ${LOG_FILE_DEBUG} | tee -a $LOG_FILE
                echo "# Sleeping for 10 seconds to let the SSD rest..."
                sleep 10
          done
          echo "" | tee -a $LOG_FILE
        done
    done
    cat $LOG_FILE
done

echo "# Note, If you want to want to integrate this into the paper, you will need to rename the files"
