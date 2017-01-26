#! /bin/bash

################################################################################
# This test compares the various compression techniques we have.
################################################################################

####
# Parse the NanoLog::printStats() output to retrieve the
# number of raw logging bytes (UNCOMP_BYTES), the number
# of bytes after compaction (OUTPUT_BYTES), and the amount
# of time spent compacting in seconds (TIME_COMPACTING).
# The variables are saved in globals since that's
# how one returns values in bash...
#
# \param $1 - file that contains the NanoLog::printStats() results
#
# \return UNCOMP_BYTES - number of raw logged bytes
# \return OUTPUT_BYTES - number of bytes output (after compaction)
# \return TIME_PROCESSING - number of seconds spent compacting (floating point)
# \return TIME_COMPRESSING - number of seconds spent doing actual compression (floating point)
parseNanoLogFile() {
  UNCOMP_BYTES=$(grep "bytes in" $1 | cut -d' ' -f 6 | cut -c 2- | tr -d '\r')
  OUTPUT_BYTES=$(grep "bytes in" $1 | cut -d' ' -f 9 | tr -d '\r')
  TIME_PROCESSING=$(grep "seconds spent processing" $1 | cut -d' ' -f 9 | cut -c 2- | tr -d '\r')
  TIME_COMPRESSING=$(grep "seconds compressing" $1 | cut -d' ' -f 13 | tr -d '\r')
}

####
# Parse the external compression application's output to retrieve the
# number of input bytes, output bytes, and compression time in seconds
#
# \param $1 - file that contains the application's output
#
# \return COMP_OUTPUT_BYTES  - Bytes in the compressed output
# \return COMP_INPUT_BYTES   - Bytes in the input file
# \return COMP_COMPRESS_TIME - Time (in seconds) spent compressing the file only
parseExternalCompressLogFile() {
  COMP_INPUT_BYTES=$(grep "Input Bytes" $1 | cut -d ':' -f2- | tr -d ' ' | tr -d '\r')
  COMP_OUTPUT_BYTES=$(grep "Output Bytes" $1 | cut -d ':' -f2- | tr -d ' ' | tr -d '\r')
  COMP_COMPRESS_TIME=$(grep "Compress Time " $1 | cut -d ':' -f2- | tr -d ' ' | tr -d '\r')
}


# Hopefully 1-time setup for snappy
pushd snappy
if [ ! -e "configure" ]
then
  ./autogen.sh || exit 1
fi

if [ ! -e "Makefile" ]
then
   CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG" ./configure
fi

if [ ! -e ".libs/snappy.a" ]
then
  make -j10
fi
popd > /dev/null

# Actual Test for NanoLog
LOG_FOLDER="results/$(date +%Y%m%d%H%M%S)_compressionComparison"
LOG_FOLDER="$(realpath ${LOG_FOLDER})"
LOG_FILE="${LOG_FOLDER}/summary_compression.log"

mkdir -p $LOG_FOLDER
rm -rf "${LOG_FOLDER}/*"

declare -A NAME_TO_OP

NAME_TO_OP["staticString"]="NANO_LOG(NOTICE, \"Starting backup replica garbage collector thread\");"
NAME_TO_OP["stringConcat"]="NANO_LOG(NOTICE, \"Opened session with coordinator at %s\", \"basic+udp:host=192.168.1.140,port=12246\");"
NAME_TO_OP["singleInteger"]="NANO_LOG(NOTICE, \"Backup storage speeds (min): %d MB/s read\", 181);"
NAME_TO_OP["twoIntegers"]="NANO_LOG(NOTICE, \"buffer has consumed %lu bytes of extra storage, current allocation: %lu bytes\", 1032024, 1016544);"
NAME_TO_OP["singleDouble"]="NANO_LOG(NOTICE, \"Using tombstone ratio balancer with ratio = %0.6lf\", 0.400000);"
NAME_TO_OP["fiveArgs"]="NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"


echo "# These tests measure the amount of (bytes saved) divided by the (processing time in seconds) for the following scenarios" | tee -a $LOG_FILE
echo "# Typically, the higher the number, the better." | tee -a $LOG_FILE
echo "#" | tee -a $LOG_FILE
# echo "# Test Name     | NanoLog Only   | NanoLog + Ext Snappy | NanoLog + Snappy | NanoLog NC + ext Snappy | NanoLog NC + Snappy | BENCH_OP" | tee -a $LOG_FILE
echo "# Test Name     | NanoLog Only   | NanoLog + Snappy | NanoLog NC + Snappy | BENCH_OP" | tee -a $LOG_FILE


