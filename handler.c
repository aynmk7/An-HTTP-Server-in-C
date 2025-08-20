/* handler.c: HTTP Request Handlers */

#include "mainServer.h"
#include <errno.h>
#include <limits.h>
#include <string.h>

#include <dirent.h>
#include <unistd.h>

/* Internal Declarations */
http_status handle_browse_request(struct request *request);
http_status handle_file_request(struct request *request);
http_status handle_cgi_request(struct request *request);
http_status handle_error(struct request *request, http_status status);

/**
 * Handle HTTP Request
 *
 * This parses a request, determines the request path, determines the request
 * type, and then dispatches to the appropriate handler type.
 *
 * On error, handle_error should be used with an appropriate HTTP status code.
 **/
http_status
handle_request(struct request *r)
{
    http_status result;

    /* Parse request */
    if (parse_request(r) < 0) {
        return handle_error(r, HTTP_STATUS_BAD_REQUEST);
    }

    /* Determine request path */
    {
        char *real = determine_request_path(r->uri);
        if (!real) {
            return handle_error(r, HTTP_STATUS_NOT_FOUND);
        }
        if (r->path) free(r->path);   /* replace any earlier value */
        r->path = real;
    }   
 
    debug("HTTP REQUEST PATH: %s", r->path);

    /* Dispatch to appropriate request handler type */
    switch (determine_request_type(r->path)) {
    case REQUEST_BROWSE:
        result = handle_browse_request(r);
        break;
    case REQUEST_FILE:
        result = handle_file_request(r);
        break;
    case REQUEST_CGI:
        result = handle_cgi_request(r);
        break;
    default:
        result = handle_error(r, HTTP_STATUS_NOT_FOUND);
        break;
    }

    log("HTTP REQUEST STATUS: %s", http_status_string(result));
    return result;
}

/**
 * Handle browse request
 *
 * This lists the contents of a directory in HTML.
 *
 * If the path cannot be opened or scanned as a directory, then handle error
 * with HTTP_STATUS_NOT_FOUND.
 **/
http_status
handle_browse_request(struct request *r)
{
    struct dirent **entries;
    int n;

    /* Open a directory for reading or scanning */
    n = scandir(r->path, &entries, NULL, alphasort);
    if (n < 0) {
        return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    /* Write HTTP Header with OK Status and text/html Content-Type */
    fprintf(r->file, "HTTP/1.0 %s\r\n", http_status_string(HTTP_STATUS_OK));
    fprintf(r->file, "Content-Type: text/html\r\n");
    fprintf(r->file, "\r\n");

    /* For each entry in directory, emit HTML list item */
    fprintf(r->file, "<html><head><title>Index of %s</title></head><body>\n", r->uri ? r->uri : "/");
    fprintf(r->file, "<h1>Index of %s</h1>\n<ul>\n", r->uri ? r->uri : "/");

    for (int i = 0; i < n; i++) {
        const char *name = entries[i]->d_name;

        /* Skip . and .. */
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
            free(entries[i]);
            continue;
        }

        /* Build link: r->uri + '/' + name (avoid double slashes) */
        const char *base = (r->uri && r->uri[0]) ? r->uri : "/";
        int need_slash = base[strlen(base) - 1] != '/';

        fprintf(r->file, "<li><a href=\"%s%s%s\">%s</a></li>\n",
                base, need_slash ? "/" : "", name, name);

        free(entries[i]);
    }
    free(entries);

    fprintf(r->file, "</ul>\n</body></html>\n");

    /* Flush socket, return OK */
    fflush(r->file);
    return HTTP_STATUS_OK;
}

/**
 * Handle file request
 *
 * This opens and streams the contents of the specified file to the socket.
 *
 * If the path cannot be opened for reading, then handle error with
 * HTTP_STATUS_NOT_FOUND.
 **/
