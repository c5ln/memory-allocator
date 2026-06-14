CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE
VPATH = src
OBJS= test.o allocator.o
TARGET= myallocator.out

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
clean:
	rm -f *.o
	rm -f $(TARGET)

test.o: test.c allocator.h
allocator.o: allocator.c allocator.h

.PHONY: clean