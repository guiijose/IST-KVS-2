# Operating Systems Project - IST-KVS

## Overview
This project is part of the Operating Systems course at Instituto Superior Técnico (IST) and focuses on implementing a key-value store (IST-KVS) with client-server communication using named pipes and signals. The server manages multiple client connections, allowing them to monitor key-value pairs, subscribe to changes, and receive notifications.

## Features
- **Client-Server Communication**: Uses named pipes (FIFOs) to enable interaction between clients and the server.
- **Key-Value Subscription**: Clients can subscribe to specific keys and receive real-time updates when they change.
- **Multi-Threaded Server**: Supports multiple concurrent client sessions.
- **Signal Handling**: Implements SIGUSR1 signal to disconnect all clients and reset subscriptions.
- **Process Synchronization**: Uses semaphores and mutexes for thread coordination.

## Compilation and Execution
### Compilation
To compile the project, run:
```sh
make clean all
```
This command ensures that both binary and output files are removed before compilation.
### Running the Server
```sh
./server <dir_jobs> <max_threads> <backups_max> <register_fifo>
```
- `dir_jobs`: Directory containing job files.
- `max_threads`: Maximum number of concurrent job-processing threads.
- `backups_max`: Maximum number of concurrent backups.
- `register_fifo`: Named pipe for client registration.

### Running a Client
```sh
./client <client_id> <register_fifo>
```
- `client_id`: Unique identifier for the client.
- `register_fifo`: Named pipe for connecting to the server.

## Project Structure
```
.
└── src
    ├── client # Client side code
    ├── common # Utility functions that are both used in server and client side
    └── server # Server side code
        └── jobs # Example of job files to run
```

## Testing
To run a test scenario:
```sh
./client <client_id> <register_fifo> < test_client.txt
```
This will execute predefined commands stored in `test_client.txt`.

## Contact
For any inquiries regarding this project, feel free to reach out.

---
This project was completed as part of the **Operating Systems** course at IST.

