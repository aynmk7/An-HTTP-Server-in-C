/* request.c: HTTP Request Functions */
#include "mainServer.h"

#include <errno.h>
#include <string.h>

#include <unistd.h>

int parse_request_method(struct request *r);
int parse_request_headers(struct request *r);

/**
 * Accept request from server socket.
 *
 * This function does the following:
 *
 *  1. Allocates a request struct initialized to 0.
 *  2. Initializes the headers list in the request struct.
 *  3. Accepts a client connection from the server socket.
 *  4. Looks up the client information and stores it in the request struct.
 *  5. Opens the client socket stream for the request struct.
 *  6. Returns the request struct.
 *
 * The returned request struct must be deallocated using free_request.
 **/
struct request *
accept_request(int sfd)
{
    struct request *r;
    struct sockaddr raddr;
    socklen_t rlen;

    /* Allocate request struct (zeroed) */
    r = calloc(1, sizeof(*r));
    if (!r) {
        return NULL;
    }
    r->fd = -1;
    r->file = NULL;
    r->headers = NULL;

    /* Accept a client */
    rlen = sizeof(raddr);
    r->fd = accept(sfd, &raddr, &rlen);
    if (r->fd < 0) {
        goto fail;
    }

    /* Lookup client information */
    if (getnameinfo(&raddr, rlen, r->host, NI_MAXHOST, r->port, NI_MAXHOST, NI_NUMERICHOST | NI_NUMERICSERV) != 0) {
        r->host[0] = '\0';
        r->port[0] = '\0';
    }

    /* Open socket stream */
    r->file = fdopen(r->fd, "r+");
    if (!r->file) {
        goto fail;
    }


    log("Accepted request from %s:%s", r->host, r->port);
    return r;

fail:
    free_request(r);
    return NULL;
}

/**
 * Deallocate request struct.
 *
 * This function does the following:
 *
 *  1. Closes the request socket stream or file descriptor.
 *  2. Frees all allocated strings in request struct.
 *  3. Frees all of the headers (including any allocated fields).
 *  4. Frees request struct.
 **/
void
free_request(struct request *r)
{
    struct header *header;

    if (r == NULL) {
    	return;
    }

    /* Close socket or fd */
    if (r->file) {
        fclose(r->file);
    } else if (r->fd >= 0) {
        close(r->fd);
    }

    /* Free allocated strings */
    free(r->method);
    free(r->uri);
    free(r->path);
    free(r->query);

    /* Free headers */
    header = r->headers;
    while (header) {
        struct header *next = header->next;
        free(header->name);
        free(header->value);
        free(header);
        header = next;
    }

    /* Free request */
    free(r);
}

/**
 * Parse HTTP Request.
 *
 * This function first parses the request method, any query, and then the
 * headers, returning 0 on success, and -1 on error.
 **/
int
parse_request(struct request *r)
{
    /* Parse HTTP Request Method */
    if (parse_request_method(r) < 0) {
        return -1;
    }

    if (r->uri) {
        char *qmark = strchr(r->uri, '?');
        if (qmark) {
            r->path = strndup(r->uri, (size_t)(qmark - r->uri));
            r->query = strdup(qmark+1);
            if (!r->path || !r->query) {
                return -1;
            }
        } else {
            r->path = strdup(r->uri);
            if (!r->path) {
                return -1;
            }

        }
    }

    /* Parse HTTP Requet Headers*/

    if (parse_request_headers(r) < 0) {
        return -1;
    }

    return 0;
}

/**
 * Parse HTTP Request Method and URI
 *
 * HTTP Requests come in the form
 *
 *  <METHOD> <URI>[QUERY] HTTP/<VERSION>
 *
 * Examples:
 *
 *  GET / HTTP/1.1
 *  GET /cgi.script?q=foo HTTP/1.0
 *
 * This function extracts the method, uri, and query (if it exists).
 **/
