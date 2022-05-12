#include "gfserver-student.h"
#define BUFFER_SIZE 4096
#define HEADER_LEN 512
#define HEADER_MAX_LEN 150

typedef struct gfserver_t
{
    int maxpending;
    unsigned short port;

    gfh_error_t (*handler)(gfcontext_t **, const char *, void *);
    void *handlerarg;

} gfserver_t;

typedef struct gfcontext_t
{
    int sockfd;
    char *file_path;
} gfcontext_t;

// Modify this file to implement the interface specified in
// gfserver.h.

void gfs_abort(gfcontext_t **ctx)
{
    close((*ctx)->sockfd);
    free(*ctx);
    (*ctx) = NULL;
}

gfserver_t *gfserver_create()
{
    // dummy for now - need to fill this part in
    gfserver_t *gfs = (gfserver_t *)malloc(sizeof(gfserver_t));
    return gfs;
}

ssize_t gfs_send(gfcontext_t **ctx, const void *data, size_t len)
{

    int sent = send((*ctx)->sockfd, data, len, 0);
    return sent;
}

ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len)
{
    // not yet implemented
    char *status_char;
    char header[HEADER_MAX_LEN];
    char *scheme = "GETFILE";
    if (status == GF_OK)
    {
        status_char = "OK";
    }
    else if (status == GF_ERROR)
    {
        status_char = "ERROR";
    }
    else if (status == GF_FILE_NOT_FOUND)
    {
        status_char = "FILE_NOT_FOUND";
    }
    else if (status == GF_INVALID)
    {
        status_char = "INVALID";
    }

    if (status == GF_OK)
    {
        sprintf(header, "%s %s %lu\r\n\r\n", scheme, status_char, file_len);
    }
    else
    {
        sprintf(header, "%s %s\r\n\r\n", scheme, status_char);
    }
    int sent = send((*ctx)->sockfd, header, strlen(header), 0);
    return sent;
}

void gfserver_serve(gfserver_t **gfs)
{
    int sockfd;
    int total_received = 0;
    gfcontext_t *ctx = (gfcontext_t *)malloc(sizeof(gfcontext_t)); // store current connect socket info
    struct sockaddr_storage their_addr;                            // store client's address
    socklen_t address_len = sizeof(their_addr);
    char buffer[BUFFER_SIZE];
    char request_buffer[BUFFER_SIZE];
    memset(buffer, '\0', BUFFER_SIZE);
    memset(request_buffer, '\0', BUFFER_SIZE);

    if ((sockfd = gfs_create_socket(gfs)) < 0)
    {
        perror("filed to connect.");
        close(sockfd);
        exit(1);
    }

    for (;;)
    {
        ctx->sockfd = accept(sockfd, (struct sockaddr *)&their_addr, &address_len);
        if (ctx->sockfd < 0)
        {
            perror("failed to accept.");
            gfs_abort(&ctx);
            continue;
        }
        // read in all the header
        memset(buffer, 0, BUFFER_SIZE);
        memset(request_buffer, 0, BUFFER_SIZE);
        while (strstr(request_buffer, "\r\n\r\n") == NULL && total_received < HEADER_MAX_LEN)
        {
            int received;
            if ((received = recv(ctx->sockfd, buffer, HEADER_MAX_LEN, 0)) < 0)
            {
                perror("No request received.");
                gfs_abort(&ctx);
                continue;
            }
            total_received += received;
            buffer[received] = '\0';
            strcat(request_buffer, buffer);
            memset(buffer, '\0', HEADER_MAX_LEN);
        }

        // if invalid header
        if (parseRequestHeader(ctx, request_buffer) < 0)
        {
            gfs_sendheader(&ctx, GF_INVALID, 0);
            gfs_abort(&ctx);
            perror("Invalid request.");
            continue;
        }
        // call request call back function
        (*gfs)->handler(&ctx, ctx->file_path, (*gfs)->handlerarg);
    }
}

void gfserver_set_handlerarg(gfserver_t **gfs, void *arg)
{
    (*gfs)->handlerarg = arg;
}

void gfserver_set_handler(gfserver_t **gfs, gfh_error_t (*handler)(gfcontext_t **, const char *, void *))
{
    (*gfs)->handler = handler;
}

void gfserver_set_maxpending(gfserver_t **gfs, int max_npending)
{
    (*gfs)->maxpending = max_npending;
}

void gfserver_set_port(gfserver_t **gfs, unsigned short port)
{
    (*gfs)->port = port;
}

int parseRequestHeader(gfcontext_t *ctx, char *header)
{
    char path[HEADER_MAX_LEN];
    if (sscanf(header, "GETFILE GET %s\r\n\r\n", path) < 0)
    {
        perror("Invalid request header");
        return -1;
    }
    printf("header: %s", header);
    // check first char to see if it's a valid path
    if (path[0] != '/')
    {
        perror("Invalid path");
        return -1;
    }
    ctx->file_path = path;
    return 0;
}

int gfs_create_socket(gfserver_t **gfs)
{
    int status;                        // status - store getaddrinfo status, 0 = success, -1 = failed
                                       // sockfd - store file descriptor of the socket used for listening
    int sockfd = -1;                   // new_fd - store file descriptor of the socket used for accepting message
    struct addrinfo hints;             // used as arguments for getaddrinfo
    struct addrinfo *servinfo, *p;     // will point to the results of getaddrinfo, will be used to free the address info
    char port[30];                     // store port number
    sprintf(port, "%d", (*gfs)->port); // store port number as char array
    int yes = 1;                       // socket_opt

    memset(&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;     // set address info to be both IPv4 and IPv6 compatible
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
    hints.ai_flags = AI_PASSIVE;     // fill in my IP for me --> this tells getaddrinfo() to assign the address of my local host to the socket structures

    // get address info of the server
    if ((status = getaddrinfo(NULL, port, &hints, &servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }

    // use the address info to create a socket
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        // There can be >1 address, try create a socket for the first one we see
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1)
        {
            perror("server: socket");
            close(sockfd);
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1)
        {
            perror("Socket option failed.");
            exit(1);
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server failed to bind");
            continue;
        }

        break;
    }
    // start listening for incoming request
    if (listen(sockfd, (*gfs)->maxpending) < 0)
    {
        perror("Failed to start listening");
        exit(1);
    }
    // free the address info
    freeaddrinfo(servinfo);
    return sockfd;
}
