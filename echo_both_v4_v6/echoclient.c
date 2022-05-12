#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>

// A buffer large enough to contain the longest allowed string
#define BUFSIZE 256

#define USAGE                                                          \
  "usage:\n"                                                           \
  "  echoclient [options]\n"                                           \
  "options:\n"                                                         \
  "  -s                  Server (Default: localhost)\n"                \
  "  -p                  Port (Default: 10823)\n"                      \
  "  -m                  Message to send to server (Default: \"Hello " \
  "Summer.\")\n"                                                       \
  "  -h                  Show this help message\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"message", required_argument, NULL, 'm'},
    {"help", no_argument, NULL, 'h'},
    {NULL, 0, NULL, 0}};

/* Main ========================================================= */
int main(int argc, char **argv)
{
  unsigned short portno = 10823;
  int option_char = 0;
  char *message = "Hello Summer!!";
  char *hostname = "localhost";

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "p:s:m:hx", gLongOptions, NULL)) != -1)
  {
    switch (option_char)
    {
    default:
      fprintf(stderr, "%s", USAGE);
      exit(1);
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
    case 'm': // message
      message = optarg;
      break;
    }
  }

  setbuf(stdout, NULL); // disable buffering

  if ((portno < 1025) || (portno > 65535)) // check validility of port
  {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  if (NULL == message)
  {
    fprintf(stderr, "%s @ %d: invalid message\n", __FILE__, __LINE__);
    exit(1);
  }

  if (NULL == hostname)
  {
    fprintf(stderr, "%s @ %d: invalid host name\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Socket Code Here */
  int status, sockfd;          // status - store getaddrinfo status, 0 = success, -1 = failed
                               // sockfd - store file descriptor of the created socket
  struct addrinfo hints;       // used as arguments for getaddrinfo
  struct addrinfo *servinfo;   // will point to the results of getaddrinfo, will be used to free the address info
  char buf[BUFSIZE];           // store message received
  char port[BUFSIZE];          // store port number
  sprintf(port, "%d", portno); // store port number as char array

  memset(&hints, 0, sizeof hints); // make sure current "hints" are empty
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
  int len = strlen(message);
  // send the message
  send(sockfd, message, len, 0);
  // receive back the message, store it in buf
  recv(sockfd, buf, BUFSIZE, 0);
  printf("%s", buf);
  // close socket when done communicating with server
  close(sockfd);

  return 0;
}