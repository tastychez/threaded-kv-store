#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// port number the server is listening on (must match server)
#define PORT 8888
// size of buffer for reading/writing data
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    /*
     * check if user provided at least one command-line argument
     * argc = argument count (number of command-line arguments)
     * argv[0] = program name (like "./client")
     * argv[1] = first argument (like "SET")
     * if argc < 2, that means no arguments were provided
     */
    if (argc < 2) {
        // print usage instructions to stderr (error output)
        fprintf(stderr, "Usage: %s <command> [key] [value]\n", argv[0]);
        fprintf(stderr, "Examples:\n");
        fprintf(stderr, "  %s SET name Hong\n", argv[0]);
        fprintf(stderr, "  %s GET name\n", argv[0]);
        fprintf(stderr, "  %s DELETE name\n", argv[0]);
        exit(1);  // exit with error code
    }
    
    int sock_fd; // file descriptor for the socket (like a file handle)
    struct sockaddr_in server_addr;    // structure to hold the server's network address
    char buffer[BUFFER_SIZE];          // buffer to store the command we'll send
    char response[BUFFER_SIZE];        // buffer to store the response we'll receive
    
    /*
     * create a socket
     * AF_INET = ipv4 internet protocol
     * SOCK_STREAM = tcp (reliable, connection-based)
     * 0 = use default protocol for tcp
     * returns a file descriptor that we use to refer to this socket
     */
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket failed");
        exit(1);
    }
    
    /*
     * configure the server's network address
     * we need to tell the socket where to connect (localhost on port 8888)
     */
    memset(&server_addr, 0, sizeof(server_addr));  // zero out the structure
    server_addr.sin_family = AF_INET;               // use ipv4
    server_addr.sin_port = htons(PORT);             // convert port number to network byte order
                                                    // htons = "host to network short" (converts endianness)
    
    /*
     * convert the text "127.0.0.1" (localhost) to a binary ip address
     * inet_pton = "internet presentation to network" (text to binary)
     * this stores the ip address in server_addr.sin_addr
     */
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton failed");
        exit(1);
    }
    
    /*
     * connect to the server
     * this establishes a tcp connection to the server at the address we configured
     * if the server isn't running, this will fail
     */
    if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        exit(1);
    }
    
    /*
     * build the command string from command-line arguments
     * example: if user ran "./client SET name Hong"
     * then argv[1]="SET", argv[2]="name", argv[3]="Hong"
     * we want to build the string "SET name Hong"
     */
    memset(buffer, 0, sizeof(buffer));  // clear the buffer (fill with zeros)
    // loop through all arguments (starting at 1, since 0 is the program name)
    for (int i = 1; i < argc; i++) {
        // add a space before each argument except the first one
        if (i > 1) {
            strcat(buffer, " ");  // append a space to the buffer
        }
        // append the argument to the buffer
        // strncat is safer than strcat because it limits the number of characters
        strncat(buffer, argv[i], sizeof(buffer) - strlen(buffer) - 1);
    }
    
    /*
     * send the command to the server
     * write() sends data over the socket to the server
     * we send the entire command string we just built
     */
    if (write(sock_fd, buffer, strlen(buffer)) < 0) {
        perror("write failed");
        close(sock_fd);
        exit(1);
    }
    
    /*
     * read the response from the server
     * read() blocks (waits) until data arrives from the server
     * BUFFER_SIZE - 1 leaves room for the null terminator '\0'
     * bytes_read tells us how many bytes were actually received
     */
    ssize_t bytes_read = read(sock_fd, response, sizeof(response) - 1);
    if (bytes_read < 0) {
        perror("read failed");
        close(sock_fd);
        exit(1);
    }
    
    // add null terminator to make it a proper c string
    // this tells c where the string ends
    response[bytes_read] = '\0';
    
    /*
     * print the response to the console
     * the server sends back things like "OK", "NOT_FOUND", or the actual value
     */
    printf("%s\n", response);
    
    // close the socket connection
    close(sock_fd);
    return 0;  // exit successfully
}
