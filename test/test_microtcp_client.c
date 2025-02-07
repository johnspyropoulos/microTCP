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

/*
 * You can use this file to write a test microTCP client.
 * This file is already inserted at the build system.
 */

/**
 * CS335 - Project Phase A
 * 
 * Ioannis Spyropoulos - csd5072
 * Georgios Evangelinos - csd4624
 * Niki Psoma - csd5038
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../lib/microtcp.h"

#define PORT 54321

int
main(int argc, char **argv)
{
    struct sockaddr_in servaddr;

    microtcp_sock_t tcpsocket = microtcp_socket(AF_INET, SOCK_DGRAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("Attemting to connect...\n");
    microtcp_connect(&tcpsocket, (const struct sockaddr*) &servaddr, sizeof(servaddr));
    printf("Connected\n");

    char sbuff[1024];
    char rbuff[1024];
    do
    {
        printf("To server: ");
        fgets(sbuff, 1024, stdin);
        microtcp_send(&tcpsocket, sbuff, strlen(sbuff), NO_FLAGS_BITS);
        microtcp_recv(&tcpsocket, rbuff, 1024, NO_FLAGS_BITS);
        printf("From server: %s\n", rbuff);
    } while (strcmp(sbuff, "exit\n") != 0);

    printf("Closing connection...\n");
    microtcp_shutdown(&tcpsocket, SHUT_RDWR);
    printf("Connection closed\n");
    
    return EXIT_SUCCESS;
}
