#include "FastLogger.h"


namespace PerfUtils {

// Define the static members of FastLogger here
LogCompressor* FastLogger::compressor = NULL;
__thread FastLogger::StagingBuffer* FastLogger::stagingBuffer = NULL;

std::vector<FastLogger::StagingBuffer*> FastLogger::threadBuffers;
std::mutex FastLogger::bufferMutex;

FastLogger::FastLogger() {
}

}