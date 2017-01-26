#! /bin/bash

######
# Different StagingBuffer sizes
# In the paper as stagingBufferSizes...
######
ITRS=100000000
for ((c=23; c>=12; c--))
do
  RELEASE_OP=""
  if (( c > 20 ))
    then
    RELEASE_OP="--releaseThresholdExp=20"
  fi
  python genConfig.py ${RELEASE_OP} --pollInterval=1 --stagingBufferExp=${c} --iterations=${ITRS} --benchOp="NANO_LOG(NOTICE, \"A\"); NANO_LOG(NOTICE, \"B\");"
  ./run_bench.sh "stagingBuffer_staticString_pow${c}"
  echo "Sleeping for 5 secs to wait for the SSD..."
  sync && sleep 5
done

for ((c=23; c>=12; c--))
do
  python genConfig.py ${RELEASE_OP} --pollInterval=1 --stagingBufferExp=${c} --iterations=${ITRS} --benchOp="NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"
  ./run_bench.sh "stagingBuffer_complexString_pow${c}"
  echo "Sleeping for 5 secs to wait for the SSD..."
  sync && sleep 5
done
