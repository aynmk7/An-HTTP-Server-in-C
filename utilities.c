/* utils.c: spidey utilities */
#include "mainServer.h"

#include <ctype.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

/**
 * Determine mime-type from file extension
 *
 * This function first finds the file's extension and then scans the contents
 * of the MimeTypesPath file to determine which mimetype the file has.
 *
 * The MimeTypesPath file (typically /etc/mime.types) consists of rules in the
 * following format:
 *
 *  <MIMETYPE>      <EXT1> <EXT2> ...
 *
 * This function simply checks the file extension version each extension for
 * each mimetype and returns the mimetype on the first match.
 *
 * If no extension exists or no matching mimetype is found, then return
 * DefaultMimeType.
 *
 * This function returns an allocated string that must be free'd.
 **/
char *
determine_mimetype(const char *path)
{
    char *ext;
    char *mimetype;
    char *token;
    char buffer[BUFSIZ];
    FILE *fs = NULL;
    
    /* Find file extension */
    ext = strrchr(path, '.');          // last '.' 
    if (!ext || !*(ext + 1)) {
        goto fail;                     // no extension 
    }
    ext++;                             // skip '.' 

    /* Open MimeTypesPath file */
    fs = fopen(MimeTypesPath, "r");
    if (!fs) {
        goto fail;
    }

    /* Scan file for matching file extensions */
    while (fgets(buffer, sizeof(buffer), fs)) {
        char *p = buffer;

        // skip leading whitespace 
        while (*p && isspace((unsigned char)*p)) p++;

        // ignore comments/blank lines 
        if (*p == '#' || *p == '\0' || *p == '\n')
            continue;

        // first token is mimetype; rest are extensions 
        char *mt = strtok(p, " \t\r\n");
        if (!mt) continue;

        while ((token = strtok(NULL, " \t\r\n")) != NULL) {
            // case-insensitive compare: ext vs token 
            const char *e = ext;
            const char *t = token;
            int same = 1;
            while (*e && *t) {
                if (tolower((unsigned char)*e) != tolower((unsigned char)*t)) {
                    same = 0; break;
                }
                e++; t++;
            }
            if (same && *e == '\0' && *t == '\0') {
                mimetype = mt;         // point into buffer; we'll strdup below 
                goto done;
            }
        }
    }
   
fail:
    mimetype = DefaultMimeType;

done:
    if (fs) {
        fclose(fs);
    }
    return strdup(mimetype);
}

/**
 * Determine actual filesystem path based on RootPath and URI
 *
 * This function uses realpath(3) to generate the realpath of the
 * file requested in the URI.
 *
 * As a security check, if the real path does not begin with the RootPath, then
 * return NULL.
 *
 * Otherwise, return a newly allocated string containing the real path.  This
 * string must later be free'd.
 **/
char *
determine_request_path(const char *uri)
{
    char path[BUFSIZ];
    char real[BUFSIZ];

    /* Build candidate filesystem path: RootPath + '/' + uri (skip leading '/') */
    const char *u = uri ? uri : "/";
    if (*u == '/') u++;  /* avoid double slash when joining */

    if (snprintf(path, sizeof(path), "%s/%s", RootPath, u) >= (int)sizeof(path)) {
        return NULL; /* path would be truncated */
    }

    /* Resolve to realpath */
    if (!realpath(path, real)) {
        return NULL;
    }

    /* Security check: real path must begin with RootPath (as a directory prefix) */
    size_t rootlen = strlen(RootPath);
    if (strncmp(real, RootPath, rootlen) != 0) {
        return NULL;
    }
    /* Ensure we didn't just match a prefix (e.g., /var/www vs /var/www2) */
    if (real[rootlen] != '\0' && real[rootlen] != '/') {
        return NULL;
    }

    return strdup(real);
}

/**
 * Determine request type from path
 *
 * Based on the file specified by path, determine what type of request
 * this is:
 *
 *  1. REQUEST_BROWSE: Path is a directory.
 *  2. REQUEST_CGI:    Path is an executable file.
 *  3. REQUEST_FILE:   Path is a readable file.
 *  4. REQUEST_BAD:    Everything else.
 **/
request_type
determine_request_type(const char *path)
{
    struct stat s;
    request_type type = REQUEST_BAD;

    if (stat(path, &s) < 0) {
        return REQUEST_BAD;
    }

    if (S_ISDIR(s.st_mode)) {
        type = REQUEST_BROWSE;
    } else if (S_ISREG(s.st_mode) && access(path, X_OK) == 0) {
        type = REQUEST_CGI;
    } else if (S_ISREG(s.st_mode) && access(path, R_OK) == 0) {
        type = REQUEST_FILE;
    } else {
        type = REQUEST_BAD;
    }

    return (type);
}

/**
 * Return static string corresponding to HTTP Status code
 *
 * http://en.wikipedia.org/wiki/List_of_HTTP_status_codes
 **/
const char *
http_status_string(http_status status)
{
    const char *status_string;

    switch (status) {
    case HTTP_STATUS_OK:
        status_string = "200 OK";
        break;
    case HTTP_STATUS_BAD_REQUEST:
        status_string = "400 Bad Request";
        break;
    case HTTP_STATUS_NOT_FOUND:
        status_string = "404 Not Found";
        break;
    case HTTP_STATUS_INTERNAL_SERVER_ERROR:
        status_string = "500 Internal Server Error";
        break;
    default:
        status_string = "500 Internal Server Error";
        break;
    }

    return status_string;
}

/**
 * Advance string pointer pass all nonwhitespace characters
 **/
char *
skip_nonwhitespace(char *s)
{
    while (s && *s && !isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/**
 * Advance string pointer pass all whitespace characters
 **/
char *
skip_whitespace(char *s)
{
    while (s && *s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
