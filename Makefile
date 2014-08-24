CC = gcc
CFLAGS = -Wall -Werror -I./inc
OBJ = utility.o myfsck.o readwrite.o fsck.o traverse.o directory.o block.o

all: myfsck

myfsck: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o myfsck
