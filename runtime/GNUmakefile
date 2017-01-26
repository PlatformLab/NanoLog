# Common Sources
SRCS=Cycles.cc Util.cc testHelper/GeneratedCode.cc Log.cc TimeTrace.cc
DEPS=Fence.h Log.h Common.h Packer.h
OBJECTS:=$(SRCS:.cc=.o)

# Test Specific Sources
TESTS=LogTest.cc  NanoLog.cc NanoLogTest.cc PackerTest.cc
TEST_OBJS=$(TESTS:.cc=.o)

GTEST_DIR="../googletest/googletest"
INCLUDES=-I../benchmark -I. -ItestHelper -I${GTEST_DIR}/include
LIBS=-L. -lgtest -lrt -pthread
CXX_ARGS=-std=c++11 -g -O3

all: test Perf

%.o:%.cc %.h $(DEPS)
	g++ $(CXX_ARGS) $(INCLUDES) -c $< -o $@

%.o:%.cc $(DEPS)
	g++ $(CXX_ARGS) $(INCLUDES) -c $< -o $@

$(TEST_OBJS): %.o:%.cc $(DEPS) libgtest.a
	g++ $(CXX_ARGS) $(INCLUDES) $(LIBS) -c $< -o $@

test: $(OBJECTS) $(TEST_OBJS) libgtest.a
	g++ $(CXX_ARGS) $(INCLUDE) -isystem ${GTEST_DIR}/include $^ $(GTEST_DIR)/src/gtest_main.cc -o test $(LIBS)

Perf: Perf.o PerfHelper.o PerfHelper.h $(OBJECTS)
	g++ $(CXX_ARGS) $^ -o Perf

libgtest.a:
	g++ -std=c++11 -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc $(LIBS)
	ar -rv libgtest.a gtest-all.o
	@rm gtest-all.o

testHelper/GeneratedCode.cc: testHelper/client.cc
	g++ $(CXX_ARGS) -E -I. testHelper/client.cc -o testHelper/client.cc.i
	python ../preprocessor/parser.py --mapOutput="testHelper/client.map" testHelper/client.cc.i
	python ../preprocessor/parser.py --combinedOutput="testHelper/GeneratedCode.cc" testHelper/client.map
	@rm -f testHelper/client.map testHelper/client.cc.*

clean:
	rm -f Perf test compressedLog testHelper/GeneratedCode.o *.o *.gch *.log

clean-all: clean
	rm -f libgtest.a testHelper/GeneratedCode.cc
