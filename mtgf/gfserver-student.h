/*
 *  This file is for use by students to define anything they wish.  It is used by the gf server implementation
 */
#ifndef __GF_SERVER_STUDENT_H__
#define __GF_SERVER_STUDENT_H__
#define DEBUG 0

#include "gf-student.h"
#include "gfserver.h"
#include "content.h"
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "steque.h"
#define HEADER_LEN 4096

void init_threads(size_t numthreads);
void cleanup_threads();
typedef struct task server_task;

typedef struct task
{
    gfcontext_t *ctx;
    char file_path[HEADER_LEN];
} server_task;

extern gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void *arg);
extern pthread_mutex_t lock;
extern pthread_cond_t boss;     // Condition associated with boss
extern pthread_cond_t worker;   // Condition associated with worker
extern steque_t *request_queue; // no size limitation

#endif // __GF_SERVER_STUDENT_H__

/* Initialize thread management */
// initialize queue, mutex, conditional variables
// both workers and boss will use the same mutex. When mutex is locked, then can change the queue
// queue contains all request waiting to be completed.

/* Create worker threads */
// command line argument will specify number of worker threads
// each worker thread needs to send the file:
// repeat forever
//      dequeue a request from the request queue; --> need to lock mutex when doing it
//      fildes = content_get(path);
//      gfs_sendheader(ctx, GF_OK, file_len);
//      while (until finished)
//        gfs_send(ctx, buffer, len);

// work
// lock the mutex
//  while queue is empty: wait(worker, mutex)
// dequeue ctx
// unlock

// get path
// ssize_t gfs_sendheader(gfcontext_t **ctx, gfstatus_t status, size_t file_len);
// while (not sending full length yet)
//    gfs_send

// Get file descriptor with content_get()
// Check to see if file descriptor is valid, else send a FILE_NOT_FOUND header
// Use fstat to get file status, and if it errors out send an ERROR header
// Send an OK header, after grabbing file-size
// Create and zero a buffer out.
// Start a loop that gets the value from pread() and makes sure it is more than 0.
// If there were bytes that were read, send them with gfs_send
// If there was an issue sending, abort and break out of loop.
// Count how many bytes were written
// Clean up buffer again
// Free up the struct from earlier
// int mutex_locked = 0;
