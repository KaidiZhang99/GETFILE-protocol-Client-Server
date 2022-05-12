/*
 *  This file is for use by students to define anything they wish.  It is used by the gf server implementation
 */
#ifndef __GF_SERVER_STUDENT_H__
#define __GF_SERVER_STUDENT_H__

#include "gf-student.h"
#include "gfserver.h"
#include "stdlib.h"

int gfs_create_socket(gfserver_t **gfs);

// return -1 for invalid request or invalid file path
// return 0 otherwise
int parseRequestHeader(gfcontext_t *context, char *request);

#endif // __GF_SERVER_STUDENT_H__