########
## This make file is adapted from the benchmark GNUmakefile.
## It compiles a test suite of tricky syntax with the preprocessor
#######

####
# User Section
####
USER_SRCS=simpleTest.cc

# Macro MUST used to identify user *.o files
USER_OBJS=$(USER_SRCS:.cc=.o)

# -O3 should always be passed for high performance
CXXFLAGS= -std=c++0x -O3
CXX=g++

all: test

# run-cxx will compile the user C++ source file into an object file using
# the FastLogger system. See documentation in the Library Compilation section.
%.o: %.cc
	$(call run-cxx, $@, $<, $(CXXFLAGS))

test: $(USER_OBJS) libFastLogger.a
	$(CXX) $(CXXFLAGS) $(USER_OBJS) -L. -lFastLogger -pthread -lrt

clean:
	@rm -f *.o a.out

####
## Library Options
####

# Specifies the path to the FastLogger's Runtime Library
RUNTIME_DIR=../runtime

# Specifies the path to the FastLogger's Preprocessor directory
PREPROC_DIR=../preprocessor

####
# Library Compilation (copy verbatim)
####

# run-cxx:
# Compile a user C++ source file to an object file using the FastLogger system.
# The first parameter $(1) should be the output filename (*.o)
# The second parameter $(2) should be the input filename (*.cc)
# The optional third parameter $(3) is any additional options compiler options.
define run-cxx
	$(CXX) -E -I $(RUNTIME_DIR) $(2) -o $(2).i $(3)
	mkdir -p generated
	python $(PREPROC_DIR)/parser.py --map="generated/mapping.map" $(2).i
	$(CXX) -I $(RUNTIME_DIR) -c -o $(1) $(2).ii $(3)
	rm -f $(2).i $(2).ii generated/BufferStuffer.h
endef

RUNTIME_CXX_FLAGS= -std=c++0x -O3 -DNDEBUG -g

RUNTIME_DEPS=$(wildcard $(RUNTIME_DIR)/*.h)
RUNTIME_CC=$(RUNTIME_DIR)/Cycles.cc $(RUNTIME_DIR)/FastLogger.cc \
																							$(RUNTIME_DIR)/Util.cc
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

generated/BufferStuffer.h: $(USER_OBJS)
	mkdir -p generated
	python $(PREPROC_DIR)/parser.py --map="generated/mapping.map" --output="$@"

$(RUNTIME_DIR)/%.o: $(RUNTIME_DIR)/%.cc generated/BufferStuffer.h
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) -c -o $@ $< -I $(RUNTIME_DIR) -Igenerated -pthread -lrt -Werror

libFastLogger.a: $(RUNTIME_OBJS) $(RUNTIME_DEPS)
	ar -cq libFastLogger.a $(RUNTIME_OBJS)
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) $(RUNTIME_DIR)/LogDecompressor.cc -o decompressor -I$(RUNTIME_DIR) -Igenerated -lpthread -lrt -Werror

decompressor: generated/BufferStuffer.h $(RUNTIME_OBJS) $(RUNTIME_DIR)/LogDecompressor.cc
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) $(RUNTIME_DIR)/LogDecompressor.cc -o decompressor -I$(RUNTIME_DIR) -Igenerated -lpthread -lrt -Werror

clean-all: clean
	@rm -f libFastLogger.a $(RUNTIME_OBJS) *.pyc decompressor
	@rm -rf generated
