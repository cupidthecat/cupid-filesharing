#ifndef CUPID_H
#define CUPID_H

// Port for the file sharing service
#define CUPID_PORT 9876

// Maximum size of file path
#define MAX_PATH_LENGTH 256

// Maximum size of a single packet
#define MAX_PACKET_SIZE 8192

// Command codes
#define CMD_LIST_FILES 1
#define CMD_GET_FILE 2
#define CMD_FILE_DATA 3
#define CMD_ERROR 4

// Function prototypes
int start_server(const char *directory);
int list_files(const char *server_ip);
int get_file(const char *server_ip, const char *filename);

#endif /* CUPID_H */ 