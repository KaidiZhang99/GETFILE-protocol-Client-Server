#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSIZE 512

#define USAGE                                            \
  "usage:\n"                                             \
  "  transferserver [options]\n"                         \
  "options:\n"                                           \
  "  -h                  Show this help message\n"       \
  "  -f                  Filename (Default: 6200.txt)\n" \
  "  -p                  Port (Default: 10823)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"filename", required_argument, NULL, 'f'},
    {NULL, 0, NULL, 0}};

// get bytes from file and send to client
void send_file(char *filename, int sockfd)
{
  setbuf(stdout, NULL);
  char data[BUFSIZE] = {0};
  // pointer points to file
  FILE *fp = fopen(filename, "r");

  if (fp == NULL)
  {
    perror("[-]Error in reading file.");
    exit(1);
  }

  // while not end of file
  while (!feof(fp))
  {
    int sent_total = 0;
    int sent_now = 0;
    // read in file and store into data
    int size = fread(data, 1, sizeof(data), fp);

    // keep sending until all bytes in data had been sent
    while (sent_total < size)
    {
      sent_now = send(sockfd, &data[sent_total], size - sent_total, 0);
      if (sent_now == -1)
      {
        perror("[-]Error in sending file.");
        exit(1);
      }
      sent_total += sent_now;
    }

    // clear "data"
    bzero(data, BUFSIZE);
  }
}

int main(int argc, char **argv)
{
  char *filename = "6200.txt"; // file to transfer
  int portno = 10823;          // port to listen on
  int option_char;

  setbuf(stdout, NULL); // disable buffering

  // Parse and set command line arguments
  while ((option_char =
              getopt_long(argc, argv, "hf:xp:", gLongOptions, NULL)) != -1)
  {
    switch (option_char)
    {
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
    case 'f': // file to transfer
      filename = optarg;
      break;
    }
  }

  if ((portno < 1025) || (portno > 65535))
  {
    fprintf(stderr, "%s @ %d: invalid port number (%d)\n", __FILE__, __LINE__,
            portno);
    exit(1);
  }

  if (NULL == filename)
  {
    fprintf(stderr, "%s @ %d: invalid filename\n", __FILE__, __LINE__);
    exit(1);
  }

  /* Socket Code Here */
  int maxnpending = 5;                // set maximum pending incoming connecting request
  int status, sockfd, new_fd;         // status - store getaddrinfo status, 0 = success, -1 = failed
                                      // sockfd - store file descriptor of the socket used for listening
                                      // new_fd - store file descriptor of the socket used for accepting message
  struct addrinfo hints;              // used as arguments for getaddrinfo
  struct addrinfo *servinfo, *p;      // will point to the results of getaddrinfo, will be used to free the address info
  struct sockaddr_storage their_addr; // store client's address
  socklen_t addr_size;                // address size
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
    // send the file through the new socket
    send_file(filename, new_fd);
    // close socket after done communicating with client
    close(new_fd);
  }
  return 0;
}
