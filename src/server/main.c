#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdio.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include "io.h"
#include "pthread.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <semaphore.h>
#include "../common/semaphore.h"
#include "../common/protocol.h"
#include "../common/constants.h"
#include "../common/io.h"

struct SharedData {
  DIR* dir;
  char* dir_name;
  pthread_mutex_t directory_mutex;
};


pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t n_current_backups_lock = PTHREAD_MUTEX_INITIALIZER;

size_t active_backups = 0;     // Number of active backups
size_t max_backups;            // Maximum allowed simultaneous backups
size_t max_threads;            // Maximum allowed simultaneous threads
char* jobs_directory = NULL;


pthread_mutex_t consumer_lock = PTHREAD_MUTEX_INITIALIZER;

Client* connect_queue[MAX_SESSION_COUNT];

pthread_t consumer_threads[MAX_SESSION_COUNT];

int request_thread_function(Client* client) {

  int fd = client->req_fd;
  // Add your function logic here
  
  while (1) {
    char message[42];
    read_all(fd, message, 1, NULL);
    char key[41];
    switch (message[0]) {
      case OP_CODE_SUBSCRIBE:
        /* code */
        read_all(fd, key, 41, NULL);
        if (subscribe(client, key) != 0) {
          fprintf(stderr, "Failed to subscribe to key: %s\n", key);
        }
        break;
      
      case OP_CODE_UNSUBSCRIBE:
        /* code */
        read_all(fd, key, 41, NULL);
        if (unsubscribe(client, key) != 0) {
          fprintf(stderr, "Failed to unsubscribe to key: %s\n", key);
        }

        break;
      
      case OP_CODE_DISCONNECT:
        /* code */
        // Remove client from array of clients
        for (int i = 0; i < MAX_SESSION_COUNT; i++) {
          if (clients[i] == client) {
            clients[i] = NULL;
            break;
          }
        }

        if (disconnect(client) != 0) {
          fprintf(stderr, "Failed to disconnect client\n");
        }
        return 0;
        break;

      default:
        fprintf(stderr, "Unknown request from client: '%c'\n", message[0]);

        break;
    }
  }
  
}

int process_message(Client* client) {
  char* req_pipe_path = client->req_pipe_path;
  char* resp_pipe_path = client->resp_pipe_path;
  char* notif_pipe_path = client->notif_pipe_path;

  if (client == NULL) {
    fprintf(stderr, "Failed to allocate memory for client\n");
    return 0;
  }
  
  client->resp_fd = open(resp_pipe_path, O_WRONLY);
  if (client->resp_fd == -1) {
    fprintf(stderr, "Failed to open response FIFO: %s\n", resp_pipe_path);
    perror("open");
    close(client->req_fd);
    free(client);
    return 1;
  }


  client->req_fd = open(req_pipe_path, O_RDONLY);
  if (client->req_fd == -1) {
    perror("open");
    fprintf(stderr, "Failed to open request FIFO: '%s'\n", req_pipe_path);
    free(client);
    return 1;
  }
  
  client->notif_fd = open(notif_pipe_path, O_WRONLY);
  if (client->notif_fd == -1) {
    fprintf(stderr, "Failed to open notification FIFO: %s\n", notif_pipe_path);
    close(client->req_fd);
    close(client->resp_fd);
    free(client);
    return 1;
  }

  // Concatenate the message and send it to the client

  char connect_message[2];
  connect_message[0] = OP_CODE_CONNECT;
  connect_message[1] = '0';

  if (write_all(client->resp_fd, connect_message, 2) == -1) {
    fprintf(stderr, "Failed to write to response FIFO\n");
    return 1;
  }

  return 0;
}

void* consumer_thread(void* arg) {
  // Ignore the argument to avoid warnings when compiling
  (void)arg;

  while (1) {
    pthread_mutex_lock(&consumer_lock);
    Client* client = connect_queue[0];

    if (client == NULL) {
      pthread_mutex_unlock(&consumer_lock);
      continue;
    }

    for (int i = 0; i < MAX_SESSION_COUNT - 1; i++) {
      connect_queue[i] = connect_queue[i + 1];
    }
    connect_queue[MAX_SESSION_COUNT - 1] = NULL;

    pthread_mutex_unlock(&consumer_lock);

    process_message(client);

    if (request_thread_function(client) != 0) {
      fprintf(stderr, "Failed to process request\n");
    }

  }
}

int filter_job_files(const struct dirent* entry) {
    const char* dot = strrchr(entry->d_name, '.');
    if (dot != NULL && strcmp(dot, ".job") == 0) {
        return 1;  // Keep this file (it has the .job extension)
    }
    return 0;
}

