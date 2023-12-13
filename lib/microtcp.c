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

#include "microtcp.h"
#include "../utils/crc32.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Start of declarations of inner working (helper) functions: */

/**
 * @param bit_stream On success is address of bit_stream, otherwise NULL (failure).
 * @param stream_len On success is the length of bit_stream, otherwise 0 (failure).
 */
static void create_microtcp_bit_stream_segment(const microtcp_segment_t *const __segment, void **__bit_stream, size_t *__stream_len);
static inline void init_microtcp_segment(microtcp_segment_t *const __segment, uint32_t __seq_num, uint32_t __ack_num,
                                         uint16_t __ctrl_bits, uint16_t __win_size, uint32_t __data_len, uint8_t *__payload);
static void extract_microtcp_bitstream(microtcp_segment_t **__segment, void *__bit_stream, const size_t __stream_len);

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
        micro_sock.seq_number = 0; /* Undefined. */
        micro_sock.ack_number = 0; /* Undefined. */
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
                if (socket == NULL)
                        fprintf(stderr, "Error: microtcp_connect() failed, socket address was NULL\n");
                else
                        fprintf(stderr, "Error: microtcp_connect() failed, as given socket was not in a READY state.\n");
                return -1;
        }
        microtcp_segment_t syn_segment;
        microtcp_segment_t *syn_ack_segment;
        microtcp_segment_t ack_segment;
        void *bit_stream = NULL;
        size_t stream_len = sizeof(microtcp_header_t); /* No payload. */

        /* Initialize header of segment: */
        srand(time(NULL));
        socket->seq_number = rand() | 0b1;
        socket->ack_number = 0;

        init_microtcp_segment(&syn_segment, socket->seq_number, socket->ack_number, SYN_BIT, socket->init_win_size, 0, NULL);
        /* data_len is set to zero as we dont sent any payload in SYN phase. */
        /* Function microtcp_connect() is called in SYN phase
         * of the connection. Thus there is no payload. All of
         * necessary information to request a connection is
         * stored in the header of the segment.
         * */

        /* Send connection request to server: */
        create_microtcp_bit_stream_segment(&syn_segment, &bit_stream, &stream_len);
        if (bit_stream == NULL || stream_len == 0)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, bit-stream conversion failed.\n");
                free(bit_stream); /* free bit_stream buffer after sendto() operation. */
                return -1;
        }
        /* Send SYN packet. */
        ssize_t syn_ret_val = sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, address_len);
        free(bit_stream); /* free bit_stream buffer after sendto() operation. */

        /* Receive SYN, ACK packet. */
        bit_stream = malloc(stream_len);
        ssize_t syn_ack_ret_val = recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, (struct sockaddr *)address, &address_len);
        extract_microtcp_bitstream(&syn_ack_segment, bit_stream, stream_len);
        if ((syn_ack_segment->header.control & (SYN_BIT | ACK_BIT) != (SYN_BIT | ACK_BIT)))
        {
                fprintf(stderr, "Error: microtcp_connect() failed, reply was not an ACK and SYN packet.\n");
                free(syn_ack_segment);
                free(bit_stream);
                return -1;
        }
        else if (syn_ack_segment->header.ack_number != syn_segment.header.seq_number + 1)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, ack_num and seq_num are not valid.\n");
                free(syn_ack_segment);
                free(bit_stream);
                return -1;
        }

        socket->seq_number += 1;
        socket->ack_number = syn_ack_segment->header.seq_number+1;

        /* Send ACK packet. */
        init_microtcp_segment(&ack_segment, socket->seq_number, socket->ack_number, ACK_BIT, socket->init_win_size, 0, NULL);
        create_microtcp_bit_stream_segment(&ack_segment, &bit_stream, &stream_len);
        if (bit_stream == NULL || stream_len == 0)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, bit-stream conversion failed.\n");
                free(syn_ack_segment);
                free(bit_stream);
                return -1;
        }
        ssize_t ack_ret_val = sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, address_len);
        
        free(syn_ack_segment);
        free(bit_stream);
        if (syn_ret_val <= 0 || syn_ack_ret_val <= 0 || ack_ret_val <= 0)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, 3-way handshake failed.\n");
        }
        
        socket->servaddr = malloc(address_len);
        memcpy(socket->servaddr, address, address_len);

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
        socket->ack_number = recv_syn_segment->header.seq_number+1;

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
        if ((sent_ack_segment.header.control & ACK_BIT) != ACK_BIT || recv_ack_segment->header.ack_number != sent_ack_segment.header.seq_number+1)
        {
                fprintf(stderr, "Error: microtcp_accept() failed, ACK segment was invalid.\n");
                free(bit_stream);
                return -1;
        }

        // TODO: ASK TA: Is this correct? (Undefined). socket->seq_number += 1;
        socket->ack_number = recv_ack_segment->header.seq_number+1;

        socket->cliaddr = malloc(address_len);
        memcpy(socket->cliaddr, address, address_len);

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
                        void* bit_stream;
                        int len;
                        init_microtcp_segment(&sent_fin_ack_segment, socket->seq_number, socket->ack_number, FIN_BIT | ACK_BIT, socket->curr_win_size, 0, NULL);
                        create_microtcp_bit_stream_segment(&sent_fin_ack_segment, &bit_stream, &stream_len);
                        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, sizeof(struct sockaddr));

                        len = sizeof(microtcp_segment_t);
                        microtcp_segment_t* recv_ack_segment;
                        stream_len = sizeof(microtcp_segment_t);
                        bit_stream = malloc(stream_len);
                        recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, &len);
                        extract_microtcp_bitstream(&recv_ack_segment, bit_stream, stream_len);
                        if ((recv_ack_segment->header.control & ACK_BIT) != ACK_BIT || recv_ack_segment->header.ack_number != socket->seq_number+1)
                        {
                                fprintf(stderr, "Error: microtcp_shutdown() failed, received ACK segment was invalid.\n");
                                return -1;
                        }

                        socket->state = CLOSING_BY_HOST;

                        len = sizeof(microtcp_segment_t);
                        microtcp_segment_t* recv_fin_ack_segment;
                        recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, &len);
                        extract_microtcp_bitstream(&recv_fin_ack_segment, bit_stream, stream_len);
                        if ((recv_fin_ack_segment->header.control & (FIN_BIT | ACK_BIT)) != (FIN_BIT | ACK_BIT))
                        {
                                fprintf(stderr, "Error: microtcp_shutdown() failed, received FIN ACK segment was invalid.\n");
                                return -1;
                        }

                        socket->seq_number += 1;
                        socket->ack_number = recv_fin_ack_segment->header.seq_number+1;

                        microtcp_segment_t sent_ack_segment;
                        init_microtcp_segment(&sent_ack_segment, socket->seq_number, socket->ack_number, ACK_BIT, socket->curr_win_size, 0, NULL);
                        create_microtcp_bit_stream_segment(&sent_ack_segment, &bit_stream, &stream_len);
                        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->servaddr, sizeof(struct sockaddr));
                        
                        socket->state = CLOSED;

                        free(socket->servaddr);
                        socket->servaddr = NULL;

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

