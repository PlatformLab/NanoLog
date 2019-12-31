#! /bin/bash -e

function runCommonTests() {
  rm -f ./testLog
  ./testApp > /dev/null
  chmod 666 ./testLog

  # Test for string special case
  test -n "$$(find ./testLog -size -100000c)" || \
    (printf "\r\n\033[0;31mError: ./testLog is very large, suggesting a failure of the 'Special case string precision' test in main.cc\033[0m" \
    && echo "" && exit 1)

  # Run the decompressor with embedded functions and cut out the timestamps before the ':'
  printf "Checking normal decompression..."
  ./decompressor decompress ./testLog | cut -d':' -f5- > output.txt
  diff -w expected/regularRun.txt output.txt
  printf " OK!\r\n"

  # Run the unordered decompressor without embedded functions and cut the timestamps before the ':'
  printf "Checking unordered decompression..."
  ./basic_decompressor decompressUnordered ./testLog | cut -d':' -f5- > basic_unordered.txt
  diff -w expected/regularRun.txt basic_unordered.txt
  printf " OK!\r\n"

  # Run the decompressor without embedded functions and cut the timestamps before the ':'
  printf "Checking decompression with a generic decompressor..."
  ./basic_decompressor decompress ./testLog | cut -d':' -f5- > basic_output.txt
  diff -w expected/regularRun.txt basic_output.txt
  printf " OK!\r\n"

  # Run the app once more as if appended to a log file and decompress again
  printf "Checking decompression with appended logs..."
  ./testApp > /dev/null
  ./decompressor decompress ./testLog | cut -d':' -f5- > appended_output.txt
  diff -w expected/appendedFile.txt appended_output.txt
  printf " OK!\r\n"

  # Empty file tests
  printf "Checking decompression error handling on malformed files..."
  touch emptyFile
  printf "Error: Could not read initial checkpoint, the compressed log may be corrupted.\r\n" > expected.txt
  printf "Unable to open file emptyFile\r\n" >> expected.txt

  ./decompressor decompress emptyFile > output.txt 2>&1 || true
  diff -w expected.txt output.txt
  printf " OK!\r\n"

  # clean up
  rm -f ./testLog basic_output.txt output.txt appended_output.txt dictionary_lookup.txt basic_unordered.txt expected.txt emptyFile
}

## Actual Test Runner
clear && clear

make clean-all
printf "Building C++17 NanoLog Integration Tests...."
mkdir -p generated
PREPROCESSOR_NANOLOG=no make -j10 > /dev/null

printf "Done!\r\nRunning Tests...\r\n"
runCommonTests
printf "\r\nIntegration Tests completed without error for C++17 NanoLog!\r\n"

make clean-all
printf "\r\nBuilding Preprocessor NanoLog Integration Tests...."
PREPROCESSOR_NANOLOG=yes make -j10 > /dev/null

printf "Done!\r\nRunning Tests...\r\n"
runCommonTests

# Run one additional test for the preprocessor NanoLog that would
# output the entire dictionary
rm -f ./testLog
./testApp > /dev/null
chmod 666 ./testLog

printf "Checking static dictionary lookups..."
# The 'cut | sort' removes the log id's and gives the messages a repeatable ordering
./decompressor find ./testLog "" | cut -c 8- | sort > dictionary_lookup.txt
cat expected/dictionaryFindAll.txt | cut -c 8- | sort > dictionaryFindAll.txt
diff -w dictionaryFindAll.txt dictionary_lookup.txt
rm -f dictionaryFindAll.txt

./decompressor find ./testLog "Debug" | cut -c 8- | sort > dictionary_lookup.txt
cat expected/dictionaryFindDebug.txt | cut -c 8- | sort > dictionaryFindDebug.txt
diff -w dictionaryFindDebug.txt dictionary_lookup.txt
rm -f dictionaryFindAll.txt dictionary_lookup.txt dictionaryFindDebug.txt
printf " OK!\r\n"

printf "\r\nIntegration Tests completed without error for Preprocessor NanoLog!\r\n"

make clean
printf "\r\n\033[0;32mAll Integration Tests completed without error!\033[0m\r\n"
