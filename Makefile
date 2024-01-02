CC := clang
CFLAGS := -Wall -Wextra -Wno-unused-parameter -O3 -flto
SRC_DIR := src
BUILD_DIR := bin
INCLUDE_DIR := include
TARGET := clox

# List of source files
SRCS := $(wildcard $(SRC_DIR)/*.c)
# List of object files
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
# List of header files
INCLUDES := -I$(INCLUDE_DIR)

$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) $(OBJS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

.PHONY: clean

clean:
	rm -f $(BUILD_DIR)/$(TARGET) $(OBJS)

