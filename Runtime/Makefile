SRCS=CacheTrace.cpp TimeTrace.cpp Cycles.cpp Util.cpp
INCLUDES=CacheTrace.h TimeTrace.h Cycles.h Util.h
OBJECTS:=$(SRCS:.cpp=.o)



all: libPerfUtils.a

libPerfUtils.a: $(SRCS) $(INCLUDES)
	g++  -O3 -c  -std=c++0x  $(SRCS)
	ar -cvq libPerfUtils.a $(OBJECTS)

TTTest: TTTest.cpp
	g++ -o TTTest -std=c++0x  TTTest.cpp TimeTrace.cpp Cycles.cpp

Benchmark : Benchmark.cpp
	g++ -o Benchmark -O3 -std=c++0x Benchmark.cpp TimeTrace.cpp Cycles.cpp -lpthread -lrt

clean:
	rm -f libPerfUtils.a TTTest Benchmark *.o *.gch *.log
