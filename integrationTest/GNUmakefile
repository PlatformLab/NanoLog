#######
## This makefile compiles integration tests containing difficult
## log statements (i.e. funny syntax) for use with both the
## preprocessor and C++17 versions of NanoLog.
#######

# Environment variable determines whether the makefile will compile
# the Preprocessor version of NanoLog (yes) or Cpp17 NanoLog (no)
#
# Note that if you change this variable, you MUST make-clean since
# the two libraries and binaries created are not cross-compatible!
PREPROCESSOR_NANOLOG ?= no

# All test sources
USER_SRCS=main.cc SimpleTestObject.cc folder/main.cc
USER_OBJS=$(USER_SRCS:.cc=.o)

# Root of the NanoLog Repository
NANOLOG_DIR=..

ifeq ($(PREPROCESSOR_NANOLOG),yes)
EXTRA_NANOLOG_FLAGS=-DPREPROCESSOR_NANOLOG
endif

# Must be specified AFTER defining NANOLOG_DIR and USER_OBJ's
include $(NANOLOG_DIR)/NanoLogMakeFrag

CXXFLAGS= -Wformat -std=c++17 -O3 $(CXXWARNS) -Werror

all: testApp basic_decompressor

# Either use the preprocessor to compile the sources or not, depending on the flag.
ifeq ($(PREPROCESSOR_NANOLOG),yes)
%.o: %.cc
	$(CXX) -E -I $(RUNTIME_DIR) $< -o $<.i -std=c++11 -DPREPROCESSOR_NANOLOG
	@mkdir -p generated
	python $(PREPROC_DIR)/parser.py --mapOutput="generated/$<.map" $<.i
	$(CXX) -I $(RUNTIME_DIR) -c -o $@ $<.ii $(CXXFLAGS)
	@rm -f $<.i $<.ii generated/GeneratedCode.cc
else
%.o: %.cc
	$(CXX) -I $(RUNTIME_DIR) -c -o $@ $< $(CXXFLAGS)
endif

testApp: $(USER_OBJS) libNanoLog.a
	$(CXX) -o testApp $(CXXFLAGS) $(USER_OBJS) -L. -lNanoLog $(NANO_LOG_LIBRARY_LIBS)

# The dependency ensures that we build testApp with the appropriate flags
# before we wipe them out to make the basicDecompressor.
basic_decompressor: testApp
	$(MAKE) -C basicDecompressor $(MAKEFLAGS)
	cp basicDecompressor/decompressor basic_decompressor

clean:
	@rm -f *.o testApp folder/*.o ./testLog output.txt emptyFile *.pyc decompressor basic_decompressor
