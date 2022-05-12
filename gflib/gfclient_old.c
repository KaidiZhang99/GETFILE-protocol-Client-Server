
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
  gfstatus_t ret_status;
  char server_path[BUFFER_SIZE];  // path of the file in server
  char file_content[BUFFER_SIZE]; // store received byte

  void (*writefunc)(void *data_buffer, size_t data_buffer_length, void *handlerarg);
  void *writearg;

  void (*headerfunc)(void *header_buffer, size_t header_buffer_length, void *handlerarg);
  void *headerarg;
} gfcrequest_t;

// check if the header is valid by checking if scheme, status, and file length are specified.
// return False/0 if len(scheme) is not 7, status is not within the required options, or file length is not a valid number
// i specifies the i_th token parsed from header
int isValidHeader(int i, char *token)
{
  printf("token: %s\n", token);
  switch (i)
  {
  case 0:
  {
    printf("isValidHeader: case 0\n");
    printf("case 0 token %s\n", token);
    if (strcmp(token, "GETFILE") == 0)
    {
      return 1;
    }
    return 0;
  }
  break;
  case 1:
  {
    printf("isValidHeader: case 1\n");
    printf("case 1 token %s\n", token);
    int cmp1 = abs(strcmp(token, "OK"));
    int cmp2 = abs(strcmp(token, "INVALID"));
    int cmp3 = abs(strcmp(token, "FILE_NOT_FOUND"));
    int cmp4 = abs(strcmp(token, "ERROR"));
    int cmp5 = abs(strcmp(token, "UNKNOWN"));
    return ((!cmp1) || (!cmp2) || (!cmp3) || (!cmp4) || (!cmp5)); // return 1/True if at least one status matches
  }
  break;
  default:
  {
    printf("74:isValidHeader: default\n");
    return FALSE;
  }
  }
}

// return length of bytes of file in currently received message
// return -1 for invalid header
// return 0 for no file content in currently received message
int parseHeader(int sockfd, gfcrequest_t **gfr, char *header, char *scheme, char *status, unsigned long long *filelen, char *file)
{
  int bytes_received;
  (*gfr)->total_bytes = 0;
  char buffer[BUFFER_SIZE];             // store received msg every time
  char temp[BUFFER_SIZE + BUFFER_SIZE]; // existing string for strcat
  memset(temp, '\0', BUFFER_SIZE + BUFFER_SIZE);
  // while not received the complete buffer yet
  while ((*gfr)->total_bytes <= BUFFER_SIZE)
  {
    // printf("93: keep reading header... \n");
    bzero(buffer, BUFFER_SIZE);
    // memset(buffer, '\0', BUFFER_SIZE);
    if ((bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0)) <= 0)
    {
      if (bytes_received < 0)
      {
        perror("Failed to connect\n");
        sscanf(temp, "%s %s %llu\r\n\r\n", scheme, status, filelen);
        storeStatus(gfr, status);
        return -1;
      }
      else
      {
        perror("Failed to connect\n");
        sscanf(temp, "%s %s %llu\r\n\r\n", scheme, status, filelen);
        storeStatus(gfr, status);
        return -1;
      }
    }
    (*gfr)->total_bytes += bytes_received;
    buffer[bytes_received] = '\0';
    // printf("buffer :%s\n", buffer);
    strcat(temp, buffer);
    // printf("temp :%s\n", temp);
  }

  // temp now contains header + file
  // extracting scheme, status, and filelen
  sscanf(temp, "%s %s %llu\r\n\r\n", scheme, status, filelen);
  printf("filelen = %llu\n", (*filelen));
  printf("127:Scheme = %s\n", scheme);

  if ((isValidHeader(0, scheme) <= FALSE) || (isValidHeader(1, status) <= FALSE))
  {
    printf("isvalidheader %d\n", (isValidHeader(0, scheme)));
    perror("127: invalid Header \n");
    return -1; // invalid header
  }

  if ((*filelen) == 0)
  {
    snprintf(header, HEADER_LEN, "%s %s\r\n\r\n%c", scheme, status, '\0');
  }
  else
  {
    snprintf(header, HEADER_LEN, "%s %s %llu\r\n\r\n%c", scheme, status, (*filelen), '\0');
  }

  printf("145:header is: %s, size of header = %ld\n", header, strlen(header));
  // printf("file content: %.5s\n", file);
  // printf("strlen of file = %ld\n", strlen(file));
  // printf("return: %ld\n", (*gfr)->total_bytes - strlen(header));

  // call header call back
  if ((*gfr)->headerfunc)
  {
    (*gfr)->headerfunc(header, strlen(header), (*gfr)->headerarg);
  }

  int file_byte = ((*gfr)->total_bytes - strlen(header));
  char *ptr = temp + strlen(header);
  strncpy(file, ptr, file_byte);
  // char *ptr = temp;
  printf("160:calling writefunc\n");
  if ((*gfr)->writefunc && (file_byte > 0))
  {
    (*gfr)->writefunc(file, file_byte, (*gfr)->writearg);
  }
  printf("165\n");
  (*gfr)->total_file_bytes = ((*gfr)->total_bytes - strlen(header));
  return ((*gfr)->total_file_bytes);
  // return (strlen(file));
}

int getFileRequestHeader(char *req_header, gfcrequest_t **gfr)
{
  return snprintf(req_header, HEADER_LEN, "%s %s %s%s", scheme, method, (*gfr)->server_path, end_marker);
  // return number of bytes written on req_header, exclude the null terminator
}

