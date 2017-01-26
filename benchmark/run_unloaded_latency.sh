######
# Unloaded Latency tests for 6 different logs
# In the paper as ...
#####
LOG_DIR="results/$(date +%Y%m%d%H%M%S)_unloadedLatency"
mkdir -p $LOG_DIR
ITTERATIONS=100000
python genConfig.py --iterations=$ITTERATIONS --benchOp="NANO_LOG(NOTICE, \"Starting backup replica garbage collector thread\");"
make clean-all > /dev/null && make -j10 > /dev/null
./unloadedLatency  |& tee -a "${LOG_DIR}/nanoLog_staticString.rcdf"
sleep 10

python genConfig.py --iterations=$ITTERATIONS --benchOp="NANO_LOG(NOTICE, \"Opened session with coordinator at %s\", \"basic+udp:host=192.168.1.140,port=12246\");"
make clean-all > /dev/null && make -j10 > /dev/null
./unloadedLatency  |& tee -a "${LOG_DIR}/nanoLog_stringConcat.rcdf"
sleep 10

python genConfig.py --iterations=$ITTERATIONS --benchOp="NANO_LOG(NOTICE, \"Backup storage speeds (min): %d MB/s read\", 181);"
make clean-all > /dev/null && make -j10 > /dev/null
./unloadedLatency  |& tee -a "${LOG_DIR}/nanoLog_singleInteger.rcdf"
sleep 10

python genConfig.py --iterations=$ITTERATIONS --benchOp="NANO_LOG(NOTICE, \"buffer has consumed %lu bytes of extra storage, current allocation: %lu bytes\", 1032024, 1016544);"
make clean-all > /dev/null && make -j10 > /dev/null
./unloadedLatency  |& tee -a "${LOG_DIR}/nanoLog_twoIntegers.rcdf"
sleep 10

python genConfig.py --iterations=$ITTERATIONS --benchOp="NANO_LOG(NOTICE, \"Using tombstone ratio balancer with ratio = %0.6lf\", 0.400000);"
make clean-all > /dev/null && make -j10 > /dev/null
./unloadedLatency  |& tee -a "${LOG_DIR}/nanoLog_singleDouble.rcdf"
sleep 10

python genConfig.py --iterations=$ITTERATIONS --benchOp="NANO_LOG(NOTICE, \"Initialized InfUdDriver buffers: %lu receive buffers (%u MB), %u transmit buffers (%u MB), took %0.1lf ms\", 50000, 97, 50, 0, 26.2);"
make clean-all > /dev/null && make -j10 > /dev/null
./unloadedLatency  |& tee -a "${LOG_DIR}/nanoLog_complexString.rcdf"
sleep 10
