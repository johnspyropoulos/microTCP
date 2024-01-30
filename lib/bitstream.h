#ifndef BITSTREAM_QUEUE_H
#define BITSTREAM_QUEUE_H

#include "microtcp.h"
#include <stdint.h>
#include <stdlib.h>


struct bitstream_node
{
        void *bitstream;
        size_t bitstream_size;
        uint32_t required_ack_number;
        uint8_t verified;
        struct bitstream_node *next;
};

struct bitstream_queue
{
        struct bitstream_node *front_node;
        struct bitstream_node *rear_node;
        ssize_t unacknowledged_bytes;
        size_t latest_seq_num;
};

struct bitstream_queue *bs_create_queue(void);
int bs_is_empty_queue(struct bitstream_queue *queue);
void bs_enqueue(struct bitstream_queue *const queue, void *const bitstream, size_t bitstream_len);
void bs_dequeue(struct bitstream_queue *queue);
struct bitstream_node *bs_front(struct bitstream_queue *queue);
struct bitstream_node *bs_rear(struct bitstream_queue *queue);

/**
 * @brief Creates a MicroTCP bitstream, containing a header and (optionally) payload
 * @param socket MicroTCP socket
 * @param control control bits
 * @param payload payload, set NULL if no payload
 * @param payload_len payload size in bytes
 * @param stream_len is set to the size of the bitstream after successful creation
 * @returns the created bitstream
 */
void *create_bitstream(const microtcp_sock_t *const socket, uint16_t control, const void *const payload, size_t payload_len, size_t *stream_len);

/**
 * @brief Extracts a MicroTCP bitstream, returning the packet's header and saving its payload in the socket receive buffer
 * @param socket MicroTCP socket
 * @param bitstream bitstream to extract
 * @returns packet header
 */
microtcp_segment_t *extract_bitstream(void *const bitstream);

int is_valid_bistream(void *const bitstream);
#endif