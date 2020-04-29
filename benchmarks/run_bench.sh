#! /bin/bash -e

if [ $# -eq 0 ]
  then
    echo "Usage $0 <testname>"
    exit 1
fi

if [ -z "$1" ]
  then
    echo "Usage $0 <testname>"
fi

TEST_NAME="$1"
TEST_DIR="results/$(date +%Y%m%d%H%M%S)_${TEST_NAME}"
SETUP_FILE="${TEST_DIR}/${TEST_NAME}_setup.txt"
CONSOLE_OUT_FILE="${TEST_DIR}/${TEST_NAME}.log"
GIT_FILE="${TEST_DIR}/${TEST_NAME}_git.diff"
RCDF_FILE="${TEST_DIR}/${TEST_NAME}.rcdf"

mkdir -p $TEST_DIR
cp ${0} ${TEST_DIR}

## Printing to SETUP_FILE
printf "Test: ${TEST_NAME}\r\n" > $SETUP_FILE
printf "Host: $(hostname)\r\n">> $SETUP_FILE
printf "Date: $(date)\r\n\r\n">> $SETUP_FILE

printf "================\r\n" >> $SETUP_FILE
printf "==  Git Info  ==\r\n" >> $SETUP_FILE
printf "================\r\n" >> $SETUP_FILE
printf "\r\n== Git Diff ==\r\n" >> $SETUP_FILE
git diff                      >> $SETUP_FILE
printf "\r\n\r\n== Git Log ==\r\n" >> $SETUP_FILE
git log -n 2                  >> $SETUP_FILE
printf "\r\n\r\n**Note** Since the benchmarking branch can change due to master rebases, see ${GIT_FILE} for more info...\r\n\r\n" >> $SETUP_FILE

printf "================\r\n" >> $SETUP_FILE
printf "== Enviroment ==\r\n" >> $SETUP_FILE
printf "================\r\n" >> $SETUP_FILE
printf "g++ version:\r\n"     >> $SETUP_FILE
g++ --version                 >> $SETUP_FILE
printf "\r\nKernel:\r\n"      >> $SETUP_FILE
uname -a                      >> $SETUP_FILE
printf "\r\nDisks:\r\n"       >> $SETUP_FILE
lsblk -io KNAME,TYPE,SIZE,MODEL,MOUNTPOINT >> $SETUP_FILE
printf "\r\nDisk Utilization:\r\n" >> $SETUP_FILE
df -h /                       >> $SETUP_FILE
printf "\r\nCPU Info:\r\n"    >> $SETUP_FILE
grep -P "model name|processor|MHz" /proc/cpuinfo >> $SETUP_FILE
grep MemTotal /proc/meminfo   >> $SETUP_FILE

### Printing to GIT_FILE
printf "=== Git Log (relative to master) ===\r\n"  >  $GIT_FILE
git log -n 3   >> $GIT_FILE

# printf "\r\n\r\n=== Git Diff (from benchmark head) ===\r\n" >> $GIT_FILE
# git diff                      >> $GIT_FILE

printf "\r\n\r\n=== Git Diff (to master) === \r\n" >> $GIT_FILE
git diff master               >> $GIT_FILE


# Run the actual test
make clean-all && make -j4 && rm -rf /tmp/logFile && clear
{ ./benchmark 2>&1; } | tee $CONSOLE_OUT_FILE
# ./decompressor rcdfTime /tmp/logFile > $RCDF_FILE

