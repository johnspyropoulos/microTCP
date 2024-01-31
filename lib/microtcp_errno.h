#ifndef MICROTCP_ERRNO_H
#define MICROTCP_ERRNO_H

#include <stdio.h>

#define DEBUG

enum MICROTCP_ERRNO
{
    ALL_GOOD = 0,

    ERROR,

    /* SPECIFICS */
    NULL_POINTER_ARGUMENT,
    INVALID_ARGUMENT,
    MALLOC_FAILED,
    SOCKET_STATE_NOT_READY,
    SOCKET_STATE_NOT_ESTABLISHED,
    TIMEOUT_SET_FAILED,
    INVALID_IP_VERSION,
    BITSTREAM_CREATION_FAILED,
    BITSTREAM_EXTRACTION_FAILED,
    SYN_PACKET_EXPECTED,
    ACK_SYN_PACKET_EXPECTED,
    ACK_PACKET_EXPECTED,
    ACK_NUMBER_MISMATCH,
    HANDSHAKE_FAILED,
    SENDTO_FAILED,
    RECVFROM_CORRUPTED_PACKET,
    CHECKSUM_VALIDATION_FAILED,
    BS_QUEUE,
    RQ_QUEUE,
    SEND_HANDLER_FAILED,
    RECV_HANDLER_FAILED,
    SEND_ACK_FAILED
};

extern enum MICROTCP_ERRNO MICRO_ERRNO;

static void microtcp_clear_errno(void)
{
    MICRO_ERRNO = ALL_GOOD;
}

static void microtcp_set_errno(enum MICROTCP_ERRNO errno_, const char *file_name_, const char *function_name_, int line_)
{
    const char *error_message;
    MICRO_ERRNO = errno_;

    switch (errno_)
    {
    case NULL_POINTER_ARGUMENT:
        "NULL pointer was given as argument.";
        break;
    case MALLOC_FAILED:
        error_message = "Memory allocation failed.";
        break;
    case SOCKET_STATE_NOT_READY:
        error_message = "Socket is not in ready state.";
        break;
    case SOCKET_STATE_NOT_ESTABLISHED:
        error_message = "Socket is not in an established connection.";
        break;
    case TIMEOUT_SET_FAILED:
        error_message = "Setting timeout in recvfrom() failed.";
        break;
    case BITSTREAM_CREATION_FAILED:
        error_message = "Bit-stream creation failed.";
        break;
    case BITSTREAM_EXTRACTION_FAILED:
        error_message = "Bit-stream extraction failed.";
        break;
    case SYN_PACKET_EXPECTED:
        error_message = "Expected packet with SYN flag.";
        break;
    case ACK_SYN_PACKET_EXPECTED:
        error_message = "Expected packet with ACK and SYN flags.";
        break;
    case ACK_PACKET_EXPECTED:
        error_message = "Expected packet with ACK flag.";
        break;
    case ACK_NUMBER_MISMATCH:
        error_message = "ACK number does not match the expected value.";
        break;
    case HANDSHAKE_FAILED:
        error_message = "Three-way handshake failed between server and client.";
        break;
    case SENDTO_FAILED:
        error_message = "Sendind bit-stream with UDP::sendto() failed.";
        break;
    case RECVFROM_CORRUPTED_PACKET:
        error_message = "UDP::recvfrom returned corrupted data.";
        break;
    case CHECKSUM_VALIDATION_FAILED:
        error_message = "Header's checksum does not verify packet's byte sequence.";
        break;
    case INVALID_ARGUMENT:
        error_message = "Argument was invalid, check function and line to determine why.";
        break;
    case BS_QUEUE:
        error_message = "Something went wrong with struct bitstream_queue check file, function and line to determine.";
        break;
    case RQ_QUEUE:
        error_message = "Somethign went wrong with struct reordering_queue check file, function and line to determine. ";
        break;
    case SEND_HANDLER_FAILED:
        error_message = "Send handler failed.";
        break;
    case RECV_HANDLER_FAILED:
        error_message = "Receive handle failed.";
        break;
    case SEND_ACK_FAILED:
        error_message = "Sending ACKnowledgment back to the receiver failed.";
        break;
    default:
        error_message = "Unknown microtcp error number (default).";
        break;
    }

#ifdef DEBUG
    fprintf(stderr, "Error in file %s, line %d (%s): %s\n", file_name_, line_, function_name_, error_message);
#endif
}

#define microtcp_set_errno(errno_) microtcp_set_errno(errno_, __FILE__, __func__, __LINE__)

#endif