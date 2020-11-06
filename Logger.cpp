#include <sys/socket.h>
#include <netinet/in.h>
#include <iostream>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "Logger.h"

// The address where the logging server is to be reached
struct sockaddr_in server_address;

// The IP address for the logging server
const std::string SERVER_ADDRESS = "127.0.0.1";

// The port number for the logging server
const int SERVER_PORT = 42424;
// The maximum buffer length for both the receiving buffer and sending buffer
const int RECEIVE_BUFFER_LENGTH = 1024;
const int SEND_BUFFER_LENGTH = 4096;

// The mutex lock for the socket connection
pthread_mutex_t mutex_lock;

// The running flag
bool is_running = true;

// The current level at which to filter logs
LOG_LEVEL log_filter_level;

// The socket which the server is to be reached on
int server_socket_descriptor;

// The recieve thread function which will receive all communication from the 
// logging server
void *recv_func(void *arg) {
    int socket_descriptor = *((int *)arg);

    socklen_t socket_length = sizeof(sockaddr_in);

    char message_buffer[RECEIVE_BUFFER_LENGTH];

    while (is_running) {
        pthread_mutex_lock(&mutex_lock);

        // Receive any data available from the server
        int byte_count = recvfrom(socket_descriptor, message_buffer,
                RECEIVE_BUFFER_LENGTH, MSG_DONTWAIT,
                (struct sockaddr *)&server_address, &socket_length);

        // If there was an error
        if (byte_count == -1) {
            // If the error is that there was no data to be received, we'll
            // sleep for one second before checking again
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                sleep(1);
            // Otherwise there's been another error and we need to print it
            } else {
                // std::cout << "[ERR] Unable to receive data from server:"
                //         << std::endl << strerror(errno) << std::endl;
            }
        } else {
            std::string command(message_buffer);

            // Check if the command from the server is to set the log filter
            // level
            if (command.compare(0, 13, "Set Log Level") == 0) {
                int position;
                // Extract the integer for the filter level and set the global
                // variable
                if ((position = command.find("=")) != std::string::npos) {
                    int level = std::stoi(command.substr(++position));

                    SetLogLevel((LOG_LEVEL)level);
                }
            }
        }

        pthread_mutex_unlock(&mutex_lock);
    }

    return NULL;
}

int InitializeLog() {
    // Setup the socket
    int socket_descriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_descriptor == -1) {
        std::cout << "[ERR] Unable to create Logger socket:" << std::endl
                << strerror(errno) << std::endl;

        exit(EXIT_FAILURE);
    }

    server_socket_descriptor = socket_descriptor;

    // Create the server address and port information
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_addr.s_addr = inet_addr(SERVER_ADDRESS.c_str());
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);

    // Initialize the mutex lock
    pthread_mutex_init(&mutex_lock, NULL);

    pthread_t thread_identifier;

    // Create the receive thread
    int return_code = pthread_create(&thread_identifier, NULL, recv_func, (void *)&server_socket_descriptor);
    if (return_code != 0)
    {
        std::cout << "[ERR]: Unable to create the receive thread." << std::endl;

        exit(EXIT_FAILURE);
    }

    return 0;
}

void SetLogLevel(LOG_LEVEL level) {
    log_filter_level = level;
}

void Log(LOG_LEVEL level, const char *program, const char *function,
        int line, const char *message) {
    // Check if the log level is high enough to pass the filter
    if (level >= log_filter_level) {
        // The different log levels available in char[] form
        char level_strings[][16] = {"DEBUG", "WARNING", "ERROR", "CRITICAL"};

        // Initialize a message buffer for data to be sent
        char message_buffer[SEND_BUFFER_LENGTH];
        memset(message_buffer, 0, SEND_BUFFER_LENGTH);

        // Grab the current time and turn it into a string
        time_t time_in_seconds = time(0);
        char * current_time = ctime(&time_in_seconds);

        // Create a new string in the buffer with all of the relevant
        // information
        int message_length = sprintf(message_buffer, "%s %s %s:%s:%d %s\n",
                current_time,
                level_strings[level],
                program,
                function,
                line,
                message) + 1;

        message_buffer[message_length - 1] = '\0';

        // Send the log information over to the server for processing
        sendto(server_socket_descriptor, message_buffer, message_length, 0,
                (const struct sockaddr *) &server_address,
                sizeof(server_address));
    }
}

void ExitLog() {
    // Stop the receive thread loop
    is_running = false;

    // Close the connection to the logging server
    close(server_socket_descriptor);
}
