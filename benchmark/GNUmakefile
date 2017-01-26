########
## This file serves as an example for how users of the NanoLog system
## should layout their maikefile rules. The "Library Compilation" section
## shall be copied verbatim while the "User Section" can be customized
## with the following exceptions:
##   1) User C++ files MUST be compiled into .o files with run-cxx
##			before being linked into the main executable/library
##   2) The Macro USER_OBJS MUST be used to identify .o files from above
##   3) The final executable links NanoLog.a, pthread, and lrt
##   4) The RUNTIME_DIR and PREPROC_DIR shall identify the directories
##			where the NanoLog's Runtime and Preproessor sources are
##
## Note that run-cxx will add additional warnings to the user compilation
## flags to detect misuses of the NanoLog API.
#######

####
## [Required] Library Variables
####

# [Required] Specifies the path to the NanoLog's Library directory
NANOLOG_DIR=..

# [Required] MUST use macro to identify user *.o files
USER_OBJS=$(USER_SRCS:.cc=.o)


####
# User Section
####
USER_SRCS=Benchmark.cc UnloadedLatency.cc

# -DNDEBUG and -O3 should always be passed for high performance
CXXFLAGS= -std=c++11 -DNDEBUG -O3 -g
CXX=g++

all: benchmark unloadedLatency

# [Required] run-cxx will compile the user C++ source file into an object file using
# the NanoLog system. See documentation in the Library Compilation section.
%.o: %.cc
	$(call run-cxx, $@, $<, $(CXXFLAGS) -I.)

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

RUNTIME_DIR=$(NANOLOG_DIR)/runtime
PREPROC_DIR=$(NANOLOG_DIR)/preprocessor

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

# run-cxx:
# Compile a user C++ source file to an object file using the NanoLog system.
# The first parameter $(1) should be the output filename (*.o)
# The second parameter $(2) should be the input filename (*.cc)
# The optional third parameter $(3) is any additional options compiler options.
define run-cxx
	$(CXX) -E -I $(RUNTIME_DIR) $(2) -o $(2).i $(3)
	@mkdir -p generated
	python $(PREPROC_DIR)/parser.py --mapOutput="generated/$(2).map" $(2).i
	$(CXX) -I $(RUNTIME_DIR) -c -o $(1) $(2).ii $(3)
	@rm -f $(2).i $(2).ii generated/GeneratedCode.cc
endef

RUNTIME_CXX_FLAGS= -std=c++11 -O3 -DNDEBUG -g
NANO_LOG_LIBRARY_LIBS=-lrt -pthread -lsnappy

RUNTIME_DEPS=$(wildcard $(RUNTIME_DIR)/*.h)
RUNTIME_CC=$(RUNTIME_DIR)/Cycles.cc $(RUNTIME_DIR)/NanoLog.cc \
		 $(RUNTIME_DIR)/Util.cc $(RUNTIME_DIR)/TimeTrace.cc $(RUNTIME_DIR)/Log.cc
RUNTIME_OBJS=$(RUNTIME_CC:.cc=.o)

COMWARNS := -Wall -Wformat=2 -Wextra \
           -Wwrite-strings -Wno-unused-parameter -Wmissing-format-attribute
CWARNS   := $(COMWARNS) -Wmissing-prototypes -Wmissing-declarations -Wshadow \
		-Wbad-function-cast
CXXWARNS := $(COMWARNS) -Wno-non-template-friend -Woverloaded-virtual \
		-Wcast-qual -Wcast-align -Wconversion
ifeq ($(COMPILER),gnu)
CXXWARNS += -Weffc++
endif

generated/GeneratedCode.o: $(USER_OBJS)
	mkdir -p generated
	python $(PREPROC_DIR)/parser.py --combinedOutput="generated/GeneratedCode.cc" $(shell find generated -type f -name "*.map" -printf ' "%h/%f" ')
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) -c -o $@ generated/GeneratedCode.cc -I. -I $(RUNTIME_DIR) -Igenerated

$(RUNTIME_DIR)/%.o: $(RUNTIME_DIR)/%.cc
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) -c -o $@ $< -I. -I $(RUNTIME_DIR) -I $(SNAPPY_DIR) -Igenerated -Werror

libNanoLog.a: $(RUNTIME_OBJS) $(RUNTIME_DEPS) generated/GeneratedCode.o decompressor libsnappy.a
	ar -cr libNanoLog.a $(RUNTIME_OBJS) generated/GeneratedCode.o

decompressor: generated/GeneratedCode.o $(RUNTIME_DIR)/Cycles.o $(RUNTIME_DIR)/Util.o $(RUNTIME_DIR)/Log.o $(RUNTIME_DIR)/LogDecompressor.cc
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) $^ -o decompressor -I. -I$(RUNTIME_DIR) -Igenerated -Werror -L. $(NANO_LOG_LIBRARY_LIBS)

clean-all: clean
	@rm -f libNanoLog.a $(RUNTIME_OBJS) decompressor
	@rm -rf generated
