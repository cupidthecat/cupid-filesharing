#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include "cupid.h"

// Shared directory path
static char shared_directory[MAX_PATH_LENGTH];

// Function to display server's network interfaces and IP addresses
void display_server_ip() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    int found = 0;
    
    // Get list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("Error getting network interfaces");
        return;
    }
    
    printf("Server IP addresses:\n");
    
    // Iterate through interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
            
        family = ifa->ifa_addr->sa_family;
        
        // Only display IPv4 addresses
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("Error getting address: %s\n", gai_strerror(s));
                continue;
            }
            
            // Don't display loopback addresses (127.0.0.1)
            if (strncmp(host, "127.", 4) == 0)
                continue;
                
            printf("  %s: %s\n", ifa->ifa_name, host);
            found = 1;
        }
    }
    
    if (!found) {
        printf("  No non-loopback IP addresses found.\n");
        printf("  Server accessible at localhost (127.0.0.1)\n");
    }
    
    freeifaddrs(ifaddr);
    
    printf("\nNOTE: If client and server are on different subnets (e.g., 172.x.x.x vs 10.x.x.x),\n");
    printf("you may need to adjust your network settings. Options include:\n");
    printf("1. Connect both devices to the same network type (both wired or both wireless)\n");
    printf("2. Configure your router to allow cross-subnet communication\n");
    printf("3. Use port forwarding on your router\n");
}

// Function to check if an IP address exists on any interface
int ip_exists_on_interface(const char *ip_to_check) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    int found = 0;
    
    // Get list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("Error getting network interfaces");
        return 0;
    }
    
    // Iterate through interfaces
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
            
        family = ifa->ifa_addr->sa_family;
        
        // Only check IPv4 addresses
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                continue;
            }
            
            if (strcmp(host, ip_to_check) == 0) {
                found = 1;
                break;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return found;
}

// Get best IP to use for binding
char *get_best_bind_ip() {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    static char best_ip[NI_MAXHOST];
    best_ip[0] = '\0';
    
    // Get list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("Error getting network interfaces");
        return NULL;
    }
    
    // First, try to find a 10.x.x.x address (common for local networks)
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
            
        family = ifa->ifa_addr->sa_family;
        
        if (family == AF_INET) {
            s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                    host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0 || strncmp(host, "127.", 4) == 0)
                continue;
                
            if (strncmp(host, "10.", 3) == 0) {
                strcpy(best_ip, host);
                break;
            }
        }
    }
    
    // If no 10.x.x.x address, try 192.168.x.x
    if (best_ip[0] == '\0') {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL)
                continue;
                
            family = ifa->ifa_addr->sa_family;
            
            if (family == AF_INET) {
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                        host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                if (s != 0 || strncmp(host, "127.", 4) == 0)
                    continue;
                    
                if (strncmp(host, "192.168.", 8) == 0) {
                    strcpy(best_ip, host);
                    break;
                }
            }
        }
    }
    
    // If still no IP, use any non-loopback IP
    if (best_ip[0] == '\0') {
        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL)
                continue;
                
            family = ifa->ifa_addr->sa_family;
            
            if (family == AF_INET) {
                s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                        host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                if (s != 0 || strncmp(host, "127.", 4) == 0)
                    continue;
                    
                strcpy(best_ip, host);
                break;
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return best_ip[0] != '\0' ? best_ip : NULL;
}

// Structure to pass data to client handler thread
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} client_data_t;

// Handle list files request
void handle_list_files(int client_socket) {
    DIR *dir;
    struct dirent *entry;
    char response[MAX_PACKET_SIZE];
    int response_len = 0;
    
    // Set command code for response
    response[0] = CMD_LIST_FILES;
    response_len = 1;
    
    dir = opendir(shared_directory);
    if (dir == NULL) {
        fprintf(stderr, "Error opening directory: %s\n", strerror(errno));
        response[0] = CMD_ERROR;
        strcpy(response + 1, "Error opening directory");
        send(client_socket, response, strlen(response + 1) + 1, 0);
        return;
    }
    
    // List files in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip directories and hidden files
        if (entry->d_name[0] == '.' || entry->d_type == DT_DIR) {
            continue;
        }
        
        // Add filename to response
        int name_len = strlen(entry->d_name);
        if (response_len + name_len + 1 >= MAX_PACKET_SIZE) {
            break; // Prevent buffer overflow
        }
        
        strcpy(response + response_len, entry->d_name);
        response_len += name_len;
        response[response_len++] = '\n';
    }
    
    if (response_len == 1) {
        // No files found
        strcpy(response + response_len, "No files available");
        response_len += strlen("No files available");
    } else {
        // Replace last newline with null terminator
        response[response_len - 1] = '\0';
    }
    
    // Send response
    send(client_socket, response, response_len, 0);
    closedir(dir);
}

