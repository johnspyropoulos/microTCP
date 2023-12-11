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

void print_bitstream(void* stream, size_t length)
{
        char* str = stream;
        for (int i = 0; i < length; i++)
                printf("%d | ",  str[i]);
        printf("\n");
}

/* End   of declarations of inner working (helper) functions. */

microtcp_sock_t microtcp_socket(int domain, int type, int protocol)
{
        microtcp_sock_t micro_sock;

        micro_sock.state = READY_TO_BIND;
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

        return micro_sock;
}

int microtcp_bind(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{
        /* Essential check-ups. Reporting errors. */
        if (socket == NULL || socket->sd < 0 || socket->state != READY_TO_BIND)
        {
                if (socket == NULL)
                        fprintf(stderr, "Error: microtcp_bind() failed, socket address was NULL.\n");
                else if (socket->sd < 0)
                        fprintf(stderr, "Error: File descriptor of MicroTCP socket was invalid.\n");
                else if (socket->state != READY_TO_BIND)
                        fprintf(stderr, "Error: MicroTCP socket was not in READY_TO_BIND state.\n");
                return -1;
        }

        /* Upon successful completion, bind() shall return 0; otherwise,
         * -1 shall be returned and errno set to indicate the error.  */
        int bind_ret_val = bind(socket->sd, address, address_len);
        if (bind_ret_val == 0)          /* Thus bind() was successful. */
                socket->state = LISTEN; /* Socket is ready for incoming connection. */
        else                            /* Bind failed. */
                fprintf(stderr, "Error: microtcp_bind() failed.\n");

        return bind_ret_val;
}

/** @return Upon successful completion, connect() shall return 0; otherwise, -1 . */
int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address, socklen_t address_len)
{
        if (socket == NULL || socket->state != LISTEN)
        {
                if (socket == NULL)
                        fprintf(stderr, "Error: microtcp_connect() failed, socket address was NULL\n");
                else
                        fprintf(stderr, "Error: microtcp_connect() failed, as given socket was not in LISTEN state.\n");
                return -1;
        }
        microtcp_segment_t init_segment;
        microtcp_segment_t* recv_ack_segment;
        microtcp_segment_t sent_ack_segment;

        /* Initialize header of segment: */
        srand(time(NULL));
                                             /*Rand odd N*/  /*FLAGS*/                        /*payload*/
        init_microtcp_segment(&init_segment, rand() | 0b1, 0, SYN_BIT, socket->init_win_size, 0, NULL);
        /* data_len is set to zero as we dont sent any payload in SYN phase. */
        /* Function microtcp_connect() is called in SYN phase
         * of the connection. Thus there is no payload. All of
         * necessary information to request a connection is
         * stored in the header of the segment.
         * */

        /* Send connection request to server: */
        void *bit_stream = NULL;
        size_t stream_len;
        create_microtcp_bit_stream_segment(&init_segment, &bit_stream, &stream_len);
        if (bit_stream == NULL || stream_len == 0)
        {
                printf("%p %d\n", bit_stream, stream_len);
                fprintf(stderr, "Error: microtcp_connect() failed, bit-stream conversion failed.\n");
                return -1;
        }

        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, address_len);  /* Sent SYN packet. */

        stream_len = sizeof(microtcp_segment_t);
        recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, (struct sockaddr*) address, &address_len);
        extract_microtcp_bitstream(&recv_ack_segment, bit_stream, stream_len);
        if ((recv_ack_segment->header.control & (SYN_BIT | ACK_BIT) != (SYN_BIT | ACK_BIT)) || recv_ack_segment->header.ack_number != init_segment.header.seq_number+1)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, ACK packet was not valid.\n");
                return -1;
        }

        init_microtcp_segment(&sent_ack_segment, recv_ack_segment->header.seq_number+1, recv_ack_segment->header.ack_number, ACK_BIT, socket->init_win_size, 0, NULL);
        create_microtcp_bit_stream_segment(recv_ack_segment, &bit_stream, &stream_len);
        if (bit_stream == NULL || stream_len == 0)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, bit-stream conversion failed.\n");
                return -1;
        }

        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, address_len);

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

        microtcp_segment_t* recv_syn_segment;
        microtcp_segment_t sent_ack_segment;
        size_t stream_len = sizeof(microtcp_header_t);
        void* bit_stream = malloc(stream_len);

        int ret_val;
        ret_val = recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, &address_len);
        extract_microtcp_bitstream(&recv_syn_segment, bit_stream, stream_len);
        if (recv_syn_segment->header.control != SYN_BIT)
        {
                fprintf(stderr, "Error: microtcp_accept() failed, SYN segment was invalid\n");
                return -1;
        }

        init_microtcp_segment(&sent_ack_segment, rand() | 0b1, recv_syn_segment->header.seq_number+1, SYN_BIT | ACK_BIT, socket->init_win_size, 0, NULL);
        create_microtcp_bit_stream_segment(&sent_ack_segment, &bit_stream, &stream_len);
        if (bit_stream == NULL || stream_len == 0)
        {
                fprintf(stderr, "Error: microtcp_accept() failed, bit-stream conversion failed.\n");
                return -1;
        }
        
        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, address_len);

        microtcp_segment_t* recv_ack_segment;
        stream_len = sizeof(microtcp_segment_t);
        recvfrom(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, &address_len);
        extract_microtcp_bitstream(&recv_ack_segment, bit_stream, stream_len);
        if ((sent_ack_segment.header.control & ACK_BIT) != ACK_BIT || recv_ack_segment->header.ack_number != sent_ack_segment.header.seq_number+1)
        {
                fprintf(stderr, "Error: microtcp_accept() failed, ACK segment was invalid.\n");
                return -1;
        }

        socket->state = ESTABLISHED;

        return 0;
}

int microtcp_shutdown(microtcp_sock_t *socket, int how)
{
        /* Your code here */
}

ssize_t
microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length,
              int flags)
{
        /* Your code here */
}

ssize_t
microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
        /* Your code here */
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
                *__bit_stream = *__stream_len = NULL;
                return;
        }
        if ((__segment->header.data_len == 0) != (__segment->payload == NULL)) /* XOR: Checking two conditions at once. */
        {
                fprintf(stderr, "Internal_Error: create_microtcp_net_segment() failed, as data_len and payload do not match.\n");
                *__bit_stream = *__stream_len = NULL;
                return;
        }
        bit_stream_size += __segment->header.data_len; /* If data_len == 0, then nothing changes. */

        /* Allocate memory for bit_stream. */
        void *bit_stream_buffer = malloc(bit_stream_size);
        if (bit_stream_buffer == NULL) /* Malloc() failed. */
        {
                fprintf(stderr, "Error: create_microtcp_bit_stream(): Memory Allocation failed, malloc() failed.\n");
                *__bit_stream = *__stream_len = NULL;
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
        segment->payload = NULL;  /* Set default payoad address. */
        /* Copy possible payload. */
        memcpy(segment->payload, __bit_stream + sizeof(microtcp_header_t), segment->header.data_len);
        *__segment = segment;
}
/* End   of definitions of inner working (helper) functions. */