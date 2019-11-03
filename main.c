#include "btcp.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

int btsend(int argc, char **argv) {
    return 0;
}

int btrecv(int argc, char **argv) {
    return 0;
}

int main(int argc, char **argv) {
    char *progname = basename(argv[0]);
    if (strcmp(progname, "btsend") == 0)
        return btsend(argc, argv);
    if (strcmp(progname, "btrecv") == 0)
        return btrecv(argc, argv);
    Logf(LOG_ERROR, "Unknown command `%s`", progname);
    return 1;
}
