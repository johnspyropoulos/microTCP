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

#include "microtcp.h"
#include "../utils/crc32.h"
#include "microtcp_errno.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#define microtcp_set_errno(errno_) microtcp_set_errno(errno_, __func__, __LINE__)

/* Start of declarations of inner working (helper) functions: */

/**
 * @deprecated
 */
static void create_microtcp_bit_stream_segment(const microtcp_segment_t *const __segment, void **__bit_stream, size_t *__stream_len);

/**
 * @deprecated
 */
static inline void init_microtcp_segment(microtcp_segment_t *const __segment, uint32_t __seq_num, uint32_t __ack_num,
                                         uint16_t __ctrl_bits, uint16_t __win_size, uint32_t __data_len, uint8_t *__payload);

/**
 * @deprecated
 */
static void extract_microtcp_bitstream(microtcp_segment_t **__segment, void *__bit_stream, const size_t __stream_len);

/**
 * @brief Creates a MicroTCP bitstream, containing a header and (optionally) payload
 * @param socket MicroTCP socket
 * @param control control bits
 * @param payload payload, set NULL if no payload
 * @param payload_len payload size in bytes
 * @param stream_len is set to the size of the bitstream after successful creation
 * @returns the created bitstream
 */
static void *create_bitstream(const microtcp_sock_t *const socket, uint16_t control, const void *const payload, size_t payload_len, size_t *stream_len);

/**
 * @brief Extracts a MicroTCP bitstream, returning the packet's header and saving its payload in the socket receive buffer
 * @param socket MicroTCP socket
 * @param bitstream bitstream to extract
 * @returns packet header
 */
static microtcp_segment_t *extract_bitstream(const void *const bitstream);

static int server_shutdown(microtcp_sock_t *socket);

/* REMOVE BEFORE SUBMISSION. */
static void print_bitstream(void *stream, size_t length)
{
        char *str = stream;
        for (size_t i = 0; i < length; i++)
                printf("%d | ", str[i]);
        printf("\n");
}

/* End   of declarations of inner working (helper) functions. */

microtcp_sock_t microtcp_socket(int domain, int type, int protocol)
{
        microtcp_sock_t micro_sock;

        micro_sock.state = READY;
        if (domain != AF_INET)
        {
                fprintf(stderr, "Warning: MicroTCP only accepts AF_INET (IPv4) as its socket domain parameter.\n");
                micro_sock.state = WARNING;
        }
        if (type != SOCK_DGRAM || (protocol != 0 && protocol != IPPROTO_UDP))
        {
                fprintf(stderr, "Warning: MicroTCP only accepts UDP protocol as its backbone.\n");
                micro_sock.state = WARNING;
        }
        if ((micro_sock.sd = socket(domain, type, protocol)) < 0)
        {
                fprintf(stderr, "Error: Unable to create socket for MicroTCP.\n");
                micro_sock.state = INVALID;
        }

        /* Default initializations: */
        micro_sock.init_win_size = MICROTCP_WIN_SIZE;

        micro_sock.curr_win_size = MICROTCP_INIT_CWND; /* TODO: Should these two be the same? */
        micro_sock.cwnd = MICROTCP_INIT_CWND;

        micro_sock.recvbuf = NULL;
        micro_sock.buf_fill_level = 0;
        micro_sock.ssthresh = MICROTCP_INIT_SSTHRESH;
        micro_sock.seq_number = rand() | 0b1; /* Random number not zero. */
        micro_sock.ack_number = 0;            /* Undefined. */
        micro_sock.packets_send = 0;
        micro_sock.packets_received = 0;
        micro_sock.packets_lost = 0;
        micro_sock.bytes_send = 0;
        micro_sock.bytes_received = 0;
        micro_sock.bytes_lost = 0;

        micro_sock.servaddr = NULL;
        micro_sock.cliaddr = NULL;

        return micro_sock;
}

