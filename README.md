# NanoLog
Nanolog is an extremely performant nanosecond scale logging system for C++ that exposes a simple printf-like API and achieves over 80 *million* logs/second at a median latency of just over *7 nanoseconds*.

How it achieves this insane performance is by extracting static log information at compile-time, only logging the dynamic components in runtime hotpath, and deferring formatting to an offline process. This basically shifts work out of the runtime and into the compilation and post-execution phases.

More information about the techniques used in this logging system can be found in the [NanoLog Paper published in the 2018 USENIX Annual Technical Conference](https://www.usenix.org/conference/atc18/presentation/yang-stephen).

## Performance

This section shows the performance of NanoLog with existing logging systems such as [spdlog v1.1.0](https://github.com/gabime/spdlog), [Log4j2 v2.8](https://logging.apache.org/log4j/2.x/), [Boost 1.55](http://www.boost.org), [glog v0.3.5](https://github.com/google/glog), and Windows Event Tracing with Windows Software Trace Preprocessor on Windows 10 [(WPP)](https://docs.microsoft.com/en-us/windows-hardware/drivers/devtest/wpp-software-tracing).

### Throughput

Maximum throughput measured with 1 million messages logged back to back with no delay and 1-16 logging threads (NanoLog logged 100 million messages to generate a log file of comparable size). ETW is "Event Tracing for Windows." The log messages used can be found in the [Log Message Map below](#Log-Messages-Map).
![N|Solid](https://raw.githubusercontent.com/wiki/PlatformLab/NanoLog/systemComparison.svg?sanitize=true)

### Runtime Latency
Measured in nanoseconds and each cell represents the 50th / 99.9th tail latencies. The log messages used can be found in the [Log Message Map below](#Log-Messages-Map).

| Message | NanoLog | spdlog | Log4j2 | glog | Boost | ETW |
|---------|:--------:|:--------:|:--------:|:--------:|:--------:|:--------:|
|staticString | 7/ 37| 214/ 2546| 174 / 3364 | 1198/ 5968| 1764/ 3772| 161/ 2967|
|stringConcat | 7/ 36| 279/ 905| 256 / 25087 | 1212/ 5881| 1829/ 5548| 191/ 3365|
|singleInteger | 7/ 32| 268/ 855| 180 / 9305 | 1242/ 5482| 1914/ 5759| 167/ 3007|
|twoIntegers | 8/ 62| 437/ 1416| 183 / 10896 | 1399/ 6100| 2333/ 7235| 177/ 3183|
|singleDouble | 8/ 43| 585/ 1562| 175 / 4351 | 1983/ 6957| 2610/ 7079| 165/ 3182|
|complexFormat | 8/ 40| 1776/ 5267| 202 / 18207 | 2569/ 8877| 3334/ 11038| 218/ 3426|

#### Log Messages Map

Log messages used in the benchmarks above. *Italics* indicate dynamic log arguments.

| Message ID | Log Message Used |
|--------------|:--------|
|staticString  | Starting backup replica garbage collector thread |
|singleInteger | Backup storage speeds (min): *181* MB/s read |
|twoIntegers   | buffer has consumed *1032024* bytes of extra storage, current allocation: *1016544* bytes |
|singleDouble  | Using tombstone ratio balancer with ratio = *0.4* |
|complexFormat | Initialized InfUdDriver buffers: *50000* receive buffers (*97* MB), *50* transmit buffers (*0* MB), took *26.2* ms |
|stringConcat  | Opened session with coordinator at *basic+udp:host=192.168.1.140,port=12246* |

# Using NanoLog

## Prerequisites
NanoLog depends on the following:
* C++17 Compiler: [GNU gcc 6.4.0](https://gcc.gnu.org) or greater
* [GNU Make 4.0](https://www.gnu.org/software/make/) or greater
* [Python 2.7.9](https://www.python.org) or greater
* POSIX AIO and Threads (usually installed with Linux)

## NanoLog Pipeline
The NanoLog system enables low latency logging by deduplicating static log metadata and outputting the dynamic log data in a binary format. This means that log files produced by NanoLog are in binary and must be passed through a separate decompression program to produce the full, human readable ASCII log. The decompression program is built as a part of the NanoLog library.

## Compiling NanoLog
There are two versions of NanoLog (Preprocessor version and C++17 version) and you must chose **one** to use with your application as they’re not interoperable. The biggest difference between the two is that the Preprocessor version requires one to integrate a Python script in their build chain while the C++17 version is closer to a regular library (simply build and link against it). The benefit of using the Preprocessor version is that it performs more work at compile-time, resulting in a slightly more optimized runtime.

If you don’t know which one to use, go with C++17 NanoLog as it’s easier to use.

### C++17 NanoLog
The C++17 version of NanoLog works like a traditional library, in that one uses the library by simply #include-ing the NanoLog header ([NanoLogCpp17.h](./runtime/NanoLogCpp17.h)), writing NANO_LOG(…) statements, and linking against the NanoLog Runtime library. A sample application and can be found in [sample](./sample).

To build the NanoLog Runtime library, simply change directory into the [runtime directory.h](./runtime/) and invoke ```make```. This will produce ```./libNanoLog.a``` to against link your application and a ```./decompressor``` application that can be used to re-inflate the binary logs.

The [NanoLog header](./runtime/NanoLogCpp17.h) is found in the [runtime directory](./runtime/) and be sure to link your application against the NanoLog library that you built above, pthreads, and POSIX AIO (-lrt). Again, a [sample GNUmakefile](./sample/GNUmakefile) can be found in the sample directory.

After you compile and run the application, the generated log file can then be passed to the decompressor application to generate the full human-readable log file (instructions below).

### Preprocessor NanoLog
The Preprocessor version of NanoLog requires a tighter integration with the user build chain and is only for advanced/extreme users.

It *requires* the user to use a GNUmakefile that includes the (NanoLogMakeFrag)[./NanoLogMakeFrag), declares the USR_SRCS and USR_OBJS variables to list all app’s source and object files respectively, and uses the pre-defined ```run-cxx``` macro to compile the user .cc files into .o files.

This version of NanoLog will then run a Python script over all the sources (via the ```run-cxx``` invocation) and generate code for the NanoLog library that is *specific* to that compilation of the user application. In other words, the NanoLog library for the Preprocessor version of NanoLog is __non-portable, even between compilations of the same application__.

The easiest way to use this version of NanoLog is to copy the [sample application](./sample_preprocessor) and follow the instructions in the [sample GNUmakefile](./sample_preprocessor/GNUmakefile).

In short:
1. Copy the ```## Required Library Variables``` section in the [preprocessor sample GNUmakefile](./sample_preprocessor/GNUmakefile) and set the ```USER_SRCS```, ```USER_OBJS```, and ```NANO_LOG_DIR``` variables to be the a list of the user sources, a list of the compiled user objects, and the root directory for NanoLog.
2. Have ```EXTRA_NANOLOG_FLAGS=-DPREPROCESSOR_NANOLOG``` verbatim in the GNUmakefile and include the [NanoLogMakeFrag](./NanoLogMakeFrag) in the GNUmakefile
3. Compile all the user sources with the predefined ```run-cxx``` function in the [NanoLogMakeFrag](./NanoLogMakeFrag)

When you compile the application, a ./decompressor executable should be generated in your application directory as well. Use this to reconstitute the full human-readable log file (instructions below).

## NanoLog API
To use the NanoLog system in the code, one just has to include the NanoLog header (either [NanoLogCpp17.h](./runtime/NanoLogCpp17.h) for C++17 NanoLog or [NanoLog.h](./runtime/NanoLog.h) for Preprocessor NanoLog) and invoke the ```NANO_LOG()``` function in a similar fashion to printf, with the exception of a log level before it. Example below:

```cpp
#include "NanoLogCpp17.h"`
using namespace NanoLog::LogLevels;

int main() {
  NANO_LOG(NOTICE, "Hello World! This is an integer %d and a double %lf\r\n", 1, 2.0);
  return 0;
}
```

Valid log levels are DEBUG, NOTICE, WARNING, and ERROR and the logging level can be set via ```NanoLog::setLogLevel(...)```

The rest of the NanoLog API is documented in the [NanoLog.h](./runtime/NanoLog.h) header file.

## Post-Execution Log Decompressor
A ./decompressor executable should have been generated when the NanoLog library is first compiled. If you're using Preprocessor NanoLog, the ./decompressor should be appear in the user application diectory, otherwise it's found in the [runtime directory](./runtime/).

To get a human-readable log file, simply pass the log file (default location: ./compressedLog or /tmp/logFile) into the executable:
```
./decompressor decompress ./compressedLog
```
