#! /bin/bash

if [ "$1" = "" ]
  then
  echo "Usage: ./run.sh <c++files+>"
  exit 1
fi

#
# We can use just the address if do objdump on the data
#
# clear
# g++ -O3 $1
# ./a.out
# objdump -s -j .rodata ./a.out

# This should be better abstracted in the future
RUNTIME_FILES="../../Runtime/Util.cpp ../../Runtime/FastLogger.cpp ../../Runtime/Cycles.cpp ../../Runtime/LogCompressor.cpp"
O_FILES="Util.o FastLogger.o Cycles.o LogCompressor.o"
EXTRA_PARAMS="-O3 -std=c++11 -lpthread"

rm -rf processedFiles 2> /dev/null
mkdir processedFiles

STRIPPED_FILES=""
for var in "$@"
do
  FILENAME=${var##*/}
  g++ -O3 -std=c++11 -E ${var} > processedFiles/${FILENAME}.i || exit 1
  python parser.py  processedFiles/${FILENAME}.i || exit 1
  STRIPPED_FILES="$STRIPPED_FILES processedFiles/${FILENAME}.ii"
done

g++ -c $RUNTIME_FILES $EXTRA_PARAMS
ar rs runtime.a $O_FILES
rm $O_FILES

g++ -fpreprocessed $EXTRA_PARAMS $STRIPPED_FILES "runtime.a" || exit 1
