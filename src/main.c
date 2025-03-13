#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cupid.h"

void print_usage() {
    printf("Cupid - LAN File Sharing Program\n\n");
    printf("Usage:\n");
    printf("  Server mode: cupid server [directory_to_share] [bind_ip]\n");
    printf("  List files:  cupid list [server_ip]\n");
    printf("  Get file:    cupid get [server_ip] [filename]\n");
    printf("\nExamples:\n");
    printf("  cupid server ./shared_files 192.168.1.5  # Bind to specific IP\n");
    printf("  cupid server ./shared_files              # Bind to all interfaces\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    // Parse command
    if (strcmp(argv[1], "server") == 0) {
        char *directory = ".";  // Default to current directory
        char *bind_ip = NULL;   // Default to all interfaces
        
        if (argc >= 3) {
            directory = argv[2];
        }
        
        if (argc >= 4) {
            bind_ip = argv[3];
        }
        
        return start_server(directory, bind_ip);
    } 
    else if (strcmp(argv[1], "list") == 0) {
        if (argc < 3) {
            printf("Error: Missing server IP address\n");
            print_usage();
            return EXIT_FAILURE;
        }
        return list_files(argv[2]);
    } 
    else if (strcmp(argv[1], "get") == 0) {
        if (argc < 4) {
            printf("Error: Missing server IP address or filename\n");
            print_usage();
            return EXIT_FAILURE;
        }
        return get_file(argv[2], argv[3]);
    } 
    else {
        printf("Unknown command: %s\n", argv[1]);
        print_usage();
        return EXIT_FAILURE;
    }
} 