/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * CS335 - Project Phase A
 *
 * Ioannis Spyropoulos - csd5072
 * Georgios Evangelinos - csd4624
 * Niki Psoma - csd5038
 */

#ifndef LIB_MICROTCP_H_
#define LIB_MICROTCP_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>

/*
 * Several useful constants
 */
#define MICROTCP_ACK_TIMEOUT_US 200000         /* US = microseconds (letter 'u' is used to specify micro). */
#define MICROTCP_MSS 1400                      /* Maximum Segment Size (in bytes) of Data/Payload (headers not included). */
#define MICROTCP_RECVBUF_LEN 8192              /* 8 KB buffer size. */
#define MICROTCP_WIN_SIZE MICROTCP_RECVBUF_LEN /* 8KBytes. Seem small for window size. */
#define MICROTCP_INIT_CWND (3 * MICROTCP_MSS)
#define MICROTCP_INIT_SSTHRESH MICROTCP_WIN_SIZE

#define ACK_BIT (0b1 << 12)
#define RST_BIT (0b1 << 13)
#define SYN_BIT (0b1 << 14)
#define FIN_BIT (0b1 << 15)

#define INITIAL_CHECKSUM_VALUE 0
#define NO_FLAGS_BITS 0

#define MIN(X,Y) (X < Y) ? X : Y
#define MAX(X,Y) (X > Y) ? X : Y

/**
 * Possible states of the microTCP socket
 *
 * NOTE: You can insert any other possible state
 * for your own convenience
 */
typedef enum
{
        READY,
        WARNING,     /* Socket created, but soft errors occured. */
        LISTEN,      /* After bind() the socket it ready for incoming connections. */
        ESTABLISHED, /* After accept() the connection is established. */
        CLOSING_BY_PEER,
        CLOSING_BY_HOST,
        CLOSED,
        INVALID
} mircotcp_state_t;

/**
 * This is the microTCP socket structure. It holds all the necessary
 * information of each microTCP socket.
 *
 * NOTE: Fill free to insert additional fields.
 */
typedef struct
{
        int sd;                 /**< The underline UDP socket descriptor */
        mircotcp_state_t state; /**< The state of the microTCP socket */
        size_t init_win_size;   /**< The window size negotiated at the 3-way handshake */
        size_t curr_win_size;   /**< The current window size */

        uint8_t *recvbuf; /**< The *receive* buffer of the TCP
                               connection. It is allocated during the connection establishment and
                               is freed at the shutdown of the connection. This buffer is used
                               to retrieve the data from the network. */

        struct bitstream_queue *unacknowledged_queue; 
        /* CSD4624 Similar to recvbuf. But for sending packets.
           Basically unacknowledged_queue holds each packet in bitstream
           form that has been sent to the receiver and has not
           yet been acknowledged.
        */
        size_t buf_fill_level; /**< Amount of data in the buffer */

        size_t cwnd; /* Bytes that can be sent before receiving an ACK. */
        size_t ssthresh;

        size_t seq_number; /**< Keep the state of the sequence number */
        size_t ack_number; /**< Keep the state of the ack number */
        uint64_t packets_send;
        uint64_t packets_received;
        uint64_t packets_lost;
        uint64_t bytes_send;
        uint64_t bytes_received;
        uint64_t bytes_lost;

        struct sockaddr *servaddr;        /* CSD5072 */
        struct sockaddr *remote_end_host; /* CSD4624: Never mallocED, just a reference. */
        struct sockaddr *cliaddr;         /* CSD5072 */
} microtcp_sock_t;

/*
 * microTCP header structure
 * NOTE: DO NOT CHANGE!
 */
typedef struct
{
        uint32_t seq_number; /**< Sequence number */ /* If SYN flag bit in set in the
                                                      **< control field, this is the initial sequence number. */
        uint32_t ack_number;                         /**< ACK number */
        uint16_t control;                            /**< Control bits (e.g. SYN, ACK, FIN) */
        uint16_t window;                             /**< Window size in bytes */
        uint32_t data_len;                           /**< Data length in bytes (EXCLUDING header) */
        uint32_t future_use0;                        /**< 32-bits for future use */
        uint32_t future_use1;                        /**< 32-bits for future use */
        uint32_t future_use2;                        /**< 32-bits for future use */
        uint32_t checksum;                           /**< CRC-32 checksum, see crc32() in utils folder */
} microtcp_header_t;

typedef struct
{
        microtcp_header_t header;
        uint8_t *payload;
        /* Notice:
         * Padding is possibly required.
         * When sending the microtcp_segment, it is required to allocate
         * enough memory for the microtcp_header_t, (the padding) and the contents of the payload.
         * The size of the payload is specified in the microtcp_header_t on field data_len.
         * HEADER + (POSSIBLE_PADDING) + PAYLOAD = segment_to_send_with_UDP_send.
         * If payload == NULL then no payload. (SYN packets)
         * */

} microtcp_segment_t; /* MicroTCP packet. */

microtcp_sock_t microtcp_socket(int domain, int type, int protocol);

int microtcp_bind(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len);

int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len);

/**
 * Blocks waiting for a new connection from a remote peer.
 *
 * @param socket the socket structure
 * @param address pointer to store the address information of the connected peer
 * @param address_len the length of the address structure.
 * @return ATTENTION despite the original accept() this function returns
 * 0 on success or -1 on failure
 */
int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len);

int microtcp_shutdown(microtcp_sock_t *socket, int how);
/* TODO: what the fuck is this */
ssize_t microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length, int flags);

ssize_t microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags);

#endif /* LIB_MICROTCP_H_ */