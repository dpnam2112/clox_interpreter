CC := gcc
INCLUDE := ./include
SRC := ./src
BIN := ./bin
OBJ_DIR := $(BIN)/obj
TEST_BIN := $(BIN)/test
TEST_SRC := ./test
TEST_EXEC := $(TEST_BIN)/test
COMMON_FLAGS := -Wall -I $(INCLUDE) -g
OBJ_FILES := value.o debug.o chunk.o memory.o vm.o scanner.o compiler.o object.o table.o
MAIN_OBJS := $(OBJ_FILES) main.o
OBJ_FILE_PATHS := $(addprefix $(OBJ_DIR)/, $(OBJ_FILES))
EXECUTABLE := $(BIN)/all

#Dependencies of each object file

memory_dp := common.h memory.h memory.c
chunk_dp := common.h memory.h chunk.h value.h chunk.c
debug_dp := common.h chunk.h debug.h debug.c
main_dp := common.h chunk.h debug.h vm.h main.c
value_dp := memory.h common.h value.h value.c
vm_dp := memory.h common.h debug.h value.h vm.h vm.c
compiler_dp := compiler.h scanner.h vm.h value.h object.h debug.h compiler.c
scanner_dp := common.h scanner.h scanner.c
object_dp := common.h value.h object.h table.h object.c
table_dp := common.h table.h memory.h value.h object.h table.c

run: all
	$(EXECUTABLE)

run_test:
	$(TEST_EXEC)

all:  $(addprefix $(OBJ_DIR)/, $(MAIN_OBJS))
	$(CC) $(COMMON_FLAGS) $(addprefix $(OBJ_DIR)/, $(MAIN_OBJS)) -I $(TEST_SRC) -o $(EXECUTABLE)

test:	$(OBJ_FILES) $(TEST_SRC)/*
	$(CC) $(COMMON_FLAGS) $(addprefix $(OBJ_DIR)/, $(OBJ_FILES)) $(TEST_SRC)/*.c -o $(TEST_EXEC)

$(OBJ_DIR)/main.o: $(addprefix $(SRC)/, $(filter %.c, $(memory_dp))) \
		$(addprefix $(INCLUDE)/, $(filter %.h, $(memory_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(main_dp))) -o $(OBJ_DIR)/main.o

$(OBJ_DIR)/chunk.o: $(addprefix $(SRC)/, $(filter %.c, $(chunk_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(chunk_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(chunk_dp))) -o $(OBJ_DIR)/chunk.o 

$(OBJ_DIR)/debug.o: $(addprefix $(SRC)/, $(filter %.c, $(debug_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(debug_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(debug_dp))) -o $(OBJ_DIR)/debug.o 

$(OBJ_DIR)/value.o: $(addprefix $(SRC)/, $(filter %.c, $(value_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(value_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(value_dp))) -o $(OBJ_DIR)/value.o 

$(OBJ_DIR)/memory.o: $(addprefix $(SRC)/, $(filter %.c, $(memory_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(memory_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(memory_dp))) -o $(OBJ_DIR)/memory.o 

$(OBJ_DIR)/vm.o: $(addprefix $(SRC)/, $(filter %.c, $(vm_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(vm_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(vm_dp))) -o $(OBJ_DIR)/vm.o 

$(OBJ_DIR)/scanner.o: $(addprefix $(SRC)/, $(filter %.c, $(scanner_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(scanner_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(scanner_dp))) -o $(OBJ_DIR)/scanner.o 

$(OBJ_DIR)/compiler.o: $(addprefix $(SRC)/, $(filter %.c, $(compiler_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(compiler_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(compiler_dp))) -o $(OBJ_DIR)/compiler.o 

$(OBJ_DIR)/object.o: $(addprefix $(SRC)/, $(filter %.c, $(object_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(object_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(object_dp))) -o $(OBJ_DIR)/object.o 

$(OBJ_DIR)/table.o: $(addprefix $(SRC)/, $(filter %.c, $(table_dp))) \
	$(addprefix $(INCLUDE)/, $(filter %.h, $(table_dp)))
	$(CC) -c $(COMMON_FLAGS) $(addprefix $(SRC)/, $(filter %.c, $(table_dp))) -o $(OBJ_DIR)/table.o 

test:

clean:
	rm bin/obj/*
