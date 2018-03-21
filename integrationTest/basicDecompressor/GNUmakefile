########
## This make file is adapted from the benchmark GNUmakefile.
## It makes a barebones decompressor application
#######

####
## Required Library Variables
####

# USER_OBJS specifies all user object files to be compiled; this is used by
# NanoLog to determine when all the user sources have been preprocessed
# and compiled.
USER_SRCS=main.cc
USER_OBJS=$(USER_SRCS:.cc=.o)

# Root of the NanoLog Repository
NANOLOG_DIR=../..

# Must be specified AFTER defining NANOLOG_DIR and USER_OBJ's
include $(NANOLOG_DIR)/NanoLogMakeFrag

####
# User Section
####
# -O3 should always be passed for high performance
CXXFLAGS= -std=c++0x -O3
CXX=g++

all: decompressor

# [Required] run-cxx will compile the user C++ source file into an object file using
# the NanoLog system. See documentation in the Library Compilation section.
%.o: %.cc
	$(call run-cxx, $@, $<, $(CXXFLAGS))

clean:
	@rm -f *.o decompressor
