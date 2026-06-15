#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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

    // 재사용 가능한가
    void *c = my_malloc(50);
    my_free(a);
    void *d = my_malloc(50);
    assert(a==b);
    printf("재사용 OK");

    // 오버헤드 측정
    #define N 1000
    size_t sizes[N];
    size_t requested = 0;
    uintptr_t used_before = (uintptr_t)sbrk(0);   // 측정 시작 직전 break
    for (int i = 0; i < N; i++) {
        sizes[i] = (size_t)(rand() % 100) + 1;    // 1~100바이트 랜덤
        void *p = my_malloc(sizes[i]);
        assert(p != NULL);
        requested += sizes[i];
    }
    uintptr_t used = (uintptr_t)sbrk(0) - used_before;

    printf("\n── 오버헤드 측정 (N=%d) ──\n", N);
    printf("요청 합계 : %zu bytes\n", requested);
    printf("실제 사용 : %zu bytes\n", (size_t)used);
    printf("오버헤드  : %zu bytes (%.1f%%)\n",
           (size_t)(used - requested),
           100.0 * (double)(used - requested) / (double)requested);

    return 0;
}