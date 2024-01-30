#ifndef SEND_HANDLER_H
#define SEND_HANDLER_H

#include <stdlib.h>
#include <stdbool.h>
#include "microtcp_errno.h"
#include "microtcp.h"
#include "bitstream.h"

static

    /**
     * @returns Amounts of bytes sent from buffer_index and after.
     */
    int
    packet_sender(microtcp_sock_t *socket, const void *const buffer, size_t length, size_t *buffer_index)
{
        const size_t max_payload_size = MICROTCP_MSS - sizeof(microtcp_header_t);
        size_t bitstream_len = 0;
        size_t remaining_bytes = length - *buffer_index;
        size_t payload_size = MIN(remaining_bytes, max_payload_size);

        /* Data that the sender is typically allowed to send, respecting
        both receiver's capacityand network's current congestion stat. */
        size_t effective_window_size = MIN(socket->cwnd, socket->curr_win_size);

        size_t byte_sent_counter = 0;
        /* Load transmittion queue. */
        while (socket->unacknowledged_queue->unacknowledged_bytes + payload_size < effective_window_size)
        {
                socket->seq_number += payload_size; /* Update seq_number. */
                void *bitstream = create_bitstream(socket, ACK_BIT, buffer + *buffer_index, payload_size, &bitstream_len);
                if (bitstream == NULL)
                {
                        microtcp_set_errno(BITSTREAM_CREATION_FAILED);
                        socket->seq_number -= payload_size;
                        return -1;
                }
                bs_enqueue(socket->unacknowledged_queue, bitstream, bitstream_len);
                socket->bytes_send += bitstream_len;
                socket->packets_send++;
                *buffer_index += payload_size;
                byte_sent_counter += payload_size;
        }

        /* Send bitstreams contained in queue. */
        struct bitstream_node *current_node = socket->unacknowledged_queue->front_node;
        while (current_node)
        {
                if (sendto(socket->sd, current_node->bitstream, current_node->bitstream_size, NO_FLAGS_BITS, socket->remote_end_host, sizeof(*(socket->remote_end_host))) < 0)
                {
                        microtcp_set_errno(SENDTO_FAILED);
                        return -1;
                }
                current_node = current_node->next;
        }

        return byte_sent_counter;
}

static int acknowldge_up_to(microtcp_sock_t *socket, const uint32_t _ack_number)
{
        /* Search for packet with that ack_number. */
        struct bitstream_node *current_node = socket->unacknowledged_queue->front_node;
        uint8_t found_valid_node = false;

        /* Search if _ack_number corresponds to a packet previously sent, waiting for ACK. */
        while (current_node)
        {
                found_valid_node = current_node->required_ack_number == _ack_number;
                if (found_valid_node)
                        break;
                current_node = current_node->next;
        }
        if (found_valid_node == false)
        {
                microtcp_set_errno(ACK_NUMBER_MISMATCH);
                return 0;
        }

        /* Remove all packets up_to packet with ack_number == _ack_number. */
        current_node = socket->unacknowledged_queue->front_node;
        while (true)
        {
                uint8_t last_node = current_node->required_ack_number == _ack_number;
                current_node = current_node->next;
                bs_dequeue(socket->unacknowledged_queue);
                if (last_node)
                        break;
        }
}

int packet_verifier(microtcp_sock_t *socket)
{
        size_t verified_bytes = 0;             /* Returing counter. */
        uint8_t static_bitsteam[MICROTCP_MSS]; /* static pass-trough buffer. */
        socklen_t addr_len = sizeof(*(socket->remote_end_host));

        while (!bs_is_empty_queue(socket->unacknowledged_queue))
        {

                ssize_t recv_ret_val = recvfrom(socket->sd, static_bitsteam, MICROTCP_MSS, NO_FLAGS_BITS, socket->remote_end_host, &addr_len);
                if (recv_ret_val < 0 && verified_bytes == 0)
                        return -1;
                /* Timeout exceeded.  No ACKs. */
                if (!is_valid_bistream(static_bitsteam)) /* Check bitstream's validity, if invalid discard. */
                        continue;
                microtcp_header_t *header = (microtcp_header_t *)static_bitsteam;
                if (header->control != ACK_BIT)
                {
                        microtcp_set_errno(ACK_PACKET_EXPECTED);
                        continue;
                }

                if (header->control & FIN_BIT)
                {

                        /* TODO: Call Shutdown, from sender. */
                        ;
                }

                acknowldge_up_to(socket, header->ack_number);

                /* ACK packet, might also contain data. */
                if (header->data_len > 0)
                {
                        /* SO far we do not accept mixed packets. But we should implement. */
                        ;
                }
        }
}

#endif
