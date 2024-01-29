#ifndef SEND_HANDLER_H
#define SEND_HANDLER_H

#include <stdlib.h>
#include "microtcp_errno.h"
#include "microtcp.h"
#include "bitstream.h"

/**
 * @returns Amounts of bytes sent from buffer_index and after.
 */
int packet_sender(microtcp_sock_t *socket, const void *const buffer, size_t length, size_t *buffer_index)
{
        const size_t max_payload_size = MICROTCP_MSS - sizeof(microtcp_header_t);
        size_t bitstream_len = 0;
        size_t remaining_bytes = length - *buffer_index;
        size_t payload_size = MIN(remaining_bytes, max_payload_size);

        /* Data that the sender is typically allowed to send, respecting
        both receiver's capacityand network's current congestion stat. */
        size_t effective_window_size = MIN(socket->cwnd, socket->curr_win_size);

        size_t byte_sent_counter = 0;
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
                ssize_t send_ret_val = sendto(socket->sd, bitstream, bitstream_len, NO_FLAGS_BITS, socket->remote_end_host, sizeof(*(socket->remote_end_host)));
                if (send_ret_val < 0)
                {
                        microtcp_set_errno(SENDTO_FAILED);
                        socket->seq_number -= payload_size;
                        free(bitstream);
                        return -1;
                }
                bs_enqueue(socket->unacknowledged_queue, bitstream, bitstream_len);
                socket->bytes_send += bitstream_len;
                socket->packets_send++;
                *buffer_index += payload_size;
                byte_sent_counter += payload_size;
        }
        return byte_sent_counter;
}

int packet_verifier()
{
}

#endif
