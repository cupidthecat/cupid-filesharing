#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "networking.h"

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