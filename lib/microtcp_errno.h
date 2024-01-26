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
    MALLOC_FAILED,
    SOCKET_STATE_NOT_READY,
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
    RECVFROM_CORRUPTED,
    CHECKSUM_VALIDATION_FAILED
};

enum MICROTCP_ERRNO MICRO_ERRNO = ALL_GOOD;

static void microtcp_set_errno(enum MICROTCP_ERRNO errno_, const char *function_name_, int line_)
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
        error_message = "Socket state is not in ready state.";
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
    case RECVFROM_CORRUPTED:
        error_message = "UDP::recvfrom returned corrupted data.";
        break;
    case CHECKSUM_VALIDATION_FAILED:
        error_message = "Header's checksum does not verify packet's byte sequence.";
        break;
    default:
        error_message = "Unknown microtcp error number (default).";
        break;
    }

#ifdef DEBUG
    fprintf(stderr, "Error in line %d (%s): %s\n", line_, function_name_, error_message);
#endif
}

#endif