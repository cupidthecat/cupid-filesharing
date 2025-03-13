CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -pthread
TARGET = cupid
SRC_DIR = src
OBJ_DIR = obj

# Find all source files
SRC = $(wildcard $(SRC_DIR)/*.c)
# Generate object file names
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRC))

# Default target
all: directories $(TARGET)

# Create directories
directories:
	@mkdir -p $(OBJ_DIR)

# Linking
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compilation
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# Install
install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin

# Phony targets
.PHONY: all clean install directories 