/* spidey: Simple HTTP Server */

#include "mainServer.h"
#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <unistd.h>

/* Global Variables */
char *Port	      = "9898";
char *MimeTypesPath   = "/etc/mime.types";
char *DefaultMimeType = "text/plain";
char *RootPath	      = "www";
mode  ConcurrencyMode = SINGLE;

/**
 * Display usage message.
 */
void
usage(const char *progname, int status)
{
    fprintf(stderr, "Usage: %s [hcmMpr]\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "    -h            Display help message\n");
    fprintf(stderr, "    -c mode       Single or Forking mode\n");
    fprintf(stderr, "    -m path       Path to mimetypes file\n");
    fprintf(stderr, "    -M mimetype   Default mimetype\n");
    fprintf(stderr, "    -p port       Port to listen on\n");
    fprintf(stderr, "    -r path       Root directory\n");
    exit(status);
}

/**
 * Parses command line options and starts appropriate server
 **/
int
main(int argc, char *argv[])
{
    /* Parse command line options */
    for (int c = 1; c < argc; c++) {
        if (strcmp(argv[c], "-h") == 0) {
            usage(argv[0], EXIT_SUCCESS);

        } else if (strcmp(argv[c], "-c") == 0) {
            if (++c >= argc) usage(argv[0], EXIT_FAILURE);
            if (strcmp(argv[c], "single") == 0) {
                ConcurrencyMode = SINGLE;
            } else if (strcmp(argv[c], "forking") == 0) {
                ConcurrencyMode = FORKING;
            } else {
                usage(argv[0], EXIT_FAILURE);
            }

        } else if (strcmp(argv[c], "-m") == 0) {
            if (++c >= argc) usage(argv[0], EXIT_FAILURE);
            MimeTypesPath = argv[c];

        } else if (strcmp(argv[c], "-M") == 0) {
            if (++c >= argc) usage(argv[0], EXIT_FAILURE);
            DefaultMimeType = argv[c];

        } else if (strcmp(argv[c], "-p") == 0) {
            if (++c >= argc) usage(argv[0], EXIT_FAILURE);
            Port = argv[c];

        } else if (strcmp(argv[c], "-r") == 0) {
            if (++c >= argc) usage(argv[0], EXIT_FAILURE);
            RootPath = argv[c];

        } else {
            usage(argv[0], EXIT_FAILURE);
        }
    }

    /* Listen to server socket */
    int sock_fd = socket_listen(Port);
    if (sock_fd < 0) {
        return EXIT_FAILURE;
    }

    /* Determine real RootPath */
    char *real = realpath(RootPath, NULL);
    if (real) {
        RootPath = real;
    }



    log("Listening on port %s", Port);
    debug("RootPath        = %s", RootPath);
    debug("MimeTypesPath   = %s", MimeTypesPath);
    debug("DefaultMimeType = %s", DefaultMimeType);
    debug("ConcurrencyMode = %s", ConcurrencyMode == SINGLE ? "Single" : "Forking");

    /* Start either forking or single HTTP server */

    if (ConcurrencyMode == FORKING){
        forking_server(sock_fd);
    } else {
        single_server(sock_fd);
    }

    return EXIT_SUCCESS;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
