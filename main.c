#include <stdio.h>

#include "peqcomp.h"

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: ./sbas <file.sbas>\n");
        return -1;
    }
    
    const char* filename = argv[1];
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "failed to open sbas file: %s\n", filename);
        return -1;
    }

    fclose(fp);
    return 0;
}