########
## This make file is adapted from the benchmark GNUmakefile.
## It compiles a test suite of tricky syntax with the preprocessor
#######

####
## Required Library Variables
####

# USER_OBJS specifies all user object files to be compiled; this is used by
# NanoLog to determine when all the user sources have been preprocessed
# and compiled.
USER_SRCS=main.cc SimpleTestObject.cc folder/main.cc
USER_OBJS=$(USER_SRCS:.cc=.o)

# Root of the NanoLog Repository
NANOLOG_DIR=..

# Must be specified AFTER defining NANOLOG_DIR and USER_OBJ's
include $(NANOLOG_DIR)/NanoLogMakeFrag

####
# User Section
####
# -O3 should always be passed for high performance
CXXFLAGS= -std=c++0x -O3
CXX=g++

all: run-test run-empty-test
	@printf "\r\n\033[0;32mIntegration Tests completed without error!\033[0m\r\n"

# [Required] run-cxx will compile the user C++ source file into an object file using
# the NanoLog system. See documentation in the Library Compilation section.
%.o: %.cc
	$(call run-cxx, $@, $<, $(CXXFLAGS))

testApp: $(USER_OBJS) libNanoLog.a
	$(CXX) -o testApp $(CXXFLAGS) $(USER_OBJS) -L. -lNanoLog $(NANO_LOG_LIBRARY_LIBS)

basic_decompressor:
	$(MAKE) -C basicDecompressor $(MAKEFLAGS)
	cp basicDecompressor/decompressor basic_decompressor

run-test: testApp basic_decompressor
	@rm -f /tmp/testLog
	./testApp > /dev/null
	@chmod 666 /tmp/testLog

	@# Test for string special case
	@test -n "$$(find /tmp/testLog -size -100000c)" || \
		(printf "\r\n\033[0;31mError: /tmp/testLog is very large, suggesting a failure of the 'Special case string precision' test in main.cc\033[0m" \
		&& echo "" && exit 1)

	@# Run the decompressor and cut out the timestamps before the ':'
	./decompressor decompress /tmp/testLog | cut -d':' -f5- > output.txt
	diff -w expected/regularRun.txt output.txt

	@# Run the decompressor without embedded functions and cut the timestamps before the ':'
	./basic_decompressor decompress /tmp/testLog | cut -d':' -f5- > basic_output.txt
	diff -w expected/regularRun.txt basic_output.txt

	@# Run the decompressor without embedded functions and cut the timestamps before the ':'
	./basic_decompressor decompressUnordered /tmp/testLog | cut -d':' -f5- > basic_unordered.txt
	diff -w expected/regularRun.txt basic_unordered.txt

	@# Lookups
	./decompressor find /tmp/testLog "" > dictionary_lookup.txt
	diff -w expected/dictionaryFindAll.txt dictionary_lookup.txt

	./decompressor find /tmp/testLog "Debug" > dictionary_lookup.txt
	diff -w expected/dictionaryFindDebug.txt dictionary_lookup.txt

	@# Run the decompressor one more time as if appending to a log file
	./testApp > /dev/null
	./decompressor decompress /tmp/testLog | cut -d':' -f5- > appended_output.txt
	diff -w expected/appendedFile.txt appended_output.txt

	@rm -f /tmp/testLog basic_output.txt output.txt appended_output.txt dictionary_lookup.txt basic_unordered.txt

run-empty-test: testApp
	@touch emptyFile
	@printf "Error: Could not read initial checkpoint, the compressed log may be corrupted.\r\n" > expected.txt
	@printf "Unable to open file emptyFile\r\n" >> expected.txt

	./decompressor decompress emptyFile > output.txt 2>&1 || true
	diff -w expected.txt output.txt
	@rm -f output.txt expected.txt emptyFile

clean:
	@rm -f *.o testApp folder/*.o /tmp/testLog output.txt emptyFile *.pyc decompressor basic_decompressor
