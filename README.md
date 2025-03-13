# Cupid - LAN File Sharing Program

A simple, lightweight file sharing program for Linux systems that operates within a local area network.

## Features

- Share files across Linux devices on the same network
- Simple command-line interface
- Fast transfer speeds using direct TCP/IP connections
- List available files from remote systems
- Download files from peers on the network
- Smart networking that automatically handles different subnet configurations
- Cross-subnet communication without manual configuration

## Building

To build the program, simply run:

```
make
```

This will compile the program and create the `cupid` executable.

## Usage

### Start the server

```
./cupid server [directory_to_share] [bind_ip]
```

If no directory is specified, the current directory is shared.
If no IP address is specified, the program will automatically select the best IP address to bind to.

### List available files on a remote server

```
./cupid list [server_ip]
```

### Download a file from a remote server

```
./cupid get [server_ip] [filename]
```

## Advanced Networking Features

Cupid includes intelligent networking that makes it work across different network configurations:

- **Smart IP selection**: The server automatically detects and binds to the most appropriate IP address
- **Cross-subnet routing**: The client automatically handles connecting across different subnets
- **Connection retry logic**: If direct connection fails, the client will attempt alternative routing
- **Dynamic interface selection**: Both client and server can work across wireless and wired networks

## Requirements

- Linux operating system or Windows Subsystem for Linux (WSL)
- GCC compiler
- Make build system

## Troubleshooting

If you experience connection issues:

1. Ensure both devices are physically connected to the same network
2. Check if any firewalls are blocking the Cupid port (9876)
3. For extreme cases, the included `network_bridge.sh` script can be used to create a virtual network interface

## License

This software is provided under the MIT License. 