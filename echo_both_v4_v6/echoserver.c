#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <getopt.h>
#include <netdb.h>

// A buffer large enough to contain the longest allowed string
#define BUFSIZE 256

#define USAGE                                                        \
  "usage:\n"                                                         \
  "  echoserver [options]\n"                                         \
  "options:\n"                                                       \
  "  -p                  Port (Default: 10823)\n"                    \
  "  -m                  Maximum pending connections (default: 5)\n" \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"maxnpending", required_argument, NULL, 'm'},
    {NULL, 0, NULL, 0}};

int main(int argc, char **argv)
{
  int maxnpending = 5; // set maximum pending incoming connecting request
  int option_char;
  int portno = 10823; /* port to listen on */

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "hx:m:p:", gLongOptions, NULL)) != -1)
  {
    switch (option_char)
    {
    case 'm': // server
      maxnpending = atoi(optarg);
      break;
    case 'p': // listen-port
      portno = atoi(optarg);
      break;
    case 'h': // help
      fprintf(stdout, "%s ", USAGE);
      exit(0);
      break;
    default:
      fprintf(stderr, "%s ", USAGE);
      exit(1);
    }
  }

  setbuf(stdout, NULL); // disable buffering

  if ((portno < 1025) || (portno > 65535))
  {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }
  if (maxnpending < 1)
  {
    fprintf(stderr, "%s @ %d: invalid pending count (%d)\n", __FILE__, __LINE__,
            maxnpending);
    exit(1);
  }

  // Socket Code Here
  int status, sockfd, new_fd;         // status - store getaddrinfo status, 0 = success, -1 = failed
                                      // sockfd - store file descriptor of the socket used for listening
                                      // new_fd - store file descriptor of the socket used for accepting message
  struct addrinfo hints;              // used as arguments for getaddrinfo
  struct addrinfo *servinfo, *p;      // will point to the results of getaddrinfo, will be used to free the address info
  struct sockaddr_storage their_addr; // store client's address
  socklen_t addr_size;                // address size
  char buf[BUFSIZE];                  // store message received
  char port[BUFSIZE];                 // store port number
  sprintf(port, "%d", portno);        // store port number as char array
  int yes = 1;

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
      continue;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                   sizeof(int)) == -1)
    {
      perror("setsockopt");
      exit(1);
    }

    if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
    {
      close(sockfd);
      perror("server: bind");
      continue;
    }

    break;
  }

  // start listening for incoming request
  listen(sockfd, maxnpending);

  // free the address info
  freeaddrinfo(servinfo);

  // continue accepting calls from clients
  while (1)
  {
    addr_size = sizeof their_addr;
    // create a new socket accepting message from client
    new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
    if (new_fd == -1)
    {
      perror("accept");
      continue;
    }
    // receive the new message, store it into buf
    recv(new_fd, buf, BUFSIZE, 0);
    // send back the new message stored
    send(new_fd, buf, BUFSIZE, 0);
    // clear the buffer when done
    memset(buf, 0, BUFSIZE);
    // close the socket when done accepting
    close(new_fd);
  }

  return 0;
}