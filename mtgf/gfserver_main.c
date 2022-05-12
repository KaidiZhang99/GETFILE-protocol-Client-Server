
#include <stdlib.h>
#include "gfserver-student.h"
#include "steque.h"
#include "content.h"
#include <pthread.h>
#define BUFFER_SIZE 10000

#define USAGE                                                               \
  "usage:\n"                                                                \
  "  gfserver_main [options]\n"                                             \
  "options:\n"                                                              \
  "  -h                  Show this help message.\n"                         \
  "  -t [nthreads]       Number of threads (Default: 21)\n"                 \
  "  -m [content_file]   Content file mapping keys to content files\n"      \
  "  -p [listen_port]    Listen port (Default: 10823)\n"                    \
  "  -d [delay]          Delay in content_get, default 0, range 0-5000000 " \
  "(microseconds)\n "

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"help", no_argument, NULL, 'h'},
    {"content", required_argument, NULL, 'm'},
    {"nthreads", required_argument, NULL, 't'},
    {"port", required_argument, NULL, 'p'},
    {"delay", required_argument, NULL, 'd'},
    {NULL, 0, NULL, 0}};

extern unsigned long int content_delay;
extern gfh_error_t gfs_handler(gfcontext_t **ctx, const char *path, void *arg);
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t boss = PTHREAD_COND_INITIALIZER; // Condition associated with boss
pthread_cond_t worker = PTHREAD_COND_INITIALIZER;
steque_t *request_queue = NULL; // no size limitation

static void _sig_handler(int signo)
{
  if ((SIGINT == signo) || (SIGTERM == signo))
  {
    exit(signo);
  }
}

void *work()
{
  while (1)
  {
    struct stat finfo;
    server_task *task;

    pthread_mutex_lock(&lock);

    while (steque_isempty(request_queue))
    {
      pthread_cond_wait(&worker, &lock);
    }

    // if (!steque_isempty(request_queue))
    // {
    task = steque_pop(request_queue);
    // }
    // else
    // {
    //   continue;
    // }

    pthread_mutex_unlock(&lock);
    pthread_cond_signal(&boss);

    int fildes = content_get(task->file_path);

    if (fildes < 0) // no file descriptor returned
    {
      gfs_sendheader(&(task->ctx), GF_FILE_NOT_FOUND, 0);
      free(task->ctx);
      task->ctx = NULL;
      free(task);
      task = NULL;
      continue;
    }

    if (fstat(fildes, &finfo) < 0) // file status issue
    {
      gfs_sendheader(&(task->ctx), GF_ERROR, 0);

      free(task->ctx);
      task->ctx = NULL;

      free(task);
      task = NULL;
      continue;
    }

    off_t file_size = finfo.st_size;

    if (gfs_sendheader(&(task->ctx), GF_OK, file_size) < 0)
    {
      printf("Failed to send header!\n");
      free(task->ctx);
      task->ctx = NULL;
      free(task);
      task = NULL;
      continue;
    }

    char buf[BUFFER_SIZE];
    int read = 0;
    int total_read = 0;
    int total_sent = 0;

    while (total_read < file_size)
    {
      bzero(buf, BUFFER_SIZE);

      if ((read = pread(fildes, buf, BUFFER_SIZE, total_read)) <= 0)
      {
        perror("[-]Error in reading file.");
        break;
      }
      total_read += read;

      int sent_per_read = 0;
      int total_sent_per_read = 0;
      while (total_sent_per_read < read)
      {
        if ((sent_per_read = gfs_send(&(task->ctx), buf + total_sent_per_read, read - total_sent_per_read)) < 0)
        {
          perror("[-]Error in sending file.");
          break;
        }

        total_sent_per_read += sent_per_read;
      }
      total_sent += total_sent_per_read;

      bzero(buf, BUFFER_SIZE);
    }

    free(task->ctx);
    task->ctx = NULL;

    free(task);
    task = NULL;
  }
}

/* Main ========================================================= */
int main(int argc, char **argv)
{
  gfserver_t *gfs = NULL;
  int nthreads = 12;
  int option_char = 0;
  unsigned short port = 10823;
  char *content_map = "content.txt";

  setbuf(stdout, NULL);

  if (SIG_ERR == signal(SIGINT, _sig_handler))
  {
    fprintf(stderr, "Can't catch SIGINT...exiting.\n");
    exit(EXIT_FAILURE);
  }

  if (SIG_ERR == signal(SIGTERM, _sig_handler))
  {
    fprintf(stderr, "Can't catch SIGTERM...exiting.\n");
    exit(EXIT_FAILURE);
  }

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:d:rhm:t:", gLongOptions,
                                    NULL)) != -1)
  {
    switch (option_char)
    {
    case 't': /* nthreads */
      nthreads = atoi(optarg);
      break;
    case 'p': /* listen-port */
      port = atoi(optarg);
      break;
    case 'm': /* file-path */
      content_map = optarg;
      break;
    default:
      fprintf(stderr, "%s", USAGE);
      exit(1);
    case 'd': /* delay */
      content_delay = (unsigned long int)atoi(optarg);
      break;
    case 'h': /* help */
      fprintf(stdout, "%s", USAGE);
      exit(0);
      break;
    }
  }

  /* not useful, but it ensures the initial code builds without warnings */
  if (nthreads < 1)
  {
    nthreads = 1;
  }

  if (content_delay > 5000000)
  {
    fprintf(stderr, "Content delay must be less than 5000000 (microseconds)\n");
    exit(__LINE__);
  }

  content_init(content_map);

  request_queue = (steque_t *)malloc(sizeof(steque_t));
  steque_init(request_queue);

  int i;
  pthread_t tid[nthreads];

  for (i = 0; i < nthreads; i++)
  {

    if (pthread_create(&tid[i], NULL, work, NULL) != 0)
    {
      fprintf(stderr, "Unable to create worker thread\n");
      steque_destroy(request_queue);
      exit(1);
    }
  }

  /*Initializing server*/

  gfs = gfserver_create();

  // Setting options
  gfserver_set_port(&gfs, port);
  gfserver_set_maxpending(&gfs, 86);
  gfserver_set_handler(&gfs, gfs_handler);
  gfserver_set_handlerarg(&gfs, NULL); // doesn't have to be NULL!

  /*Loops forever*/
  gfserver_serve(&gfs);
}
