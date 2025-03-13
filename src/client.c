#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "cupid.h"

// Connect to a server
int connect_to_server(const char *server_ip) {
    int client_socket;
    struct sockaddr_in server_addr;
    
    // Create socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Error creating socket");
        return -1;
    }
    
    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CUPID_PORT);
    
    // Convert IP address to binary form
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address: %s\n", server_ip);
        close(client_socket);
        return -1;
    }
    
    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        close(client_socket);
        return -1;
    }
    
    return client_socket;
}

// List files available on server
int list_files(const char *server_ip) {
    int client_socket;
    char buffer[MAX_PACKET_SIZE];
    ssize_t bytes_received;
    
    // Connect to server
    client_socket = connect_to_server(server_ip);
    if (client_socket == -1) {
        return EXIT_FAILURE;
    }
    
    // Send list files command
    buffer[0] = CMD_LIST_FILES;
    if (send(client_socket, buffer, 1, 0) <= 0) {
        perror("Error sending command");
        close(client_socket);
        return EXIT_FAILURE;
    }
    
    // Receive response
    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        perror("Error receiving response");
        close(client_socket);
        return EXIT_FAILURE;
    }
    
    // Process response
    if (buffer[0] == CMD_ERROR) {
        fprintf(stderr, "Server error: %s\n", buffer + 1);
        close(client_socket);
        return EXIT_FAILURE;
    } else if (buffer[0] == CMD_LIST_FILES) {
        printf("Files available on server %s:\n", server_ip);
        printf("%s\n", buffer + 1);
    }
    
    close(client_socket);
    return EXIT_SUCCESS;
}

// Get file from server
int get_file(const char *server_ip, const char *filename) {
    int client_socket, file_fd;
    char buffer[MAX_PACKET_SIZE];
    ssize_t bytes_received;
    
    // Connect to server
    client_socket = connect_to_server(server_ip);
    if (client_socket == -1) {
        return EXIT_FAILURE;
    }
    
    // Send get file command
    buffer[0] = CMD_GET_FILE;
    strcpy(buffer + 1, filename);
    if (send(client_socket, buffer, strlen(filename) + 2, 0) <= 0) {
        perror("Error sending command");
        close(client_socket);
        return EXIT_FAILURE;
    }
    
    // Create local file for writing
    file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd == -1) {
        perror("Error creating local file");
        close(client_socket);
        return EXIT_FAILURE;
    }
    
    printf("Downloading %s from %s...\n", filename, server_ip);
    
    // Receive file data
    size_t total_bytes = 0;
    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        if (buffer[0] == CMD_ERROR) {
            fprintf(stderr, "Server error: %s\n", buffer + 1);
            close(file_fd);
            close(client_socket);
            unlink(filename); // Delete partial file
            return EXIT_FAILURE;
        } else if (buffer[0] == CMD_FILE_DATA) {
            // Write data to file
            if (write(file_fd, buffer + 1, bytes_received - 1) != bytes_received - 1) {
                perror("Error writing to file");
                close(file_fd);
                close(client_socket);
                unlink(filename); // Delete partial file
                return EXIT_FAILURE;
            }
            total_bytes += bytes_received - 1;
        }
    }
    
    close(file_fd);
    close(client_socket);
    
    if (bytes_received == -1) {
        perror("Error receiving data");
        unlink(filename); // Delete partial file
        return EXIT_FAILURE;
    }
    
    printf("Downloaded %s (%zu bytes)\n", filename, total_bytes);
    return EXIT_SUCCESS;
} 