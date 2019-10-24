#include "btcp.h"
#include "logging.h"

#include <stdlib.h>
#include <sys/socket.h>

int BTSendPacket(BTcpConnection* conn, const void *data, size_t len, uint8_t seq, uint8_t flag) {
    char buf[1024];
    int buflen;
    int socket = conn->socket;
    struct sockaddr *addr = conn->addr;

    // Tell the remote to close
    if (len == 0)
        return sendto(socket, buf, 0, 0, (struct sockaddr*)addr, sizeof(struct sockaddr));

    if (len > 64)
        len = 64;
    buflen = sizeof(BTcpHeader) + len;

    BTcpHeader hdr = {
        .btcp_seq = seq,
        .data_off = sizeof(BTcpHeader),
        .flag = flag
    };

    memcpy(buf + sizeof(BTcpHeader), data, len);
    return sendto(socket, data, len, 0, (struct sockaddr*)addr, sizeof(struct sockaddr));
}

int BTAcknowledgePacket(BTcpConnection* conn, uint8_t seq) {
    int socket = conn->socket;
    struct sockaddr *addr = conn->addr;
    BTcpHeader hdr = {
        .btcp_ack = seq,
        .data_off = sizeof(BTcpHeader),
        .flag = F_ACK
    };
    return sendto(socket, data, len, 0, (struct sockaddr*)addr, sizeof(struct sockaddr));
}

int BTSend(BTcpConnection* conn, const void *data, size_t len) {
    uint8_t *buf = malloc(conn->config.max_packet_size);
    if (buf == NULL) {
        Log(LOG_ERROR, "Buffer allocation failed");
        return -1;
    }

    int socket = conn->socket;
    const struct sockaddr *addr = &conn->addr;

    uint8_t next_seq = conn->state.packet_sent,
            last_acked = next_seq,
            win_size = 1,  // Assume 1-packet window at the beginning
            packet_sent;
    size_t sent_len = 0;  // Nothing sent so far

    while (sent_len < len) {
        packet_sent = 0;
        while (packet_sent < win_size) {
            int packet_size = conn->config.max_packet_size;
            if (len - sent_len < conn->config.max_packet_size)
                packet_size = sizeof(BTcpHeader) + (len - sent_len);
            BTcpHeader hdr = {
                .btcp_seq = next_seq,
                .data_off = sizeof(BTcpHeader)
            };
            memcpy(buf, &hdr, sizeof hdr);
            memcpy(buf + sizeof hdr, data + sent_len, packet_size - sizeof(BTcpHeader));
            sendto(socket, buf, packet_size, 0, addr, sizeof(*addr));
            packet_sent++;
        }
    }
    free(buf);
    return sent_len;
}

int BTRecv(BTcpConnection* conn, const void *data, size_t len) {
    int socket = conn->socket;
    struct sockaddr *addr = &conn->addr;
    int addrlen;
    recvfrom(socket, data, len, addr, &addrlen);
}

BTcpConnection* BTOpen(unsigned long addr, unsigned short port) {
    BTcpConnection* conn = malloc(sizeof(BTcpConnection));
    conn->socket = socket(AF_INET, SOCK_DGRAM, 0);
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_addr.s_addr = addr;
    conn->addr.sin_port = htons(port);
    return conn;
}

void BTClose(BTcpConnection* conn) {
    BTSendPacket(conn, NULL, 0);
    close(conn->socket);
    free(conn);
}

void BTDefaultConfig(BTcpConnection* conn) {
    conn->config.timeout.tv_usec = 10000;  // default to 10 ms
}
