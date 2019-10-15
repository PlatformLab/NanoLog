# WARNING

The benchmarks in this directory are primarily used for creating figures for the NanoLog Publications (i.e. for ATC 2018 and thesis dissertations). As a result, they may be of bad form, output hard to understand metrics, and/or access internal NanoLog variables in an unsafe way. Thus, one *should not* use these benchmarks as examples of how to use NanoLog, and one should be weary of directly trusting the output without scrutinizing the code. You have been warned.

## Description
This directory houses some of the benchmarks of the NanoLog system ported over from the benchmark branch. The user is never supposed to build or run the executables directly, but instead rely on the ``run_*.sh`` scripts in the directory. Some of these benchmarks are used to produce the figures in the NanoLog Paper (ATC 2018) and dissertation.

## Run Scripts

### run_bench.sh
This script is at the center of all other ``run_*.sh`` scripts. It profiles the test machine, runs the core benchmark application, and stores the results in a subdirectory of ``results/``. The subdirectory is named based on execution time (YYYYMMDDHHMMSS) and the user can specify a suffix when invoking ``run_bench.sh``.

### run_aggregation.sh
Creates a NanoLog log file with two log statements in varying ratios and measures how fast Python, Awk, C++, and NanoLog can process them.

### run_decompressionCosts.sh
Creates a log file with 1 of 6 log statements and measures the time to decompress each log file variant.

### run_sortedDecompressionThreads.sh
Varies the number of runtime logging threads that produce log messages at runtime and measures the time to decompress the log file at post-execution.