CC := cc
CFLAGS := -Wall -Wextra -Wno-unused-parameter #-O3 -flto
DEBUG_CFLAGS := -g
SRC_DIR := src
BUILD_DIR := bin
INCLUDE_DIR := include
TARGET := clox

# List of source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
# List of object files for release build
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
# List of object files for debugging build
DEBUG_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%_debug.o,$(SRCS))
# List of header files
INCLUDES := -I$(INCLUDE_DIR)

# Release build rule
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Debugging build rule
debug: $(BUILD_DIR)/$(TARGET)_debug

$(BUILD_DIR)/$(TARGET)_debug: $(DEBUG_OBJS)
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) $(INCLUDES) $(DEBUG_OBJS) -o $@

$(BUILD_DIR)/%_debug.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(TARGET)_debug $(OBJS) $(DEBUG_OBJS)