http_status
handle_file_request(struct request *r)
{
    FILE *fs;
    char buffer[BUFSIZ];
    char *mimetype = NULL;
    size_t nread;

    /* Open file for reading */
    fs = fopen(r->path, "rb");
    if (!fs) {
        return handle_error(r, HTTP_STATUS_NOT_FOUND);
    }

    /* Determine mimetype */
    mimetype = determine_mimetype(r->path);
    if (!mimetype) {
        mimetype = strdup(DefaultMimeType);
        if (!mimetype) {
            fclose(fs);
            return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
        }
    }

    /* Write HTTP Headers with OK status and determined Content-Type */
    fprintf(r->file, "HTTP/1.0 %s\r\n", http_status_string(HTTP_STATUS_OK));
    fprintf(r->file, "Content-Type: %s\r\n", mimetype);
    fprintf(r->file, "\r\n");

    /* Read from file and write to socket in chunks */
    while ((nread = fread(buffer, 1, sizeof(buffer), fs)) > 0) {
        if (fwrite(buffer, 1, nread, r->file) != nread) {
            fclose(fs);
            free(mimetype);
            return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
        }
    }

    /* Close file, flush socket, deallocate mimetype, return OK */
    fclose(fs);
    fflush(r->file);
    free(mimetype);
    return HTTP_STATUS_OK;
}

/**
 * Handle file request
 *
 * This popens and streams the results of the specified executables to the
 * socket.
 *
 *
 * If the path cannot be popened, then handle error with
 * HTTP_STATUS_INTERNAL_SERVER_ERROR.
 **/
http_status
handle_cgi_request(struct request *r)
{
    FILE *pfs;
    char buffer[BUFSIZ];
    struct header *header;

    /* Export CGI environment variables from request:
    * http://en.wikipedia.org/wiki/Common_Gateway_Interface */
    if (r->method) setenv("REQUEST_METHOD", r->method, 1);
    if (r->uri)    setenv("REQUEST_URI",   r->uri,    1);
    if (r->path)   setenv("SCRIPT_FILENAME", r->path, 1);
    setenv("QUERY_STRING", r->query ? r->query : "", 1);

    /* Server and client info */
    if (RootPath)  setenv("DOCUMENT_ROOT", RootPath, 1);
    if (Port)      setenv("SERVER_PORT",   Port,     1);
    if (r->host[0]) setenv("REMOTE_ADDR",  r->host,  1);
    if (r->port[0]) setenv("REMOTE_PORT",  r->port,  1);

    /* Export CGI environment variables from request headers */
    for (header = r->headers; header != NULL; header = header->next) {
        // Build env name: HTTP_<NAME>, uppercase, '-' -> '_' 
        char envname[PATH_MAX];
        int n = snprintf(envname, sizeof(envname), "HTTP_%s", header->name ? header->name : "");
        if (n <= 0 || n >= (int)sizeof(envname)) {
            continue;
        }
        for (int i = 5; envname[i]; i++) {
            if (envname[i] >= 'a' && envname[i] <= 'z') envname[i] -= 32; /* to upper */
            if (envname[i] == '-') envname[i] = '_';
        }
        if (header->value) {
            setenv(envname, header->value, 1);
        }
    }

    /* POpen CGI Script */
    pfs = popen(r->path, "r");
    if (!pfs) {
        return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
    }
    fprintf(r->file, "HTTP/1.0 %s\r\n", http_status_string(HTTP_STATUS_OK));

    /* Copy data from popen to socket */
    size_t nread;
    while ((nread = fread(buffer, 1, sizeof(buffer), pfs)) > 0) {
        if (fwrite(buffer, 1, nread, r->file) != nread) {
            pclose(pfs);
            return handle_error(r, HTTP_STATUS_INTERNAL_SERVER_ERROR);
        }
    }

    /* Close popen, flush socket, return OK */
    pclose(pfs);
    fflush(r->file);
    return HTTP_STATUS_OK;
}

/**
 * Handle displaying error page
 *
 * This writes an HTTP status error code and then generates an HTML message to
 * notify the user of the error.
 **/
http_status
handle_error(struct request *r, http_status status)
{
    const char *status_string = http_status_string(status);

    /* Write HTTP Header */
    fprintf(r->file, "HTTP/1.0 %s\r\n", status_string);
    fprintf(r->file, "Content-Type: text/html\r\n");
    fprintf(r->file, "\r\n");

    /* Write HTML Description of Error*/
    fprintf(r->file, "<html><head><title>%s</title></head><body>\n", status_string);
    fprintf(r->file, "<h1>%s</h1>\n", status_string);
    fprintf(r->file, "<p>The requested URL %s resulted in an error.</p>\n",
            r->uri ? r->uri : "/");
    fprintf(r->file, "</body></html>\n");

    fflush(r->file);

    /* Return specified status */
    return status;
}

/* vim: set expandtab sts=4 sw=4 ts=8 ft=c: */
