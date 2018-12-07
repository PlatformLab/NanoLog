# Common Sources
SRCS=Cycles.cc Util.cc testHelper/GeneratedCode.cc Log.cc NanoLog.cc RuntimeLogger.cc  TimeTrace.cc
OBJECTS:=$(SRCS:.cc=.o)

# Test Specific Sources
TESTS=LogTest.cc NanoLogTest.cc NanoLogCpp17Test.cc PackerTest.cc
TEST_OBJS=$(TESTS:.cc=.o)

GTEST_DIR="../googletest/googletest"
INCLUDES=-I. -ItestHelper -I${GTEST_DIR}/include
LIBS=-L. -lgtest -lrt -pthread
CXX_ARGS=-std=c++17 -g -O3
CXX?=g++

all: test Perf decompressor libNanoLog.a

%.o:%.cc %.h
	$(CXX) $(CXX_ARGS) $(INCLUDES) -c $< -o $@

%.o:%.cc
	$(CXX) $(CXX_ARGS) $(INCLUDES) -c $< -o $@

$(TEST_OBJS): %.o:%.cc libgtest.a
	$(CXX) $(CXX_ARGS) $(INCLUDES) $(LIBS) -c $< -o $@

test: $(OBJECTS) $(TEST_OBJS) libgtest.a
	$(CXX) $(CXX_ARGS) $(INCLUDE) -isystem ${GTEST_DIR}/include $^ $(GTEST_DIR)/src/gtest_main.cc -o test $(LIBS)

Perf: Perf.o PerfHelper.o PerfHelper.h $(OBJECTS)
	$(CXX) $(CXX_ARGS) $^ -o Perf -lrt -pthread

libgtest.a:
	$(CXX) -std=c++11 -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc $(LIBS)
	ar -rv libgtest.a gtest-all.o
	@rm gtest-all.o

libNanoLog.a: $(OBJECTS)
	ar -cr libNanoLog.a $(OBJECTS)

testHelper/GeneratedCode.cc: testHelper/client.cc
	$(CXX) $(CXX_ARGS) -E -I. testHelper/client.cc -o testHelper/client.cc.i
	python ../preprocessor/parser.py --mapOutput="testHelper/client.map" testHelper/client.cc.i
	python ../preprocessor/parser.py --combinedOutput="testHelper/GeneratedCode.cc" testHelper/client.map
	@rm -f testHelper/client.map testHelper/client.cc.*

# Compiles a generic decompressor; the GeneratedCode.o is only necessary for
# legacy code compatibility.
decompressor: testHelper/GeneratedCode.o Cycles.o Util.o Log.o LogDecompressor.cc
	$(CXX) $(CXX_ARGS) $^ -o decompressor $(INCLUDES) -Igenerated -Werror

clean:
	rm -f Perf test compressedLog testHelper/GeneratedCode.o *.o *.gch *.log ./.depend

clean-all: clean
	rm -f libgtest.a testHelper/GeneratedCode.cc

# Automatic rules to build *.h dependencies. Taken from
# https://stackoverflow.com/questions/2394609/makefile-header-dependencies
depend: .depend

.depend: $(SRCS)
	rm -f ./.depend
	$(CXX) $(CXX_ARGS) $(INCLUDES) -MM $^  > ./.depend;

include .depend
