########
## This file serves as an example for how users of the NanoLog system
## should layout their makefile rules.
##
## The first half ("Required Library Variables") should always be defined
## in the user GNUmakefile and the only requirements on the second half
## ("User Section") are:
##   1) User C++ files MUST be compiled into .o files with run-cxx
##			before being linked into the main executable/library.
##   2) The final executable links NanoLog.a and $(NANO_LOG_LIBRARY_LIBS)
##
## The high level idea is that the NanoLog System needs to preprocess the
## user sources to extract static information and compile that information
## into the NanoLog library before linking it with the user application.
## The way we go about this is to use run-cxx to preprocess the user
## source files, use the USER_OBJS variable to determine when all user
## sources have been compiled, and have the final executable depend on
## libNanoLog.a to force a final library compilation.
########

####
## Required Library Variables
####

# USER_OBJS specifies all user object files to be compiled; this is used by
# NanoLog to determine when all the user sources have been preprocessed
# and compiled.
USER_SRCS=main.cc
USER_OBJS=$(USER_SRCS:.cc=.o)

# Root of the NanoLog Repository
NANOLOG_DIR=..

# Indicates that we want to use the Preprocessor version of NanoLog
EXTRA_NANOLOG_FLAGS=-DPREPROCESSOR_NANOLOG

# Must be specified AFTER defining NANOLOG_DIR and USER_OBJ's
include $(NANOLOG_DIR)/NanoLogMakeFrag

####
# User Section
####
# -DNDEBUG and -O3 should always be passed for high performance
CXXFLAGS= -std=c++17 -DNDEBUG -O3 -g

all: sampleApplication

# [Required] run-cxx will compile the user C++ source file into an object file using
# the NanoLog system. For run-cxx, the first parameter is the output file name (*.o),
# the second parameter is the input file (*.cc), and the third parameter is for
# compiler options.
%.o: %.cc
	$(call run-cxx, $@, $<, $(CXXFLAGS))

# [Required] $(NANO_LOG_LIBRARY_LIBS) must be used
sampleApplication: $(USER_OBJS) libNanoLog.a
	$(CXX) $(CXXFLAGS) -o sampleApplication $(USER_OBJS) -L. -lNanoLog $(NANO_LOG_LIBRARY_LIBS)

clean:
	@rm -f *.o sampleApplication /tmp/logFile compressedLog decompressor
