#include "api.h"
#include "../common/constants.h"
#include "../common/protocol.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>
#include "../common/protocol.h"
#include "../common/io.h"

int kvs_connect(char const *req_pipe_path, char const *resp_pipe_path, char const *notifications_pipe_path, char const *server_pipe_path, int *fds) {
  // Wait for the server to create the FIFO
  while (access(server_pipe_path, F_OK) == -1) {
    fprintf(stderr, "FIFO doesn't exist yet: %s\n", server_pipe_path);
    sleep(1);
  }

  int fifo_fd = open(server_pipe_path, O_WRONLY);

  if (fifo_fd == -1) {
    fprintf(stderr, "Failed to open resigister pipe\n");
    return 1;
  }

  fds[0] = fifo_fd;

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
  int response_fd = open(resp_pipe_path, O_RDONLY);
  if (response_fd == -1) {
    fprintf(stderr, "Failed to open response pipe\n");
    return 1;
  }

  //fprintf(stdout, "Request path: '%s'\n", req_pipe_path);
  int request_fd = open(req_pipe_path, O_WRONLY);
  //fprintf(stdout, "Request fd: %d\n", request_fd);
  if (request_fd == -1) {
    fprintf(stderr, "Failed to open request pipe\n");
    return 1;
  }

  int notifications_fd = open(notifications_pipe_path, O_RDONLY);

  if (notifications_fd == -1) {
    fprintf(stderr, "Failed to open notifications pipe\n");
    return 1;
  }

  fds[1] = response_fd;
  fds[2] = request_fd;
  fds[3] = notifications_fd;
  
  while (1) {
    char resp_message[2];
    read_all(response_fd, resp_message, 2, NULL);
    if (resp_message[0] == OP_CODE_CONNECT) {
      fprintf(stdout, "Server returned %c for operation: connect\n", resp_message[1]);
      return (resp_message[1] == '0') ? 0 : 1;
    } else {
      fprintf(stderr, "Unknown response from server: %c\n", resp_message[0]);
    }
  
  }
  
  return 0;
}
 
int kvs_disconnect(char const *req_pipe_path, char const *resp_pipe_path, char const *notifications_pipe_path,  int *fds) {
  char message = OP_CODE_DISCONNECT;
  write_all(fds[2], &message, 1);
  fprintf(stdout, "Sent disconnect message to server\n");
  char response[2];
  read_all(fds[1], response, 2, NULL);
  fprintf(stdout, "got response from disconnect\n");
  if (response[0] == OP_CODE_DISCONNECT) {

    fprintf(stdout, "Server returned %c for operation: disconnect\n", response[1]);

    // close pipes and unlink pipe files
    for (int i = 0; i < 4; i++) {
      if (close(fds[i]) == -1) {
        fprintf(stderr, "fds[%d]: %d\n", i, fds[i]);
        perror("Failed to close pipe");
        return 1;
      }
    }



    if (unlink(req_pipe_path) == -1) {
      perror("Failed to unlink request pipe");
      return 1;
    }

    if (unlink(resp_pipe_path) == -1) {
      perror("Failed to unlink response pipe");
      return 1;
    }

    if (unlink(notifications_pipe_path) == -1) {
      perror("Failed to unlink notifications pipe");
      return 1;
    }
    return (response[1] == '0') ? 0 : 1;
  } else {
    fprintf(stderr, "Unknown response from server: %c\n", response[0]);
  }



  return 0;
}

int kvs_subscribe(const char* key, const int req_fd, const int resp_fd) {
  // send subscribe message to request pipe and wait for response in response pipe
  char message[42];
  memset(message, 0, 42);
  message[0] = OP_CODE_SUBSCRIBE;
  strcpy(message + 1, key);
  write_all(req_fd, message, 42);

  while (1) {
    char response[2];
    read_all(resp_fd, response, 2, NULL);
    if (response[0] == OP_CODE_SUBSCRIBE) {
      fprintf(stdout, "Server returned %c for operation: subscribe\n", response[1]);
      return (response[1] == '0') ? 0 : 1;
    } else {
      fprintf(stderr, "Unknown response from server: %c\n", response[0]);
    }
  }
}

int kvs_unsubscribe(const char* key, const int req_fd, const int resp_fd) {
    // send unsubscribe message to request pipe and wait for response in response pipe
  char message[42];
  memset(message, 0, 42);
  message[0] = OP_CODE_SUBSCRIBE;
  strcpy(message + 1, key);
  write_all(req_fd, message, 42);

  while (1) {
    char response[2];
    read_all(resp_fd, response, 2, NULL);
    if (response[0] == OP_CODE_SUBSCRIBE) {
      fprintf(stdout, "Server returned %c for operation: unsubscribe\n", response[1]);
      return (response[1] == '0') ? 0 : 1;
    } else {
      fprintf(stderr, "Unknown response from server: %c\n", response[0]);
    }
  }  
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
                

