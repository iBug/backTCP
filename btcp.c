#include "btcp.h"
#include "logging.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <poll.h>

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
            send_win_size = 1,  // # of pending packets
            recv_win_size = 1,  // # of available slots
            win_size = 1,  // Assume 1-packet window at the beginning
            packet_sent,
            is_retransmission = 0;
    size_t sent_len = 0,
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
                .win_size = win_size - packet_sent - 1, // # of remaining packets
                .flags = 0,
                .data_len = payload_size
            };
            if (is_retransmission)
                hdr.flags |= F_RETRANSMISSION;

            memcpy(buf, &hdr, sizeof hdr);
            memcpy(buf + sizeof hdr, data, payload_size);
            send(socket, data, len, 0);
            next_seq++;
            packet_sent++;
        }
        is_retransmission = 0;

        // Poll for response for 10 ms (configurable)
        struct pollfd pfd = {socket, POLLIN, 0};
        ssize_t presult = poll(&pfd, 1, conn->config.timeout);
        if (presult == 1 && pfd.revents == POLLIN) {  // Something's ready
            recv(socket, buf, bufsize, 0);
            BTcpHeader hdr;
            memcpy(&hdr, buf, sizeof hdr);
            last_acked = hdr.btcp_ack;
            recv_win_size = hdr.win_size;
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
            Logf(LOG_ERROR, "Unknown error");
        }
    }

    free(buf);
    return sent_len;
}

int BTRecv(BTcpConnection* conn, void *data, size_t len) {
    const size_t bufsize = conn->recv_buffer_size;
    uint8_t *buf = malloc((1 + bufsize) * conn->config.max_packet_size);
    if (buf == NULL) {
        Log(LOG_ERROR, "Failed to allocate memory");
        return 0;
    }
    uint8_t *packet_buf = buf + bufsize * conn->config.max_packet_size;
    int socket = conn->socket;
    struct sockaddr *addr = &conn->addr;
    int addrlen;

    uint8_t last_acked = conn->state.packet_sent, // hmm
            win_start = last_acked,               // hmmm
            win_size = bufsize,                   // hmmmm?
            *packet_flags = malloc(bufsize);
    size_t recv_len = 0,
           payload_size,
           offset = 0;
    ssize_t received;
    BTcpHeader hdr, response;

    for (int i = 0; i < bufsize; i++)
        packet_flags[i] = 0;

    // Fetch the first packet
    // Sender address will be given
    recv_len = conn->config.max_packet_size;
    received = recvfrom(socket, buf, recv_len, 0, addr, &addrlen);
    if (received < 0) {
        Log(LOG_ERROR, "Failed to receive initial packet");
        goto cleanup;
    }
    payload_size = received - sizeof(BTcpHeader);
    memcpy(&hdr, buf, sizeof(BTcpHeader));
    memcpy(data + offset, buf + sizeof(BTcpHeader), payload_size);
    offset += payload_size;

    // Send the initial reply
    win_start = last_acked = hdr.btcp_seq + 1;
    response.btcp_ack = last_acked;
    response.win_size = bufsize;
    response.flags = F_ACK;
    sendto(socket, &response, sizeof response, 0, addr, addrlen);

    struct pollfd pfd = {socket, POLLIN, 0};
    ssize_t presult;
    while (recv_len < len) {
        presult = poll(&pfd, 1, conn->config.recv_timeout);
        if (presult == 1 && pfd.revents == POLLIN) {  // A packet is here
            ssize_t packet_len = recv(socket, packet_buf, conn->config.max_packet_size, 0);
            // Copy the header and inspect it
            memcpy(&hdr, packet_buf, packet_len);
            uint8_t win_ind = hdr.btcp_seq - win_start;
            if (win_ind >= bufsize) {
                // Not in window = unexpected packet
                continue;
            } else if (packet_flags[win_ind]) {
                // Already received - ignore
                continue;
            } else if (packet_len != hdr.data_off + hdr.data_len) {
                Logf(LOG_WARNING, "Wrong packet length: Expected %d, got %d", hdr.data_off + hdr.data_len, packet_len);
                Log(LOG_WARNING, "Discarded invalid packet");
            }
            // Save the packet
            packet_flags[win_ind] = 1;
            memcpy(buf + win_ind * conn->config.max_packet_size, packet_buf, packet_len);
        } else if (presult == 0) {
            // Timeout, handle received packets
            int i;

            // Copy complete sequences to receiver buffer
            for (i = 0; i < bufsize; i++) {
                if (packet_flags[i] == 0) {
                    // This one's missing, stop
                    break;
                }
                void *p = buf + i * conn->config.max_packet_size;
                memcpy(&hdr, p, sizeof hdr);
                memcpy(data + recv_len, p + hdr.data_off, hdr.data_len);
                recv_len += hdr.data_len;
            }
            // ACK complete ones
            last_acked += i;
            if (i > 0) {
                // Rotate window and buffer
                // TODO
            }
        } else {
            // pardon?
            Log(LOG_ERROR, "Unknown error");
        }
    }

cleanup:
    free(buf);
    free(packet_seq);
    return;
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
    conn->config.timeout = 10;  // default to 10 ms - lab requirement
    conn->config.recv_timeout = 5;  // default to 5 ms - optimal value
    conn->config.recv_buffer_size = 10;  // 10-packet buffer for receiving
}
