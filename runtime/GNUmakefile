####
# This GNUmakefile is in charge of building the C++17 version of the NanoLog
# library, its gtest unit tests, and some internal performance benchmarks.
# To build the Preprocessor version of the library, see ../NanoLogMakeFrag
###

# Common Sources
SRCS=Cycles.cc Util.cc Log.cc NanoLog.cc RuntimeLogger.cc TimeTrace.cc
OBJECTS:=$(SRCS:.cc=.o)

# Test Specific Sources
TESTS=LogTest.cc NanoLogTest.cc NanoLogCpp17Test.cc PackerTest.cc
TEST_OBJS=$(addprefix $(TEST_BUILD_DIR)/, $(TESTS:.cc=.o))
GENERATED_OBJ=testHelper/GeneratedCode.o

# Configuration
TEST_BUILD_DIR=test_build
GTEST_DIR="../googletest/googletest"
INCLUDES=-I. -ItestHelper -I${GTEST_DIR}/include
LIBS=-L. -lgtest -lrt -pthread
CXX_ARGS=-std=c++17 -g -O3
CXX?=g++

all: decompressor libNanoLog.a

%.o: %.cc
	$(CXX) $(CXX_ARGS) $(EXTRA_NANOLOG_FLAGS) $(INCLUDES) -c $< -o $@

$(TEST_BUILD_DIR)/%.o: %.cc
	$(CXX) $(CXX_ARGS) -DPREPROCESSOR_NANOLOG $(EXTRA_NANOLOG_FLAGS) $(INCLUDES) -c $< -o $@

# Builds a C++17 version of the library; the generated OBJ is only necessary for error checking
libNanoLog.a: $(OBJECTS) $(GENERATED_OBJ)
	ar -cr libNanoLog.a $(OBJECTS) $(GENERATED_OBJ)

libgtest.a:
	$(CXX) -std=c++11 -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc $(LIBS)
	ar -rv libgtest.a gtest-all.o
	@rm gtest-all.o

# The unit tests also require bits of the preprocessor library to be build,
# these files are stored in a test build directory
PREPROCESSOR_OBJECTS=$(addprefix $(TEST_BUILD_DIR)/, $(OBJECTS))

testHelper/GeneratedCode.cc: testHelper/client.cc
	$(CXX) $(CXX_ARGS) $(EXTRA_NANOLOG_FLAGS) -E -I. testHelper/client.cc -o testHelper/client.cc.i
	python ../preprocessor/parser.py --mapOutput="testHelper/client.map" testHelper/client.cc.i
	python ../preprocessor/parser.py --combinedOutput="testHelper/GeneratedCode.cc" testHelper/client.map
	@rm -f testHelper/client.map testHelper/client.cc.*

test: $(PREPROCESSOR_OBJECTS) $(TEST_OBJS) $(GENERATED_OBJ) libgtest.a
	$(CXX) $(CXX_ARGS) $(EXTRA_NANOLOG_FLAGS) $(INCLUDE) -isystem ${GTEST_DIR}/include $^ $(GTEST_DIR)/src/gtest_main.cc -o test $(LIBS)

# Builds the internal benchmarks
perf: Perf.o PerfHelper.o PerfHelper.h $(OBJECTS) $(GENERATED_OBJ)
	$(CXX) $(CXX_ARGS) $(EXTRA_NANOLOG_FLAGS) $^ -o perf -lrt -pthread

# Compiles a generic decompressor that works for C++17 and Preprocessor NanoLog.
# Note: the GeneratedCode.o is only necessary for legacy code compatibility.
decompressor: $(GENERATED_OBJ) Cycles.o Util.o Log.o LogDecompressor.cc
	$(CXX) $(CXX_ARGS) $(EXTRA_NANOLOG_FLAGS) $^ -o decompressor $(INCLUDES) -Igenerated -Werror

clean:
	rm -f Perf test compressedLog ./decompressor $(GENERATED_OBJ) $(TEST_BUILD_DIR)/*.o *.o *.gch *.log ./.depend

clean-all: clean
	rm -f libgtest.a testHelper/GeneratedCode.cc

# Automatic rules to build *.h dependencies. Taken from
# https://stackoverflow.com/questions/2394609/makefile-header-dependencies
depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(CXX_ARGS) $(INCLUDES) -MM $^ > ./.depend;
	sed 's|[a-zA-Z0-9_-]*\.o|$(TEST_BUILD_DIR)/&|' ./.depend >> ./.depend

include .depend
