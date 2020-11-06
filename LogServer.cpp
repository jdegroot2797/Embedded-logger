// LogServer.cpp
// 14-Aug-20  D. Jonathan      Modified

#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mutex>
#include <thread>
#include <queue>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>

// Global Macros
#define MAX_BUF        256

// Logging Macros
#define DEBUG          1
#define WARNING        2
#define ERROR          3
#define CRITICAL       4

// Menu Macros
#define SET_LOG_LEVEL  1
#define DUMP_LOG       2
#define SHUT_DOWN      0
 
using namespace std;

// Socket variables
int fd, rc;
struct sockaddr_in addr, client_addr;
socklen_t socklen;
const int SERVER_PORT = 42424;

// I/O variables
pthread_mutex_t mutex_lock;
pthread_t tid;
char buffer[MAX_BUF];
int len;
const char LOG_FILE[] = "./log";

// Global boolean flags
bool is_running = true;
bool keep_menu_running = true;

// Function declarations
int create_and_bind_socket();
void *recv_func(void *arg);
void shut_down(pthread_t tid);
void set_level();
void dump_log();
static void signalHandler(int signum);

int main() 
{
    // Populate the sigaction struct's members for the handler and the mask
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&(action.sa_mask));
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
    
    // Socket file descriptor for communications
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0) 
    {
        cout << "Log server: Cannot create the socket" << endl;
        cout << strerror(errno) << endl;
        exit(-1);
    }

    // Set socket data members
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    socklen = sizeof(client_addr);
    
    // Bind socket to fd
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        cout << "Log server: Cannot bind the socket to the local address" << endl;
        cout << strerror(errno) << endl;
        close(fd);
        exit(-1);
    }

    // Create thread
    rc = pthread_create(&tid, NULL, recv_func, &fd);
    if(rc !=0)
    {
        cout << "Log server: Cannot create thread" << endl;
        cout << strerror(errno) << endl;
        exit(-1);
    }

    // Initialize mutex
    pthread_mutex_init(&mutex_lock, NULL);

    // Loops menu until while is_running...
    while(is_running)
    {
        // Store user selection
        int user_selection;

        cout << endl << "___________________LOG SERVER___________________" << endl;
        cout << "Please select enter a number to select an option" << endl << endl;
        cout << "1 - Set log level" << endl;
        cout << "2 - Dump log file" << endl; 
        cout << "0 - Shut down" << endl;

        // Take user input
        cin >> user_selection;

        //Ensure input is a number - issue blocks SIGINT
        // while(cin.fail())
        // {
        //     cout << "Error: Please enter a valid number" << endl;
        //     cin.clear();
        //     cin.ignore(256, '\n');
        //     cin >> user_selection;
        // }

        switch(user_selection)
        {
            case(SET_LOG_LEVEL):
                set_level();
                break;

            case(DUMP_LOG):
                dump_log();
                break;

            case(SHUT_DOWN):
                shut_down(tid);
                break;

            default:
                cout << "ERROR: Please enter a valid numerical option" << endl;
        }
    }
    shut_down(tid);
    return 0;
}


void *recv_func(void *arg)
{
    int socket_fd = *(int *)arg;

    // Set 1 second timeout for the socket
    struct timeval time;
    time.tv_sec = 1;

    // Sets socket timeout
    // Reference: https://stackoverflow.com/questions/2876024/linux-is-there-a-read-or-recv-from-socket-with-timeout
    setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &time, sizeof(time));
    
    while(is_running) 
    {
        // Clear buffer and lock mutex
        memset(buffer, 0, MAX_BUF);
        pthread_mutex_lock(&mutex_lock);

        // Get data from client
        int bytes_received = recvfrom(socket_fd, buffer, MAX_BUF, 0, (struct sockaddr *)&client_addr, &socklen);
        string client_message = buffer;

        //open the server log file for write only with permissions rw-rw-rw-
        int fdLog = open(LOG_FILE, O_CREAT | O_WRONLY, 0666);
        close(fdLog);
        FILE* file;
        file =  fopen(LOG_FILE, "a");
        
        // Ensure that data was received
        if(bytes_received > 0)
        {
            // Append new data to log file
            fprintf(file, "%s", client_message.c_str());
        }
        else 
        {
            // Otherwise sleep for 1 second
            sleep(1);
        }

        // Close file and unlock mutex
        fclose(file);
        
        //close(fdLog);
        pthread_mutex_unlock(&mutex_lock);
    }
    return NULL;
}

void send_level(int level)
{   
    // Clear the buffer of existing data and then copy the message into it
    memset(buffer, 0, MAX_BUF);
    len=sprintf(buffer, "Set Log Level=%d", level)+1;
    sendto(fd, buffer, len, 0, (struct sockaddr *)&client_addr, socklen);
}


// The user will be prompted to enter the filter log severity
void set_level()
{
    int user_log_selection;

    // Log level serverity menu
    cout << "_____________________SET SEVERITY___________________"  << endl;
    cout << "Please select enter a number to set log severity level" << endl << endl;
    cout << "1 - Debug" << endl;
    cout << "2 - Warning" << endl; 
    cout << "3 - Error" << endl;
    cout << "4 - Critical" << endl;

    // Take user input
    cin >> user_log_selection;

    //Ensure input is a number - issue blocks SIGINT
    // while(cin.fail())
    // {
    //     cout << "Error: Please enter a valid number" << endl;
    //     cin.clear();
    //     cin.ignore(256, '\n');
    //     cin >> user_log_selection;
    // }

    switch(user_log_selection)
    {
        case(DEBUG):
            send_level(0);
            break;

        case(WARNING):
            send_level(1);
            break;

        case(ERROR):
            send_level(2);
            break;

        case(CRITICAL):
            send_level(3);
            break;

        default:
            cout << "ERROR: Please enter a valid numerical option" << endl;
    }
}


// The server will open its server log file for read only.
// It will read the server’s log file contents and display them on the screen.
// Then it will promp user to "Press any key to continue..."
void dump_log()
{
    // Open file with read only rights
    FILE* file= fopen(LOG_FILE, "r");
    if(file != NULL)
    {
        // Reads each and displays each line of the file
        // until there is no lines left to read.
        char* line;
        size_t size = 0;
        while(getline(&line, &size, file) != -1)
        {
            cout << line << endl;
        }
        fclose(file);
    }
    else
    {
        cout << "Log server: Cannot read log file" << endl;
        cout << strerror(errno) << endl;
        exit(-1);
    }
    
    cout << endl << "Press any key to continue..." << endl;
    cin.get();
}


// Shuts down child intfMonitor processes and cleans up program resources
void shut_down(pthread_t tid)
{
    // The server will exit its user menu. 
    is_running = false;
    
    close(fd);

    // The server will join the receive thread to itself so it doesn’t shut down before the receive thread does.
    pthread_join(tid, NULL);
}

// The signal handler (only used for SIGINT)
static void signalHandler(int signum)
{
    // If user inputs ctrl+c program will exit socket communications loop
    // and begin shutting down child processes and cleanup program resources
    switch(signum) {
        case SIGINT:
            shut_down(tid);
	        break;

        default:
            cout << "LogServer: Undefined signal" << endl;
    }
}