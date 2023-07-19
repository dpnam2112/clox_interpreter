CC := gcc
INCLUDE := ./include
SRC := ./src
BIN := ./bin
OBJ_DIR := $(BIN)/obj
TEST_BIN := $(BIN)/test
TEST_SRC := ./test
TEST_EXEC := $(TEST_BIN)/test
COMMON_FLAGS := -Wall -I $(INCLUDE) -g
OBJ_FILES := value.o debug.o chunk.o memory.o vm.o
OBJ_FILE_PATHS := $(addprefix $(OBJ_DIR)/, $(OBJ_FILES))
EXECUTABLE := $(BIN)/all

memory_dp := common.h memory.h memory.c
chunk_dp := common.h memory.h chunk.h value.h chunk.c
debug_dp := common.h chunk.h debug.h debug.c
main_dp := common.h chunk.h debug.h main.c
value_dp := memory.h common.h value.h value.c
vm_dp := memory.h common.h debug.h value.h vm.h vm.c

run:
	$(EXECUTABLE)

run_test:
	$(TEST_EXEC)

main:  $(OBJ_FILES) main.o
	$(CC) $(COMMON_FLAGS) $(addprefix $(OBJ_DIR)/, $(OBJ_FILES)) -I $(TEST_SRC) -o $(EXECUTABLE)

test:	$(OBJ_FILES) $(TEST_SRC)/*
	$(CC) $(COMMON_FLAGS) $(addprefix $(OBJ_DIR)/, $(OBJ_FILES)) $(TEST_SRC)/*.c -o $(TEST_EXEC)

main.o: $(addprefix $(SRC)/, $(filter %.c, $(memory_dp))) $(addprefix $(INCLUDE)/, $(filter %.h, $(memory_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(main_dp))) -o $(OBJ_DIR)/main.o 

chunk.o: $(addprefix $(SRC)/, $(filter %.c, $(chunk_dp))) $(addprefix $(INCLUDE)/, $(filter %.h, $(chunk_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(chunk_dp))) -o $(OBJ_DIR)/chunk.o 

debug.o: $(addprefix $(SRC)/, $(filter %.c, $(debug_dp))) $(addprefix $(INCLUDE)/, $(filter %.h, $(debug_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(debug_dp))) -o $(OBJ_DIR)/debug.o 

value.o: $(addprefix $(SRC)/, $(filter %.c, $(value_dp))) $(addprefix $(INCLUDE)/, $(filter %.h, $(value_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(value_dp))) -o $(OBJ_DIR)/value.o 

memory.o: $(addprefix $(SRC)/, $(filter %.c, $(memory_dp))) $(addprefix $(INCLUDE)/, $(filter %.h, $(memory_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(memory_dp))) -o $(OBJ_DIR)/memory.o 

vm.o: $(addprefix $(SRC)/, $(filter %.c, $(vm_dp))) $(addprefix $(INCLUDE)/, $(filter %.h, $(vm_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(vm_dp))) -o $(OBJ_DIR)/vm.o 

test:

clean:
	rm bin/obj/*