int microtcp_bind(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{
        /* Essential check-ups. Reporting errors. */
        if (socket == NULL || socket->sd < 0 || socket->state != READY)
        {
                if (socket == NULL)
                        fprintf(stderr, "Error: microtcp_bind() failed, socket address was NULL.\n");
                else if (socket->sd < 0)
                        fprintf(stderr, "Error: File descriptor of MicroTCP socket was invalid.\n");
                else if (socket->state != READY)
                        fprintf(stderr, "Error: MicroTCP socket was not in a READY state.\n");
                return -1;
        }

        /* Upon successful completion, bind() shall return 0; otherwise,
         * -1 shall be returned and errno set to indicate the error.  */
        int bind_ret_val = bind(socket->sd, address, address_len);
        if (bind_ret_val == 0)          /* Thus bind() was successful. */
                socket->state = LISTEN; /* Socket is ready for incoming connection. */
        else                            /* Bind failed. */
                fprintf(stderr, "Error: microtcp_bind() failed.\n");

        socket->servaddr = malloc(address_len);
        memcpy(socket->servaddr, address, address_len);

        return bind_ret_val;
}

/* PHASE B TODO: compartmentalize functions even further.
 *Inner loops will be need for packet loss management.
 * */
/** @return Upon successful completion, connect() shall return 0; otherwise, -1 . */
int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{

        if (socket == NULL || socket->state != READY)
        {
                microtcp_set_errno(socket == NULL ? NULL_POINTER_ARGUMENT : SOCKET_STATE_NOT_READY);
                return -1;
        }

        /* Send SYN packet. */
        size_t stream_len = 0;
        void *bitstream_send = create_bitstream(socket, SYN_BIT, NULL, 0, &stream_len); /* Used to send SYN packet and later ACK packet. */
        if (bitstream_send == NULL)
        {
                microtcp_set_errno(BITSTREAM_CREATION_FAILED);
                return -1;
        }
        do
        {
                ssize_t syn_ret_val = sendto(socket->sd, bitstream_send, stream_len, NO_FLAGS_BITS, address, address_len);
                if (syn_ret_val < 0)
                {
                        microtcp_set_errno(SENDTO_FAILED);
                        socket->bytes_lost += stream_len;
                        free(bitstream_send);
                        return -1;
                }

                /* Receive ACK-SYN packet. */
                ssize_t ack_syn_ret_val = recvfrom(socket->sd, socket->recvbuf, stream_len, NO_FLAGS_BITS, (struct sockaddr *)address, &address_len);
                if (ack_syn_ret_val < 0)
                        continue; /* Nothing in receive buffers yet. */ /*TODO add counter.*/
                else if ((size_t) ack_syn_ret_val <= sizeof(microtcp_header_t))
                {
                        microtcp_set_errno(RECVFROM_CORRUPTED);
                        socket->packets_lost++;
                        socket->bytes_lost += (ack_syn_ret_val > 0) ? ack_syn_ret_val : 0;
                        continue;
                }
                microtcp_segment_t *received_segment = extract_bitstream(socket->recvbuf);
                if (received_segment == NULL)
                {
                        microtcp_set_errno(BITSTREAM_EXTRACTION_FAILED);
                        continue;
                }
                if (received_segment->header.ack_number != socket->seq_number + 1)
                {
                        microtcp_set_errno(ACK_NUMBER_MISMATCH);
                        continue;
                }
                if ((received_segment->header.control & (SYN_BIT | ACK_BIT)) != (SYN_BIT | ACK_BIT))
                {
                        microtcp_set_errno(ACK_SYN_PACKET_EXPECTED);
                        continue;
                }
                break;
        } while (true);

        socket->servaddr = malloc(address_len);
        memcpy(socket->servaddr, address, address_len);

        socket->recvbuf = malloc(MICROTCP_RECVBUF_LEN);

        socket->state = ESTABLISHED;
        return 0;
}

int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len)
{
        if (socket == NULL || socket->state != LISTEN)
        {
                if (socket == NULL)
                        fprintf(stderr, "Error: microtcp_accept() failed, socket address was NULL.\n");
                else
                        fprintf(stderr, "Error: microtcp_accept() failed, as given socket was not in LISTEN state.\n");
                return -1;
        }

        microtcp_segment_t *recv_syn_segment;
        microtcp_segment_t sent_ack_segment;
        size_t stream_len = sizeof(microtcp_header_t);
        void *bit_stream = malloc(stream_len);

        int ret_val;
        ret_val = recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, &address_len);
        extract_microtcp_bitstream(&recv_syn_segment, bit_stream, stream_len);
        if (recv_syn_segment->header.control != SYN_BIT)
        {
                fprintf(stderr, "Error: microtcp_accept() failed, SYN segment was invalid\n");
                free(bit_stream);
                return -1;
        }

        socket->seq_number = rand() | 0b1;
        socket->ack_number = recv_syn_segment->header.seq_number + 1;

        init_microtcp_segment(&sent_ack_segment, socket->seq_number, socket->ack_number, SYN_BIT | ACK_BIT, socket->init_win_size, 0, NULL);
        create_microtcp_bit_stream_segment(&sent_ack_segment, &bit_stream, &stream_len);
        if (bit_stream == NULL || stream_len == 0)
        {
                fprintf(stderr, "Error: microtcp_accept() failed, bit-stream conversion failed.\n");
                return -1;
        }

        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, address_len);

        microtcp_segment_t *recv_ack_segment;
        stream_len = sizeof(microtcp_segment_t);
        recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, &address_len);
        extract_microtcp_bitstream(&recv_ack_segment, bit_stream, stream_len);
        if ((sent_ack_segment.header.control & ACK_BIT) != ACK_BIT || recv_ack_segment->header.ack_number != sent_ack_segment.header.seq_number + 1)
        {
                fprintf(stderr, "Error: microtcp_accept() failed, ACK segment was invalid.\n");
                free(bit_stream);
                return -1;
        }

        // TODO: ASK TA: Is this correct? (Undefined). socket->seq_number += 1;
        socket->ack_number = recv_ack_segment->header.seq_number + 1;

        socket->cliaddr = malloc(address_len);
        memcpy(socket->cliaddr, address, address_len);

        socket->recvbuf = malloc(MICROTCP_RECVBUF_LEN);

        socket->state = ESTABLISHED;

        free(bit_stream);

        return 0;
}