ssize_t
microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length,
              int flags)
{
        if (socket == NULL)
        {
                fprintf(stderr, "Error: microtcp_send() failed, socket was NULL.\n");
                return -1;
        }
        if (socket->state != ESTABLISHED)
        {
                fprintf(stderr, "Error: microtcp_send() failed, socket was not in ESTABLISHED state.\n");
                return -1;
        }

        /* Send packet */
        socket->seq_number += length;

        microtcp_segment_t packet;
        void* bit_stream;
        size_t stream_len;

        init_microtcp_segment(&packet, socket->seq_number, socket->ack_number, NO_FLAGS_BITS, socket->curr_win_size, length, (char*) buffer);
        create_microtcp_bit_stream_segment(&packet, &bit_stream, &stream_len);

        struct sockaddr* dest = (socket->cliaddr == NULL) ? socket->servaddr : socket->cliaddr;
        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, dest, sizeof(*dest));

        /* Receive ACK packet */
        stream_len = sizeof(microtcp_segment_t);
        bit_stream = malloc(stream_len);

        socklen_t len = sizeof(*dest);
        microtcp_segment_t* ack_packet;

        recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, dest, &len);
        extract_microtcp_bitstream(&ack_packet, bit_stream, stream_len);
        if ((ack_packet->header.control & ACK_BIT != ACK_BIT) || (ack_packet->header.ack_number != packet.header.seq_number+1))
        {
                fprintf(stderr, "Error: microtcp_send() failed, ACK packet was invalid.\n");
                return -1;
        }
        
        return 0;
}

