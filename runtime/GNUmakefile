# Common Sources
SRCS=Cycles.cc Util.cc testHelper/BufferStuffer.cc
DEPS=Fence.h BufferUtils.h Common.h Packer.h
OBJECTS:=$(SRCS:.cc=.o)

# Test Specific Sources
TESTS=BufferUtilsTest.cc  NanoLog.cc NanoLogTest.cc PackerTest.cc
TEST_OBJS=$(TESTS:.cc=.o)

GTEST_DIR="../googletest/googletest"
INCLUDES=-I. -ItestHelper -I${GTEST_DIR}/include
LIBS=-L. -lgtest -lrt -pthread
CXX_ARGS=-std=c++11 -g -O3

all: test Perf

%.o:%.cc %.h $(DEPS)
	g++ $(CXX_ARGS) $(INCLUDES) -c $< -o $@

%.o:%.cc
	g++ $(CXX_ARGS) $(INCLUDES) -c $< -o $@

$(TEST_OBJS): %.o:%.cc $(DEPS) libgtest.a
	g++ $(CXX_ARGS) $(INCLUDES) $(LIBS) -c $< -o $@

test: $(TEST_OBJS) $(OBJECTS) libgtest.a
	g++ $(CXX_ARGS) $(INCLUDE) -isystem ${GTEST_DIR}/include $^ $(GTEST_DIR)/src/gtest_main.cc -o test $(LIBS)

Perf: Perf.cc PerfHelper.cc PerfHelper.h $(OBJECTS) $(DEPS)
	g++ $(CXX_ARGS) $^ -o Perf

libgtest.a:
	g++ -std=c++11 -isystem $(GTEST_DIR)/include -I$(GTEST_DIR) -c $(GTEST_DIR)/src/gtest-all.cc $(LIBS)
	ar -rv libgtest.a gtest-all.o
	@rm gtest-all.o

clean:
	rm -f Perf test compressedLog testHelper/*.o *.o *.gch *.log

clean-all: clean
	rm -f libgtest.a
