#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "qkd_c_api.h"

#define KEYSIZE 8
#define BUFSIZE 1024

void error(char *msg) {
    perror(msg);
    exit(1);
}

uint32_t QKD_OPEN(uint32_t destination, qos_t qos, key_handle_t* key_handle) {
    printf("-> Init key_handle: %s\n", *key_handle);

    if (strcmp(*key_handle, "init_value") == 0) {
        for (size_t i = 0; i < KEYHANDLE_SIZE; i++) {
            (*key_handle)[i] = (uint8_t) rand() % 256;
        }
    } else {
        strcpy(*key_handle, "specified_key_handle");
    }

    return 0;
}

uint32_t QKD_CONNECT(key_handle_t* key_handle, uint32_t timeout) {
    printf("-> Final key_handle: %s\n", *key_handle);

    return 0;
}

uint32_t QKD_GET_KEY(key_handle_t* key_handle, uint8_t* key_buffer) {
    const char* hostname = "localhost";
    const char* portname = "8080";
    char buf[BUFSIZE];
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_ADDRCONFIG;
    struct addrinfo* res = 0;
    int err = getaddrinfo(hostname, portname, &hints, &res);
    if (err != 0) {
        error("ERROR with getaddrinfo");
    }

    int sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sd == -1) {
        error("ERROR opening socket");
    }

    int reuseaddr = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr)) == -1) {
        error("ERROR with setsockopt");
    }

    if (bind(sd, res->ai_addr, res->ai_addrlen) == -1) {
        /************
         *  CLIENT  *
         ************/
        printf("ERROR on binding -> it's a CLIENT\n");
        if (connect(sd, res->ai_addr, res->ai_addrlen) < 0) {
            error("ERROR connecting");
        }

        /* Send key_handle first */
        int n = write(sd, key_handle, KEYHANDLE_SIZE * sizeof(uint8_t));
        if (n < 0) {
            error("ERROR writing to socket");
        }
        puts("-> client requested the server to generate key");

        /* Check whether key_handle was accepted */
        n = read(sd, buf, sizeof(uint8_t));
        if (n < 0) {
            error("ERROR reading from socket");
        }

        if (strncmp(buf, "1", 1) == 0) {
            /* key_handle is correct */
            n = read(sd, key_buffer, KEYSIZE * sizeof(uint8_t));
            if (n < 0) {
                error("ERROR reading from socket");
            }
            freeaddrinfo(res);
            close(sd);
            return 0;
        } else {
            /* key_handle is different from what was expected */
            printf("--> WRONG key_handle\n");
            freeaddrinfo(res);
            close(sd);
            return -1;
        }
    } else {
        /************
         *  SERVER  *
         ************/
        printf("SUCCESSFUL binding -> it's a SERVER\n");
        if (listen(sd, SOMAXCONN)) {
            error("FAILED to listen for connections");
        }
        int session_fd = accept(sd, 0, 0);

        /* Check whether key_handle is correct */
        int n = read(session_fd, buf, KEYHANDLE_SIZE * sizeof(uint8_t));
        if (n < 0) {
            error("ERROR reading from socket");
        }

        if (strncmp(*key_handle, buf, KEYHANDLE_SIZE) != 0) {
            /* key_handle is different from what was expected */
            printf("--> WRONG key_handle\n");
            int n = write(session_fd, "0", sizeof(uint8_t));
            if (n < 0) {
                error("ERROR writing to socket");
            }
            freeaddrinfo(res);
            close(sd);
            close(session_fd);
            return -1;
        } else {
            /* key_handle is correct */
            strcpy(buf, "1");
            int n = write(session_fd, buf, sizeof(uint8_t));
            if (n < 0) {
                error("ERROR writing to socket");
            }

            /* Generating of the key */
            puts("-> key generated on the server");
            for (size_t i = 0; i < KEYSIZE; i++) {
                key_buffer[i] = (uint8_t) rand() % 256;
            }
            printf("-> key sent: %s\n", key_buffer);

            n = write(session_fd, key_buffer, KEYSIZE * sizeof(uint8_t));
            if (n < 0) {
                error("ERROR writing to socket");
            }
            close(session_fd);
            freeaddrinfo(res);
            close(sd);
            return 0;
        }
    }
}

uint32_t QKD_CLOSE(key_handle_t* key_handle) {

    return 0;
}
