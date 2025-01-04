#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include "../common/semaphore.h"

int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path,
                char const* notif_pipe_path, int* notif_pipe) {
  // create pipes and connect
  // use the argument variables so the program compiles
  req_pipe_path = req_pipe_path;
  resp_pipe_path = resp_pipe_path;
  server_pipe_path = server_pipe_path;
  notif_pipe_path = notif_pipe_path;
  notif_pipe = notif_pipe;

  char *base_path = "/tmp/";
  char fifo_registo[1000]; 
  strcpy(fifo_registo, base_path);
  strcat(fifo_registo, server_pipe_path);
  fifo_registo[strlen(server_pipe_path) + strlen(base_path)] = '\0';

  // Wait for the server to create the FIFO
  while (access(fifo_registo, F_OK) == -1) {
    fprintf(stderr, "FIFO doesn't exist yet: %s\n", fifo_registo);
    sleep(1);
  }

  fprintf(stdout, "FIFO exists: '%s'\n", fifo_registo);
  sleep(1);

  int fifo_fd = open(fifo_registo, O_WRONLY);

  if (fifo_fd == -1) {
    fprintf(stderr, "Failed to open fifo: '%s'\n", fifo_registo);
    return 1;
  }

  const char* message = "connect client\n";
  write(fifo_fd, message, strlen(message));

  sleep(1);
  close(fifo_fd);


  int response_fd = open(resp_pipe_path, O_RDONLY);

  if (response_fd == -1) {
    fprintf(stderr, "Failed to open response fifo: '%s'\n", resp_pipe_path);
    return 1;
  }

  char response[256];
  read(response_fd, response, 256);

  switch (response[0]) {
  {
  case OP_CODE_CONNECT:
    /* code */
    break;
  
  default:
    break;
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


