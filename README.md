Minimal HTTP Server (C, POSIX Sockets)
===================

Relevance
-------
A compact HTTP/1.0 server implemented in C with POSIX sockets, showcasing networking, concurrency, and systems programming fundamentals. Supports directory browsing, static file serving, and CGI execution with safe path resolution and clean resource management.

- Systems & Networking: Built from scratch with socket()/bind()/listen()/accept(), manual request parsing, and HTTP response formatting.
- Concurrency: Single-process and forking modes; safe child handling and independent request lifecycles.
- Reliability: Defensive parsing, error paths, and memory-safety (clean free_request, leak-checked).
- Security-minded: Canonical path resolution via realpath and root-prefix checks to block traversal (e.g., /../../etc/passwd).
- Practicality: MIME detection from /etc/mime.types, CGI environment setup, and clear logging for operability.

Testing
-------
1) download all files
2) type make in main directory
3) run:
   - ./httpServer -r ./www -- single process, default port is 9898 with root www/
   - ./httpServer -p __8080__ -c __forking__ -r ./www -- customizable port and forking
4) In another terminal:
   - you may test with curl or other commands to see its response
   - for example:
       - curl -i http://localhost:9898/  -- directory listing (browse handler)
       - curl -i http://localhost:9898/html/index.html  -- static files
       - chmod +x www/scripts/*.sh && curl -i 'http://localhost:9898/scripts/env.sh'  -- cgi scripts (must make sure they are executable)
       - etc.

Features
----------
Functionality:
- Browse: HTML directory listing via scandir + simple templating.
- Static Files: Buffered streaming (fread/fwrite) with correct Content-Type.
- CGI Execution: popen + standard CGI environment variables (REQUEST_METHOD, QUERY_STRING, DOCUMENT_ROOT, HTTP_* from headers).
- Error Handling: Consistent 400/404/500 responses via handle_error.

Engineering Quality:
- Path Security: determine_request_path() joins RootPath + URI, resolves with realpath, and enforces root prefix.
- Resource Hygiene: Centralized cleanup (free_request) closes FILE*/FDs, frees headers & strings.
- Mime Types: Extension lookup against /etc/mime.types, fallback to DefaultMimeType.
- Operability: Human-readable log()/debug() lines with file & line numbers.

Architecture
--------
- mainServer.c — CLI parsing (-p, -r, -c, -m, -M), bootstraps server.
- socket.c — socket_listen: getaddrinfo → socket → setsockopt(SO_REUSEADDR) → bind → listen.
- single.c / forking.c — Accept loop; in forking mode, parent accepts and child handles one request.
- request.c — accept_request (peer info, fdopen), parse_request (start line, headers, query).
- handler.c — handle_request dispatches to:
- handle_browse_request
- handle_file_request
- handle_cgi_request
- utils.c — MIME resolution, secure path computation, request type detection, status strings, whitespace helpers.
- www/ — Sample content: html/, text/, scripts/.

File Structure
----------
```
http-server/
├── Makefile
├── mainServer.c            # main: flags, listen, mode dispatch
├── socket.c            # socket_listen()
├── single.c            # single-process accept loop
├── forking.c           # fork-per-connection server
├── request.c           # accept_request(), parse_request()
├── handler.c           # routing to browse/file/cgi
├── utils.c             # mimetype, realpath, request type, helpers
├── mainServer.h            # shared types, prototypes, logging macros
└── www/                # sample site root
    ├── html/index.html
    ├── text/hackers.txt
    └── scripts/{env.sh,cowsay.sh}
```
