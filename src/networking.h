#ifndef NETWORKING_H
#define NETWORKING_H

// Function to extract network portion of an IP
void get_network_address(const char *ip_address, char *network, const char *netmask);

// Add routing table entry for cross-subnet communication
int add_route(const char *target_network, const char *netmask, const char *gateway);

// Get default gateway IP
char *get_default_gateway();

#endif /* NETWORKING_H */ 