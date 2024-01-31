#ifndef RECV_HANDLER_H
#define RECV_HANDLER_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "microtcp.h"
#include "microtcp_errno.h"
#include "bitstream.h"
#include "send_handler.h"

struct reordering_node
{
        void *pure_payload; /* Without header*/
        uint32_t payload_len;
        uint32_t seq_number;
        struct reordering_node *next;
};

struct reordering_queue
{
        struct reordering_node *head;
        struct reordering_node *tail;
        uint32_t bytes_kept;
};

struct reordering_queue *rq_create_queue(void)
{
        struct reordering_queue *queue = malloc(sizeof(struct reordering_queue));
        if (queue == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return NULL;
        }
        queue->head = NULL;
        queue->tail = NULL;
        queue->bytes_kept = 0;

        return queue;
}

int rq_is_empty_queue(struct reordering_queue *queue)
{
        if (queue == NULL)
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
        /* We dont return a legal value, in this case, after the error message, we want a segmentation fault. */
        return queue->head == NULL;
}

void rq_enqueue(struct reordering_queue *const queue, const void *const pure_payload_, const uint32_t payload_len_, const uint32_t seq_number_)
{
        if (queue == NULL || pure_payload_ == NULL)
        {
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
                return;
        }

        struct reordering_node *new_node = malloc(sizeof(struct reordering_node));
        if (new_node == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return;
        }
        new_node->pure_payload = malloc(payload_len_);
        if (new_node->payload_len ==  NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return;
        }
        memcpy(new_node->pure_payload, pure_payload_, payload_len_);
        new_node->payload_len = payload_len_;
        new_node->seq_number = seq_number_;
        new_node->next = NULL;

        queue->bytes_kept += new_node->payload_len;
        if (rq_is_empty_queue(queue))
        {
                queue->head = queue->tail = new_node;
                return;
        }

        if (new_node->seq_number < queue->head->seq_number)
        {
                new_node->next = queue->head->seq_number;
                queue->head = new_node;
                return;
        }

        struct reordering_node *current = queue->head;
        struct reordering_node *prev_current = NULL;
        while (current)
        {
                prev_current = current;
                if (current->seq_number < new_node->seq_number)
                {
                        current = current->next;
                }
                else if (current->seq_number > new_node->seq_number)
                {
                        prev_current->next = new_node;
                        new_node->next = current;
                        return;
                }
        }
        prev_current->next = new_node;
        queue->tail = new_node;
}



/**
 * @brief Copies data from a microTCP socket's internal receive buffer to a specified user buffer,
 * up to a given length. It then shifts any remaining data in the internal buffer to the start,
 * maintaining the integrity of the buffer's data sequence. If the function encounters any errors
 * (e.g., NULL pointers), it sets an appropriate error code.
 *
 * @param socket Pointer to the microTCP socket from which data is to be fetched.
 *               Must be properly allocated, else undefined behavior.
 * @param buffer Pointer to the user-allocated buffer where fetched data will be copied.
 *               Must be properly allocated and should have enough space for at least `length` bytes.
 * @param length The maximum number of bytes to copy into `buffer`.
 *
 * @returns The number of bytes successfully copied into `buffer`. Returns -1 if an error occurs,
 *          indicating either a NULL pointer was passed for `socket` or `buffer`, or if `length`
 *          is invalid. In the case of an error, an appropriate error code is set using
 *          microtcp_set_errno.
 *
 * @note This function modifies the internal state of the `socket`'s buffer, including its fill level.
 *       Callers should ensure that concurrent access to the socket's buffer is properly synchronized.
 *       Future optimization may include replacing the current memmove operation with circular buffering
 *       to improve efficiency.
 */