int microtcp_shutdown(microtcp_sock_t *socket, int how)
{
        if (socket == NULL)
        {
                fprintf(stderr, "Error: microtcp_shutdown() failed, socket was NULL.\n");
                return -1;
        }
        if (socket->state != ESTABLISHED)
        {
                fprintf(stderr, "Error: microtcp_shutdown() failed, socket state was not ESTABLISHED.\n");
                return -1;
        }
        if (socket->cliaddr != NULL)
        {
                fprintf(stderr, "Error: microtcp_shutdown() failed, shutdown cannot be called by the server.\n");
                return -1;
        }

        switch (how)
        {
        /* Block both */
        default:
                microtcp_segment_t sent_fin_ack_segment;
                size_t stream_len;
                void *bit_stream;
                int len;
                init_microtcp_segment(&sent_fin_ack_segment, socket->seq_number, socket->ack_number, FIN_BIT | ACK_BIT, socket->curr_win_size, 0, NULL);
                create_microtcp_bit_stream_segment(&sent_fin_ack_segment, &bit_stream, &stream_len);
                sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, sizeof(struct sockaddr));

                len = sizeof(microtcp_segment_t);
                microtcp_segment_t *recv_ack_segment;
                stream_len = sizeof(microtcp_segment_t);
                bit_stream = malloc(stream_len);
                recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, &len);
                extract_microtcp_bitstream(&recv_ack_segment, bit_stream, stream_len);
                if ((recv_ack_segment->header.control & ACK_BIT) != ACK_BIT || recv_ack_segment->header.ack_number != socket->seq_number + 1)
                {
                        fprintf(stderr, "Error: microtcp_shutdown() failed, received ACK segment was invalid.\n");
                        return -1;
                }

                socket->state = CLOSING_BY_HOST;

                len = sizeof(microtcp_segment_t);
                microtcp_segment_t *recv_fin_ack_segment;
                recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, &len);
                extract_microtcp_bitstream(&recv_fin_ack_segment, bit_stream, stream_len);
                if ((recv_fin_ack_segment->header.control & (FIN_BIT | ACK_BIT)) != (FIN_BIT | ACK_BIT))
                {
                        fprintf(stderr, "Error: microtcp_shutdown() failed, received FIN ACK segment was invalid.\n");
                        return -1;
                }

                socket->seq_number += 1;
                socket->ack_number = recv_fin_ack_segment->header.seq_number + 1;

                microtcp_segment_t sent_ack_segment;
                init_microtcp_segment(&sent_ack_segment, socket->seq_number, socket->ack_number, ACK_BIT, socket->curr_win_size, 0, NULL);
                create_microtcp_bit_stream_segment(&sent_ack_segment, &bit_stream, &stream_len);
                sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, sizeof(struct sockaddr));

                socket->state = CLOSED;

                free(socket->servaddr);
                socket->servaddr = NULL;

                free(socket->recvbuf);
                socket->recvbuf = NULL;

                break;

        /* TODO: Phase B (probably?) */
        /* Block recv */
        case SHUT_RD:
                break;

        /* Block send */
        case SHUT_WR:
                break;
        }

        return 0;
}

