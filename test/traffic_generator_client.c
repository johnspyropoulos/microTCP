/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2017  Manolis Surligas <surligas@gmail.com>
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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>

#include "../lib/microtcp.h"
#include "../utils/log.h"

#define BUFF_SIZE 4096

static char running = 1;

static void
sig_handler(int signal)
{
  if(signal == SIGINT) {
    LOG_INFO("Stopping traffic generator client...");
    running = 0;
  }
}

int
main(int argc, char **argv) {
  if (argc < 3)
  {
    printf("Usage: traffic_generator_client ip port\n");
    exit(EXIT_FAILURE);
  }

  uint16_t port = atoi(argv[2]);

  struct sockaddr_in servaddr;
  bzero(&servaddr, sizeof(struct sockaddr));
  
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = inet_addr(argv[1]);
  servaddr.sin_port = htons(port);

  microtcp_sock_t socket = microtcp_socket(AF_INET, SOCK_DGRAM, 0);

  microtcp_connect(&socket, (struct sockaddr*) &servaddr, sizeof(servaddr));

  uint8_t buffer[BUFF_SIZE];

  /*
   * Register a signal handler so we can terminate the client with
   * Ctrl+C
   */
  signal(SIGINT, sig_handler);

  LOG_INFO("Start receiving traffic from port %u\n", port);
  while(running) {
    struct timespec start_time;
    struct timespec end_time;

    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    ssize_t bytes_read = microtcp_recv(&socket, buffer, BUFF_SIZE, 0);
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

    if (bytes_read < 0)
    {
      fprintf(stderr, "microtcp_recv failed\n");
      exit(EXIT_FAILURE);
    }

    double time_elapsed = end_time.tv_sec - start_time.tv_sec + (end_time.tv_nsec - start_time.tv_nsec) * 1e-9;
    printf("\rReceived %ld bytes in %lf seconds", bytes_read, time_elapsed);
    fflush(stdout);
  }

  /* Ctrl+C pressed! Store properly time measurements for plotting */
}

