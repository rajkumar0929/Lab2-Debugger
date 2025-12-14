#include <stdio.h>
#include <stdlib.h>
#include "debugger.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <program>\n", argv[0]);
        return 1;
    }

    debugger_start(argv[1]);
    return 0;
}