int
parse_request_method(struct request *r)
{
    char line[BUFSIZ];
    char method[BUFSIZ], uri[BUFSIZ], version[BUFSIZ];

    /* Read line from socket */
    if (!fgets(line, sizeof(line), r->file)) {
        goto fail;
    }

    // Trim CRLF 
    char *crlf = strstr(line, "\r\n");
    if (crlf) *crlf = '\0';
    else {
        char *lf = strchr(line, '\n');
        if (lf) *lf = '\0';
    }

    /* Parse method and uri */
    if (sscanf(line, "%s %s %s", method, uri, version) != 3) {
        goto fail;
    }

    /* Parse query from uri */
    char *qmark = strchr(uri, '?');
    char *query_str = NULL;
    if (qmark) {
        *qmark = '\0';                 // keep only path in uri 
        query_str = qmark + 1;         // after '?' 
    }

    /* Record method, uri, and query in request struct */
    r->method = strdup(method);
    r->uri    = strdup(uri);
    r->query  = strdup(query_str ? query_str : "");

    if (!r->method || !r->uri || !r->query) {
        goto fail;
    }

    debug("HTTP METHOD: %s", r->method);
    debug("HTTP URI:    %s", r->uri);
    debug("HTTP QUERY:  %s", r->query);

    return 0;

fail:
    return -1;
}

/**
 * Parse HTTP Request Headers
 *
 * HTTP Headers come in the form:
 *
 *  <NAME>: <VALUE>
 *
 * Example:
 *
 *  Host: localhost:8888
 *  User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:29.0) Gecko/20100101 Firefox/29.0
 *  Accept: text/html,application/xhtml+xml
 *  Accept-Language: en-US,en;q=0.5
 *  Accept-Encoding: gzip, deflate
 *  Connection: keep-alive
 *
 * This function parses the stream from the request socket using the following
 * pseudo-code:
 *
 *  while (buffer = read_from_socket() and buffer is not empty):
 *      name, value = buffer.split(':')
 *      header      = new Header(name, value)
 *      headers.append(header)
 **/
int
parse_request_headers(struct request *r)
{
    struct header *curr = NULL;
    char buffer[BUFSIZ];
    char *name;
    char *value;
    
    /* Parse headers from socket */
    while (fgets(buffer, sizeof(buffer), r->file)) {
        size_t len = strlen(buffer);

        // Strip trailing CRLF 
        while (len > 0 && (buffer[len - 1] == '\n' || buffer[len - 1] == '\r')) {
            buffer[--len] = '\0';
        }

        // Empty line marks end of headers 
        if (len == 0) {
            break;
        }

        // Split on first ':' 
        char *colon = strchr(buffer, ':');
        if (!colon) {
            // Malformed header: ignore this line 
            continue;
        }

        *colon = '\0';
        name  = buffer;
        value = colon + 1;

        // Trim leading/trailing whitespace from name 
        while (*name == ' ' || *name == '\t') name++;
        char *end = name + strlen(name);
        while (end > name && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';

        // Trim leading/trailing whitespace from value 
        while (*value == ' ' || *value == '\t') value++;
        end = value + strlen(value);
        while (end > value && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';

        // Allocate header node 
        struct header *h = calloc(1, sizeof(*h));
        if (!h) {
            goto fail;
        }
        h->name  = strdup(name);
        h->value = strdup(value);
        if (!h->name || !h->value) {
            if (h->name)  free(h->name);
            if (h->value) free(h->value);
            free(h);
            goto fail;
        }
        h->next = NULL;

        // Append to list
        if (r->headers == NULL) {
            r->headers = h;
            curr = h;
        } else {
            curr->next = h;
            curr = h;
        }
    }



#ifndef NDEBUG
    for (struct header *header = r->headers; header != NULL; header = header->next) {
    	debug("HTTP HEADER %s = %s", header->name, header->value);
    }
#endif
    return 0;

fail:
    return -1;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