for OP_NAME in "${!NAME_TO_OP[@]}";
do
  BENCH_OP="${NAME_TO_OP[$OP_NAME]}"
  printf "%-16s" $OP_NAME | tee -a $LOG_FILE

  # NanoLog Only
  ITTRS=20000000
  python ./genConfig.py --iterations=$ITTRS --benchOp="${BENCH_OP}"
  make clean-all > /dev/null && make -j10 > /dev/null && rm -rf /tmp/logFile
  ./benchmark > "${LOG_FOLDER}/${OP_NAME}.txt"

  parseNanoLogFile "${LOG_FOLDER}/${OP_NAME}.txt"
  BYTES_SAVED_PER_COMPUTE=$(echo "(${UNCOMP_BYTES} - ${OUTPUT_BYTES} ) / ${TIME_PROCESSING} " | bc)
  printf "%17s" $BYTES_SAVED_PER_COMPUTE | tee -a $LOG_FILE

  # # External Snappy
  # pushd compressionTests > /dev/null
  # make > /dev/null
  # ./snappyCompression /tmp/logFile > ${LOG_FOLDER}/${OP_NAME}_snappy_ext.txt
  # popd > /dev/null

  # parseExternalCompressLogFile "${LOG_FOLDER}/${OP_NAME}_snappy_ext.txt"
  # BYTES_SAVED_PER_COMPUTE=$(echo "(${UNCOMP_BYTES} - ${COMP_OUTPUT_BYTES} ) / ( ${TIME_PROCESSING} + ${COMP_COMPRESS_TIME} )" | bc)
  # printf "%22s" $BYTES_SAVED_PER_COMPUTE | tee -a $LOG_FILE

  # Internal Snappy
  python ./genConfig.py --iterations=$ITTRS --benchOp="${BENCH_OP}" --useSnappy
  make clean-all > /dev/null && make -j10 > /dev/null && rm -rf /tmp/logFile
  ./benchmark > "${LOG_FOLDER}/${OP_NAME}_snappy_int.txt"

  parseNanoLogFile "${LOG_FOLDER}/${OP_NAME}_snappy_int.txt"
  BYTES_SAVED_PER_COMPUTE=$(echo "(${UNCOMP_BYTES} - ${OUTPUT_BYTES} ) / ( ${TIME_PROCESSING} + ${TIME_COMPRESSING} )" | bc)
  printf "%19s" $BYTES_SAVED_PER_COMPUTE | tee -a $LOG_FILE

  # NanoLog No Compact + External
  # python ./genConfig.py --iterations=$ITTRS --benchOp="${BENCH_OP}" --disableCompaction
  # make clean-all > /dev/null && make -j10 > /dev/null && rm -rf /tmp/logFile
  # ./benchmark > "${LOG_FOLDER}/${OP_NAME}_noCompact.txt"

  # pushd compressionTests > /dev/null
  # make > /dev/null
  # ./snappyCompression /tmp/logFile > ${LOG_FOLDER}/${OP_NAME}_noCompact_snappy_ext.txt
  # popd > /dev/null

  # parseExternalCompressLogFile "${LOG_FOLDER}/${OP_NAME}_noCompact_snappy_ext.txt"
  # BYTES_SAVED_PER_COMPUTE=$(echo "(${UNCOMP_BYTES} - ${COMP_OUTPUT_BYTES} ) / ( ${TIME_PROCESSING} + ${COMP_COMPRESS_TIME} )" | bc)
  # printf "%25s" $BYTES_SAVED_PER_COMPUTE | tee -a $LOG_FILE

  # NanoLog No Compact + Internal
  python ./genConfig.py --iterations=$ITTRS --benchOp="${BENCH_OP}" --useSnappy --disableCompaction
  make clean-all > /dev/null && make -j10 > /dev/null && rm -rf /tmp/logFile
  ./benchmark > "${LOG_FOLDER}/${OP_NAME}_noCompact_snappy_int.txt"

  parseNanoLogFile "${LOG_FOLDER}/${OP_NAME}_noCompact_snappy_int.txt"
  BYTES_SAVED_PER_COMPUTE=$(echo "(${UNCOMP_BYTES} - ${OUTPUT_BYTES} ) / ( ${TIME_PROCESSING} + ${TIME_COMPRESSING} )" | bc)
  printf "%22s" $BYTES_SAVED_PER_COMPUTE | tee -a $LOG_FILE

  printf "  %20s\r\n" "${BENCH_OP}" | tee -a $LOG_FILE
done

THROUGHPUT_FILE="${LOG_FOLDER}/summary_throughput.log"
echo "# Measures the throughput (MLogs/second) of using compression" | tee -a ${THROUGHPUT_FILE}
echo "# Test Name     | NanoLog Only   | NanoLog + Snappy | NanoLog NC + Snappy | BENCH_OP" | tee -a ${THROUGHPUT_FILE}

for OP_NAME in "${!NAME_TO_OP[@]}";
do
  BENCH_OP="${NAME_TO_OP[$OP_NAME]}"

  printf "%-16s" $OP_NAME | tee -a $THROUGHPUT_FILE
  printf "%17s" "$(tail -n 1 "${LOG_FOLDER}/${OP_NAME}.txt" | awk '{print $1}')" | tee -a $THROUGHPUT_FILE
  printf "%19s" "$(tail -n 1 "${LOG_FOLDER}/${OP_NAME}_snappy_int.txt" | awk '{print $1}')" | tee -a $THROUGHPUT_FILE
  printf "%22s" "$(tail -n 1 "${LOG_FOLDER}/${OP_NAME}_noCompact_snappy_int.txt" | awk '{print $1}')" | tee -a $THROUGHPUT_FILE
  printf "  %20s\r\n" "${BENCH_OP}" | tee -a $THROUGHPUT_FILE
done