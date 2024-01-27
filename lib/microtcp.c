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
static microtcp_segment_t *extract_bitstream(void *const bitstream);

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
        micro_sock.curr_win_size = MICROTCP_WIN_SIZE;
        micro_sock.cwnd = MICROTCP_INIT_CWND;

        if ((micro_sock.recvbuf = malloc(MICROTCP_RECVBUF_LEN)) == NULL)
                microtcp_set_errno(MALLOC_FAILED);

        micro_sock.buf_fill_level = 0;
        micro_sock.ssthresh = MICROTCP_INIT_SSTHRESH;
        srand(time(NULL));
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
        micro_sock.remote_end_host = NULL;

        /* Set blocking time for receive at 0.2 seconds. */
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = MICROTCP_ACK_TIMEOUT_US;
        if (setsockopt(micro_sock.sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval)) < 0)
                microtcp_set_errno(TIMEOUT_SET_FAILED);

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

/** @return Upon successful completion, connect() shall return 0; otherwise, -1 . */
int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{
        if (socket == NULL || socket->state != READY)
        {
                microtcp_set_errno(socket == NULL ? NULL_POINTER_ARGUMENT : SOCKET_STATE_NOT_READY);
                return -1;
        }

        /* Send SYN packet. */
        /* Maybe create a wrapper function that wraps incrementation of seq_num and creating the bitstream, and sending. */
        size_t stream_len = 0;
        void *payload = NULL;
        size_t payload_size = 0;
        socket->seq_number += payload_size + 1;
        void *bitstream_send = create_bitstream(socket, SYN_BIT, NULL, 0, &stream_len); /* Used to send SYN packet and later ACK packet. */
        if (bitstream_send == NULL)
        {
                microtcp_set_errno(BITSTREAM_CREATION_FAILED);
                return -1;
        }
        while (true)
        {
                ssize_t syn_ret_val = sendto(socket->sd, bitstream_send, stream_len, NO_FLAGS_BITS, address, address_len);
                if (syn_ret_val < 0)
                {
                        microtcp_set_errno(SENDTO_FAILED);
                        free(bitstream_send);
                        return -1;
                }
                socket->packets_send++;

                /* Receive ACK-SYN packet. */
                ssize_t ack_syn_ret_val = recvfrom(socket->sd, socket->recvbuf, stream_len, NO_FLAGS_BITS, (struct sockaddr *)address, &address_len);
                if (ack_syn_ret_val < 0)
                {
                        socket->packets_lost++;
                        /* We resend the packet, as, there was no response in the
                        previous sending. Thus the previous packet is considered lost. */
                        continue; /* Nothing in receive buffers yet. */
                }
                socket->packets_received++;                              /* We got somesort of a packet. */
                if ((size_t)ack_syn_ret_val < sizeof(microtcp_header_t)) /* Possible corrupted UDP packet. */
                {
                        microtcp_set_errno(RECVFROM_CORRUPTED);
                        socket->packets_lost++;
                        continue;
                }

                microtcp_segment_t *received_segment = extract_bitstream(socket->recvbuf);
                if (received_segment == NULL)
                {
                        microtcp_set_errno(BITSTREAM_EXTRACTION_FAILED);
                        free(bitstream_send);
                        return -1;
                }
                if (received_segment->header.ack_number != socket->seq_number + 1 ||
                    ((received_segment->header.control & (SYN_BIT | ACK_BIT)) != (SYN_BIT | ACK_BIT)))
                {
                        if (received_segment->header.ack_number != socket->seq_number + 1)
                                microtcp_set_errno(ACK_NUMBER_MISMATCH);
                        else
                                microtcp_set_errno(ACK_SYN_PACKET_EXPECTED);
                        free(received_segment);
                        socket->packets_lost++;
                        continue;
                }
                socket->curr_win_size = socket->init_win_size = received_segment->header.window;
                socket->ack_number = received_segment->header.seq_number + 1;
                free(received_segment);
                break;
        }

        /* Send ACK packet. */
        socket->seq_number += payload_size + 1;
        free(bitstream_send);
        bitstream_send = create_bitstream(socket, ACK_BIT, NULL, 0, &stream_len);
        if (bitstream_send == NULL)
        {
                microtcp_set_errno(BITSTREAM_CREATION_FAILED);
                return -1;
        }
        ssize_t ack_ret_val = sendto(socket->sd, bitstream_send, stream_len, NO_FLAGS_BITS, (struct sockaddr *)address, address_len);
        socket->servaddr = malloc(address_len);
        if (ack_ret_val < 0 || socket->servaddr == NULL)
        {
                microtcp_set_errno(ack_ret_val < 0 ? SENDTO_FAILED : MALLOC_FAILED);
                free(bitstream_send);
                return -1;
        }
        memcpy(socket->servaddr, address, address_len);
        socket->remote_end_host = socket->servaddr;
        socket->packets_send++;
        socket->state = ESTABLISHED;
        free(bitstream_send);
        return 0;
}

