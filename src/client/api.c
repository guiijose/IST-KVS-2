#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include "../common/semaphore.h"
#include "../common/protocol.h"

int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path, char const *notifications_pipe_path, char const *server_pipe_path) {

  // Wait for the server to create the FIFO
  while (access(server_pipe_path, F_OK) == -1) {
    fprintf(stderr, "FIFO doesn't exist yet: %s\n", server_pipe_path);
    sleep(1);
  }

  fprintf(stdout, "FIFO exists: '%s'\n", server_pipe_path);
  sleep(1);

  int fifo_fd = open(server_pipe_path, O_WRONLY);

  if (fifo_fd == -1) {
    fprintf(stderr, "Failed to open fifo: '%s'\n", server_pipe_path);
    return 1;
  }
  sem_wait(&register_fifo_sem);
  fprintf(stdout, "Client locked the semaphore\n");
  char message = OP_CODE_CONNECT;
  write(fifo_fd, &message, 1);
  write(fifo_fd, req_pipe_path, 40);
  write(fifo_fd, resp_pipe_path, 40);
  write(fifo_fd, notifications_pipe_path, 40);
  sem_post(&register_fifo_sem);

  sleep(1);
  close(fifo_fd);

  // Open response FIFO to read server response
  int response_fd = open(resp_pipe_path, O_RDONLY);

  if (response_fd == -1) {
    fprintf(stderr, "Failed to open response fifo: '%s'\n", resp_pipe_path);
    return 1;
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
                

