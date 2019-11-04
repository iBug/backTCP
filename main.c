#include "btcp.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <libgen.h>

static struct _GlobalOptions {
    int action;
    const char *addr;
    unsigned short port;
    const char *filename;
    int logLevel;
} GlobalOptions = {};

static const char *const cliArgs = "+l:";
static const struct option cliLongArgs[] = {
    {"log-level", required_argument, NULL, 'l'},
    {NULL, 0, NULL, 0}  // Terminator
};

int btsend(int argc, char **argv) {
    Log(LOG_DEBUG, "Sending via backTCP");
    FILE *fp = fopen(argv[0], "rb");
    void *buf = malloc(65536);
    BTcpConnection *conn = BTOpen(inet_addr("127.0.0.1"), 6666);
    fread(buf, 65536, 1, fp);
    BTSend(conn, buf, 65536);
    BTClose(conn);
    free(buf);
    fclose(fp);
    return 0;
}

int btrecv(int argc, char **argv) {
    Log(LOG_DEBUG, "Receiving via backTCP");
    FILE *fp = fopen(argv[0], "wb");
    void *buf = malloc(65536);
    BTcpConnection *conn = BTOpen(inet_addr("0.0.0.0"), 6666);
    BTRecv(conn, buf, 65536);
    BTClose(conn);
    fwrite(buf, 65536, 1, fp);
    free(buf);
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    // Default settings
    GlobalOptions.logLevel = LOG_WARNING;

    // Now parse arguments
    int c;
    while ((c = getopt_long(argc, argv, cliArgs, cliLongArgs, NULL)) != -1)
        switch (c) {
            case 'l':
                if (toupper(optarg[0]) == 'D')
                    GlobalOptions.logLevel = LOG_DEBUG;
                else if (toupper(optarg[0]) == 'I')
                    GlobalOptions.logLevel = LOG_INFO;
                else if (toupper(optarg[0]) == 'W')
                    GlobalOptions.logLevel = LOG_WARNING;
                else if (toupper(optarg[0]) == 'E')
                    GlobalOptions.logLevel = LOG_ERROR;
                else if (toupper(optarg[0]) == 'F' || toupper(optarg[0]) == 'C')
                    GlobalOptions.logLevel = LOG_FATAL;
                else {
                    fprintf(stderr, "Unknown log level, valid values are\n\tdebug, info, warning, error, fatal\n");
                    return 1;
                }
                break;
            case '?':
                if (optopt == 'l')
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr,
                            "Unknown option character `\\x%x'.\n",
                            optopt);
                return 1;
            default:
                abort();
        }

    // Validate arguments
    if (argc - optind < 1) {
        Log(LOG_FATAL, "Missing filename");
        return 1;
    }

    // Apply settings from arguments
    SetLogLevel(GlobalOptions.logLevel);

    char *progname = basename(argv[0]);
    if (strcmp(progname, "btsend") == 0)
        return btsend(argc - optind, argv + optind);
    if (strcmp(progname, "btrecv") == 0)
        return btrecv(argc - optind, argv + optind);
    Logf(LOG_ERROR, "Unknown command `%s`", progname);
    return 1;
}
