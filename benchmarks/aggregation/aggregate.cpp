#include <fstream>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, const char** argv) {

    if (argc < 3) {
        printf("Searches within a NanoLog decompressed file for a leading\r\n"
                "string and then aggregates the element after the string\r\n"
                "for a min/max/mean as an int. An example would be\r\n"
                "aggregating on \"Hello World # 10\", the invocation is:\r\n"
                "   %s \"Hello World # \" <logFile>\r\n\r\n", argv[0]);
        return 1;
    }

    const char *searchString = argv[1];
    const char *filename = argv[2];

    std::ifstream in(filename, std::ifstream::in);
    if (!in.is_open() || !in.good()) {
        printf("Unable to open file: %s\r\n", filename);
        exit(-1);
    }

    long min, max, total, count;
    min = max = total = count = 0;

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

        char *pos = strstr(line, searchString);

        // Didn't match
        if (pos == nullptr)
            continue;

        // Move the string to the right place
        pos += strlen(searchString);
        long num = atol(pos);

        if (count) {
            if (min > num) min = num;
            if (max < num) max = num;
            total += num;
            ++count;
        } else {
            total = min = max = num;
            count = 1;
        }
    }


    printf("File Searched: %s\r\n", filename);
    printf("Search String: %s\r\n", searchString);
    printf("Min  : %ld\r\n", min);
    printf("Max  : %ld\r\n", max);
    printf("Mean : %ld\r\n", total/count);
    printf("Total: %ld\r\n", total);
    printf("Count: %ld\r\n", count);
}