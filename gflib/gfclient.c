
#include <stdlib.h>

#include "gfclient-student.h"

#define BUFFER_SIZE 4096
#define FALSE 0
#define TRUE 1
#define HEADER_LEN 512
#define HEADER_MAX_LEN 60

const char *const scheme = "GETFILE"; // char[8]
const char *const method = "GET";
const char *const end_marker = "\r\n\r\n"; // 7+3+4+10+1+1

typedef struct gfcrequest_t
{
  unsigned long file_len;            // total bytes of complete file,
  int total_bytes, total_file_bytes; // total bytes received, total file bytes received so far
  char port[15];
  char server[50];
  gfstatus_t ret_status;          // return status
  char server_path[BUFFER_SIZE];  // path of the file in server
  char file_content[BUFFER_SIZE]; // store received byte
  char *status_str;               // store status read in
  char header[HEADER_MAX_LEN];    // store header

  void (*writefunc)(void *data_buffer, size_t data_buffer_length, void *handlerarg);
  void *writearg;

  void (*headerfunc)(void *header_buffer, size_t header_buffer_length, void *handlerarg);
  void *headerarg;
} gfcrequest_t;

// return -1 for invalid header or communication issue
// return 0 for successful execution when status = OK
// return 1 for ERROR or FILE_NOT_FOUND, no write call back called
int parseHeader(int sockfd, gfcrequest_t **gfr, unsigned long long *filelen)
{
  (*gfr)->total_bytes = 0;
  (*gfr)->total_file_bytes = 0;
  char *ptr;                                       // used for strstr, i.e. find the end marker of header
  char buffer[BUFFER_SIZE];                        // recv() store content each time
  char complete_buffer[BUFFER_SIZE + BUFFER_SIZE]; // store everything read in so far, should include the complete header
  memset(buffer, '\0', BUFFER_SIZE);
  memset(complete_buffer, '\0', BUFFER_SIZE * 2);
  char status_str[20];
  int file_pointer;

  // while not received the complete buffer yet
  while ((ptr = strstr(complete_buffer, "\r\n\r\n")) == NULL)
  {
    bzero(buffer, BUFFER_SIZE);
    int received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);

    // if failed received, communication error, return -1;
    if (received < 0)
    {
      printf("Failed to connect to server. \n");
      (*gfr)->ret_status = GF_ERROR;
      return -1;
    }
    else if (received == 0)
    {
      printf("Server didn't send anything \n");
      (*gfr)->ret_status = GF_INVALID;
      return -1;
    }
    (*gfr)->total_bytes += received;
    // concatenate meg received so far
    buffer[received + 1] = '\0';
    strcat(complete_buffer, buffer);
  }

  // checking header validity and parse header, try both header with or without file len
  int header_with_file_len = sscanf(complete_buffer, "GETFILE %s %llu\r\n\r\n%n", status_str, filelen, &file_pointer);
  if (header_with_file_len < 0)
  {
    int header_without_file_len = sscanf(complete_buffer, "GETFILE %s\r\n\r\n", status_str);
    if (header_without_file_len < 0)
    {
      printf("Header not valid. \n");
      (*gfr)->ret_status = GF_INVALID;
      return -1;
    }
  }

  int status = storeStatus(gfr, status_str);
  if (status < 0)
  {
    printf("INVALID status %s\n", gfc_strstatus((*gfr)->ret_status));
    return -1;
  }

  if ((*filelen) == 0)
  {
    snprintf((*gfr)->header, HEADER_LEN, "GETFILE %s\r\n\r\n%c", status_str, '\0');
  }
  else
  {
    (*gfr)->file_len = (*filelen);
    snprintf((*gfr)->header, HEADER_LEN, "GETFILE %s %llu\r\n\r\n%c", status_str, (*filelen), '\0');
  }

  // call header call back
  if ((*gfr)->headerfunc)
  {
    (*gfr)->headerfunc((*gfr)->header, strlen((*gfr)->header), (*gfr)->headerarg);
  }

  if ((*gfr)->ret_status == GF_ERROR || (*gfr)->ret_status == GF_FILE_NOT_FOUND)
  {
    // memset(buffer, '\0', BUFFER_SIZE);
    return 1;
  }

  int file_byte = ((*gfr)->total_bytes - strlen((*gfr)->header));

  // calling write call back on file content that come with header
  if ((*gfr)->writefunc && (file_byte > 0))
  {
    (*gfr)->writefunc(complete_buffer + file_pointer + 4, file_byte, (*gfr)->writearg);
  }
  (*gfr)->total_file_bytes = file_byte;
  return 0;
}

