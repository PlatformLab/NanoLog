########
## This make file is adapted from the benchmark GNUmakefile.
## It compiles a test suite of tricky syntax with the preprocessor
#######

####
## Library Options [Required]
####

# Specifies the path to the FastLogger's Runtime Library
RUNTIME_DIR=../runtime

# Specifies the path to the FastLogger's Preprocessor directory
PREPROC_DIR=../preprocessor

####
# User Section
####
USER_SRCS=main.cc SimpleTestObject.cc folder/main.cc

# [Required] Macro MUST used to identify user *.o files
USER_OBJS=$(USER_SRCS:.cc=.o)

# -O3 should always be passed for high performance
CXXFLAGS= -std=c++0x -O3
CXX=g++

all: run-test

# [Required] run-cxx will compile the user C++ source file into an object file using
# the FastLogger system. See documentation in the Library Compilation section.
%.o: %.cc
	$(call run-cxx, $@, $<, $(CXXFLAGS))

testApp: $(USER_OBJS) libFastLogger.a
	$(CXX) -o testApp $(CXXFLAGS) $(USER_OBJS) -L. -lFastLogger $(LIBRARY_LIBS)


run-test: testApp
	@rm -f /tmp/testLog
	./testApp > /dev/null
	@chmod 666 /tmp/testLog
	@# Run the decompressor and cut out the timestamps before the ':'
	./decompressor /tmp/testLog | cut -d':' -f2- > output.txt
	diff -w expectedOutput.txt output.txt

	@printf "\r\n\033[0;32mIntegration Tests completed without error!\033[0m\r\n"
	@rm -f /tmp/testLog

clean:
	@rm -f *.o testApp folder/*.o /tmp/testLog output.txt

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
	@mkdir -p generated
	python $(PREPROC_DIR)/parser.py --mapOutput="generated/$(2).map" $(2).i
	$(CXX) -I $(RUNTIME_DIR) -c -o $(1) $(2).ii $(3)
	@rm -f $(2).i $(2).ii generated/BufferStuffer.h
endef

RUNTIME_CXX_FLAGS= -std=c++0x -O3 -DNDEBUG -g
LIBRARY_LIBS=-lrt -pthread

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

generated/BufferStuffer.o: $(USER_OBJS)
	mkdir -p generated
	python $(PREPROC_DIR)/parser.py --combinedOutput="generated/BufferStuffer.cc" $(shell find generated -type f -name "*.map" -printf ' "%h/%f" ')
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) -c -o $@ generated/BufferStuffer.cc -I $(RUNTIME_DIR) -Igenerated

$(RUNTIME_DIR)/%.o: $(RUNTIME_DIR)/%.cc generated/BufferStuffer.o
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) -c -o $@ $< -I $(RUNTIME_DIR) -Igenerated -Werror $(LIBRARY_LIBS)

libFastLogger.a: $(RUNTIME_OBJS) $(RUNTIME_DEPS) $(RUNTIME_DIR)/Cycles.o decompressor
	ar -cr libFastLogger.a $(RUNTIME_OBJS) generated/BufferStuffer.o

decompressor: generated/BufferStuffer.o $(RUNTIME_OBJS) $(RUNTIME_DIR)/LogDecompressor.cc
	$(CXX) $(RUNTIME_CXX_FLAGS) $(CXXWARNS) $(RUNTIME_OBJS) generated/BufferStuffer.o $(RUNTIME_DIR)/LogDecompressor.cc -o decompressor -I$(RUNTIME_DIR) -Igenerated -Werror $(LIBRARY_LIBS)

clean-all: clean
	@rm -f libFastLogger.a $(RUNTIME_OBJS) *.pyc decompressor
	@rm -rf generated
