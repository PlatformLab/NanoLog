
#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include <fstream>

/**
 * Describes a log message found in the user sources by the original format
 * string provided, the file where the log message occurred, and the line number
 */
struct LogMetadata {
  const char *fmtString;
  const char *fileName;
  uint32_t lineNumber;
};

// Map of numerical ids to log message metadata
extern struct LogMetadata logId2Metadata[];

// Map of numerical ids to compression functions
extern ssize_t (*compressFnArray[]) (BufferUtils::UncompressedLogEntry *re, char* out);

// Map of numerical ids to decompression functions
extern void (*decompressAndPrintFnArray[]) (std::ifstream &in, FILE *outputFd);

#endif /* BUFFER_STUFFER */