// return number of bytes written on req_header, exclude the null terminator
int getFileRequestHeader(char *req_header, gfcrequest_t **gfr)
{
  return snprintf(req_header, HEADER_LEN, "GETFILE GET %s\r\n\r\n", (*gfr)->server_path);
}

// store status to gfr as gfrstatus
int storeStatus(gfcrequest_t **gfr, char *status)
{
  if (strncmp(status, "OK", 2) == 0)
  {
    (*gfr)->status_str = "OK";
    (*gfr)->ret_status = GF_OK;
    return 0;
  }
  else if (strncmp(status, "FILE_NOT_FOUND", 14) == 0)
  {
    (*gfr)->status_str = "FILE_NOT_FOUND";
    (*gfr)->ret_status = GF_FILE_NOT_FOUND;
    return 0;
  }
  else if (strncmp(status, "ERROR", 5) == 0)
  {
    (*gfr)->status_str = "ERROR";
    (*gfr)->ret_status = GF_ERROR;
    return 0;
  }
  else
  {
    (*gfr)->status_str = "INVALID";
    (*gfr)->ret_status = GF_INVALID;
    return -1;
  }
  return 0;
}

gfcrequest_t *gfc_create()
{
  gfcrequest_t *gfr = (gfcrequest_t *)malloc(1 * sizeof(gfcrequest_t));
  memset(gfr, 0, sizeof(gfcrequest_t));
  return (gfcrequest_t *)gfr;
}

// optional function for cleaup processing.
void gfc_cleanup(gfcrequest_t **gfr)
{
  free((*gfr));
  (*gfr) = NULL;
}

size_t gfc_get_bytesreceived(gfcrequest_t **gfr)
{
  return (((*gfr)) ? (*gfr)->total_file_bytes : 0);
  // return (*gfr)->total_file_bytes;
}

size_t gfc_get_filelen(gfcrequest_t **gfr)
{
  return (((*gfr)) ? (*gfr)->file_len : 0);
  // return (*gfr)->file_len;
}

gfstatus_t gfc_get_status(gfcrequest_t **gfr)
{
  return (((*gfr)) ? (*gfr)->ret_status : GF_INVALID);
}

void gfc_global_init() {}

void gfc_global_cleanup() {}

int gfc_create_socket(gfcrequest_t **gfr)
{
  int sockfd, connect_status, socket_status; // socket id
                                             // sending-out-herder length
                                             // bytes received of current recv()
                                             // get server address info status
  struct addrinfo *servinfo, *p, hints;

  memset(&hints, 0, sizeof hints); // make sure the struct is empty
  hints.ai_family = AF_UNSPEC;     // don't care IPv4 or IPv6
  hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

  if ((socket_status = getaddrinfo((*gfr)->server, (*gfr)->port, &hints, &servinfo)) != 0)
  { // servinfo now points to a linked list of 1 or more struct addrinfos
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(socket_status));
    exit(1);
  }
  // try connecting until connected
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
    {
      close(sockfd);
      continue;
    }

    if ((connect_status = connect(sockfd, p->ai_addr, p->ai_addrlen)) < 0)
    {
      close(sockfd);
      continue;
    }
    break;
  }
  freeaddrinfo(servinfo);

  // if failed to connect at all
  if (p == NULL)
  {
    close(sockfd);
    exit(1);
  }
  return sockfd;
}