static int entry_files(const char* dir, struct dirent* entry, char* in_path, char* out_path) {
  const char* dot = strrchr(entry->d_name, '.');
  if (dot == NULL || dot == entry->d_name || strlen(dot) != 4 || strcmp(dot, ".job")) {
    return 1;
  }

  if (strlen(entry->d_name) + strlen(dir) + 2 > MAX_JOB_FILE_NAME_SIZE) {
    fprintf(stderr, "%s/%s\n", dir, entry->d_name);
    return 1;
  }

  strcpy(in_path, dir);
  strcat(in_path, "/");
  strcat(in_path, entry->d_name);

  strcpy(out_path, in_path);
  strcpy(strrchr(out_path, '.'), ".out");

  return 0;
}

static int run_job(int in_fd, int out_fd, char* filename) {
  size_t file_backups = 0;
  while (1) {
    char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    unsigned int delay;
    size_t num_pairs;

    switch (get_next(in_fd)) {
      case CMD_WRITE:
        num_pairs = parse_write(in_fd, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
        if (num_pairs == 0) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_write(num_pairs, keys, values)) {
          write_str(STDERR_FILENO, "Failed to write pair\n");
        }
        break;

      case CMD_READ:
        num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_read(num_pairs, keys, out_fd)) {
          write_str(STDERR_FILENO, "Failed to read pair\n");
        }
        break;

      case CMD_DELETE:
        num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_delete(num_pairs, keys, out_fd)) {
          write_str(STDERR_FILENO, "Failed to delete pair\n");
        }
        break;

      case CMD_SHOW:
        kvs_show(out_fd);
        break;

      case CMD_WAIT:
        if (parse_wait(in_fd, &delay, NULL) == -1) {
          write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay > 0) {
          printf("Waiting %d seconds\n", delay / 1000);
          kvs_wait(delay);
        }
        break;

      case CMD_BACKUP:
        pthread_mutex_lock(&n_current_backups_lock);
        if (active_backups >= max_backups) {
          wait(NULL);
        } else {
          active_backups++;
        }
        pthread_mutex_unlock(&n_current_backups_lock);
        int aux = kvs_backup(++file_backups, filename, jobs_directory);

        if (aux < 0) {
            write_str(STDERR_FILENO, "Failed to do backup\n");
        } else if (aux == 1) {
          return 1;
        }
        break;

      case CMD_INVALID:
        write_str(STDERR_FILENO, "Invalid command. See HELP for usage\n");
        break;

      case CMD_HELP:
        write_str(STDOUT_FILENO,
            "Available commands:\n"
            "  WRITE [(key,value)(key2,value2),...]\n"
            "  READ [key,key2,...]\n"
            "  DELETE [key,key2,...]\n"
            "  SHOW\n"
            "  WAIT <delay_ms>\n"
            "  BACKUP\n" // Not implemented
            "  HELP\n");

        break;

      case CMD_EMPTY:
        break;

      case EOC:
        printf("EOF\n");
        return 0;
    }
  }
}

//frees arguments
static void* get_file(void* arguments) {
  struct SharedData* thread_data = (struct SharedData*) arguments;
  DIR* dir = thread_data->dir;
  char* dir_name = thread_data->dir_name;

  if (pthread_mutex_lock(&thread_data->directory_mutex) != 0) {
    fprintf(stderr, "Thread failed to lock directory_mutex\n");
    return NULL;
  }

  struct dirent* entry;
  char in_path[MAX_JOB_FILE_NAME_SIZE], out_path[MAX_JOB_FILE_NAME_SIZE];
  while ((entry = readdir(dir)) != NULL) {
    if (entry_files(dir_name, entry, in_path, out_path)) {
      continue;
    }

    if (pthread_mutex_unlock(&thread_data->directory_mutex) != 0) {
      fprintf(stderr, "Thread failed to unlock directory_mutex\n");
      return NULL;
    }

    int in_fd = open(in_path, O_RDONLY);
    if (in_fd == -1) {
      write_str(STDERR_FILENO, "Failed to open input file: ");
      write_str(STDERR_FILENO, in_path);
      write_str(STDERR_FILENO, "\n");
      pthread_exit(NULL);
    }

    int out_fd = open(out_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out_fd == -1) {
      write_str(STDERR_FILENO, "Failed to open output file: ");
      write_str(STDERR_FILENO, out_path);
      write_str(STDERR_FILENO, "\n");
      pthread_exit(NULL);
    }

    int out = run_job(in_fd, out_fd, entry->d_name);

    close(in_fd);
    close(out_fd);

    if (out) {
      if (closedir(dir) == -1) {
        fprintf(stderr, "Failed to close directory\n");
        return 0;
      }

      exit(0);
    }

    if (pthread_mutex_lock(&thread_data->directory_mutex) != 0) {
      fprintf(stderr, "Thread failed to lock directory_mutex\n");
      return NULL;
    }
  }

  if (pthread_mutex_unlock(&thread_data->directory_mutex) != 0) {
    fprintf(stderr, "Thread failed to unlock directory_mutex\n");
    return NULL;
  }

  pthread_exit(NULL);
}



