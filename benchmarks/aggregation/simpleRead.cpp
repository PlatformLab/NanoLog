#include <fstream>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

/* Not technically required, but needed on some UNIX distributions */
#include <sys/types.h>
#include <sys/stat.h>

void printHelp(const char *exec) {
    printf("Opens a file and reads its contents in 1GB chunks\r\n\r\n");
    printf("Usage:\r\n\t%s <filename>\r\n", exec);
}

int main(int argc, const char** argv) {
    const char *exec = argv[0];
    if (argc < 2) {
        printHelp(exec);
        exit(0);
    }

    const char *filename = argv[1];
    if (strcmp(filename, "--help") == 0) {
        printHelp(exec);
        exit(0);
    }

    // int fp = open(filename, O_RDONLY);
    // if (fp < 0) {
    //     printf("Unable to open file %s\r\n", filename);
    //     exit(-1);
    // }

    // size_t bufferSize = 1<<20;
    // void *buffer = malloc(bufferSize);
    // if (!buffer) {
    //     printf("Unable to malloc an array of size %ld bytes to read file %s\r\n",
    //         bufferSize, filename);
    //     exit(-1);
    // }

    // size_t totalRead = 0;
    // size_t elementsRead = 0;
    // do {
    //    elementsRead = read(fp, buffer, bufferSize);
    //    totalRead += elementsRead;
    // } while (elementsRead > 0);

    // printf("Read %d bytes from %s\r\n", totalRead, filename);
    // free(buffer);

/*
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Unable to open file %s\r\n", filename);
        exit(-1);
    }

    size_t bufferSize = 1<<30;
    void *buffer = malloc(bufferSize);
    if (!buffer) {
        printf("Unable to malloc an array of size %ld bytes to read file %s\r\n",
            bufferSize, filename);
        exit(-1);
    }

    size_t totalRead = 0;
    size_t elementsRead = 0;
    do {
       elementsRead = fread(buffer, 1, bufferSize, fp);
       totalRead += elementsRead;
    } while (elementsRead > 0 && !feof(fp) && !ferror(fp));

    printf("Read %d bytes from %s\r\n", totalRead, filename);
    free(buffer);
*/
    std::ifstream in(filename, std::ifstream::in);
    if (!in.is_open() || !in.good()) {
        printf("Unable to open file: %s\r\n", filename);
        exit(-1);
    }

    char line[1024];
    while (!in.eof() && in.good()) {
        in.getline(line, sizeof(line));

        if (in.eof())
            break;

        if (in.fail()) {
            printf("Failed to read a line... Perhaps the buffer isn't large"
                    " enough at %lu bytes\r\n", sizeof(line));
            exit(1);
        }
    }
}