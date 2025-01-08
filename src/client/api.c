#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include "src/common/protocol.h"
#include "src/common/io.h"

int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path, char const *notifications_pipe_path, char const *server_pipe_path) {

  // Wait for the server to create the FIFO
  while (access(server_pipe_path, F_OK) == -1) {
    fprintf(stderr, "FIFO doesn't exist yet: %s\n", server_pipe_path);
    sleep(1);
  }


  int fifo_fd = open(server_pipe_path, O_WRONLY);

  if (fifo_fd == -1) {
    fprintf(stderr, "Failed to open fifo: '%s'\n", server_pipe_path);
    return 1;
  }

  // Send connect message to server with empty chars as \0
  char message = OP_CODE_CONNECT;
  char buffer[121];
  memset(buffer, 0, 121);
  buffer[0] = message;
  strcpy(buffer + 1, req_pipe_path);
  strcpy(buffer + 41, resp_pipe_path);
  strcpy(buffer + 81, notifications_pipe_path);

  if (write_all(fifo_fd, &buffer, 121) == -1) {
    fprintf(stderr, "Failed to write to FIFO\n");
    return 1;
  }

  // Open response FIFO to read server response
  int response_fd = open(resp_pipe_path, O_RDONLY | O_NONBLOCK);

  if (response_fd == -1) {
    fprintf(stderr, "Failed to open response fifo: '%s'\n", resp_pipe_path);
    return 1;
  }


  while (1) {
    char resp_message;
    char result;
    ssize_t bytes_read = read(response_fd, &resp_message, 1);
    if (bytes_read > 0) {
      switch (resp_message) {
        case OP_CODE_CONNECT:
          read(response_fd, &result, 1);
          if (result == '0') {
            fprintf(stdout, "0\n");
            return 0;
          } else {
            fprintf(stderr, "%i\n", result);
            return 1;
          }
          break;
        
        default:
          break;
      }
    }
  }
  char response;
  char result;
  read(response_fd, &response, 1);
  read(response_fd, &result, 1);

  switch (response) {
    case OP_CODE_CONNECT:
      /* code */
      if (result == 0) {
        fprintf(stdout, "\033[0;32mClient connected\033[0m\n");
        return 0;
      } else {
        fprintf(stderr, "\033[0;31mFailed to connect to server\033[0m\n");
        return 1;
      }
      return 0;

    default:
      fprintf(stderr, "\033[0;31mFailed to connect to server\033[0m\n");
      return 1;
  }
  
  return 0;
}
 
int kvs_disconnect(void) {
  // close pipes and unlink pipe files
  return 0;
}

int kvs_subscribe(const char* key) {
  // send subscribe message to request pipe and wait for response in response pipe
  key = key;
  return 0;
}

int kvs_unsubscribe(const char* key) {
    // send unsubscribe message to request pipe and wait for response in response pipe
    key = key;
  return 0;
}

int create_pipes(char const* req_pipe_path, char const* resp_pipe_path, char const* notifications_pipe_path) {
  // Remove existing pipes if they exist
  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notifications_pipe_path);

  // Create each pipe and return 0 if all were created successfully
  if (mkfifo(req_pipe_path, 0666) == -1) {
    perror("Failed to create request pipe");
    return 1;
  }

  if (mkfifo(resp_pipe_path, 0666) == -1) {
    perror("Failed to create response pipe");
    return 1;
  }

  if (mkfifo(notifications_pipe_path, 0666) == -1) {
    perror("Failed to create notifications pipe");
    return 1;
  }

  return 0;
}
                

