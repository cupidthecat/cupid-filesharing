#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include "cupid.h"

// Function to extract network portion of an IP
void get_network_address(const char *ip_address, char *network, const char *netmask) {
    struct in_addr ip, mask, net;
    
    if (inet_pton(AF_INET, ip_address, &ip) <= 0 ||
        inet_pton(AF_INET, netmask, &mask) <= 0) {
        // Error in conversion
        strcpy(network, "0.0.0.0");
        return;
    }
    
    // Calculate network address
    net.s_addr = ip.s_addr & mask.s_addr;
    
    // Convert back to string
    inet_ntop(AF_INET, &net, network, INET_ADDRSTRLEN);
}

// Add routing table entry for cross-subnet communication
int add_route(const char *target_network, const char *netmask, const char *gateway) {
    char cmd[256];
    int result;
    
    // Check if running as root
    if (geteuid() != 0) {
        fprintf(stderr, "Warning: Adding routes requires root privileges.\n");
        fprintf(stderr, "To add route manually: sudo ip route add %s/%s via %s\n", 
                target_network, netmask, gateway);
        return -1;
    }
    
    // Prepare and execute route command
    snprintf(cmd, sizeof(cmd), "ip route add %s/%s via %s 2>/dev/null", 
             target_network, netmask, gateway);
    result = system(cmd);
    
    if (result != 0) {
        // Route might already exist or other error
        fprintf(stderr, "Note: Route may already exist or could not be added.\n");
        return -1;
    }
    
    printf("Added route to %s/%s via %s\n", target_network, netmask, gateway);
    return 0;
}

// Get default gateway IP
char *get_default_gateway() {
    FILE *fp;
    static char gateway[INET_ADDRSTRLEN];
    char line[256], *p;
    
    gateway[0] = '\0';
    
    // Read route table
    fp = fopen("/proc/net/route", "r");
    if (fp == NULL) {
        perror("Error opening route table");
        return NULL;
    }
    
    // Skip header line
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return NULL;
    }
    
    // Look for default route (destination 0.0.0.0)
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Find the gateway field (3rd field)
        char iface[16], dest[9], gate[9], flags[9];
        int count = sscanf(line, "%s %s %s %s", iface, dest, gate, flags);
        
        if (count == 4 && strcmp(dest, "00000000") == 0) {
            // Convert hex gateway to IP format
            unsigned int g1, g2, g3, g4;
            sscanf(gate, "%02x%02x%02x%02x", &g1, &g2, &g3, &g4);
            snprintf(gateway, sizeof(gateway), "%u.%u.%u.%u", g4, g3, g2, g1);
            break;
        }
    }
    
    fclose(fp);
    return gateway[0] != '\0' ? gateway : NULL;
}

// Function to determine if IPs are on the same subnet
int is_same_subnet(const char *ip1, const char *ip2, const char *mask) {
    struct in_addr addr1, addr2, netmask;
    
    if (inet_pton(AF_INET, ip1, &addr1) <= 0 ||
        inet_pton(AF_INET, ip2, &addr2) <= 0 ||
        inet_pton(AF_INET, mask, &netmask) <= 0) {
        return 0; // Error in conversion
    }
    
    // Compare network portions
    return ((addr1.s_addr & netmask.s_addr) == (addr2.s_addr & netmask.s_addr));
}

// Get local IP that's on the same subnet as the target IP
char *get_matching_local_ip(const char *target_ip) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[NI_MAXHOST];
    static char matching_ip[NI_MAXHOST];
    matching_ip[0] = '\0';
    
    // Determine target subnet
    char first_octet[4];
    strncpy(first_octet, target_ip, 3);
    first_octet[3] = '\0';
    
    // Get list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("Error getting network interfaces");
        return NULL;
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
            
            // Skip loopback addresses
            if (strncmp(host, "127.", 4) == 0)
                continue;
            
            // Check if this interface is on the same subnet as target
            if (is_same_subnet(host, target_ip, "255.255.0.0") || 
                strncmp(host, first_octet, strlen(first_octet)) == 0) {
                strcpy(matching_ip, host);
                break;
            }
            
            // If we haven't found a match yet, store this as a potential match
            if (matching_ip[0] == '\0') {
                strcpy(matching_ip, host);
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return matching_ip[0] != '\0' ? matching_ip : NULL;
}

// Connect to a server with intelligent routing
int connect_to_server(const char *server_ip) {
    int client_socket;
    struct sockaddr_in server_addr, client_addr;
    char *local_ip;
    char server_network[INET_ADDRSTRLEN];
    char local_network[INET_ADDRSTRLEN];
    char *gateway;
    
    printf("Connecting to server at %s:%d...\n", server_ip, CUPID_PORT);
    
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
    
    // Try direct connection first
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != -1) {
        printf("Connected directly to server\n");
        return client_socket;
    }
    
    // If direct connection fails, try to find a matching local IP
    local_ip = get_matching_local_ip(server_ip);
    if (local_ip != NULL) {
        printf("Direct connection failed. Trying with local IP %s...\n", local_ip);
        
        // Close the previous socket and create a new one
        close(client_socket);
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket == -1) {
            perror("Error creating socket");
            return -1;
        }
        
        // Bind to the specific local IP
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sin_family = AF_INET;
        client_addr.sin_port = 0; // Let the OS choose a port
        if (inet_pton(AF_INET, local_ip, &client_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid local address: %s\n", local_ip);
            close(client_socket);
            return -1;
        }
        
        if (bind(client_socket, (struct sockaddr *)&client_addr, sizeof(client_addr)) == -1) {
            perror("Failed to bind to local address");
            close(client_socket);
            return -1;
        }
        
        // Try to connect again with bound socket
        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != -1) {
            printf("Connected to server using local IP %s\n", local_ip);
            return client_socket;
        }
    }
    
    // If we're still here, try to add a route to the server's network
    get_network_address(server_ip, server_network, "255.255.0.0");
    if (local_ip) {
        get_network_address(local_ip, local_network, "255.255.0.0");
        
        if (strcmp(server_network, local_network) != 0) {
            printf("Server is on a different subnet (%s vs %s)\n", 
                   server_network, local_network);
            
            // Try to get default gateway
            gateway = get_default_gateway();
            if (gateway != NULL) {
                printf("Attempting to add route to server network via default gateway %s\n", gateway);
                if (add_route(server_network, "16", gateway) == 0) {
                    // Try connecting again after adding route
                    close(client_socket);
                    return connect_to_server(server_ip); // Recursive call after adding route
                }
            } else {
                // Try direct routing through server
                printf("Attempting to add direct route to server network\n");
                if (add_route(server_network, "16", server_ip) == 0) {
                    // Try connecting again after adding route
                    close(client_socket);
                    return connect_to_server(server_ip); // Recursive call after adding route
                }
            }
        }
    }
    
    // If we're here, all connection attempts failed
    perror("Connection failed");
    fprintf(stderr, "Could not connect to server at %s:%d\n", server_ip, CUPID_PORT);
    fprintf(stderr, "Possible solutions:\n");
    fprintf(stderr, "1. Make sure server and client are on the same network or can route to each other\n");
    fprintf(stderr, "2. Check if server is running and bound to the correct IP\n");
    fprintf(stderr, "3. Check if any firewall is blocking the connection\n");
    fprintf(stderr, "4. Try running the client with sudo to enable automatic route configuration\n");
    
    close(client_socket);
    return -1;
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