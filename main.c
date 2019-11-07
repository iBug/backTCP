#include "btcp.h"
#include "logging.h"
#include "help.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <arpa/inet.h>
#include <libgen.h>

#define ACTION_MAIN 0
#define ACTION_HELP 1
#define ACTION_VERSION 2

static struct _GlobalOptions {
    int action;
    in_addr_t addr;
    unsigned short port;
    const char *filename;
    int logLevel;
} GlobalOptions = {
    .action = ACTION_MAIN,
    .addr = 0,
    .port = 6666,
    .logLevel = LOG_WARNING
};

static const char *const cliArgs = "A:a:hl:p:Vv";
static const struct option cliLongArgs[] = {
    {"address", required_argument, NULL, 'a'},
    {"port", required_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},
    {"log-level", required_argument, NULL, 'l'},
    {"version", no_argument, NULL, 'v'},
    {NULL, 0, NULL, 0}  // Terminator
};

int btsend(int argc, char **argv) {
    Log(LOG_DEBUG, "Sending via backTCP");
    FILE *fp = fopen(argv[0], "rb");
    fseek(fp, 0L, SEEK_END);
    size_t filesize = ftell(fp);
    fseek(fp, 0L, SEEK_SET);
    void *buf = malloc(filesize);
    BTcpConnection *conn = BTOpen(GlobalOptions.addr, GlobalOptions.port);
    fread(buf, filesize, 1, fp);
    fclose(fp);
    BTSend(conn, buf, filesize);
    BTClose(conn);
    free(buf);
    return 0;
}

int btrecv(int argc, char **argv) {
    Log(LOG_DEBUG, "Receiving via backTCP");
    FILE *fp = fopen(argv[0], "wb");
    const size_t bufsize = 1UL << 15;
    void *buf = malloc(bufsize);
    BTcpConnection *conn = BTOpen(GlobalOptions.addr, GlobalOptions.port);
    size_t recv_size;
    do {
        recv_size = BTRecv(conn, buf, bufsize);
        fwrite(buf, recv_size, 1, fp);
    } while (recv_size > 0);
    BTClose(conn);
    free(buf);
    fclose(fp);
    return 0;
}

int main(int argc, char **argv) {
    // Default values
    GlobalOptions.addr = inet_addr("127.0.0.1");

    // Parse arguments first
    int c;
    while ((c = getopt_long(argc, argv, cliArgs, cliLongArgs, NULL)) != -1)
        switch (c) {
            case 'a': case 'A':
                {
                    struct in_addr addr;
                    int result = inet_aton(optarg, &addr);
                    if (result == 0) {
                        Logf(LOG_ERROR, "Invalid address '%s'", optarg);
                        return 1;
                    }
                    GlobalOptions.addr = addr.s_addr;
                } break;
            case 'p':
                {
                    char *endptr;
                    long result = strtoll(optarg, &endptr, 10);
                    if (*endptr || result == 0) {
                        Logf(LOG_ERROR, "Invalid port number '%s'", optarg);
                        return 1;
                    } else if (result < 0 || result >= (1 << 16)) {
                        Logf(LOG_ERROR, "Port number '%d' out of range", result);
                        return 1;
                    }
                    GlobalOptions.port = result;
                } break;
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
                    Log(LOG_ERROR, "Unknown log level, valid values are\n\tdebug, info, warning, error, fatal\n");
                    return 1;
                }
                break;
            case 'h':
                GlobalOptions.action = ACTION_HELP;
                break;
            case 'v':
                GlobalOptions.action = ACTION_VERSION;
                break;
            case '?':
                if (optopt == 'l')
                    Logf(LOG_ERROR, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    Logf(LOG_ERROR, "Unknown option '-%c'.\n", optopt);
                else
                    Logf(LOG_ERROR, "Unknown option character `\\x%x'.\n", optopt);
                return 1;
            default:
                abort();
        }

    // Apply settings from arguments
    SetLogLevel(GlobalOptions.logLevel);

    if (GlobalOptions.action == ACTION_MAIN) {
        // Validate arguments
        if (argc - optind < 1) {
            Log(LOG_FATAL, "Missing filename");
            return 1;
        }

        char *progname = basename(argv[0]);
        if (strcmp(progname, "btsend") == 0)
            return btsend(argc - optind, argv + optind);
        if (strcmp(progname, "btrecv") == 0)
            return btrecv(argc - optind, argv + optind);
        Logf(LOG_ERROR, "Unknown command `%s`", progname);
    } else if (GlobalOptions.action == ACTION_HELP) {
        printf("%s", HELP);
        return 0;
    } else if (GlobalOptions.action == ACTION_VERSION) {
        printf("%s\n", VERSION);
        return 0;
    }
    return 1;
}
