#include <stdlib.h>
#include <stdint.h>
#include "microtcp_errno.h"
#include "microtcp.h"
#include "bitstream.h"
#include "../utils/crc32.h"
#include <string.h>


struct bitstream_queue *
bs_create_queue(void)
{
        struct bitstream_queue *queue = malloc(sizeof(struct bitstream_queue));
        if (queue == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return NULL;
        }
        queue->front_node = queue->rear_node = NULL;
        queue->unacknowledged_bytes = 0;

        return queue;
}

int bs_is_empty_queue(struct bitstream_queue *queue)
{
        if (queue == NULL)
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
        /* We dont return a legal value, in this case, after the error message, we want a segmentation fault. */
        return queue->front_node == NULL;
}

void bs_enqueue(struct bitstream_queue *const queue, void *const bitstream, const size_t bitstream_len)
{
        if (bitstream == NULL || bitstream_len < sizeof(microtcp_header_t))
        {
                microtcp_set_errno(bitstream == NULL ? NULL_POINTER_ARGUMENT : INVALID_ARGUMENT);
                return;
        }

        struct bitstream_node *node = malloc(sizeof(struct bitstream_node));
        if (node == NULL)
        {
                microtcp_set_errno(MALLOC_FAILED);
                return;
        }

        /* Node initialization. */
        node->bitstream = bitstream;
        node->bitstream_size = bitstream_len;
        node->required_ack_number = ((microtcp_header_t *)bitstream)->seq_number + 1;
        node->next = NULL;

        if (bs_is_empty_queue(queue))
                queue->front_node = queue->rear_node = node;
        else
                queue->rear_node = queue->rear_node->next = node;

        queue->unacknowledged_bytes += bitstream_len;
}

void bs_dequeue(struct bitstream_queue *queue)
{
        if (queue == NULL)
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
        if (bs_is_empty_queue(queue))
                microtcp_set_errno(BS_QUEUE);

        /* We want a segmentation fault to occur, if queue is NULL. */

        struct bitstream_node *old_front = queue->front_node;

        queue->front_node = queue->front_node->next;
        if (queue->front_node == NULL)
                queue->rear_node = NULL;
        queue->unacknowledged_bytes -= (((microtcp_header_t *)old_front->bitstream)->data_len + sizeof(microtcp_header_t));

        if (queue->unacknowledged_bytes < 0)
                microtcp_set_errno(BS_QUEUE); /* Queue can not hold negative number of info (bytes. )*/
        free(old_front->bitstream);
        free(old_front);
}

struct bitstream_node *bs_front(struct bitstream_queue *queue)
{
        if (queue == NULL)
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
        /* We want a segmentation fault to occur. */
        return queue->front_node;
}
struct bitstream_node *bs_rear(struct bitstream_queue *queue)
{
        if (queue == NULL)
                microtcp_set_errno(NULL_POINTER_ARGUMENT);
        /* We want a segmentation fault to occur. */
        return queue->rear_node;
}

void *create_bitstream(const microtcp_sock_t *const socket, uint16_t control, const void *const payload, size_t payload_len, size_t *stream_len)
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

int is_valid_bistream(void *const bitstream)
{
        uint32_t *bitstream_checksum = &(((microtcp_header_t *)bitstream)->checksum);
        size_t bitstream_size = sizeof(microtcp_header_t) + ((microtcp_header_t *)bitstream)->data_len;

        uint32_t included_checksum_value = *bitstream_checksum;
        *bitstream_checksum = INITIAL_CHECKSUM_VALUE;
        uint32_t calculated_checksum_value = crc32(bitstream, bitstream_size);
        *bitstream_checksum = included_checksum_value;

        return included_checksum_value == calculated_checksum_value;
}

microtcp_segment_t *extract_bitstream(void *const bitstream)
{
        if (!is_valid_bistream)
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