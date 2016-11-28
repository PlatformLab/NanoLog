SRCS=Cycles.cc Util.cc FastLogger.cc LogCompressor.cc
INCLUDES=Cycles.h Util.h Packer.h Common.h FastLogger.h BufferUtils.h
BENCHMARKS=Benchmark.cc
PERF=Perf.cc PerfHelper.cc
PERF_DEPS=PerfHelper.h
TESTS=PackerTest.cc FastLoggerTest.cc

GTEST_DIR="../googletest/googletest"
OBJECTS:=$(SRCS:.cc=.o)

CXX_ARGS=-std=c++11 -pthread -lrt -g -O3



all: test Perf

libPerfUtils.a: $(SRCS) $(INCLUDES)
	g++  -O3 -c  -std=c++0x  $(SRCS)
	ar -cvq libPerfUtils.a $(OBJECTS)

$(OBJECTS) : %.o :%.cc
	g++ $(CXX_ARGS) -c $< -o $@

test: $(TESTS) $(OBJECTS) $(INCLUDES) libgtest.a
	g++ $(CXX_ARGS) -isystem ${GTEST_DIR}/include $(TESTS) $(OBJECTS) $(GTEST_DIR)/src/gtest_main.cc libgtest.a -DEXPOSE_PRIVATES -o test

Perf : $(OBJECTS) $(INCLUDES) $(PERF) $(PERF_DEPS)
	g++ $(CXX_ARGS) $(PERF) $(OBJECTS) -o Perf

libgtest.a:
	g++ -std=c++11 -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -pthread -c $(GTEST_DIR)/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o
	rm gtest-all.o

benchmark : $(SRCS) $(INCLUDES)
	g++ -o benchmark -O3 -std=c++0x $(SRCS)  $(BENCHMARKS) -lpthread -lrt

clean:
	rm -f Perf test benchmark compressedLog *.o *.gch *.log

clean-all: clean
	rm -f libPerfUtils.a
