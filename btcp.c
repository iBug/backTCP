#include "btcp.h"
#include "logging.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>

int BTSend(BTcpConnection* conn, const void *data, size_t len) {
    const size_t bufsize = conn->config.max_packet_size;
    uint8_t *buf = malloc(bufsize);  // Buffer for a single packet
    if (buf == NULL) {
        Logf(LOG_ERROR, "Buffer allocation failed: %s", strerror(errno));
        return -1;
    }

    int socket = conn->socket;
    const struct sockaddr_in *addr = (struct sockaddr_in *)&conn->addr;
    Logf(LOG_INFO, "Connecting to %s:%d", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    if (connect(socket, (struct sockaddr *)addr, sizeof *addr) == -1) {
        Logf(LOG_ERROR, "Failed to connect to server: %s", strerror(errno));
        goto cleanup;
    }

    uint8_t last_acked = conn->state.packet_sent,
            recv_win_size = 1,  // # of available slots
            win_size = 1,  // Assume 1-packet window at the beginning
            packet_sent,
            is_retransmission = 0;
    size_t sent_len = 0;  // Size of data successfully sent (acked)

    while (sent_len < len) {
        packet_sent = 0;
        uint8_t next_seq = last_acked;
        win_size = recv_win_size;
        while (packet_sent < win_size) {
            size_t payload_size = bufsize - sizeof(BTcpHeader);
            off_t offset = sent_len + packet_sent * payload_size;
            if (offset >= len)
                // Already sent all data in window
                break;
            else if (len - offset < payload_size)
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
            memcpy(buf + sizeof hdr, data + offset, payload_size);
            Logf(LOG_DEBUG, "Sending packet seq=%d, size=%d, data offset=%d", next_seq, sizeof hdr + payload_size, offset);
            send(socket, buf, sizeof hdr + payload_size, 0);
            next_seq++;
            packet_sent++;
        }
        is_retransmission = 0;

        // Poll for response for 10 ms (configurable)
        struct pollfd pfd = {socket, POLLIN, 0};
        ssize_t presult = poll(&pfd, 1, conn->config.timeout * 100);
        if (presult == 1 && pfd.revents == POLLIN) {  // Something's ready
            recv(socket, buf, bufsize, 0);
            BTcpHeader hdr;
            memcpy(&hdr, buf, sizeof hdr);
            sent_len += (uint8_t)(hdr.btcp_ack - last_acked) * (bufsize - sizeof hdr);
            Logf(LOG_DEBUG, "Response received, ack=%d, win=%d, last_ack=%d, sent=%d", hdr.btcp_ack, hdr.win_size, last_acked, sent_len);
            last_acked = hdr.btcp_ack;
            recv_win_size = hdr.win_size;
            if (last_acked != next_seq) {
                // Some packets are lost, retransmit them
                Log(LOG_DEBUG, "Packets loss detected, retransmitting");
                is_retransmission = 1;
                continue;
            }
        } else if (presult == 0) {
            // Timeout
            Log(LOG_DEBUG, "ACK timeout, retransmitting");
            is_retransmission = 1;
            continue;
        } else {
            Logf(LOG_ERROR, "Unknown error: %s", strerror(errno));
        }
    }

cleanup:
    free(buf);
    return sent_len;
}

int BTRecv(BTcpConnection* conn, void *data, size_t len) {
    const size_t bufsize = conn->config.recv_buffer_size;
    uint8_t *buf = malloc((1 + bufsize) * conn->config.max_packet_size);
    if (buf == NULL) {
        Log(LOG_ERROR, "Failed to allocate memory");
        return 0;
    }
    uint8_t *packet_buf = buf + bufsize * conn->config.max_packet_size;
    int socket = conn->socket;
    struct sockaddr *addr = (struct sockaddr *)&conn->addr;
    socklen_t addrlen = sizeof *addr;
    bind(socket, addr, addrlen);

    uint8_t last_acked = conn->state.packet_sent,
            win_start = last_acked,
            *packet_flags = malloc(bufsize);
    size_t recv_len = 0,
           payload_size,
           offset = 0;
    ssize_t received;
    BTcpHeader hdr, response;

    for (int i = 0; i < bufsize; i++)
        packet_flags[i] = 0;

    // Fetch the first packet
    // Sender address will be given by recvfrom(2)
    Log(LOG_DEBUG, "Waiting for first packet");
    received = recvfrom(socket, buf, conn->config.max_packet_size, 0, addr, &addrlen);
    if (received < 0) {
        Log(LOG_ERROR, "Failed to receive initial packet");
        goto cleanup;
    }
    payload_size = received - sizeof(BTcpHeader);
    memcpy(&hdr, buf, sizeof hdr);
    memcpy(data + offset, buf + sizeof(BTcpHeader), payload_size);
    recv_len += payload_size;
    Logf(LOG_DEBUG, "Received first packet, len=%d seq=%d", received, hdr.btcp_seq);

    // Send the initial reply
    win_start = last_acked = hdr.btcp_seq + 1;
    response.btcp_ack = last_acked;
    response.win_size = bufsize;
    response.flags = F_ACK;
    Logf(LOG_DEBUG, "Responding with ACK=%d", last_acked);
    sendto(socket, &response, sizeof response, 0, addr, addrlen);

    struct pollfd pfd = {socket, POLLIN, 0};
    ssize_t presult;
    while (recv_len < len) {
        presult = poll(&pfd, 1, conn->config.recv_timeout);
        if (presult == 1 && pfd.revents == POLLIN) {  // A packet is here
            ssize_t packet_len = recv(socket, packet_buf, conn->config.max_packet_size, 0);
            if (packet_len == 0) {
                // 0-length packet: close
                Log(LOG_DEBUG, "Received zero-length packet, closing");
                break;
            }
            // Copy the header and inspect it
            memcpy(&hdr, packet_buf, sizeof hdr);
            uint8_t win_ind = hdr.btcp_seq - win_start;
            if (win_ind >= bufsize) {
                // Not in window = unexpected packet
                Log(LOG_WARNING, "Unexpected packet: sequence number not in window");
                continue;
            } else if (packet_flags[win_ind]) {
                // Already received - ignore
                Log(LOG_WARNING, "Unexpected packet: sequence number already received");
                continue;
            } else if (packet_len != hdr.data_off + hdr.data_len) {
                Logf(LOG_WARNING, "Wrong packet length: Expected %d, got %d", hdr.data_off + hdr.data_len, packet_len);
                Log(LOG_WARNING, "Discarded invalid packet");
                continue;
            } else {
                // Save the packet
                Logf(LOG_DEBUG, "Received packet, len=%d, seq=%d, saving to [%d]", packet_len, hdr.btcp_seq, win_ind);
                packet_flags[win_ind] = 1;
                memcpy(buf + win_ind * conn->config.max_packet_size, packet_buf, packet_len);
            }
        } else if (presult == 0) {
            // Timeout, handle received packets
            Log(LOG_DEBUG, "Handling received packets");
            int i;

            // Copy complete sequences to receiver buffer
            for (i = 0; i < bufsize; i++) {
                if (packet_flags[i] == 0) {
                    // This one's missing, stop
                    Logf(LOG_DEBUG, "Copied %d packets to receiver buffer", i);
                    break;
                }
                void *p = buf + i * conn->config.max_packet_size;
                memcpy(&hdr, p, sizeof hdr);
                Logf(LOG_DEBUG, "Copying packet [%d] to data+0x%X", i, recv_len);
                memcpy(data + recv_len, p + hdr.data_off, hdr.data_len);
                recv_len += hdr.data_len;
            }
            if (i > 0) {
                // Rotate window and buffer
                Logf(LOG_DEBUG, "Rotating window by %d", i);
                memmove(buf, buf + i * conn->config.max_packet_size, (bufsize - i) * conn->config.max_packet_size);
                memmove(packet_flags, packet_flags + i, bufsize - i);
                for (int j = bufsize - i; j < bufsize; j++)
                    packet_flags[j] = 0;
                win_start += i;
            }

            // ACK complete ones
            last_acked += i;
            response.btcp_ack = last_acked;
            response.win_size = bufsize;
            response.flags = F_ACK;
            Logf(LOG_DEBUG, "Received packets up to %hhu, acknowledging", (unsigned char)last_acked - 1U);
            sendto(socket, &response, sizeof response, 0, addr, addrlen);
        } else {
            Logf(LOG_ERROR, "Unknown error: %s", strerror(errno));
        }
    }

cleanup:
    Log(LOG_DEBUG, "Cleaning up");
    free(buf);
    free(packet_flags);
    return recv_len;
}

BTcpConnection* BTOpen(unsigned long addr, unsigned short port) {
    BTcpConnection* conn = malloc(sizeof(BTcpConnection));
    BTDefaultConfig(conn);
    conn->socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    conn->addr.sin_family = AF_INET;
    conn->addr.sin_addr.s_addr = addr;
    conn->addr.sin_port = htons(port);
    conn->state.packet_sent = 0;
    return conn;
}

void BTClose(BTcpConnection* conn) {
    close(conn->socket);
    free(conn);
}

void BTDefaultConfig(BTcpConnection* conn) {
    conn->config.max_packet_size = sizeof(BTcpHeader) + 64;
    conn->config.timeout = 10;  // default to 10 ms - lab requirement
    conn->config.recv_timeout = 5;  // default to 5 ms - optimal value
    conn->config.recv_buffer_size = 10;  // 10-packet buffer for receiving
}