ssize_t microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length, int flags)
{
        if (length > MICROTCP_MSS)
        {
                size_t bytes_left = length;
                while (bytes_left > MICROTCP_MSS)
                {
                        socket->seq_number += MICROTCP_MSS;
                        microtcp_send(socket, buffer, MICROTCP_MSS, flags);
                        buffer += MICROTCP_MSS;
                        bytes_left -= MICROTCP_MSS;
                }

                socket->seq_number += bytes_left;
                microtcp_send(socket, buffer, bytes_left, flags); /* Sends final chunk. */

                return length;
        }

        socket->seq_number += length;

        size_t stream_len;
        void *bitstream = create_bitstream(socket, NO_FLAGS_BITS, buffer, length, &stream_len);

        struct sockaddr *dest = (socket->cliaddr == NULL) ? socket->servaddr : socket->cliaddr;
        sendto(socket->sd, bitstream, stream_len, NO_FLAGS_BITS, dest, sizeof(*dest));

        free(bitstream);
        if (bitstream == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return -1;
        }

        socklen_t len = sizeof(*dest);
        recvfrom(socket->sd, socket->recvbuf, sizeof(microtcp_header_t), NO_FLAGS_BITS, dest, &len);
        microtcp_segment_t *ack_packet = extract_bitstream(socket->recvbuf);

        if ((ack_packet->header.control & ACK_BIT != ACK_BIT) || (ack_packet->header.seq_number != socket->seq_number + 1))
        {
                microtcp_set_errno(ACK_NUMBER_MISMATCH);
                return -1;
        }

        return stream_len - sizeof(microtcp_header_t);
}

ssize_t microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
        void *bitstream = malloc(length);
        struct sockaddr *dest = (socket->cliaddr == NULL) ? socket->servaddr : socket->cliaddr;

        socklen_t len = sizeof(*dest);
        ssize_t bytes_read = recvfrom(socket->sd, bitstream, length, NO_FLAGS_BITS, dest, &len);
        microtcp_header_t packet; /* = extract_bitstream(socket, bitstream);*/

        free(bitstream);

        socket->ack_number = packet.seq_number + 1;

        memcpy(buffer, socket->recvbuf, packet.data_len);

        if (socket->cliaddr != NULL && (packet.control & (FIN_BIT | ACK_BIT)) == (FIN_BIT | ACK_BIT))
        {
                return server_shutdown(socket);
        }

        size_t stream_len;
        bitstream = create_bitstream(socket, ACK_BIT, NULL, 0, &stream_len);
        sendto(socket->sd, bitstream, stream_len, NO_FLAGS_BITS, dest, sizeof(*dest));
        free(bitstream);

        if (bytes_read == -1)
                return -1;
        return bytes_read - sizeof(microtcp_header_t);
}

