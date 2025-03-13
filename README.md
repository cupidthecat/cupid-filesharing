# Cupid - LAN File Sharing Program

A simple, lightweight file sharing program for Linux systems that operates within a local area network.

## Features

- Share files across Linux devices on the same network
- Simple command-line interface
- Fast transfer speeds using direct TCP/IP connections
- List available files from remote systems
- Download files from peers on the network

## Building

To build the program, simply run:

```
make
```

This will compile the program and create the `cupid` executable.

## Usage

### Start the server

```
./cupid server [directory_to_share]
```

If no directory is specified, the current directory is shared.

### List available files on a remote server

```
./cupid list [server_ip]
```

### Download a file from a remote server

```
./cupid get [server_ip] [filename]
```

## Requirements

- Linux operating system
- GCC compiler
- Make build system

## License

This software is provided under the MIT License. 