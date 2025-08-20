/* forking.c: Forking HTTP Server */

#include "mainServer.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

#include <unistd.h>


/**
 * Fork incoming HTTP requests to handle the concurrently.
 *
 * The parent should accept a request and then fork off and let the child
 * handle the request.
 **/
void
forking_server(int sfd)
{
    struct request *request;
    pid_t pid;

    /* Accept and handle HTTP request */
    while (true) {
    	/* Accept request */
        request = accept_request(sfd);
        if (!request) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

	/* Ignore children */
        signal(SIGCHLD, SIG_IGN);

	/* Fork off child process to handle request */
        pid = fork();
        if (pid < 0) {
            // Fork failed: send 500 and clean up in parent 
            handle_error(request, HTTP_STATUS_INTERNAL_SERVER_ERROR);
            free_request(request);
            continue;
        } else if (pid == 0) {
            // Child handles the request 
            handle_request(request);
            free_request(request);
            _exit(0);
        } else {
            // Parent: close its copy and continue accepting 
            free_request(request);
        }
    }

    /* Close server socket and exit*/
    close(sfd);
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
