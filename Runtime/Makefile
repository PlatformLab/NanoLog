SRCS=CacheTrace.cpp Cycles.cpp Util.cpp FastLogger.cpp LogCompressor.cpp
INCLUDES=CacheTrace.h Cycles.h Util.h Packer.h Common.h FastLogger.h
BENCHMARKS=Benchmark.cpp
TESTS=PackerTest.cpp FastLoggerTest.cpp

GTEST_DIR="../googletest/googletest"
OBJECTS:=$(SRCS:.cpp=.o)



all: test

libPerfUtils.a: $(SRCS) $(INCLUDES)
	g++  -O3 -c  -std=c++0x  $(SRCS)
	ar -cvq libPerfUtils.a $(OBJECTS)

test: $(TESTS) $(SRCS) $(INCLUDES) libgtest.a
	g++ -std=c++11 -isystem ${GTEST_DIR}/include -pthread $(TESTS) $(SRCS) libgtest.a -DEXPOSE_PRIVATES -o test

libgtest.a:
	g++ -std=c++11 -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -pthread -c $(GTEST_DIR)/src/gtest-all.cc
	ar -rv libgtest.a gtest-all.o
	rm gtest-all.o

TTTest: TTTest.cpp
	g++ -o TTTest -std=c++0x  TTTest.cpp TimeTrace.cpp Cycles.cpp

benchmark : $(SRCS) $(INCLUDES)
	g++ -o benchmark -O3 -std=c++0x $(SRCS)  $(BENCHMARKS) -lpthread -lrt

clean:
	rm -f libPerfUtils.a TTTest test benchmark *.o *.gch *.log *.a
