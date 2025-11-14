#include <stdio.h>

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
 * Converts the null-terminated string `str` to an integer.
 * Runs in O(n) - single pass.
 * Doesn't correctly handle broken inputs such as those with space, letters
 * and overflowing integers
 */
static int stringToInt(char* str) {
    int num = 0;
    int digit = 0;
    int isNegative = 0;

    // is first char a minus sign?
    if (*str == '-') {
        isNegative = 1;
        str++;
    }

    while(*str != '\0') {
        // subtract the value of the digit char from 0 in ASCII table, getting its true value
        digit = *str - '0';
        // each new digit "adds" another power of 10 to the overall number
        num *= 10;
        num += digit; 
        str++;
    }

    if (isNegative) {
        num *= -1;
    }
    
    return num;
}