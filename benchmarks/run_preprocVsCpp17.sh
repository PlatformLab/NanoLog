#! /bin/bash

#####
# Throughput tests comparing Preprocessor NanoLog with C++17 NanoLog using 7
# different log messages. The data generated here is graphable by the
# the NanoLog dissertation figure cppVsPreproc.
####
declare -a BENCH_OPS=(
                "NANO_LOG(NOTICE, \"Starting backup replica garbage collector thread\");"
                "NANO_LOG(NOTICE, \"Opened session with coordinator at %s\", \"basic+udp:host=192.168.1.140,port=12246\");"
                "NANO_LOG(NOTICE, \"Backup storage speeds (min): %d MB/s read\", 181);"
                "NANO_LOG(NOTICE, \"buffer has consumed %lu bytes of extra storage, current allocation: %lu bytes\", 1032024, 1016544);"
                "NANO_LOG(NOTICE, \"Using tombstone ratio balancer with ratio = %0.6lf\", 0.400000);"
                "NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"
                "NANO_LOG(NOTICE, \"%p %s %d %s %p\", \"abc\", \"abc\", 50, \"abc\", \"abc\");  NANO_LOG(NOTICE, \"%d %p %s %p %d\", 50, \"abc\", \"abc\", \"abc\", 60);"
                )


declare -a OPTIONS=(
                ""
                "--disableOutput"
                "--discardEntriesAtStagingBuffer"
                )

declare -a MAKE_OPTIONS=(
                "PREPROCESSOR_NANOLOG=no"
                "PREPROCESSOR_NANOLOG=yes"
                )

ITTERATIONS_ALL="100000000"

for itterations in $ITTERATIONS_ALL
do
    LOG_DIR="results/$(date +%Y%m%d%H%M%S)"
    LOG_FILE="${LOG_DIR}_throughput6Logs.txt"
    LOG_FILE_DEBUG="${LOG_DIR}_throughput6Logs_debug.txt"

    mkdir -p $LOG_DIR
    touch $LOG_FILE
    touch $LOG_FILE_DEBUG

    echo "#  Mlogs/s        Ops       Time  record()* compress()   Discard    Threads Compaction OutputFile     System    BenchOp" > $LOG_FILE
    for MAKE_OPTION in "${MAKE_OPTIONS[@]}"
    do
        echo "# Make Option: ${MAKE_OPTION}" | tee -a $LOG_FILE
        for OPTION in "${OPTIONS[@]}"
        do
            echo "# Options ${OPTION}" | tee -a $LOG_FILE
            for ((THREADS=1; THREADS<=6; THREADS=THREADS+=1))
            do
                for BENCH_OP in "${BENCH_OPS[@]}"
                do
                    echo "Running Threads=${THREADS} Options=${OPTION} Operation=${BENCH_OP}"
                    ((its = $itterations/$THREADS))
                    python genConfig.py --iterations=${its} --benchOp="$BENCH_OP" --threads="$THREADS" $OPTION
                    make $MAKE_OPTION clean-all      > /dev/null
                    make $MAKE_OPTION -j10           > /dev/null
                    ./benchmark        >> ${LOG_FILE_DEBUG}
                    tail -n1 ${LOG_FILE_DEBUG} | tee -a $LOG_FILE
                    echo "# Sleeping for 10 seconds to let the SSD rest..."
                    sleep 10
              done
              echo "" | tee -a $LOG_FILE
            done
        done
    done
    cat $LOG_FILE
done

echo "# Restoring ../runtime/Config.h"
git checkout ../runtime/Config.h
echo "# Done"
date