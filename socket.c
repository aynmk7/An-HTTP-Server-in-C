/* socket.c: Simple Socket Functions */
#include "mainServer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/**
 * Allocate socket, bind it, and listen to specified port.
 **/
int
socket_listen(const char *port)
{
    struct addrinfo  hints;
    struct addrinfo *results;
    int    socket_fd = -1;

    /* Lookup server address information */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; // ipv4 or ipv6
    hints.ai_socktype = SOCK_STREAM; // tcp
    hints.ai_flags = AI_PASSIVE; // 0.0.0.0 for bind

    if (getaddrinfo(NULL, port, &hints, &results) != 0)
        return -1;
    

    /* For each server entry, allocate socket and try to connect */
    for (struct addrinfo *p = results; p != NULL && socket_fd < 0; p = p->ai_next) {
	/* Allocate socket */
        socket_fd = socket(p->ai_family,p->ai_socktype, p->ai_protocol);
        if (socket_fd<0)
            continue;

	/* Bind socket */
        if (bind(socket_fd, p->ai_addr, p->ai_addrlen) < 0) {
            close(socket_fd);
            socket_fd = -1;
            continue;
        }

    	/* Listen to socket */
        if (listen(socket_fd, SOMAXCONN) == -1){
            close(socket_fd);
            socket_fd = -1;
            continue;
        }
    }

    freeaddrinfo(results);
    return socket_fd;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
