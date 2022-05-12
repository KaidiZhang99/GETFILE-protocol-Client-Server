#include "gfserver-student.h"
#include "gfserver.h"
#include "content.h"
#include "workload.h"
#include "steque.h"

gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void *arg)
{

	server_task *task = (server_task *)malloc(sizeof(server_task));

	sscanf(path, "%s", task->file_path);

	task->ctx = *ctx;
	*ctx = NULL; // claim ownership by keeping only one copy of the key in struct

	// enque the task
	pthread_mutex_lock(&lock);
	steque_enqueue(request_queue, task);
	task = NULL; // claim ownership
	pthread_mutex_unlock(&lock);
	// telling workers to accept tasks
	pthread_cond_broadcast(&worker);

	return gfh_success;
}
