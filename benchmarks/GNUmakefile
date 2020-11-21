########
## Builds the base application used in a lot of NanoLog benchmarks
########

# Environment variable determines whether the makefile will compile
# the Preprocessor version of NanoLog (yes) or Cpp17 NanoLog (no)
#
# Note that if you change this variable, you MUST make-clean since
# the two libraries and binaries created are not cross-compatible!
PREPROCESSOR_NANOLOG ?= yes

# All user sources
USER_SRCS=Benchmark.cc
USER_OBJS=$(USER_SRCS:.cc=.o)

# Root of the NanoLog Repository
NANOLOG_DIR=..

# Enables logging for producer statistics
EXTRA_NANOLOG_FLAGS=-DRECORD_PRODUCER_STATS

ifeq ($(PREPROCESSOR_NANOLOG),yes)
EXTRA_NANOLOG_FLAGS=-DRECORD_PRODUCER_STATS -DPREPROCESSOR_NANOLOG
endif

# Must be specified AFTER defining NANOLOG_DIR and USER_OBJ's
include $(NANOLOG_DIR)/NanoLogMakeFrag

####
# User Section
####
USER_SRCS=Benchmark.cc

# -DNDEBUG and -O3 should always be passed for high performance
ifeq ($(PREPROCESSOR_NANOLOG),yes)
CXXFLAGS= -std=c++11 -DNDEBUG -O3 -g
else
CXXFLAGS= -Werror=format -std=c++17 -DNDEBUG -O3 -g
endif
CXX=g++

all: benchmark

ifeq ($(PREPROCESSOR_NANOLOG),yes)
# [Required] run-cxx will compile the user C++ source file into an object file using
# the NanoLog system. See documentation in the Library Compilation section.
%.o: %.cc BenchmarkConfig.h
	$(call run-cxx, $@, $<, $(CXXFLAGS) -I., -DPREPROCESSOR_NANOLOG)
else
%.o: %.cc BenchmarkConfig.h
	$(CXX) -I $(RUNTIME_DIR) -c -o $@ $< $(CXXFLAGS)
endif

BenchmarkConfig.h: ./genConfig.py
	python ./genConfig.py

# [Required] $(LIBRARY_LIBS) must be used
benchmark: Benchmark.o libNanoLog.a
	$(CXX) $(CXXFLAGS) -o benchmark Benchmark.o -L. -lNanoLog $(NANO_LOG_LIBRARY_LIBS)

unloadedLatency: UnloadedLatency.o libNanoLog.a
	$(CXX) $(CXXFLAGS) -o unloadedLatency UnloadedLatency.o -L. -lNanoLog $(NANO_LOG_LIBRARY_LIBS)

interference: Interference.o libFastLogger.a
	$(CXX) $(CXXFLAGS) -o interference Interference.o -L. -lFastLogger $(LIBRARY_LIBS)

clean:
	@rm -f *.o benchmark /tmp/logFile compressedLog

####
# Library Compilation (copy verbatim)
####

# This is for compression testing (benchmark branch only)
SNAPPY_DIR=./snappy/

$(SNAPPY_DIR)/installDir/lib/libsnappy.a:
	cd $(SNAPPY_DIR) && \
	./autogen.sh && \
	CFLAGS="-O3 -DNDEBUG" CXXFLAGS="-O3 -DNDEBUG" ./configure && \
	make -j10 && \
	make install prefix=$(realpath $(SNAPPY_DIR))/installDir

libsnappy.a: $(SNAPPY_DIR)/installDir/lib/libsnappy.a
	cp $(SNAPPY_DIR)/installDir/lib/libsnappy.a .
