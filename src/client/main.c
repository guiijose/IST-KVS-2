#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "../client/api.h"
#include "../common/constants.h"
#include "../common/io.h"
#include "api.h"
#include "../common/protocol.h"

// Mutex lock to synchronize who writes/reads to stdout



void *notifications_thread_function(void *arg) {
  int *notifications_fd = (int*)arg;
  // This thread will read from the notifications FIFO and print the messages to stdout
  while (1) {
    char message[82];
    int result = read_all(*notifications_fd, message, 82, NULL);
    if (result == 0) {
      return NULL;
    } else if (result == -1) {
      fprintf(stderr, "Failed to read from notifications FIFO\n");
      exit(1);
    }
    fprintf(stdout, "(%s,%s)\n", message, message + 41);
  }

  return NULL;

}

int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n", argv[0]);
    return 1;
  }

  char req_pipe_path[256] = "/tmp/req";
  char resp_pipe_path[256] = "/tmp/resp";
  char notifications_pipe_path[256] = "/tmp/notif";

  char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
  unsigned int delay_ms;
  size_t num;

  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notifications_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

  // Create all pipes
  if (create_pipes(req_pipe_path, resp_pipe_path, notifications_pipe_path) != 0) {
    fprintf(stderr, "Failed to create pipes\n");
    return 1;
  }

  int fds[4];

  // Connect to the server and fill array with file descriptors for request, response and notifications
  if (kvs_connect(req_pipe_path, resp_pipe_path, notifications_pipe_path, argv[2], fds) != 1) {
    fprintf(stderr, "Failed to connect to the server\n");
    return 1;
  }

  // Initialize and start the notifications thread
  pthread_t notifications_thread;
  pthread_create(&notifications_thread, NULL, notifications_thread_function, (void*)&fds[3]);

  // Main loop to read from standard input and process commands
  while (1) {
    switch (get_next(STDIN_FILENO)) {
      case CMD_DISCONNECT:
        if (kvs_disconnect(req_pipe_path, resp_pipe_path, notifications_pipe_path, fds) != 1) {
          fprintf(stderr, "Failed to disconnect to the server\n");
          return 1;
        }

        return 0;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
         
        if (kvs_subscribe(keys[0],fds[2], fds[1]) != 1) {
            fprintf(stderr, "Command subscribe failed\n");
        }
        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }
        
        if (kvs_unsubscribe(keys[0], fds[2], fds[1]) != 1) {
            fprintf(stderr, "Command subscribe failed\n");
        }

        break;

      case CMD_DELAY:
        if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay_ms > 0) {
            printf("Waiting...\n");
            delay(delay_ms);
        }
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_EMPTY:
        break;

      case EOC:
        // input should end in a disconnect, or it will loop here forever
        break;
    }
  }

}
