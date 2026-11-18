CC = gcc
CFLAGS = -Wall -Wextra -Iinclude

SRC = src/main.c src/shell.c src/execute.c
OBJ = obj/main.o obj/shell.o obj/execute.o
TARGET = bin/myshell

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) -lreadline

obj/%.o: src/%.c
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f obj/*.o bin/myshell

.PHONY: all clean
