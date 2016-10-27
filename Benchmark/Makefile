# User Sources
SRCS=Benchmark.cpp
OBJS=$(SRCS:.cpp=.o)

CXX=g++
CXXFLAGS= -std=c++0x -O3

all: benchmark

# Step 1: Compile all the user code w/ preprocessing
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $^ -o $<.i -I $(RUNTIME_DIR) -E
	python $(PREPROC_DIR)/parser.py --map="mapping.map" $^.i
	$(CXX) $(CXXFLAGS) -c -o $@ $<.ii -I $(RUNTIME_DIR)
	rm -f $<.i $<.ii

# Step 4: Final output
benchmark: $(OBJS) libFastLogger.a
	$(CXX) $(CXXFLAGS) -o benchmark $(OBJS) $(RUNTIME_OBJS) -pthread -lrt

clean:
	@rm -f *.o $(OBJS) *.i *.ii benchmark

# Library Specific Options
RUNTIME_DIR=../Runtime
PREPROC_DIR=../Preprocessor

RUNTIME_DEPS=$(wildcard $(RUNTIME_DIR)/*.h)
RUNTIME_CPP=$(RUNTIME_DIR)/Cycles.cpp $(RUNTIME_DIR)/FastLogger.cpp $(RUNTIME_DIR)/LogCompressor.cpp $(RUNTIME_DIR)/Util.cpp
RUNTIME_OBJS=$(RUNTIME_CPP:.cpp=.o)

# Step 2: Generate the runtime libraries after the compilation of user code
generated/BufferStuffer.h: $(OBJS)
	mkdir -p generated
	python $(PREPROC_DIR)/parser.py --map="mapping.map" --"output=$@"

# Step 3: Compile the runtime library
$(RUNTIME_DIR)/%.o: $(RUNTIME_DIR)/%.cpp generated/BufferStuffer.h
	$(CXX) $(CXXFLAGS) -c -o $@ $< -I $(RUNTIME_DIR) -Igenerated -pthread -lrt

libFastLogger.a: $(RUNTIME_OBJS) $(RUNTIME_DEPS)
	ar -cvq libFastLogger.a $(RUNTIME_OBJS)

clean-all: clean
	@rm -rf libFastLogger.a $(RUNTIME_OBJS) mapping.map generated runtime