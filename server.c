#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include "uthash.h"

// port number the server will listen on
#define PORT 8888
// size of buffer for reading/writing data
#define BUFFER_SIZE 1024

/*
 * hash table entry structure
 * this is what gets stored in our key-value store
 * each entry has a key, a value, and a special field (hh) that uthash needs
 */
typedef struct {
    char key[256];        // the key (like "name")
    char value[256];      // the value (like "Hong")
    UT_hash_handle hh;    // special field required by uthash library to make this hashable
} kv_entry_t;

// global hash table - this is where all key-value pairs are stored
// starts as null (empty) and gets entries added as clients send SET commands
kv_entry_t *kv_store = NULL;

/*
 * mutex (mutual exclusion lock)
 * this protects the hash table from race conditions when multiple threads
 * try to access it at the same time. think of it like a bathroom key -
 * only one thread can "have the key" (be locked) at a time, so only one
 * thread can modify the hash table at a time. this prevents data corruption.
 */
pthread_mutex_t kv_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * function that handles each client connection
 * this function runs in a separate thread for each client
 * the arg parameter contains the client's socket file descriptor
 */
void* handle_client(void* arg) {
    // extract the client socket file descriptor from the argument
    // we passed a pointer to an int, so we need to dereference it
    int client_fd = *(int*)arg;
    char buffer[BUFFER_SIZE];      // buffer to read incoming commands from client
    char response[BUFFER_SIZE];    // buffer to store response we'll send back
    
    /*
     * read the command from the client
     * read() blocks (waits) until data arrives from the client
     * BUFFER_SIZE - 1 leaves room for the null terminator '\0'
     */
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    // if read failed or connection closed, clean up and exit this thread
    if (bytes_read <= 0) {
        close(client_fd);  // close the socket
        free(arg);         // free the memory we allocated for the file descriptor
        return NULL;       // exit this thread
    }
    
    // add null terminator to make it a proper c string
    // this tells c where the string ends
    buffer[bytes_read] = '\0';
    
    /*
     * parse the command string
     * commands are space-delimited like "SET name Hong" or "GET name"
     * sscanf reads formatted input from the buffer string
     * %31s means read up to 31 characters (leaving room for null terminator)
     * parsed tells us how many items were successfully read
     */
    char cmd[32], key[256], value[256];
    int parsed = sscanf(buffer, "%31s %255s %255s", cmd, key, value);
    
    /*
     * MUTEX LOCK 
     * lock the mutex before accessing the hash table
     * this ensures only one thread can modify the hash table at a time
     * if another thread already has the lock, this thread will wait here
     */
    pthread_mutex_lock(&kv_mutex);
    
    /*
     * handle SET command: store a key-value pair
     * example: "SET name Hong" stores key="name", value="Hong"
     * parsed >= 3 means we got command, key, and value
     */
    if (strcmp(cmd, "SET") == 0 && parsed >= 3) {
        kv_entry_t *entry;
        // search the hash table to see if this key already exists
        HASH_FIND_STR(kv_store, key, entry);
        
        if (entry) {
            // key already exists, so update the existing value
            // strncpy safely copies the value, limiting the size to prevent overflow
            strncpy(entry->value, value, sizeof(entry->value) - 1);
            // ensure null terminator is in place
            entry->value[sizeof(entry->value) - 1] = '\0';
        } else {
            // key doesn't exist, so create a new entry
            // allocate memory for the new entry
            entry = (kv_entry_t*)malloc(sizeof(kv_entry_t));
            // copy the key into the entry
            strncpy(entry->key, key, sizeof(entry->key) - 1);
            entry->key[sizeof(entry->key) - 1] = '\0';
            // copy the value into the entry
            strncpy(entry->value, value, sizeof(entry->value) - 1);
            entry->value[sizeof(entry->value) - 1] = '\0';
            // add the entry to the hash table
            HASH_ADD_STR(kv_store, key, entry);
        }
        strcpy(response, "OK");
        
    /*
     * handle GET command: retrieve a value for a key
     * example: "GET name" returns the value stored for "name"
     * parsed >= 2 means we got command and key
     */
    } else if (strcmp(cmd, "GET") == 0 && parsed >= 2) {
        kv_entry_t *entry;
        // search the hash table for the key
        HASH_FIND_STR(kv_store, key, entry);
        
        if (entry) {
            // key found, copy the value to the response
            strncpy(response, entry->value, sizeof(response) - 1);
            response[sizeof(response) - 1] = '\0';
        } else {
            // key not found in the hash table
            strcpy(response, "NOT_FOUND");
        }
        
    /*
     * handle DELETE command: remove a key-value pair
     * example: "DELETE name" removes the "name" entry
     * parsed >= 2 means we got command and key
     */
    } else if (strcmp(cmd, "DELETE") == 0 && parsed >= 2) {
        kv_entry_t *entry;
        // search the hash table for the key
        HASH_FIND_STR(kv_store, key, entry);
        
        if (entry) {
            // key found, remove it from the hash table
            HASH_DEL(kv_store, entry);
            // free the memory that was allocated for this entry
            free(entry);
            strcpy(response, "OK");
        } else {
            // key doesn't exist, but we still return OK
            // this is called "idempotent" - deleting something that doesn't exist
            // is the same as deleting something that does exist (both result in it not existing)
            strcpy(response, "OK");
        }
        
    } else {
        // invalid command or wrong number of arguments
        strcpy(response, "ERROR");
    }
    
    /*
     * MUTEX UNLOCK 
     * unlock the mutex after we're done with the hash table
     * this allows other waiting threads to now access the hash table
     */
    pthread_mutex_unlock(&kv_mutex);
    
    /*
     * send the response back to the client
     * write() sends data over the socket to the client
     */
    write(client_fd, response, strlen(response));
    
    // clean up: close the socket and free the memory we allocated
    close(client_fd);
    free(arg);
    
    // return null to indicate this thread is done
    return NULL;
}

