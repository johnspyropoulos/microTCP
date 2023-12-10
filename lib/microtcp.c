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

microtcp_sock_t
microtcp_socket (int domain, int type, int protocol)
{
  microtcp_sock_t sock;

  sock.sd = socket(domain, type, protocol);
  sock.state = LISTEN;
  sock.init_win_size = MICROTCP_WIN_SIZE;
  sock.curr_win_size = MICROTCP_WIN_SIZE;
  sock.recvbuf = NULL;
  sock.buf_fill_level = 0;
  sock.cwnd = MICROTCP_INIT_CWND;
  sock.ssthresh = MICROTCP_INIT_SSTHRESH;
  sock.seq_number = 0;
  sock.ack_number = 0;

  sock.packets_send = 0;
  sock.packets_received = 0;
  sock.packets_lost = 0;

  sock.packets_send = 0;
  sock.packets_received = 0;
  sock.packets_lost = 0;

  return sock;
}

int
microtcp_bind (microtcp_sock_t *socket, const struct sockaddr *address,
               socklen_t address_len)
{
  return bind(socket->sd, address, address_len);
}

int
microtcp_connect (microtcp_sock_t *socket, const struct sockaddr *address,
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

  if (hfrom.control != SYN | ACK || hfrom.ack_number !=  hto.seq_number+1)
    return -1;

  hto.seq_number = hfrom.ack_number;
  hto.ack_number = hfrom.seq_number+1;
  hto.control = ACK;
  hto.window = socket->curr_win_size;
  hto.window = 0;
  hto.checksum = 0;

  sendto(socket->sd, &hto, sizeof(hto), MSG_CONFIRM, address, address_len);

  return 0;
}

int
microtcp_accept (microtcp_sock_t *socket, struct sockaddr *address,
                 socklen_t address_len)
{
  /* Your code here */
}

int
microtcp_shutdown (microtcp_sock_t *socket, int how)
{
  /* Your code here */
}

ssize_t
microtcp_send (microtcp_sock_t *socket, const void *buffer, size_t length,
               int flags)
{
  /* Your code here */
}

ssize_t
microtcp_recv (microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
  /* Your code here */
}
