# NanoLog
![](https://github.com/PlatformLab/NanoLog/workflows/Build%20Tests/badge.svg)

Nanolog is an extremely performant nanosecond scale logging system for C++ that exposes a simple printf-like API and achieves over 80 *million* logs/second at a median latency of just over *7 nanoseconds*.

How it achieves this insane performance is by extracting static log information at compile-time, only logging the dynamic components in runtime hotpath, and deferring formatting to an offline process. This basically shifts work out of the runtime and into the compilation and post-execution phases.

For more information about the techniques used in this logging system, please refer to either the [NanoLog Paper](https://www.usenix.org/conference/atc18/presentation/yang-stephen) published in the 2018 USENIX Annual Technical Conference or the original author's [doctoral thesis](https://web.stanford.edu/~ouster/cgi-bin/papers/YangPhD.pdf).

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
Currently NanoLog only works for Linux-based systems and depends on the following:
* C++17 Compiler: [GNU g++ 7.5.0](https://gcc.gnu.org) or newer
* [GNU Make 4.0](https://www.gnu.org/software/make/) or greater
* [Python 3.4.2](https://www.python.org) or greater
* POSIX AIO and Threads (usually installed with Linux)

## NanoLog Pipeline
The NanoLog system enables low latency logging by deduplicating static log metadata and outputting the dynamic log data in a binary format. This means that log files produced by NanoLog are in binary and must be passed through a separate decompression program to produce the full, human readable ASCII log.

## Compiling NanoLog
There are two versions of NanoLog (Preprocessor version and C++17 version) and you must chose **one** to use with your application as they’re not interoperable. The biggest difference between the two is that the Preprocessor version requires one to integrate a Python script in their build chain while the C++17 version is closer to a regular library (simply build and link against it). The benefit of using the Preprocessor version is that it performs more work at compile-time, resulting in a slightly more optimized runtime.

If you don’t know which one to use, go with C++17 NanoLog as it’s easier to use.

### C++17 NanoLog
The C++17 version of NanoLog works like a traditional library; just [``#include "NanoLogCpp17.h"``](./runtime/NanoLogCpp17.h) and link against the NanoLog library. A sample application can be found in the [sample directory](./sample).

To build the C++17 NanoLog Runtime library, go in the [runtime directory](./runtime/) and invoke ```make```. This will produce ```./libNanoLog.a``` to against link your application and a ```./decompressor``` application that can be used to re-inflate the binary logs.

When you compile your application, be sure to include the NanoLog header directory ([``-I ./runtime``](./runtime/)), link against NanoLog, pthreads, and POSIX AIO (``-L ./runtime/ -lNanoLog -lrt -pthread``), and enable format checking in the compiler (e.g. passing in ``-Werror=format`` as a compilation flag). The latter step is incredibly important as format errors may silently corrupt the log file at runtime. Sample g++ invocations can be found in the [sample GNUmakefile](./sample/GNUmakefile).

After you compile and run the application, the log file generated can then be passed to the ```./decompressor``` application to generate the full human-readable log file (instructions below).

### Preprocessor NanoLog
The Preprocessor version of NanoLog requires a tighter integration with the user build chain and is only for advanced/extreme users.

It *requires* the user's GNUmakefile to include the [NanoLogMakeFrag](./NanoLogMakeFrag), declare USR_SRCS and USR_OBJS variables to list all app’s source and object files respectively, and use the pre-defined ```run-cxx``` macro to compile *ALL* the user .cc files into .o files instead of ``g++``. See the [preprocessor sample GNUmakefile](./sample_preprocessor/GNUmakefile) for more details.

Internally, the ```run-cxx``` invocation will run a Python script over the source files and generate library code that is *specific* to each compilation of the user application. In other words, the compilation builds a version of the NanoLog library that is __non-portable, even between compilations of the same application__ and each ```make``` invocation rebuilds this library.

Additionally, the compilation should also generate a ```./decompressor``` executable in the app directory and this can be used to reconstitute the full human-readable log file (instructions below).

## Sample Applications
The sample applications are intended as a guide for how users are to interface with the NanoLog library. Users can modify these applications to test NanoLog's various API and functionality. The C++17 and Preprocessor versions of these applications reside in [./sample](./sample) and [./sample_preprocessor](./sample_preprocessor) respectively. One can modify ```main.cc``` in each directory, build/run the application, and execute the decompressor to examine the results.

Below is an example for C++17 NanoLog's [sample application](./sample).
```bash
cd sample

# Modify the application
nano main.cc

make clean-all
make
./sampleApplication
./decompressor decompress /tmp/logFile
```
Note: The sample application sets the log file to ```/tmp/logFile```.

## NanoLog API
To use the NanoLog system in the code, one just has to include the NanoLog header (either [NanoLogCpp17.h](./runtime/NanoLogCpp17.h) for C++17 NanoLog or [NanoLog.h](./runtime/NanoLog.h) for Preprocessor NanoLog) and invoke the ```NANO_LOG()``` function in a similar fashion to printf, with the exception of a log level before it. Example below:

```cpp
#include "NanoLogCpp17.h"
using namespace NanoLog::LogLevels;

int main() 
{
  NANO_LOG(NOTICE, "Hello World! This is an integer %d and a double %lf\r\n", 1, 2.0);
  return 0;
}
```

Valid log levels are DEBUG, NOTICE, WARNING, and ERROR and the logging level can be set via ```NanoLog::setLogLevel(...)```

The rest of the NanoLog API is documented in the [NanoLog.h](./runtime/NanoLog.h) header file.

## Post-Execution Log Decompressor
The execution of the user application should generate a compressed, binary log file (default locations: ./compressedLog or /tmp/logFile). To make the log file human-readable, simply invoke the ```decompressor``` application with the log file.

```
./decompressor decompress ./compressedLog
```

After building the NanoLog library, the decompressor executable can be found in either the [./runtime directory](./runtime/) (for C++17 NanoLog) or the user app directory (for Preprocessor NanoLog).

## Unit Tests
The NanoLog project contains a plethora of tests to ensure correctness. Below is a description of each and how to access/build/execute them.


#### Integration Tests
The integration tests build and test the Nanolog system end-to-end. For both C++17 NanoLog and Preprocessor NanoLog, it compiles a client application with the NanoLog library, executes the application, and runs the resulting log file through the decompressor. It additionally compares the output of the decompressor to ensure that the log contents match the expected result.

One can execute these tests with the following commands:
```bash
cd integrationTest
./run.sh
```

#### Preprocessor and Library Unit Tests
The NanoLog Library and Preprocessor engine also contain a suit of their own unit tests. These will test the inner-workings of each component by invoking individual functions and checking their returns match the expected results.

To run the NanoLog preprocessor unit tests, execute the following commands:
```bash
cd preprocessor
python UnitTests.py
```

To build and run the NanoLog library unit tests, execute the following commands:
```bash
git submodule update --init

cd runtime
make clean
make test
./test --gtest_filter=-*assert*
```
Note: The gtest filter is used to removed tests with assert death statements in them.
