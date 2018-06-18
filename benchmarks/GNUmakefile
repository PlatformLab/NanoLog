########
## This file serves as an example for how users of the NanoLog system
## should layout their makefile rules.
##
## The first half ("Required Library Variables") should always be defined
## in the user GNUmakefile and the only requirements on the second half
## ("User Section") are:
##   1) User C++ files MUST be compiled into .o files with run-cxx
##			before being linked into the main executable/library.
##   2) The final executable links NanoLog.a and $(NANO_LOG_LIBRARY_LIBS)
##
## The high level idea is that the NanoLog System needs to preprocess the
## user sources to extract static information and compile that information
## into the NanoLog library before linking it with the user application.
## The way we go about this is to use run-cxx to preprocess the user
## source files, use the USER_OBJS variable to determine when all user
## sources have been compiled, and have the final executable depend on
## libNanoLog.a to force a final library compilation.
########

####
## Required Library Variables
####

# USER_OBJS specifies all user object files to be compiled; this is used by
# NanoLog to determine when all the user sources have been preprocessed
# and compiled.
USER_SRCS=Benchmark.cc
USER_OBJS=$(USER_SRCS:.cc=.o)

# Root of the NanoLog Repository
NANOLOG_DIR=..

# Enables logging for producer statistics
EXTRA_NANOLOG_FLAGS=-DRECORD_PRODUCER_STATS

# Must be specified AFTER defining NANOLOG_DIR and USER_OBJ's
include $(NANOLOG_DIR)/NanoLogMakeFrag

####
# User Section
####
USER_SRCS=Benchmark.cc

# -DNDEBUG and -O3 should always be passed for high performance
CXXFLAGS= -std=c++11 -DNDEBUG -O3 -g
CXX=g++

all: benchmark

# [Required] run-cxx will compile the user C++ source file into an object file using
# the NanoLog system. See documentation in the Library Compilation section.
%.o: %.cc BenchmarkConfig.h
	$(call run-cxx, $@, $<, $(CXXFLAGS) -I.)

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
	@rm -f *.o benchmark /tmp/logFile

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
