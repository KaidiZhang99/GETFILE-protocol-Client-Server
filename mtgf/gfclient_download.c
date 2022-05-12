#include <stdlib.h>
#include <pthread.h>
#include "gfclient-student.h"
#include "steque.h"

#define MAX_THREADS (1024)
#define PATH_BUFFER_SIZE 1221
#define DEBUG 1

#define USAGE                                                    \
  "usage:\n"                                                     \
  "  webclient [options]\n"                                      \
  "options:\n"                                                   \
  "  -h                  Show this help message\n"               \
  "  -s [server_addr]    Server address (Default: 127.0.0.1)\n"  \
  "  -p [server_port]    Server port (Default: 10823)\n"         \
  "  -r [num_requests]   Request download total (Default: 21)\n" \
  "  -t [nthreads]       Number of threads (Default 12)\n"       \
  "  -w [workload_path]  Path to workload file (Default: workload.txt)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"nrequests", required_argument, NULL, 'r'},
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"workload-path", required_argument, NULL, 'w'},
    {"nthreads", required_argument, NULL, 't'},
    {NULL, 0, NULL, 0}};

static void writecb(void *data, size_t data_len, void *arg);
pthread_mutex_t c_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c_boss = PTHREAD_COND_INITIALIZER; // Condition associated with boss
pthread_cond_t c_worker = PTHREAD_COND_INITIALIZER;
steque_t *crequest_queue = NULL;
char *poison_pill = "stop";
char *server = "localhost";
char *workload_path = "workload.txt";
unsigned short port = 10823;
int num_eaten_pills = 0;
int nthreads;

static void Usage() { fprintf(stderr, "%s", USAGE); }

