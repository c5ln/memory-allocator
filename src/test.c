#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "allocator.h"

int main(void) {
    // 정렬 검증, 여러 크기로
    for (size_t s = 1; s <= 256; s++) {
        void *p = my_malloc(s);
        assert(p != NULL);
        assert((uintptr_t)p % 16 == 0);   //16정렬 강제 확인
    }
    printf("정렬 OK\n");

    // 쓸 수 있는가
    char *q = my_malloc(100);
    memset(q, 0xAB, 100);                 // 100바이트 전부 써봄
    for (int i = 0; i < 100; i++)
        assert((unsigned char)q[i] == 0xAB);
    printf("쓰기 OK\n");

    // 겹치지 않는가 
    char *a = my_malloc(64);
    char *b = my_malloc(64);
    memset(a, 'A', 64);
    memset(b, 'B', 64);
    for (int i = 0; i < 64; i++) assert(a[i] == 'A');  // b 쓴 게 a를 안 건드림
    printf("침범 X OK\n");
    return 0;
}