// Handle get file request
void handle_get_file(int client_socket, const char *filename) {
    char filepath[MAX_PATH_LENGTH];
    char buffer[MAX_PACKET_SIZE];
    int file_fd;
    ssize_t bytes_read;
    
    // Check for path traversal attacks
    if (strstr(filename, "..") != NULL) {
        buffer[0] = CMD_ERROR;
        strcpy(buffer + 1, "Invalid filename");
        send(client_socket, buffer, strlen(buffer + 1) + 1, 0);
        return;
    }
    
    // Construct full file path
    snprintf(filepath, sizeof(filepath), "%s/%s", shared_directory, filename);
    
    // Open the file
    file_fd = open(filepath, O_RDONLY);
    if (file_fd == -1) {
        buffer[0] = CMD_ERROR;
        strcpy(buffer + 1, "File not found or cannot be accessed");
        send(client_socket, buffer, strlen(buffer + 1) + 1, 0);
        return;
    }
    
    // Send file data
    buffer[0] = CMD_FILE_DATA;
    
    while ((bytes_read = read(file_fd, buffer + 1, MAX_PACKET_SIZE - 1)) > 0) {
        if (send(client_socket, buffer, bytes_read + 1, 0) <= 0) {
            break;
        }
    }
    
    close(file_fd);
}

// Client handler thread function
void *handle_client(void *arg) {
    client_data_t *client_data = (client_data_t *)arg;
    int client_socket = client_data->client_socket;
    struct sockaddr_in client_addr = client_data->client_addr;
    char buffer[MAX_PACKET_SIZE];
    ssize_t bytes_received;
    
    printf("Client connected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    
    // Receive command from client
    bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        // Process command
        switch (buffer[0]) {
            case CMD_LIST_FILES:
                handle_list_files(client_socket);
                break;
                
            case CMD_GET_FILE:
                handle_get_file(client_socket, buffer + 1);
                break;
                
            default:
                // Unknown command
                buffer[0] = CMD_ERROR;
                strcpy(buffer + 1, "Unknown command");
                send(client_socket, buffer, strlen(buffer + 1) + 1, 0);
                break;
        }
    }
    
    close(client_socket);
    free(client_data);
    printf("Client disconnected from %s:%d\n", 
           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
    return NULL;
}

// Start the file sharing server
int start_server(const char *directory, const char *bind_ip) {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t thread_id;
    char *auto_bind_ip = NULL;
    
    // Store shared directory
    strncpy(shared_directory, directory, MAX_PATH_LENGTH - 1);
    shared_directory[MAX_PATH_LENGTH - 1] = '\0';
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        return EXIT_FAILURE;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("Error setting socket options");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    // Prepare server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    
    // Bind to specific IP if provided, otherwise try to automatically select best IP
    if (bind_ip != NULL && strlen(bind_ip) > 0) {
        // Check if the IP address is valid
        if (inet_pton(AF_INET, bind_ip, &server_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid IP address: %s\n", bind_ip);
            close(server_socket);
            return EXIT_FAILURE;
        }
        
        // Check if the IP exists on any interface
        if (!ip_exists_on_interface(bind_ip)) {
            fprintf(stderr, "Warning: IP address %s doesn't exist on any network interface.\n", bind_ip);
            
            // Try to find the best IP automatically
            auto_bind_ip = get_best_bind_ip();
            if (auto_bind_ip != NULL) {
                fprintf(stderr, "Auto-selecting best IP address: %s\n", auto_bind_ip);
                if (inet_pton(AF_INET, auto_bind_ip, &server_addr.sin_addr) <= 0) {
                    fprintf(stderr, "Error with auto-selected IP. Falling back to INADDR_ANY\n");
                    server_addr.sin_addr.s_addr = INADDR_ANY;
                }
            } else {
                fprintf(stderr, "Could not auto-select IP. Falling back to INADDR_ANY\n");
                server_addr.sin_addr.s_addr = INADDR_ANY;
            }
        } else {
            printf("Binding to specific IP address: %s\n", bind_ip);
        }
    } else {
        // Try to auto-select best IP
        auto_bind_ip = get_best_bind_ip();
        if (auto_bind_ip != NULL) {
            printf("Auto-selecting best IP address: %s\n", auto_bind_ip);
            if (inet_pton(AF_INET, auto_bind_ip, &server_addr.sin_addr) <= 0) {
                fprintf(stderr, "Error with auto-selected IP. Falling back to INADDR_ANY\n");
                server_addr.sin_addr.s_addr = INADDR_ANY;
            }
        } else {
            server_addr.sin_addr.s_addr = INADDR_ANY;
            printf("Binding to all available interfaces (INADDR_ANY)\n");
        }
    }
    
    server_addr.sin_port = htons(CUPID_PORT);
    
    // Bind socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding socket");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    // Listen for connections
    if (listen(server_socket, 10) == -1) {
        perror("Error listening on socket");
        close(server_socket);
        return EXIT_FAILURE;
    }
    
    printf("Cupid server started. Sharing directory: %s\n", shared_directory);
    display_server_ip();
    printf("Listening on port %d...\n", CUPID_PORT);
    
    // Accept client connections
    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }
        
        // Create client data structure
        client_data_t *client_data = malloc(sizeof(client_data_t));
        if (client_data == NULL) {
            perror("Error allocating memory");
            close(client_socket);
            continue;
        }
        
        client_data->client_socket = client_socket;
        client_data->client_addr = client_addr;
        
        // Create thread to handle client
        if (pthread_create(&thread_id, NULL, handle_client, client_data) != 0) {
            perror("Error creating thread");
            free(client_data);
            close(client_socket);
            continue;
        }
        
        // Detach thread
        pthread_detach(thread_id);
    }
    
    // Never reached, but good practice
    close(server_socket);
    return EXIT_SUCCESS;
} 