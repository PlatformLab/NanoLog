
#ifndef BUFFER_STUFFER
#define BUFFER_STUFFER

#include <fstream>


// Map of numerical ids to compression functions
extern ssize_t (*compressFnArray[]) (BufferUtils::UncompressedLogEntry *re, char* out);

// Map of numerical ids to decompression functions
extern void (*decompressAndPrintFnArray[]) (std::ifstream &in);

#endif /* BUFFER_STUFFER */
