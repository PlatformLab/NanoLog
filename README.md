# NanoLog
Nanolog is an extremely performant nanosecond scale logging system for C++ that exposes a simple printf-like API and achieves over 60 *million* logs/second at a median latency of just over *12.5 nanoseconds*.

How it achieves this insane performance is by extracting static log information at runtime, only logging the dynamic components at runtime, and deferring formatting to an offline process. This basically shifts work out of the runtime and into the compilation and post-execution phases.

## Usage
As alluded to by the introduction text, in addition to using the NanoLog API, one has to integrate with NanoLog at the compilation and post-execution phases.

### Sample NanoLog Code
To use the NanoLog system in the code, one just has to ```#include "NanoLog.h"``` and invoke the ```NANO_LOG()``` function in the same fashion as a printf. Example below:

```cpp
#include "NanoLog.h"

int main() {
  NANO_LOG("Hello World! This is an integer %d and a double %lf\r\n", 1, 2.0);
  return 0;
}
```

### Compile-time Requirements
NanoLog requires users to compile their C++ files into *.o files using the NanoLog system.

#### New Projects
For new projects, the easiest way to bootstrap this process is to copy the [sample GNUMakefile](./benchmark/GNUmakefile) and make the following changes:

* Change the ```RUNTIME_DIR``` and ```PREPROC_DIR``` variables to refer to the [runtime](./runtime) and [preprocessor](./preprocessor) directories respectively

* Change the ```USER_SRC``` variable to refer to all your sources.

#### Advanced Configuration
If you wish to integrate into NanoLog into an existing system with an existing GNUmakefile, perform the following:
* Copy every line after the "Library Compilation (copy verbatim)" section in the [sample GNUMakefile](./benchmark/GNUmakefile) into your makefile.
* Make ```RUNTIME_DIR``` and ```PREPROC_DIR``` variables that to refer to the [runtime](./runtime) and [preprocessor](./preprocessor) directories respectively.
* Compile your sources into *.o files using the ```run-cxx``` function and ensure that the ```USER_OBJS``` captures all the *.o files.
* Link the following the NanoLog library, posix aio and pthread libraries i.e. ```-lNanoLog -lrt -pthread```.


### Post-Execution Log Decompressor
After compilation, a ./decompressor executable should have been generated in your makefile directory and after execution, you should have a binary log (default location: /tmp/compressedLog or /tmp/logFile).

To get a human readable log out, pass one into the other. Example:
```
./decompressor /tmp/logFile
```