int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len)
{
        if (socket == NULL || socket->state != LISTEN)
        {
                microtcp_set_errno(socket == NULL ? NULL_POINTER_ARGUMENT : SOCKET_STATE_NOT_READY);
                return -1;
        }
        size_t stream_len = sizeof(microtcp_header_t);
        while (true)
        {
                /* Receive request for connection. SYN FLAG BIT. */
                ssize_t syn_ret_val = recvfrom(socket->sd, socket->recvbuf, stream_len, NO_FLAGS_BITS, address, &address_len);
                if (syn_ret_val < 0)
                        continue;
                socket->packets_received++;
                if ((size_t)syn_ret_val < sizeof(microtcp_header_t)) /* Corrupted UDP packet. */
                {
                        microtcp_set_errno(RECVFROM_CORRUPTED);
                        continue;
                }
                microtcp_segment_t *syn_segment = extract_bitstream(socket->recvbuf);
                if (syn_segment == NULL)
                {
                        microtcp_set_errno(BITSTREAM_EXTRACTION_FAILED);
                        return -1;
                }
                if (syn_segment->header.control != SYN_BIT) /* Not a SYN packet. */
                {
                        microtcp_set_errno(SYN_PACKET_EXPECTED);
                        free(syn_segment);
                        continue;
                }
                /* Got a packet requesting a connection. */

                /* Negotiated window size. */
                socket->curr_win_size = socket->init_win_size = syn_segment->header.window;
                socket->ack_number = syn_segment->header.seq_number + 1;
                free(syn_segment);
                break;
        }
        /* Sent ACK-SYN packet. */
        void *payload = NULL;
        size_t payload_size = 0;
        socket->seq_number += payload_size + 1;
        void *bitstream_send = create_bitstream(socket, ACK_BIT | SYN_BIT, payload, payload_size, &stream_len);
        if (bitstream_send == NULL)
        {
                microtcp_set_errno(BITSTREAM_CREATION_FAILED);
                return -1;
        }
        while (true)
        {
                ssize_t ack_syn_ret_val = sendto(socket->sd, bitstream_send, stream_len, NO_FLAGS_BITS, address, address_len);
                if (ack_syn_ret_val < 0)
                {
                        microtcp_set_errno(SENDTO_FAILED);
                        free(bitstream_send);
                        return -1;
                }
                socket->packets_send++;
                ssize_t ack_ret_val = recvfrom(socket->sd, socket->recvbuf, stream_len, NO_FLAGS_BITS, address, &address_len);
                if (ack_ret_val < 0) /* No repsonse yet. */
                {
                        socket->packets_lost++;
                        /* We resend the packet, as, there was no response in the
                        previous sending. Thus the previous packet is considered lost. */
                        continue; /* Nothing in receive buffers yet. */
                }
                socket->packets_received++;
                if ((size_t)ack_ret_val < sizeof(microtcp_header_t)) /* Possibly corrupted UDP packet. */
                {
                        microtcp_set_errno(RECVFROM_CORRUPTED);
                        continue;
                }
                microtcp_segment_t *ack_segment = extract_bitstream(socket->recvbuf);
                if (ack_segment == NULL)
                {
                        microtcp_set_errno(BITSTREAM_EXTRACTION_FAILED);
                        free(bitstream_send);
                        return -1;
                }

                if (ack_segment->header.control != ACK_BIT ||
                    ack_segment->header.ack_number != socket->seq_number + 1)
                {
                        microtcp_set_errno((ack_segment->header.control != ACK_BIT) ? ACK_PACKET_EXPECTED : ACK_NUMBER_MISMATCH);
                        free(ack_segment);
                        continue;
                }
                socket->ack_number = ack_segment->header.seq_number + 1;
                free(ack_segment);
                break;
        }
        socket->cliaddr = malloc(address_len);
        if (socket->cliaddr == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                free(bitstream_send);
                return -1;
        }
        /* What about the other participant????? */
        memcpy(socket->cliaddr, address, address_len);
        socket->remote_end_host = socket->cliaddr;
        socket->state = ESTABLISHED;
        free(bitstream_send);
        return 0;
}

