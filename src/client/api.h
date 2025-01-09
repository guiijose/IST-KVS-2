#ifndef CLIENT_API_H
#define CLIENT_API_H

#include <stddef.h>
#include "../common/constants.h"

/// Connects to a kvs server.
/// @param req_pipe_path Path to the name pipe to be created for requests.
/// @param resp_pipe_path Path to the name pipe to be created for responses.
/// @param server_pipe_path Path to the name pipe where the server is listening.
/// @return 0 if the connection was established successfully, 1 otherwise.
int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* notifications_pipe_path, char const* server_pipe_path, int *fds);
/// Disconnects from an KVS server.
/// @return 0 in case of success, 1 otherwise.
int kvs_disconnect(char const* req_pipe_path, char const* resp_pipe_path, char const* notifications_pipe_path, int *fds);

/// Requests a subscription for a key
/// @param key Key to be subscribed
/// @return 1 if the key was subscribed successfully (key existing), 0 otherwise.

int kvs_subscribe(const char* key, const int req_fd, const int resp_fd);

/// Remove a subscription for a key
/// @param key Key to be unsubscribed
/// @return 0 if the key was unsubscribed successfully  (subscription existed and was removed), 1 otherwise.

int kvs_unsubscribe(const char* key, const int req_fd, const int resp_fd);

int create_pipes(char const* req_pipe_path, char const* resp_pipe_path,char const* notifications_pipe_path);
 
#endif  // CLIENT_API_H
