#include "btcp.h"
#include "logging.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <libgen.h>

int btsend(int argc, char **argv) {
    Log(LOG_DEBUG, "Sending via backTCP");
    FILE *fp = fopen(argv[1], "rb");
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
    FILE *fp = fopen(argv[1], "wb");
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
    SetLogLevel(0);
    char *progname = basename(argv[0]);
    if (strcmp(progname, "btsend") == 0)
        return btsend(argc, argv);
    if (strcmp(progname, "btrecv") == 0)
        return btrecv(argc, argv);
    Logf(LOG_ERROR, "Unknown command `%s`", progname);
    return 1;
}
