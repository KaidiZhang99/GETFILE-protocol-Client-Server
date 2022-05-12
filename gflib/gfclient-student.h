/*
 *  This file is for use by students to define anything they wish.  It is used by the gf client implementation
 */
#ifndef __GF_CLIENT_STUDENT_H__
#define __GF_CLIENT_STUDENT_H__

#include "gfclient.h"
#include "gf-student.h"
#include <string.h>

// return -1 for invalid header or communication issue
// return 0 for successful execution when status = OK
// return 1 for ERROR or FILE_NOT_FOUND, no write call back called
int parseHeader(int sockfd, gfcrequest_t **gfr, unsigned long long *filelen);

// create header to send to server
int getFileRequestHeader(char *req_header, gfcrequest_t **gfr);

// store status to gfr as gfrstatus
int storeStatus(gfcrequest_t **gfr, char *status);

#endif // __GF_CLIENT_STUDENT_H__