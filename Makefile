# Compiler settings
CC = clang
LD = clang
CC_FLAGS = -O2 -Wall -Wextra -c
LD_FLAGS = -Wall -Wextra
OBJ = obj

ips: $(OBJ) $(OBJ)/ips.o
	$(CC) $(LD_FLAGS) $(OBJ)/ips.o -o ips

$(OBJ):
	mkdir $(OBJ)

$(OBJ)/ips.o: ips.c
	$(CC) $(CC_FLAGS) ips.c -o $(OBJ)/ips.o

.PHONY: clean
clean:
	rm -rf $(OBJ) ips