int microtcp_shutdown(microtcp_sock_t *socket, int how)
{
}

ssize_t microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length, int flags)
{
}

ssize_t microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
}
/* Start of definitions of inner working (helper) functions: */

static void *create_bitstream(const microtcp_sock_t *const socket, uint16_t control, const void *const payload, size_t payload_len, size_t *stream_len)
{
        if (socket == NULL)
        {
                microtcp_set_errno(BITSTREAM_CREATION_FAILED);
                return NULL;
        }

        microtcp_header_t header;
        header.seq_number = socket->seq_number;
        header.ack_number = socket->ack_number;
        header.control = control;
        header.window = socket->curr_win_size;
        header.data_len = payload_len;
        header.future_use0 = 0;
        header.future_use1 = 0;
        header.future_use2 = 0;
        header.checksum = INITIAL_CHECKSUM_VALUE;

        void *bitstream = malloc(sizeof(microtcp_header_t) + payload_len);
        if (bitstream == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return NULL;
        }

        memcpy(bitstream, &header, sizeof(microtcp_header_t));
        memcpy(bitstream + sizeof(microtcp_header_t), payload, payload_len);

        *stream_len = sizeof(microtcp_header_t) + payload_len;

        ((microtcp_header_t *)bitstream)->checksum = crc32(bitstream, *stream_len);
        return bitstream;
}

static int isValidChecksum(void *const bitstream)
{
        uint32_t *bitstream_checksum = &(((microtcp_header_t *)bitstream)->checksum);
        size_t bitstream_size = sizeof(microtcp_header_t) + ((microtcp_header_t *)bitstream)->data_len;

        uint32_t included_checksum_value = *bitstream_checksum;
        *bitstream_checksum = INITIAL_CHECKSUM_VALUE;
        uint32_t calculated_checksum_value = crc32(bitstream, bitstream_size);
        *bitstream_checksum = included_checksum_value;

        return included_checksum_value == calculated_checksum_value;
}

static microtcp_segment_t *extract_bitstream(void *const bitstream)
{
        if (!isValidChecksum)
        {
                microtcp_set_errno(CHECKSUM_VALIDATION_FAILED);
                return NULL;
        }

        microtcp_segment_t *segment = malloc(sizeof(microtcp_segment_t));
        segment->payload = NULL;
        if (bitstream == NULL || segment == NULL)
        {
                microtcp_set_errno(bitstream == NULL ? NULL_POINTER_ARGUMENT : MALLOC_FAILED);
                return NULL;
        }

        memcpy(&(segment->header), bitstream, sizeof(microtcp_header_t));

        if (segment->header.data_len > 0)
                segment->payload = malloc(segment->header.data_len);

        if (segment->payload == NULL && segment->header.data_len > 0)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return NULL;
        }

        memcpy(segment->payload, bitstream + sizeof(microtcp_header_t), segment->header.data_len);

        return segment;
}
/* End   of definitions of inner working (helper) functions. */

#undef microtcp_set_errno