/* Start of definitions of inner working (helper) functions: */

static inline void init_microtcp_segment(microtcp_segment_t *const __segment, uint32_t __seq_num, uint32_t __ack_num,
                                         uint16_t __ctrl_bits, uint16_t __win_size, uint32_t __data_len, uint8_t *const __payload)
{
        __segment->header.seq_number = __seq_num;
        __segment->header.ack_number = __ack_num;
        __segment->header.control = __ctrl_bits;
        __segment->header.window = __win_size;
        __segment->header.data_len = __data_len;
        /* init_segment.header.future_use0 = NYI */
        /* init_segment.header.future_use1 = NYI */
        /* init_segment.header.future_use2 = NYI */
        /* init_segment.header.checksum = NYI */
        __segment->payload = __payload;
}

static void create_microtcp_bit_stream_segment(const microtcp_segment_t *const __segment, void **__bit_stream, size_t *__stream_len)
{
        /* To create an actual segment for the IP layer (3rd layer), we must pack and serialize
         * the header and the payload. Essentially converting the microtcp_segment_t (2nd layer)
         * into a bit-stream. Note: Padding may be required between the header and the payload.
         * After packing and converting everything into a bit-stream, the microtcp_segment can
         * be sent using the send() function of the underlying UDP protocol (backbone).
         * */

        size_t bit_stream_size = sizeof(microtcp_header_t); /* Valid segments contain at least a header. */
        if (__segment == NULL)
        {
                fprintf(stderr, "Internal_Error: create_microtcp_net_segment() failed, as segment address was NULL.\n");
                *__stream_len = (size_t)(*__bit_stream = NULL);
                return;
        }
        if ((__segment->header.data_len == 0) != (__segment->payload == NULL)) /* XOR: Checking two conditions at once. */
        {
                fprintf(stderr, "Internal_Error: create_microtcp_net_segment() failed, as data_len and payload do not match.\n");
                *__stream_len = (size_t)(*__bit_stream = NULL);
                return;
        }
        bit_stream_size += __segment->header.data_len; /* If data_len == 0, then nothing changes. */

        /* Allocate memory for bit_stream. */
        void *bit_stream_buffer = malloc(bit_stream_size);
        if (bit_stream_buffer == NULL) /* Malloc() failed. */
        {
                fprintf(stderr, "Error: create_microtcp_bit_stream(): Memory Allocation failed, malloc() failed.\n");
                *__stream_len = (size_t)(*__bit_stream = NULL);
                return;
        }
        /* bit_stream = header + (actual) payload. */
        memcpy(bit_stream_buffer, &(__segment->header), sizeof(microtcp_header_t));
        memcpy(bit_stream_buffer + sizeof(microtcp_header_t), __segment->payload, __segment->header.data_len);
        *__bit_stream = bit_stream_buffer;
        *__stream_len = bit_stream_size;
}

static void *create_bitstream(const microtcp_sock_t *const socket, uint16_t control, const void *const payload, size_t payload_len, size_t *stream_len)
{
        if (socket == NULL)
        {
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
                return NULL;
        }

        microtcp_header_t header;
        header.seq_number = socket->seq_number;
        header.ack_number = socket->ack_number;
        header.control = control;
        header.window = socket->curr_win_size;
        header.data_len = payload_len;
        /* init_segment.header.future_use0 = NYI */
        /* init_segment.header.future_use1 = NYI */
        /* init_segment.header.future_use2 = NYI */
        /* init_segment.header.checksum = NYI */

        void *bitstream = malloc(sizeof(microtcp_header_t) + payload_len);
        if (bitstream == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return NULL;
        }

        memcpy(bitstream, &header, sizeof(microtcp_header_t));
        memcpy(bitstream + sizeof(microtcp_header_t), payload, payload_len);

        *stream_len = sizeof(microtcp_header_t) + payload_len;
        return bitstream;
}

