#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 512

#define USAGE                                           \
  "usage:\n"                                            \
  "  transferclient [options]\n"                        \
  "options:\n"                                          \
  "  -h                  Show this help message\n"      \
  "  -p                  Port (Default: 10823)\n"       \
  "  -s                  Server (Default: localhost)\n" \
  "  -o                  Output file (Default cs6200.txt)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {{"server", required_argument, NULL, 's'},
                                       {"output", required_argument, NULL, 'o'},
                                       {"port", required_argument, NULL, 'p'},
                                       {"help", no_argument, NULL, 'h'},
                                       {NULL, 0, NULL, 0}};

// write the message received from socket to the designated file until all message received
void write_file(char *filename, int sockfd)
{
  int n;
  FILE *fp;
  char *file = filename;
  setbuf(stdout, NULL);

  char buffer[BUFSIZE];

  fp = fopen(file, "w");
  while (1)
  {
    // receive message from socket, write message to buffer
    n = recv(sockfd, buffer, BUFSIZE, 0);

    // if no message received, file is fully received.
    if (n <= 0)
    {
      break;
      return;
    }

    // write the message from buffer to the file opened
    fwrite(buffer, n, 1, fp);
    // clear the buffer
    bzero(buffer, BUFSIZE);
  }
  return;
}

/* Main ========================================================= */
int main(int argc, char **argv)
{
  unsigned short portno = 10823;
  int option_char = 0;
  char *hostname = "localhost";
  char *filename = "cs6200.txt";

  setbuf(stdout, NULL);

  /* Parse and set command line arguments */
  while ((option_char =
              getopt_long(argc, argv, "xp:s:h:o:", gLongOptions, NULL)) != -1)
  {
    switch (option_char)
    {
    case 's': // server
      hostname = optarg;
      break;
    case 'p': // listen-port
      portno = atoi(optarg);
      break;
    case 'h': // help
      fprintf(stdout, "%s", USAGE);
      exit(0);
      break;
    default:
      fprintf(stderr, "%s", USAGE);
      exit(1);
    case 'o': // filename
      filename = optarg;
      break;
    }
  }

  if (NULL == hostname)
  {
    fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
    exit(1);
  }

  if (NULL == filename)
  {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
    exit(1);
  }

  if ((portno < 1025) || (portno > 65535))
  {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  // Socket Code Here
  int status, sockfd;          // status - store getaddrinfo status, 0 = success, -1 = failed
                               // sockfd - store file descriptor of the socket used for listening
  struct addrinfo hints;       // used as arguments for getaddrinfo
  struct addrinfo *servinfo;   // will point to the results of getaddrinfo, will be used to free the address info
  char port[BUFSIZE];          // store port number
  sprintf(port, "%d", portno); // store port number as char array

  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;     // set address info to be both IPv4 and IPv6 compatible
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

  // get address info of the server
  if ((status = getaddrinfo(hostname, port, &hints, &servinfo)) != 0)
  {
    // servinfo now points to a linked list of 1 or more struct addrinfos
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }

  // use the address info to create a socket
  sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
  // connect with the server through the socket
  if ((status = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)) != 0)
  {
    fprintf(stderr, "connect error: %s\n", gai_strerror(status));
    exit(1);
  }
  // release the address info
  freeaddrinfo(servinfo);

  // receive the message and write to file
  write_file(filename, sockfd);
  // close socket when done communicating with server
  close(sockfd);

  return 0;
}
