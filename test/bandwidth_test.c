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
#include <time.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../lib/microtcp.h"

#define CHUNK_SIZE 4096

#define UPDATE_TIME 0.5f

int server_tcp(uint16_t listen_port, const char* file_path)
{
    uint8_t* recvbuf = malloc(CHUNK_SIZE);
    if (recvbuf == NULL)
    {
        fprintf(stderr, "Server receive buffer allocation failed\n");
        return EXIT_FAILURE;
    }

    FILE* file = fopen(file_path, "w");
    if (file == NULL)
    {
        fprintf(stderr, "File %s could not be created.\n", file_path);
        free(recvbuf);
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(struct sockaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(listen_port);

    if (bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0)
    {
        fprintf(stderr, "TCP server bind failed.\n");
        return EXIT_FAILURE;
    }

    if (listen(sockfd, 1) != 0)
    {
        fprintf(stderr, "TCP server listen failed.\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in cliaddr;
    socklen_t cliaddr_len = sizeof(cliaddr);

    int connfd = accept(sockfd, (struct sockaddr*) &cliaddr, &cliaddr_len);
    if (connfd < 0)
    {
        fprintf(stderr, "TCP server accept failed.\n");
        return EXIT_FAILURE;
    }

    printf("Receiving file...\n\n");

    size_t total_bytes = 0;
    size_t seq_bytes = 0;

    struct timespec start_time;
    struct timespec end_time;

    struct timespec seq_start;

    ssize_t recv_bytes;
    
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
    while ((recv_bytes = recv(connfd, recvbuf, CHUNK_SIZE, 0)) > 0)
    {
        struct timespec seq_now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &seq_now);
        double seq_time = seq_now.tv_sec - seq_start.tv_sec + (seq_now.tv_nsec - seq_start.tv_nsec) * 1e-9;
        if (seq_time >= UPDATE_TIME)
        {
            double seq_mb = seq_bytes / (1024.0 * 1024.0);
            printf("\rDownload speed: %.2f MB/s", seq_mb / seq_time);
            fflush(stdout);

            seq_bytes = 0;
            clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
        }

        ssize_t written = fwrite(recvbuf, sizeof(uint8_t), recv_bytes, file);
        total_bytes += recv_bytes;
        seq_bytes += recv_bytes;

        if (written != recv_bytes)
        {
            printf("Failed to write to the file the amount of data received from the network.\n");
            shutdown(connfd, SHUT_RDWR);
            close(connfd);
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            free(recvbuf);
            fclose(file);
            return EXIT_FAILURE;
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

    printf("\n\nFile download complete: %lu bytes received\n", total_bytes);

    double total_time = end_time.tv_sec - start_time.tv_sec + (end_time.tv_nsec - start_time.tv_nsec) * 1e-9;
    double total_mb = total_bytes / (1024.0 * 1024.0);
    printf("Time elapsed: %0.1f seconds\n", total_time);
    printf("Average download speed: %.2f MB/s\n", total_mb / total_time);

    free(recvbuf);

    shutdown(connfd, SHUT_RDWR);
    close(connfd);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);

    fclose(file);

    return EXIT_SUCCESS;
}

int client_tcp(const char* server_ip, uint16_t server_port, const char* file_path)
{
    FILE* file = fopen(file_path, "r");
    if (file == NULL)
    {
        fprintf(stderr, "File %s was not found.\n", file_path);
        return EXIT_FAILURE;
    }

    uint8_t* sendbuf = malloc(CHUNK_SIZE);
    if (sendbuf == NULL)
    {
        fprintf(stderr, "Client send buffer allocation failed.\n");
        return EXIT_FAILURE;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(struct sockaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    servaddr.sin_port = htons(server_port);

    if (connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0)
    {
        fprintf(stderr, "TCP client connect failed.\n");
        return EXIT_FAILURE;
    }

    printf("Sending file...\n\n");

    struct timespec start_time;
    struct timespec end_time;
    
    size_t total_bytes = 0;
    size_t seq_bytes = 0;
    
    struct timespec seq_start;
    clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    while (!feof(file))
    {
        struct timespec seq_now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &seq_now);
        double seq_time = seq_now.tv_sec - seq_start.tv_sec + (seq_now.tv_nsec - seq_start.tv_nsec) * 1e-9;
        if (seq_time >= UPDATE_TIME)
        {
            double seq_mb = seq_bytes / (1024.0 * 1024.0);
            printf("\rUpload speed: %.2f MB/s", seq_mb / seq_time);
            fflush(stdout);

            seq_bytes = 0;
            clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
        }

        ssize_t read_bytes = fread(sendbuf, sizeof(uint8_t), CHUNK_SIZE, file);
        if (read_bytes < 1)
        {
            fprintf(stderr, "File reading failed.\n");
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            free(sendbuf);
            fclose(file);
            return EXIT_FAILURE;
        }

        ssize_t data_sent = send(sockfd, sendbuf, read_bytes, 0);
        if (data_sent != read_bytes)
        {
            fprintf(stderr, "Failed to send the amount of data read from the file\n");
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            free(sendbuf);
            fclose(file);
            return EXIT_FAILURE;
        }

        total_bytes += data_sent;
        seq_bytes += data_sent;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

    printf("\n\nFile sent\n");

    double total_time = end_time.tv_sec - start_time.tv_sec + (end_time.tv_nsec - start_time.tv_nsec) * 1e-9;
    double total_mb = total_bytes / (1024.0 * 1024.0);
    printf("Time elapsed: %0.1f seconds\n", total_time);
    printf("Average upload speed: %.2f MB/s\n", total_mb / total_time);

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    free(sendbuf);
    fclose(file);

    return EXIT_SUCCESS;
}

int server_microtcp(uint16_t listen_port, const char* file_path)
{
    uint8_t* recvbuf = malloc(CHUNK_SIZE);
    if (recvbuf == NULL)
    {
        fprintf(stderr, "Server receive buffer allocation failed\n");
        return EXIT_FAILURE;
    }

    FILE* file = fopen(file_path, "w");
    if (file == NULL)
    {
        fprintf(stderr, "File %s could not be created.\n", file_path);
        free(recvbuf);
        return EXIT_FAILURE;
    }

    microtcp_sock_t socket = microtcp_socket(AF_INET, SOCK_DGRAM, 0);

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(struct sockaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(listen_port);

    if (microtcp_bind(&socket, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0)
    {
        fprintf(stderr, "MicroTCP server bind failed.\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in cliaddr;
    socklen_t cliaddr_len = sizeof(cliaddr);

    if (microtcp_accept(&socket, (struct sockaddr*) &cliaddr, cliaddr_len) != 0)
    {
        fprintf(stderr, "MicroTCP server accept failed.\n");
        return EXIT_FAILURE;
    }

    printf("Receiving file...\n\n");

    size_t total_bytes = 0;
    size_t seq_bytes = 0;

    struct timespec start_time;
    struct timespec end_time;

    struct timespec seq_start;

    ssize_t recv_bytes;
    
    clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    while ((recv_bytes = microtcp_recv(&socket, recvbuf, CHUNK_SIZE, 0)) > 0)
    {
        struct timespec seq_now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &seq_now);
        double seq_time = seq_now.tv_sec - seq_start.tv_sec + (seq_now.tv_nsec - seq_start.tv_nsec) * 1e-9;
        if (seq_time >= UPDATE_TIME)
        {
            double seq_mb = seq_bytes / (1024.0 * 1024.0);
            printf("\rDownload speed: %.2f MB/s", seq_mb / seq_time);
            fflush(stdout);

            seq_bytes = 0;
            clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
        }

        ssize_t written = fwrite(recvbuf, sizeof(uint8_t), recv_bytes, file);
        total_bytes += recv_bytes;
        seq_bytes += recv_bytes;

        if (written != recv_bytes)
        {
            printf("\nFailed to write to the file the amount of data received from the network.\n");
            free(recvbuf);
            fclose(file);
            return EXIT_FAILURE;
        }
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

    printf("\n\nFile download complete: %lu bytes received\n", total_bytes);

    double total_time = end_time.tv_sec - start_time.tv_sec + (end_time.tv_nsec - start_time.tv_nsec) * 1e-9;
    double total_mb = total_bytes / (1024.0 * 1024.0);
    printf("Time elapsed: %0.1f seconds\n", total_time);
    printf("Average download speed: %.2f MB/s\n", total_mb / total_time);

    free(recvbuf);

    if (socket.state == CLOSING_BY_PEER)
        microtcp_shutdown(&socket, SHUT_RDWR);

    fclose(file);

    return EXIT_SUCCESS;
}

int client_microtcp(const char* server_ip, uint16_t server_port, const char* file_path)
{
    FILE* file = fopen(file_path, "r");
    if (file == NULL)
    {
        fprintf(stderr, "File %s was not found.\n", file_path);
        return EXIT_FAILURE;
    }

    uint8_t* sendbuf = malloc(CHUNK_SIZE);
    if (sendbuf == NULL)
    {
        fprintf(stderr, "Client send buffer allocation failed.\n");
        return EXIT_FAILURE;
    }

    microtcp_sock_t socket = microtcp_socket(AF_INET, SOCK_DGRAM, 0);
    
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(struct sockaddr));
    
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(server_ip);
    servaddr.sin_port = htons(server_port);

    if (microtcp_connect(&socket, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0)
    {
        fprintf(stderr, "MicroTCP client connect failed.\n");
        return EXIT_FAILURE;
    }

    printf("Sending file...\n\n");

    struct timespec start_time;
    struct timespec end_time;
    
    size_t total_bytes = 0;
    size_t seq_bytes = 0;
    
    struct timespec seq_start;
    clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
    clock_gettime(CLOCK_MONOTONIC_RAW, &start_time);
    while (!feof(file))
    {
        struct timespec seq_now;
        clock_gettime(CLOCK_MONOTONIC_RAW, &seq_now);
        double seq_time = seq_now.tv_sec - seq_start.tv_sec + (seq_now.tv_nsec - seq_start.tv_nsec) * 1e-9;
        if (seq_time >= UPDATE_TIME)
        {
            double seq_mb = seq_bytes / (1024.0 * 1024.0);
            printf("\rUpload speed: %.2f MB/s", seq_mb / seq_time);
            fflush(stdout);

            seq_bytes = 0;
            clock_gettime(CLOCK_MONOTONIC_RAW, &seq_start);
        }

        ssize_t read_bytes = fread(sendbuf, sizeof(uint8_t), CHUNK_SIZE, file);
        if (read_bytes < 1)
        {
            fprintf(stderr, "\nFile reading failed.\n");
            microtcp_shutdown(&socket, SHUT_RDWR);
            free(sendbuf);
            fclose(file);
            return EXIT_FAILURE;
        }

        ssize_t data_sent = microtcp_send(&socket, sendbuf, read_bytes, 0);
        if (data_sent != read_bytes)
        {
            fprintf(stderr, "\nFailed to send the amount of data read from the file\n");
            microtcp_shutdown(&socket, SHUT_RDWR);
            free(sendbuf);
            fclose(file);
            return EXIT_FAILURE;
        }

        total_bytes += data_sent;
        seq_bytes += data_sent;
    }
    clock_gettime(CLOCK_MONOTONIC_RAW, &end_time);

    printf("\n\nFile sent\n");

    double total_time = end_time.tv_sec - start_time.tv_sec + (end_time.tv_nsec - start_time.tv_nsec) * 1e-9;
    double total_mb = total_bytes / (1024.0 * 1024.0);
    printf("Time elapsed: %0.1f seconds\n", total_time);
    printf("Average upload speed: %.2f MB/s\n", total_mb / total_time);

    microtcp_shutdown(&socket, SHUT_RDWR);
    free(sendbuf);
    fclose(file);

    return EXIT_SUCCESS;
}

void print_help()
{
    printf(
    "Usage: bandwidth_test [-s] [-m] -p port -f file\n"
    "Options:\n"
    "   -s                  If set, the program runs as server. Otherwise as client.\n"
    "   -m                  If set, the program uses the microTCP implementation. Otherwise the normal TCP.\n"
    "   -f <string>         If -s is set the -f option specifies the filename of the file that will be saved.\n"
    "                       If not, is the source file at the client side that will be sent to the server.\n"
    "   -p <int>            The listening port of the server\n"
    "   -a <string>         The IP address of the server. This option is ignored if the tool runs in server mode.\n"
    "   -h                  prints this help\n"
    );
}

int is_strnum(char* str)
{
    char* p = str;
    while (*p != '\0')
    {
        if (*p < '0' || *p > '9')
            return 0;

        p++;
    }

    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        print_help();
        return EXIT_FAILURE;
    }

    int opt;
    int port;
    int exit_code = 0;
    char *filestr = NULL;
    char *ipstr = NULL;
    uint8_t is_server = 0;
    uint8_t use_microtcp = 0;
    while ((opt = getopt(argc, argv, "hsmf:p:a:")) != -1)
    {
        switch (opt)
        {
        /* If -s is set, program runs on server mode */
        case 's':
            is_server = 1;
            break;
            /* if -m is set the program should use the microTCP implementation */
        case 'm':
            use_microtcp = 1;
            break;
        case 'f':
            filestr = strdup(optarg);
            break;
        case 'p':
            port = atoi(optarg);

            if (!is_strnum(optarg) || port < 0 || port > 65535)
            {
                fprintf(stderr, "Invalid port given\n");
                return EXIT_FAILURE;
            }

            break;
        case 'a':
            ipstr = strdup(optarg);
            break;
        default:
            print_help();
            return EXIT_SUCCESS;
        }
    }

    if (filestr == NULL)
    {
        print_help();
        return EXIT_FAILURE;
    }

    if (is_server)
    {
        if (!use_microtcp)
            return server_tcp(port, filestr);
        else
            return server_microtcp(port, filestr);
    }
    else
    {
        if (!use_microtcp)
            return client_tcp(ipstr, port, filestr);
        else
            return client_microtcp(ipstr, port, filestr);
            
    }
    
    return EXIT_SUCCESS;
}