static microtcp_segment_t *extract_bitstream(const void *const bitstream)
{
        microtcp_segment_t *segment = malloc(sizeof(microtcp_segment_t));
        if (bitstream == NULL || segment == NULL)
        {
                microtcp_set_errno(bitstream == NULL ? NULL_POINTER_ARGUMENT : MALLOC_FAILED);
                return NULL;
        }

        memcpy(&(segment->header), bitstream, sizeof(microtcp_header_t));
        if (segment->header.data_len > 0)
                segment->payload = malloc(segment->header.data_len);
        if (segment->payload == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return segment;
        }

        memcpy((segment->payload), bitstream + sizeof(microtcp_header_t), segment->header.data_len);

        return segment;
}

static void extract_microtcp_bitstream(microtcp_segment_t **__segment, void *__bit_stream, const size_t __stream_len)
{

        if (__stream_len < sizeof(microtcp_header_t))
        {
                fprintf(stderr, "Error: extract_microtcp_bitstream() __stream_len less than minimum valid value.\n");
                *__segment = NULL;
                return;
        }
        microtcp_segment_t *segment = malloc(sizeof(microtcp_segment_t));

        if (segment == NULL)
        {
                fprintf(stderr, "Error: extract_microtcp_bitstream(): Memory Allocation failed, malloc() failed.\n");
                *__segment = NULL;
                return;
        }
        memcpy(&(segment->header), __bit_stream, sizeof(microtcp_header_t));
        segment->payload = (segment->header.data_len > 0) ? malloc(segment->header.data_len) : NULL;
        memcpy(segment->payload, __bit_stream + sizeof(microtcp_header_t), segment->header.data_len);
        *__segment = segment;
}

static int server_shutdown(microtcp_sock_t *socket)
{
        microtcp_segment_t sent_ack_segment;
        void *bit_stream;
        size_t stream_len;
        int payload_size = 0;

        socket->seq_number += payload_size + 1;

        init_microtcp_segment(&sent_ack_segment, socket->seq_number, socket->ack_number, ACK_BIT, socket->curr_win_size, payload_size, NULL);
        create_microtcp_bit_stream_segment(&sent_ack_segment, &bit_stream, &stream_len);
        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->cliaddr, sizeof(struct sockaddr));

        socket->state = CLOSING_BY_PEER;

        socket->seq_number += payload_size + 1;

        microtcp_segment_t sent_fin_ack_segment;
        init_microtcp_segment(&sent_fin_ack_segment, socket->seq_number, socket->ack_number, FIN_BIT | ACK_BIT, socket->curr_win_size, payload_size, NULL);
        create_microtcp_bit_stream_segment(&sent_fin_ack_segment, &bit_stream, &stream_len);
        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->cliaddr, sizeof(struct sockaddr));

        int len = sizeof(microtcp_segment_t);
        microtcp_segment_t *recv_ack_segment;
        stream_len = sizeof(microtcp_segment_t);
        bit_stream = malloc(stream_len);
        recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->cliaddr, &len);
        extract_microtcp_bitstream(&recv_ack_segment, bit_stream, stream_len);
        if ((recv_ack_segment->header.control & ACK_BIT) != ACK_BIT)
        {
                fprintf(stderr, "Error: microtcp_recv() failed, shutdown ACK bit was not valid.\n");
                return -1;
        }

        socket->state = CLOSED;

        free(socket->cliaddr);
        socket->cliaddr = NULL;

        free(socket->servaddr);
        socket->servaddr = NULL;

        free(socket->recvbuf);
        socket->recvbuf = NULL;

        free(bit_stream);
        return 0;
}

/* End   of definitions of inner working (helper) functions. */

#undef microtcp_set_errno