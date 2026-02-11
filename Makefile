CC       := gcc
CFLAGS   := -Iinclude -Wall -Wextra -Werror -g -std=c11
SRC_DIR  := src
OBJ_DIR  := build
BIN_DIR  := bin
TARGET   := $(BIN_DIR)/clox
FORMATTER = clang-format
FORMAT_FLAGS = -i
SRC_FILES = $(shell find . -iname "*.c" -o -iname "*.h")


SRCS     := $(wildcard $(SRC_DIR)/*.c)
OBJS     := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

# external flags
CFLAGS += $(EXT_FLAGS)

clox: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

run: $(TARGET)
	$(TARGET)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: format
format:
	@echo "Formatting source files..."
	@$(FORMATTER) $(FORMAT_FLAGS) $(SRC_FILES)

.PHONY: all clean