// store status to gfr as gfrstatus
int storeStatus(gfcrequest_t **gfr, char *status)
{
  if (strcmp(status, "OK") == 0)
  {
    (*gfr)->ret_status = GF_OK;
    return 0;
  }
  else if (strcmp(status, "FILE_NOT_FOUND") == 0)
  {
    (*gfr)->ret_status = GF_FILE_NOT_FOUND;
    return 1;
  }
  else if (strcmp(status, "ERROR") == 0)
  {
    (*gfr)->ret_status = GF_ERROR;
    return 1;
  }
  else if (strcmp(status, "INVALID") == 0)
  {
    (*gfr)->ret_status = GF_INVALID;
    return -1;
  }
  return 0;
}

gfcrequest_t *gfc_create()
{
  // dummy for now - need to fill this part in
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

  // printf("202:server is %s\n", (*gfr)->server);
  // printf("203:port is %s\n", (*gfr)->port);

  if ((socket_status = getaddrinfo((*gfr)->server, (*gfr)->port, &hints, &servinfo)) != 0)
  { // servinfo now points to a linked list of 1 or more struct addrinfos
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(socket_status));
    exit(1);
  }
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
  // printf("217:connect status is %d\n", connect_status);
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

  char req_header[HEADER_LEN];                                                             // store header used to send to server
  char received_scheme[10], received_status[20], received_file[BUFFER_SIZE + BUFFER_SIZE]; // store scheme, status, and actual file of the received bytes
  char header[HEADER_LEN];                                                                 // store received header / header sent by sever
  unsigned long long received_filelen = 0ULL;
  // memset(req_header, 0, sizeof(req_header));
  // memset(received_scheme, 0, sizeof(received_scheme));
  // memset(received_status, 0, sizeof(received_status));
  // memset(received_file, 0, sizeof(received_file));

  printf("creating socket...\n");
  if ((sockfd = gfc_create_socket(gfr)) < 0)
  {
    return -1;
  }

  // send the header
  header_len = getFileRequestHeader(req_header, gfr);
  // printf("%.*s\n", header_len, req_header);

  printf("sending header to server...\n");
  int bytes_sent = 0;
  while (bytes_sent < header_len)
  {
    int sent;
    if ((sent = send(sockfd, req_header, header_len, 0)) < 0)
    {
      perror("send() sent a different number of bytes than expected");
      (*gfr)->ret_status = GF_ERROR;
      return -1;
    }
    bytes_sent += sent;
  }

  // printf("235:sending the header...\n");

  // parse received header
  int file_byte = parseHeader(sockfd, gfr, header, received_scheme, received_status, &received_filelen, received_file);

  // if invalid header
  if (file_byte < 0)
  {
    perror("326:invalid header");
    return -1;
  }
  else
  {
    // update the attributes of gfr
    // (*gfr)->total_file_bytes = 0;
    // (*gfr)->total_file_bytes += file_byte;
    printf("341:storing Status based on header...\n");
    int status_type = storeStatus(gfr, received_status);
    if (status_type < 0) // INVALID
    {
      close(sockfd);
      return -1;
    }
    else if (status_type > 0) // FILE_NOT_FOUND or ERROR
    {
      close(sockfd);
      return 0;
    }
    (*gfr)->file_len = received_filelen;
  }

  printf("356:stored_status = %d\n", (*gfr)->ret_status);
  if ((*gfr)->ret_status != GF_OK)
  {
    return 0;
  }
  // printf("260: received_filelen = %llu\n", received_filelen);

  bzero((*gfr)->file_content, BUFFER_SIZE);

  printf("365: entering while loop...\n");
  while ((*gfr)->total_file_bytes < (*gfr)->file_len)
  {
    // memset((*gfr)->file_content, 0, BUFFER_SIZE);
    bytes_received = recv(sockfd, (*gfr)->file_content, BUFFER_SIZE, 0);
    printf("received = %d, bufsize = %d\n", bytes_received, BUFFER_SIZE);
    if (bytes_received <= 0)
    {
      printf("372:total_received%d", (*gfr)->total_file_bytes);
      close(sockfd);
      if ((*gfr)->total_file_bytes >= (*gfr)->file_len)
      {
        return 0;
      }
      else
      {
        return -1;
      }
    }
    // fprintf(fp, "%s", buffer);
    // (*gfr)->total_bytes += bytes_received;
    (*gfr)->total_file_bytes += bytes_received;
    (*gfr)->total_bytes += bytes_received;
    if ((*gfr)->writefunc)
    {
      (*gfr)->writefunc((*gfr)->file_content, bytes_received, (*gfr)->writearg);
    }
    bzero((*gfr)->file_content, BUFFER_SIZE);
    printf("392:received %d data, total so far%d\n", bytes_received, (*gfr)->total_file_bytes);
    // printf("total_bytes: %d, file_len: %ld, bytes_received: %d\n", (*gfr)->total_file_bytes, (*gfr)->file_len, bytes_received);
    // if ((*gfr)->total_file_bytes >= (*gfr)->file_len)
    // {
    //   (*gfr)->total_bytes = (*gfr)->total_file_bytes + strlen(header);
    //   // printf("317:total_received%d", (*gfr)->total_bytes);
    //   close(sockfd);
    //   return 0;
    // }
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
  // snprintf((*gfr)->port, 12, "%u", port);
  // A terminating null character is automatically appended after the content written. --> ???
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
