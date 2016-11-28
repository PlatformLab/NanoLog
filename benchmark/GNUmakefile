########
## This file serves as an example for how users of the FastLogger system
## should layout their maikefile rules. The "Library Compilation" section
## shall be copied verbatim while the "User Section" can be customized
## with the exception that compiling C++ source files into object
## files shall be performed with the macro run-cxx and the final executable
## is links FastLogger.a, pthread, and lrt.
##
## Note that run-cxx will add additional warnings to the user compilation
## flags to detect misuses of the FastLogger API.
#######

####
# User Section
####
USER_SRCS=Benchmark.cc
USER_OBJS=$(USER_SRCS:.cc=.o)

# -O3 should always be passed for high performance
CXXFLAGS= -std=c++0x -O3
CXX=g++

all: benchmark

# run-cxx will compile the user C++ source file into an object file using
# the FastLogger system. See documentation in the Library Compilation section.
%.o: %.cc
	$(call run-cxx, $@, $<, $(CXXFLAGS))

benchmark: $(USER_OBJS) libFastLogger.a
	$(CXX) $(CXXFLAGS) -o benchmark $(USER_OBJS) -L. -lFastLogger -pthread -lrt

clean:
	@rm -f *.o benchmark

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
	python $(PREPROC_DIR)/parser.py --map="mapping.map" $(2).i
	$(CXX) -I $(RUNTIME_DIR) -c -o $(1) $(2).ii $(3)
	rm $(2).i $(2).ii
endef

RUNTIME_CXX_FLAGS= -std=c++0x -O3

RUNTIME_DEPS=$(wildcard $(RUNTIME_DIR)/*.h)
RUNTIME_CC=$(RUNTIME_DIR)/Cycles.cc $(RUNTIME_DIR)/FastLogger.cc \
		$(RUNTIME_DIR)/LogCompressor.cc $(RUNTIME_DIR)/Util.cc
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

generated/BufferStuffer.h: $(OBJS)
	mkdir -p generated
	python $(PREPROC_DIR)/parser.py --map="mapping.map" --"output=$@"

$(RUNTIME_DIR)/%.o: $(RUNTIME_DIR)/%.cc generated/BufferStuffer.h
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) -c -o $@ $< -I $(RUNTIME_DIR) -Igenerated -pthread -lrt -Werror

libFastLogger.a: $(RUNTIME_OBJS) $(RUNTIME_DEPS)
	ar -cvq libFastLogger.a $(RUNTIME_OBJS)

clean-all: clean
	@rm -rf libFastLogger.a $(RUNTIME_OBJS) mapping.map generated runtime