ssize_t fetch_buffered_data(microtcp_sock_t *const socket, void *const buffer, const size_t length)
{
        if (socket == NULL || buffer == NULL)
        {
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
                return -1;
        }

        size_t bytes_to_copy = MIN(length, socket->buf_fill_level);
        memcpy(buffer, socket->recvbuf, bytes_to_copy); /* Copy to user's buffer. */
        socket->buf_fill_level -= bytes_to_copy;
        memmove(socket->recvbuf, socket->recvbuf + bytes_to_copy, socket->buf_fill_level); /* TODO: Optimizaztion: Replace memmove with circle buffering. */

        return bytes_to_copy;
}
/**
 * @brief Receives a new data packet from a microTCP socket, handling retransmissions and discarding
 * corrupted or non-data packets.
 *
 * @param socket Pointer to an initialized microTCP socket.
 * @param bitstream_buffer Pointer to a buffer where the received data will be stored.
 * @param buffer_len Size of the buffer pointed to by bitstream_buffer.
 *
 * @return The number of bytes received and stored in bitstream_buffer on success, or -1 on error.
 *         On error, an appropriate error code is set with microtcp_set_errno.
 *
 * @note The function updates socket statistics for received and potentially lost packets.
 */
static ssize_t fetch_new_data_packet(microtcp_sock_t *const socket, void *const bitstream_buffer, size_t buffer_len)
{
        socklen_t addr_len = sizeof(*(socket->remote_end_host));
        ssize_t recvfrom_ret_val = 0;

        while (true)
        {
                recvfrom_ret_val = recvfrom(socket->sd, bitstream_buffer, buffer_len, NO_FLAGS_BITS, socket->remote_end_host, &addr_len);
                if (recvfrom_ret_val < 0) /* No packets in UDP buffers, yet. Nothing to retrieve. */
                {
                        recvfrom_ret_val = 0;
                        break;
                }
                socket->packets_received++;
                socket->bytes_received += recvfrom_ret_val;

                if (recvfrom_ret_val < (ssize_t)sizeof(microtcp_header_t) || !is_valid_bistream(bitstream_buffer)) /* Corrupted packet, discard. */
                {
                        microtcp_set_errno(RECVFROM_CORRUPTED_PACKET);
                        continue;
                }
                microtcp_header_t *header = (microtcp_header_t *)bitstream_buffer;
                if (header->seq_number < socket->ack_number) /* Retransmittion of already retrieved packet. Maybe ACK was lost. */
                {
                        ssize_t retransmitted_bytes = send_pure_ack_packet(socket);
                        /* If ack_number was already greater than the retrieved packet's
                        seq_number then we already have send an ACKnowledgement back for
                        this packet, and it most likely lost.  */
                        if (retransmitted_bytes < 0)
                        {
                                microtcp_set_errno(SEND_ACK_FAILED);
                                return -1;
                        }
                        socket->packets_lost++;
                        socket->bytes_lost += retransmitted_bytes;
                }
                if (header->data_len <= 0) /* Discard packet. */
                        continue;
                break;
        }
        microtcp_clear_errno();
        return recvfrom_ret_val;
}

/* Receives a packet, maybe puts it in reordering queue. */
static ssize_t send_to_reordering_queue(microtcp_sock_t *socket, void *bitstream_buffer, size_t bitstream_len)
{
        microtcp_header_t *packet_header = (microtcp_header_t *)bitstream_buffer;
        if (packet_header->seq_number >= socket->seq_number + 0)
        {
                ;
        }
}

ssize_t fetch_new_data(microtcp_sock_t *socket, void *const buffer, size_t *const bytes_in_buffer, const size_t length)
{
        if (socket == NULL || buffer == NULL || bytes_in_buffer == NULL)
        {
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
                return -1;
        }

        uint8_t local_bitstream_buffer[MICROTCP_MSS];
        while (true)
        {
                ssize_t new_packet_bytes = fetch_new_data_packet(socket, local_bitstream_buffer, MICROTCP_MSS);
                if (new_packet_bytes < 0)
                {
                        microtcp_set_errno(RECV_HANDLER_FAILED);
                        return -1;
                }
                if (new_packet_bytes == 0) /* There where no valid packets to receive. Timeout exceeded. */
                        break;
                send_to_reordering_queue(socket, local_bitstream_buffer, new_packet_bytes);
                /* Do a sequencial extraction to user's buffer. Remember buffer mgiht be half full. */
                /* Well... We maybe have received a new packet. But maybe the sender did not */
        }
}
#endif