#####
# StagingBuffer Release Threshold
# In the paper as releaseThreshold
#####
for ((c=20; c>=10; c--))
do
  python genConfig.py --releaseThresholdExp=${c} --stagingBufferExp=20 --iterations=50000000 --benchOp="NANO_LOG(NOTICE, \"A\"); NANO_LOG(NOTICE, \"B\");"
  ./run_bench.sh "stagingBuffer_releaseThreshold_pow${c}"
  echo "Sleeping for 5 secs to wait for the SSD..."
  sync && sleep 5
done