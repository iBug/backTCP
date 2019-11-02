#ifndef __BTCP_H
#define __BTCP_H

#include <stddef.h>
#include <stdint.h>

typedef struct _BTcpConfig {
    // If ACK is not received within $timeout, consider packet loss
    int timeout;  // In milliseconds

    // If no subsequent packets come in within $timeout, ACK received ones
    int recv_timeout;  // In milliseconds

    // Self-explanatory
    size_t max_packet_size;  // Expect sizeof(header) + max_payload
    size_t recv_buffer_size;  // # of packets for receiving buffer
} BTcpConfig;

typedef struct _BTcpState {
    uint16_t packet_sent;
} BTcpState;

typedef struct _BTcpConnection {
    int socket;
    struct sockaddr_in addr;
    BTcpState state;
    BTcpConfig config;
} BTcpConnection;

typedef struct _BTcpHeader {
    uint8_t btcp_sport;  // source port - unused
    uint8_t btcp_dport;  // destination port - unused
    uint8_t btcp_seq;    // sequence number
    uint8_t btcp_ack;    // acknowledgement number
    uint8_t data_off;    // data offset in bytes
    uint8_t win_size;    // window size
    uint8_t flags;       // flags
    uint8_t data_len;      // data length (excl .header)
} BTcpHeader;

/********
* Flags *
********/

#define F_RETRANSMISSION 0x01
#define F_EOT            0x02
#define F_ACK            0x40

/*****************
* Main Functions *
*****************/

// Send a stream of data
int BTSend(BTcpConnection* conn, const void* data, size_t len);

// Receive a stream of data
int BTRecv(BTcpConnection* conn, void* data, size_t len);

// Open a backTCP connection - both for connecting to server and listening
BTcpConnection* BTOpen(unsigned long addr, unsigned short port);

// Close a backTCP connection
void BTClose(BTcpConnection* conn);

// Reset a backTCP config to default
void BTDefaultConfig(BTcpConfig* config);

/*********************
* Internal Functions *
*********************/

// Removed from this header

#endif /* __BTCP_H */
