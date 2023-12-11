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
static void create_microtcp_bit_stream_segment(const microtcp_segment_t *const __segment, void *__bit_stream, size_t *__stream_len);
static inline void init_microtcp_segment(microtcp_segment_t *const __segment, uint32_t __seq_num, uint32_t __ack_num,
                                        uint16_t __ctrl_bits, uint16_t __win_size, uint32_t __data_len, uint8_t *__payload);
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
        microtcp_segment_t recv_ack_segment;
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
        void *bit_stream;
        size_t stream_len;
        create_microtcp_bit_stream_segment(&init_segment, bit_stream, stream_len);
        if (bit_stream == NULL || stream_len == 0)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, bit-stream conversion failed.\n");
                return -1;
        }

        sendto(socket->sd, bit_stream, stream_len, NO_FLAGS_BITS, address, address_len);  /* Sent SYN packet. */

        recvfrom(socket->sd, &recv_ack_segment, sizeof(recv_ack_segment), NO_FLAGS_BITS, address, address_len);
        if (recv_ack_segment.header.control != SYN_BIT | ACK_BIT || recv_ack_segment.header.ack_number != init_segment.header.seq_number+1)
        {
                fprintf(stderr, "Error: microtcp_connect() failed, ACK packet was not valid.\n");
                return -1;
        }

        init_microtcp_segment(&sent_ack_segment, recv_ack_segment.header.ack_number, recv_ack_segment.header.seq_number+1, ACK_BIT, socket->init_win_size, 0, NULL);
        create_microtcp_bit_stream_segment(&recv_ack_segment, bit_stream, stream_len);
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
                        fprintf(stderr, "Error: microtcp_accept() failed, socket address was NULL\n");
                else
                        fprintf(stderr, "Error: microtcp_accept() failed, as given socket was not in LISTEN state.\n");
                return -1;
        }

        microtcp_segment_t recv_syn_segment;
        microtcp_segment_t sent_ack_segment;

        recvfrom(socket->sd, &recv_syn_segment, sizeof(recv_syn_segment), MSG_WAITALL, address, address_len);


        return 0;
}

/*
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address,
                     socklen_t address_len)
{
        microtcp_header_t hto;
        microtcp_header_t hfrom;

        hto.seq_number = 1000;
        hto.ack_number = 0;
        hto.control = SYN;
        hto.window = socket->curr_win_size;
        hto.data_len = 0;
        hto.checksum = 0;

        sendto(socket->sd, &hto, sizeof(hto), MSG_CONFIRM, address, address_len);

        recvfrom(socket->sd, &hfrom, sizeof(hfrom), MSG_WAITALL, address, address_len);

        if (hfrom.control != SYN | ACK || hfrom.ack_number != hto.seq_number + 1)
                return -1;

        hto.seq_number = hfrom.ack_number;
        hto.ack_number = hfrom.seq_number + 1;
        hto.control = ACK;
        hto.window = socket->curr_win_size;
        hto.window = 0;
        hto.checksum = 0;

        sendto(socket->sd, &hto, sizeof(hto), MSG_CONFIRM, address, address_len);

        return 0;
}

int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address, socklen_t address_len)
{
        if (socket == NULL || socket->state != LISTEN)
        {
                // The socket is not in a state to accept connections
                return -1;
        }

        int new_sd = accept(socket->sd, address, address_len);
        if (new_sd < 0)
        {
                perror("Accept failed");
                return -1;
        }

        // Initialize a new microTCP socket for the accepted connection
        microtcp_sock_t new_socket;
        new_socket.sd = new_sd;
        new_socket.state = ESTABLISHED;
        new_socket.init_win_size = MICROTCP_WIN_SIZE;
        new_socket.curr_win_size = MICROTCP_WIN_SIZE;
        // Allocate memory for the receive buffer and initialize other fields
        new_socket.recvbuf = malloc(MICROTCP_RECVBUF_LEN);
        if (new_socket.recvbuf == NULL)
        {
                // Handle memory allocation failure
                close(new_sd);
                return -1;
        }
        new_socket.buf_fill_level = 0;
        new_socket.cwnd = MICROTCP_INIT_CWND;
        new_socket.ssthresh = MICROTCP_INIT_SSTHRESH;
        new_socket.seq_number = 0; // This would be a random number in a real TCP implementation
        new_socket.ack_number = 0; // This should be set based on the incoming SYN packet

        // Copy the new socket structure to the user-provided socket
        *socket = new_socket;

        return 0;
}
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
COMMENTED_OUT CODE -- OLD NON_WORKING CODE
*/

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
static void create_microtcp_bit_stream_segment(const microtcp_segment_t *const __segment, void *__bit_stream, size_t *__stream_len)
{
        /* To create an actual segment for the IP layer (3rd layer), we must pack and serialize
         * the header and the payload. Essentially converting the microtcp_segment_t (2nd layer)
         * into a bit-stream. Note: Padding may be required between the header and the payload.
         * After packing and converting everything into a bit-stream, the microtcp_segment can
         * be sent using the send() function of the underlying UDP protocol (backbone).
         * */

        size_t segment_size = sizeof(microtcp_header_t);  /* Valid segments contain at least a header. */
        if (__segment == NULL)
        {
                fprintf(stderr, "Internal_Error: create_microtcp_net_segment() failed, as segment address was NULL.\n");
                __bit_stream = *__stream_len = NULL;
                return;
        }
        if ((__segment->header.data_len == 0) != (__segment->payload == NULL)) /* XOR: Checking two conditions at once. */
        {
                fprintf(stderr, "Internal_Error: create_microtcp_net_segment() failed, as data_len and payload do not match.\n");
                __bit_stream = *__stream_len = NULL;
                return;
        }
        segment_size += __segment->header.data_len;  /* If data_len == 0, then nothing changes. */
        
        /* Allocate memory for bit_stream. */
        void *segment_buffer = malloc(segment_size);
        if (segment_buffer == NULL) /* Malloc() failed. */
        {
                fprintf(stderr, "Error: Memory Allocation failed, malloc() failed.\n");
                __bit_stream = *__stream_len = NULL;
                return;
        }

        memcpy(segment_buffer, __segment, sizeof(microtcp_segment_t));
        memcpy(segment_buffer + sizeof(microtcp_segment_t), __segment->payload, __segment->header.data_len);
        __bit_stream = segment_buffer;
        *__stream_len = segment_size;
}
/* End   of definitions of inner working (helper) functions. */