static void localPath(char *req_path, char *local_path)
{
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE *openFile(char *path)
{
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while (NULL != (cur = strchr(prev + 1, '/')))
  {
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU))
    {
      if (errno != EEXIST)
      {
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if (NULL == (ans = fopen(&path[0], "w")))
  {
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void *data, size_t data_len, void *arg)
{
  FILE *file = (FILE *)arg;

  fwrite(data, 1, data_len, file);
}

void *request_work()
{
  int returncode = 0;
  char *poison_pil = "stop";

  for (;;)
  {
    gfcrequest_t *gfr = gfc_create();
    FILE *file = NULL;
    char *req_path;
    char local_path[PATH_BUFFER_SIZE];
    bzero(local_path, PATH_BUFFER_SIZE);

    // hold the lock to accept tasks from the queue
    pthread_mutex_lock(&c_lock);
    // wait if there's no tasks yet
    while (steque_isempty(crequest_queue))
    {
      pthread_cond_wait(&c_worker, &c_lock);
    }

    // if saw the poison pill
    if (!steque_isempty(crequest_queue))
    {
      if (strcmp(steque_front(crequest_queue), poison_pil) == 0)
      { // increment the counter for # of workers eaten poison pill
        num_eaten_pills += 1;
        pthread_mutex_unlock(&c_lock);
        // if all worker eaten poison, notify boss
        if (num_eaten_pills >= nthreads)
        {
          pthread_cond_signal(&c_boss);
        }
        gfc_cleanup(&gfr);
        gfr = NULL;
        break;
      }
    }

    // accept a task by deque from the client_request_queue
    if (!steque_isempty(crequest_queue))
    {
      req_path = steque_pop(crequest_queue);
    }
    else
    {
      continue;
    }
    // release the lock for other workers to accept tasks
    pthread_mutex_unlock(&c_lock);
    // notify boss and worker
    pthread_cond_signal(&c_boss);
    pthread_cond_broadcast(&c_worker);

    localPath(req_path, local_path);
    file = openFile(local_path); // FILE *

    gfc_set_server(&gfr, server);
    gfc_set_path(&gfr, req_path);
    gfc_set_port(&gfr, port);
    gfc_set_writefunc(&gfr, writecb);
    gfc_set_writearg(&gfr, file);

    fprintf(stdout, "Requesting %s%s\n", server, req_path);

    if (0 > (returncode = gfc_perform(&gfr)))
    {
      fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
      fclose(file);
      if (0 > unlink(local_path))
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
    }
    else
    {
      fclose(file);
    }

    if (gfc_get_status(&gfr) != GF_OK)
    {
      if (0 > unlink(local_path))
      {
        fprintf(stderr, "warning: unlink failed on %s\n", local_path);
        break;
      }
    }

    fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(&gfr)));
    fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(&gfr),
            gfc_get_filelen(&gfr));

    gfc_cleanup(&gfr);
    gfr = NULL;
    continue;
  }
  // exit the thread, join, etc.

  return 0;
}
/* Main ========================================================= */
int main(int argc, char **argv)
{
  /* COMMAND LINE OPTIONS ============================================= */
  int option_char = 0;
  int nrequests = 5;
  // int nthreads = 12;
  nthreads = 12;
  char *req_path = NULL;

  setbuf(stdout, NULL); // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:n:hs:t:r:w:", gLongOptions,
                                    NULL)) != -1)
  {
    switch (option_char)
    {
    case 'r':
      nrequests = atoi(optarg);
      break;
    case 'n': // nrequests
      break;
    default:
      Usage();
      exit(1);
    case 'p': // port
      port = atoi(optarg);
      break;
    case 'h': // help
      Usage();
      exit(0);
      break;
    case 's': // server
      server = optarg;
      break;
    case 't': // nthreads
      nthreads = atoi(optarg);
      break;
    case 'w': // workload-path
      workload_path = optarg;
      break;
    }
  }

  if (EXIT_SUCCESS != workload_init(workload_path))
  {
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }

  if (nthreads < 1)
  {
    nthreads = 1;
  }
  if (nthreads > MAX_THREADS)
  {
    nthreads = MAX_THREADS;
  }

  gfc_global_init();

  // creating request queue
  crequest_queue = (steque_t *)malloc(sizeof(steque_t));
  steque_init(crequest_queue);

  int i = 0;
  pthread_t thread_id[nthreads];

  // creating threads
  for (i = 0; i < nthreads; i++)
  {
    if (pthread_create(&thread_id[i], NULL, request_work, NULL) != 0)
    {
      fprintf(stderr, "Unable to create worker thread\n");
      steque_destroy(crequest_queue);
      exit(1);
    }
  }

  /* Build your queue of requests here */
  for (i = 0; i < nrequests; i++)
  {
    // enqueue file path
    req_path = workload_get_path();

    if (strlen(req_path) > 1024)
    {
      fprintf(stderr, "Request path exceeded maximum of 1024 characters\n.");
      exit(EXIT_FAILURE);
    }

    // hold the lock to enque the task to client_request_queue
    pthread_mutex_lock(&c_lock);
    steque_enqueue(crequest_queue, req_path);
    pthread_mutex_unlock(&c_lock);
    // notify workers to start accepting tasks
    pthread_cond_broadcast(&c_worker);
  }

  // add a poison pill at the end of all tasks
  pthread_mutex_lock(&c_lock);
  steque_enqueue(crequest_queue, poison_pill);
  pthread_mutex_unlock(&c_lock);
  pthread_cond_broadcast(&c_worker);

  // waiting for poison pill eaten by all
  pthread_mutex_lock(&c_lock);
  while (num_eaten_pills < nthreads)
  {
    pthread_cond_wait(&c_boss, &c_lock);
  }
  pthread_mutex_unlock(&c_lock);

  // once eaten by all, join all threads
  for (i = 0; i < nthreads; i++)
  {
    pthread_join(thread_id[i], NULL);
  }

  steque_destroy(crequest_queue);
  free(crequest_queue);
  crequest_queue = NULL;
  gfc_global_cleanup(); /* use for any global cleanup for AFTER your thread
                         pool has terminated. */
  return 0;
}