int gfc_perform(gfcrequest_t **gfr)
{
  int sockfd, header_len, bytes_received; // socket id
                                          // sending-out-herder length
                                          // bytes received of current recv()
                                          // get server address info status

  char req_header[HEADER_LEN]; // store header used to send to server

  unsigned long long received_filelen = 0ULL;

  if ((sockfd = gfc_create_socket(gfr)) < 0)
  {
    return -1;
  }

  // send the header
  header_len = getFileRequestHeader(req_header, gfr);

  int bytes_sent = 0;
  while (bytes_sent < header_len)
  {
    int sent;
    if ((sent = send(sockfd, req_header + bytes_sent, header_len - bytes_sent, 0)) < 0)
    {
      perror("send() sent a different number of bytes than expected");
      // if fail to send request, ERROR occured
      (*gfr)->ret_status = GF_ERROR;
      return -1;
    }
    bytes_sent += sent;
  }

  // parse received header
  int parse = parseHeader(sockfd, gfr, &received_filelen);

  // if invalid header
  if (parse < 0)
  {
    return -1;
  }
  // FILE_NOT_FOUND or ERROR
  else if (parse > 0)
  {
    close(sockfd);
    return 0;
  }

  bzero((*gfr)->file_content, BUFFER_SIZE);

  while ((*gfr)->total_file_bytes < (*gfr)->file_len)
  {
    bytes_received = recv(sockfd, (*gfr)->file_content, BUFFER_SIZE, 0);
    if (bytes_received <= 0)
    {
      close(sockfd);
      if ((*gfr)->total_file_bytes >= (*gfr)->file_len) // successfully received all content
      {
        return 0;
      }
      else // error occured
      {
        return -1;
      }
    }

    (*gfr)->total_file_bytes += bytes_received;
    (*gfr)->total_bytes += bytes_received;
    if ((*gfr)->writefunc)
    {
      (*gfr)->writefunc((*gfr)->file_content, bytes_received, (*gfr)->writearg);
    }
    bzero((*gfr)->file_content, BUFFER_SIZE);
  }

  close(sockfd);
  return 0;
}

void gfc_set_headerarg(gfcrequest_t **gfr, void *headerarg)
{
  (*gfr)->headerarg = headerarg;
}

void gfc_set_headerfunc(gfcrequest_t **gfr, void (*headerfunc)(void *, size_t, void *))
{
  (*gfr)->headerfunc = headerfunc;
}

void gfc_set_path(gfcrequest_t **gfr, const char *path)
{
  strcpy((*gfr)->server_path, path);
}

void gfc_set_port(gfcrequest_t **gfr, unsigned short port)
{
  sprintf((*gfr)->port, "%d", port);
}

void gfc_set_server(gfcrequest_t **gfr, const char *server)
{
  strcpy((*gfr)->server, server);
}

void gfc_set_writearg(gfcrequest_t **gfr, void *writearg)
{
  (*gfr)->writearg = writearg;
}

void gfc_set_writefunc(gfcrequest_t **gfr, void (*writefunc)(void *, size_t, void *))
{
  (*gfr)->writefunc = writefunc;
}

const char *gfc_strstatus(gfstatus_t status)
{
  const char *strstatus = NULL;

  switch (status)
  {
  default:
  {
    strstatus = "UNKNOWN";
  }
  break;

  case GF_FILE_NOT_FOUND:
  {
    strstatus = "FILE_NOT_FOUND";
  }
  break;

  case GF_INVALID:
  {
    strstatus = "INVALID";
  }
  break;

  case GF_ERROR:
  {
    strstatus = "ERROR";
  }
  break;

  case GF_OK:
  {
    strstatus = "OK";
  }
  break;
  }

  return strstatus;
}
