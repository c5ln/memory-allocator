CC = gcc
CFLAGS = -g -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -DCHECK
VPATH = src
OBJS= test.o allocator.o
TARGET= myallocator.out
LIB= liballoc.so

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

# LD_PRELOAD용 공유 라이브러리.
# PIC/비PIC 오브젝트 혼용을 피하려고 소스에서 바로 -fPIC로 빌드한다.
$(LIB): src/allocator.c src/preload.c src/allocator.h
	$(CC) $(CFLAGS) -fPIC -shared -o $@ src/allocator.c src/preload.c

clean:
	rm -f *.o
	rm -f $(TARGET) $(LIB)

test.o: test.c allocator.h
allocator.o: allocator.c allocator.h

.PHONY: clean