int main() {
    int server_fd, client_fd;                    // file descriptors for server and client sockets
    struct sockaddr_in server_addr, client_addr;  // structures to hold network addresses
    socklen_t client_len = sizeof(client_addr);   // size of client address structure
    
    /*
     * create a socket
     * AF_INET = ipv4 internet protocol
     * SOCK_STREAM = tcp (reliable, connection-based)
     * 0 = use default protocol for tcp
     * returns a file descriptor (like a file handle) that we use to refer to this socket
     */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        exit(1);
    }
    
    /*
     * set socket option to allow address reuse
     * this lets us restart the server immediately without waiting
     * for the port to be released (otherwise we'd get "address already in use" error)
     */
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /*
     * configure the server's network address
     * we need to tell the socket what ip address and port to listen on
     */
    memset(&server_addr, 0, sizeof(server_addr));  // zero out the structure
    server_addr.sin_family = AF_INET;               // use ipv4
    server_addr.sin_addr.s_addr = INADDR_ANY;       // listen on all network interfaces (0.0.0.0)
    server_addr.sin_port = htons(PORT);             // convert port number to network byte order
    
    /*
     * bind the socket to the address
     * this associates our socket with the ip address and port
     * clients will connect to this address and port
     */
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(1);
    }
    
    /*
     * start listening for incoming connections
     * the socket is now ready to accept connections
     * 5 is the backlog - how many pending connections can queue up
     */
    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        exit(1);
    }
    
    printf("Server listening on port %d\n", PORT);
    
    /*
     * MAIN ACCEPT LOOP
     * this loop runs forever, accepting new client connections
     * for each connection, we spawn a new thread to handle it
     */
    while (1) {
        /*
         * accept a new client connection
         * accept() blocks (waits) until a client connects
         * when a client connects, it returns a new file descriptor for that client
         * the original server_fd continues listening for more connections
         */
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept failed");
            continue;  // if accept fails, just try again
        }
        
        /*
         * allocate memory to store the client file descriptor
         * we need to pass this to the thread, but we can't just pass the value
         * because the variable might change before the thread reads it
         * so we allocate memory on the heap and pass a pointer to it
         */
        int *client_fd_ptr = malloc(sizeof(int));
        *client_fd_ptr = client_fd;  // store the file descriptor in the allocated memory
        
        /*
         * create a new thread to handle this client
         * pthread_create spawns a new thread that runs the handle_client function
         * the new thread runs independently, so the main thread can immediately
         * go back to accepting more connections
         */
        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, client_fd_ptr) != 0) {
            perror("pthread_create failed");
            close(client_fd);
            free(client_fd_ptr);
            continue;  // if thread creation fails, try accepting next connection
        }
        
        /*
         * detach the thread so it cleans up automatically when done
         * without detaching, we'd need to call pthread_join() later to clean up
         * detached threads clean themselves up automatically
         */
        pthread_detach(thread_id);
    }
    
    // this code never runs because of the infinite loop above
    // but it's good practice to close the socket if we ever exit
    close(server_fd);
    return 0;
}
