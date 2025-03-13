#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cupid.h"

void print_usage() {
    printf("Cupid - LAN File Sharing Program\n\n");
    printf("Usage:\n");
    printf("  Server mode: cupid server [directory_to_share]\n");
    printf("  List files:  cupid list [server_ip]\n");
    printf("  Get file:    cupid get [server_ip] [filename]\n");
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return EXIT_FAILURE;
    }

    // Parse command
    if (strcmp(argv[1], "server") == 0) {
        char *directory = ".";  // Default to current directory
        if (argc >= 3) {
            directory = argv[2];
        }
        return start_server(directory);
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