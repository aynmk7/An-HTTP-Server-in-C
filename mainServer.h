#ifndef SPIDEY_H
#define SPIDEY_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <unistd.h>

/* Constants */

#define WHITESPACE	" \t\n"

/**
 * Concurrency modes
 */
typedef enum {
    SINGLE,     /**< Single connection */
    FORKING,    /**< Process per connection */
    UNKNOWN
} mode;


/* Global Variables */

extern char *Port;                  /**< Port number */
extern char *MimeTypesPath;         /**< Path to mime.types file */
extern char *DefaultMimeType;       /**< Default file mimetype */
extern char *RootPath;              /**< Path to root directory */

/* Logging Macros */

#ifdef NDEBUG
#define debug(M, ...)
#else
#define debug(M, ...)   fprintf(stderr, "[%5d] DEBUG %10s:%-4d " M "\n", getpid(), __FILE__, __LINE__, ##__VA_ARGS__)
#endif

#define fatal(M, ...)   fprintf(stderr, "[%5d] FATAL %10s:%-4d " M "\n", getpid(), __FILE__, __LINE__, ##__VA_ARGS__); exit(EXIT_FAILURE)
#define log(M, ...)     fprintf(stderr, "[%5d] LOG   %10s:%-4d " M "\n", getpid(), __FILE__, __LINE__, ##__VA_ARGS__)

/* HTTP Request */

struct header {
    char *name;
    char *value;

    struct header *next;
};

struct request {
    int   fd;               /*< Client socket file descripter */
    FILE *file;             /*< Client socket file stream */
    char *method;           /*< HTTP method */
    char *uri;              /*< HTTP uniform resource identifier */
    char *path;             /*< Real path corrsponding to URI and RootPath */
    char *query;            /*< HTTP query string */

    char host[NI_MAXHOST];
    char port[NI_MAXSERV];

    struct header *headers; /*< List of name, value pairs */
};

struct request *    accept_request(int sfd);
void		    free_request(struct request *request);
int		    parse_request(struct request *request);

/* HTTP Request Handlers */

typedef enum {
    REQUEST_BROWSE,
    REQUEST_FILE,
    REQUEST_CGI,
    REQUEST_BAD,
} request_type;

typedef enum {
    HTTP_STATUS_OK,			/* 200 OK */
    HTTP_STATUS_BAD_REQUEST,		/* 400 Bad Request */
    HTTP_STATUS_NOT_FOUND,		/* 404 Not Found */
    HTTP_STATUS_INTERNAL_SERVER_ERROR,	/* 500 Internal Server Error */
} http_status;

http_status	    handle_request(struct request *request);
http_status handle_error(struct request *r, http_status status);

/* HTTP Server */

void		    single_server(int sfd);
void		    forking_server(int sfd);
void		    threaded_server(int sfd);

/* Socket */

int		    socket_listen(const char *port);

/* Utilities */

#define chomp(s)    (s)[strlen(s) - 1] = '\0'
#define streq(a, b) (strcmp((a), (b)) == 0)

char *		    determine_mimetype(const char *path);
char *		    determine_request_path(const char *uri);
request_type	    determine_request_type(const char *path);
const char *        http_status_string(http_status status);
char *		    skip_nonwhitespace(char *s);
char *		    skip_whitespace(char *s);

#endif

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