static void dispatch_threads(DIR* dir, char* fifo_registo) {
  pthread_t* threads = malloc(max_threads * sizeof(pthread_t));

  if (threads == NULL) {
    fprintf(stderr, "Failed to allocate memory for threads\n");
    return;
  }

  struct SharedData thread_data = {dir, jobs_directory, PTHREAD_MUTEX_INITIALIZER};


  for (size_t i = 0; i < max_threads; i++) {
    if (pthread_create(&threads[i], NULL, get_file, (void*)&thread_data) != 0) {
      fprintf(stderr, "Failed to create thread %zu\n", i);
      pthread_mutex_destroy(&thread_data.directory_mutex);
      free(threads);
      return;
    }
  }

  // Create consumer threads to initialize client sessions
  for (size_t i = 0; i < MAX_SESSION_COUNT; i++) {
    if (pthread_create(&consumer_threads[i], NULL, consumer_thread, NULL) != 0) {
      fprintf(stderr, "Failed to create consumer thread %zu\n", i);
      free(threads);
      return;
    }
  }

  // ler do FIFO de registo
  int fd = open(fifo_registo, O_RDWR);
  if (fd == -1) {
    fprintf(stderr, "Failed to open register FIFO\n");
    return;
  }

  while (1) {
    // Read from the register FIFO
    char response[121];
    read_all(fd, response, 121, NULL);
    if (response[0] != OP_CODE_CONNECT) {
      continue;
    }
    
    Client* client = malloc(sizeof(Client));
    if (client == NULL) {
      fprintf(stderr, "Failed to allocate memory for client\n");
      continue;
    }

    strn_memcpy(client->req_pipe_path, response + 1, MAX_PIPE_PATH_LENGTH);
    strn_memcpy(client->resp_pipe_path, response + 41, MAX_PIPE_PATH_LENGTH);
    strn_memcpy(client->notif_pipe_path, response + 81, MAX_PIPE_PATH_LENGTH);

    // Add client to the queue
    pthread_mutex_lock(&consumer_lock);

    for (int i = 0; i < MAX_SESSION_COUNT; i++) {
      if (connect_queue[i] == NULL) {
        connect_queue[i] = client;
        break;
      }
    }

    pthread_mutex_unlock(&consumer_lock);

  }



  pthread_t* request_thread = malloc(sizeof(pthread_t));
  

  if (request_thread == NULL) {
    fprintf(stderr, "Failed to allocate memory for register thread\n");
    return;
  }


  for (unsigned int i = 0; i < max_threads; i++) {
    if (pthread_join(threads[i], NULL) != 0) {
      fprintf(stderr, "Failed to join thread %u\n", i);
      pthread_mutex_destroy(&thread_data.directory_mutex);
      free(threads);
      return;
    }
  }

  if (pthread_mutex_destroy(&thread_data.directory_mutex) != 0) {
    fprintf(stderr, "Failed to destroy directory_mutex\n");
  }

  free(threads);
}


int main(int argc, char** argv) {
  if (argc < 5) {
    write_str(STDERR_FILENO, "Usage: ");
    write_str(STDERR_FILENO, argv[0]);
    write_str(STDERR_FILENO, " <jobs_dir>");
		write_str(STDERR_FILENO, " <max_threads>");
		write_str(STDERR_FILENO, " <max_backups>");
    write_str(STDERR_FILENO, " <nome_do_FIFO_de_registo>\n");
    return 1;
  }

  jobs_directory = argv[1];

  char* endptr;
  max_backups = strtoul(argv[3], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_proc value\n");
    return 1;
  }

  max_threads = strtoul(argv[2], &endptr, 10);

  if (*endptr != '\0') {
    fprintf(stderr, "Invalid max_threads value\n");
    return 1;
  }

	if (max_backups <= 0) {
		write_str(STDERR_FILENO, "Invalid number of backups\n");
		return 0;
	}

	if (max_threads <= 0) {
		write_str(STDERR_FILENO, "Invalid number of threads\n");
		return 0;
	}

  char *fifo_registo = argv[4];

  /* remove pipe if it exists */
  if (unlink(fifo_registo) != 0 && errno != ENOENT) {
      perror("[ERR]: unlink(%s) failed");
      exit(EXIT_FAILURE);
  }

  /* create pipe */
  if (mkfifo(fifo_registo, 0640) != 0) {
      perror("[ERR]: mkfifo failed");
      exit(EXIT_FAILURE);
  }

  if (kvs_init()) {
    write_str(STDERR_FILENO, "Failed to initialize KVS\n");
    return 1;
  }

  DIR* dir = opendir(argv[1]);
  if (dir == NULL) {
    fprintf(stderr, "Failed to open directory: %s\n", argv[1]);
    return 0;
  }

  dispatch_threads(dir, fifo_registo);

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory\n");
    return 0;
  }

  while (active_backups > 0) {
    wait(NULL);
    active_backups--;
  }

  kvs_terminate();

  return 0;
}
