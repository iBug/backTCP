#include "btcp.h"
#include "logging.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>

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
    const size_t bufsize = conn->config.max_packet_size;
    uint8_t *buf = malloc(bufsize);
    if (buf == NULL) {
        Log(LOG_ERROR, "Buffer allocation failed");
        return -1;
    }

    int socket = conn->socket;
    const struct sockaddr *addr = &conn->addr;

    uint8_t last_acked = conn->state.packet_sent,
            next_seq = last_acked,
            win_size = 1,  // Assume 1-packet window at the beginning
            packet_sent,
            is_retransmission = 0;
    size_t sent_len = 0,  // Nothing sent so far
           offset = 0;

    while (sent_len < len) {
        packet_sent = 0;
        next_seq = last_acked;
        while (packet_sent < win_size) {
            int payload_size = bufsize - sizeof(BTcpHeader);
            offset = sent_len + packet_sent * payload_size;
            if (len - offset < payload_size)
                payload_size = len - offset;
            BTcpHeader hdr = {
                .btcp_seq = next_seq,
                .data_off = sizeof(BTcpHeader),
                .flags = 0
            };
            if (is_retransmission)
                hdr.flags |= F_RETRANSMISSION;

            memcpy(buf, &hdr, sizeof hdr);
            memcpy(buf + sizeof hdr, data, payload_size);
            send(socket, data, len, 0);
            next_seq++;
        }
        is_retransmission = 0;

        // Poll for response for 10 ms (configurable)
        struct pollfd pfd = {conn->socket, POLLIN, 0};
        ssize_t presult = poll(&pfd, 1, conn->config.timeout);
        if (presult == 1 && pfd.revents == POLLIN) {  // Something's ready
            recv(socket, buf, bufsize, 0);
            BTcpHeader hdr;
            memcpy(&hdr, buf, sizeof hdr);
            last_acked = hdr.btcp_ack;
            win_size = hdr.win_size;
            if (last_acked != next_seq) {
                // Some packets are lost, retransmit them
                is_retransmission = 1;
                continue;
            }
        } else if (presult == 0) {
            // Timeout
            is_retransmission = 1;
            continue;
        } else {
            // uh what?
            Logf(LOG_ERROR, "Unknown");
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
    conn->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_addr.s_addr = addr;
    conn->addr.sin_port = htons(port);
    connect(conn->socket, (struct sockaddr*)&conn->addr, sizeof(struct sockaddr));
    return conn;
}

void BTClose(BTcpConnection* conn) {
    BTSendPacket(conn, NULL, 0);
    close(conn->socket);
    free(conn);
}

void BTDefaultConfig(BTcpConnection* conn) {
    conn->config.max_packet_size = sizeof(BTcpHeader) + 64;
    conn->config.timeout.tv_nsec = 10;  // default to 10 ms
}