ssize_t
microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
        if (socket == NULL)
        {
                fprintf(stderr, "Error: microtcp_recv() failed, socket was NULL.\n");
                return 1;
        }
        if (socket->state != ESTABLISHED)
        {
                fprintf(stderr, "Error: microtcp_recv() failed, socket was not in ESTABLISHED state.\n");
                return -1;
        }

        /* Receive packet */
        microtcp_segment_t* packet;
        size_t stream_len = sizeof(microtcp_segment_t) + socket->curr_win_size;
        void* bit_stream = malloc(stream_len);

        struct sockaddr* src = (socket->cliaddr == NULL) ? socket->servaddr : socket->cliaddr;
        int len = sizeof(src);
        recvfrom(socket->sd, bit_stream, stream_len, flags, src, &len);
        extract_microtcp_bitstream(&packet, bit_stream, stream_len);

        socket->ack_number = packet->header.seq_number+1;
        free(bit_stream);

        /* Shutdown case */
        if (socket->cliaddr != NULL && (packet->header.control & (FIN_BIT | ACK_BIT)) == (FIN_BIT | ACK_BIT))
        {
                printf("Shutdown case\n");
                microtcp_segment_t sent_ack_segment;
                void* bit_stream;
                size_t stream_len;
                init_microtcp_segment(&sent_ack_segment, socket->seq_number, socket->ack_number, ACK_BIT, socket->curr_win_size, 0, NULL);
                create_microtcp_bit_stream_segment(&sent_ack_segment, &bit_stream, &stream_len);
                sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->cliaddr, sizeof(struct sockaddr));

                socket->state = CLOSING_BY_PEER;
                printf("Shutdown case 1\n");

                microtcp_segment_t sent_fin_ack_segment;
                init_microtcp_segment(&sent_fin_ack_segment, socket->seq_number, socket->ack_number, FIN_BIT | ACK_BIT, socket->curr_win_size, 0, NULL);
                create_microtcp_bit_stream_segment(&sent_fin_ack_segment, &bit_stream, &stream_len);
                sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->cliaddr, sizeof(struct sockaddr));

                socket->seq_number += 1;

                len = sizeof(microtcp_segment_t);
                microtcp_segment_t* recv_ack_segment;
                stream_len = sizeof(microtcp_segment_t);
                bit_stream = malloc(stream_len);
                recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, socket->cliaddr, &len);
                extract_microtcp_bitstream(&recv_ack_segment, bit_stream, stream_len);
                if ((recv_ack_segment->header.control & ACK_BIT) != ACK_BIT)
                {
                        fprintf(stderr, "Error: microtcp_recv() failed, shutdown ACK bit was not valid.\n");
                        return -1;
                }

                printf("Shutdown case 2\n");
                socket->state = CLOSED;

                free(socket->cliaddr);
                socket->cliaddr = NULL;

                free(socket->servaddr);
                socket->servaddr = NULL;
                
                free(bit_stream);
                printf("Shutdown case 3\n");
                return 0;
        }

        memcpy(buffer, packet->payload, packet->header.data_len);

        /* Send ACK packet */
        microtcp_segment_t ack_segment;
        init_microtcp_segment(&ack_segment, socket->seq_number, socket->ack_number, ACK_BIT, socket->curr_win_size, 0, NULL);
        create_microtcp_bit_stream_segment(&ack_segment, &bit_stream, &stream_len);
        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, src, len);
        return 0;
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

/**
 * @param bit_stream On success is address of bit_stream, otherwise NULL (failure).
 * @param stream_len On success is the length of bit_stream, otherwise 0 (failure).
 */
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
                *__stream_len = (size_t) (*__bit_stream = NULL);
                return;
        }
        if ((__segment->header.data_len == 0) != (__segment->payload == NULL)) /* XOR: Checking two conditions at once. */
        {
                fprintf(stderr, "Internal_Error: create_microtcp_net_segment() failed, as data_len and payload do not match.\n");
                *__stream_len = (size_t) (*__bit_stream = NULL);
                return;
        }
        bit_stream_size += __segment->header.data_len; /* If data_len == 0, then nothing changes. */

        /* Allocate memory for bit_stream. */
        void *bit_stream_buffer = malloc(bit_stream_size);
        if (bit_stream_buffer == NULL) /* Malloc() failed. */
        {
                fprintf(stderr, "Error: create_microtcp_bit_stream(): Memory Allocation failed, malloc() failed.\n");
                *__stream_len = (size_t) (*__bit_stream = NULL);
                return;
        }
        /* bit_stream = header + (actual) payload. */
        memcpy(bit_stream_buffer, &(__segment->header), sizeof(microtcp_header_t));
        memcpy(bit_stream_buffer + sizeof(microtcp_header_t), __segment->payload, __segment->header.data_len);
        *__bit_stream = bit_stream_buffer;
        *__stream_len = bit_stream_size;
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
/* End   of definitions of inner working (helper) functions. */