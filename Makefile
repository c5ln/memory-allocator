CC = gcc
CFLAGS= -g -Wall -Wextra -std=c11
VPATH = src
OBJS= main.o
TARGET= myallocator.out

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)
clean:
	rm -f *.o
	rm -f $(TARGET)

main.o: main.c
