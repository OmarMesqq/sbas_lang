#include <stdio.h>
#include <string.h>
#include <math.h>

#include "sbas.h"

static int stringToInt(char* str);

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 5) {
        fprintf(stderr, "usage: ./sbas <file.sbas> <param1> <param2> <param3>\n");
        return -1;
    }

    const char* filename;
    FILE* fp;
    funcp sbasFunction;
    int p1, p2, p3;
    int res;

    filename = argv[1];
    fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "failed to open sbas file: %s\n", filename);
        return -1;
    }

    sbasFunction = sbasCompile(fp);
    if (!sbasFunction) {
        fprintf(stderr, "failed to compile sbas file: %s\n", filename);
        fclose(fp);
        return -1;
    }
    
    switch (argc) {
        case 2:
            res = sbasFunction();
            break;
        case 3:
            p1 = stringToInt(argv[2]);
            res = sbasFunction(p1);
            break;
        case 4:
            p1 = stringToInt(argv[2]);
            p2 = stringToInt(argv[3]);
            res = sbasFunction(p1, p2);
            break;
        case 5:
            p1 = stringToInt(argv[2]);
            p2 = stringToInt(argv[3]);
            p3 = stringToInt(argv[4]);
            res = sbasFunction(p1, p2, p3);
            break;
    }

    
    printf("SBas function at %s returned %d\n", filename, res);

    sbasCleanup(sbasFunction);
    fclose(fp);
    return 0;
}

/**
 * Converts null-terminated string to an integer
 * @param str 
 */
static int stringToInt(char* str) {
    int num;
    size_t ssize;
    int digit;

    num = 0;
    ssize = strlen(str);

    while(*str != '\0') {
        digit = *str - 48;  // 48 is ASCII code for '0', so we always get the corresponding int to a char
        digit = digit * pow(10, --ssize);
        num += digit;
        str++;
    }

    return num;
}