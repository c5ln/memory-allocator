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
    my_free(c);
    void *d = my_malloc(50);
    assert(c==d);
    printf("재사용 OK\n");

    // 재사용 후 데이터 정상
    memset(d, 0xCD, 50);
    for(int i=0;i<50;i++) assert(((unsigned char*)d)[i]==0xCD);
    printf("재사용 후 쓰기 OK\n");

    // 여러 블록 free 후 재사용 (리스트 순회 확인)
    void *x = my_malloc(30), *y = my_malloc(30), *z = my_malloc(30);
    my_free(x); my_free(y); my_free(z);   // 셋 다 리스트에
    void *w = my_malloc(30);
    assert(w==z || w==y || w==x);          // 셋 중 하나 재사용
    printf("다중 free 재사용 OK\n");

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

    // 헤더 오버헤드
    printf("\n 오버헤드 측정 (N=%d) \n", N);
    printf("요청 합계 : %zu bytes\n", requested);
    printf("실제 사용 : %zu bytes\n", (size_t)used);
    printf("오버헤드  : %zu bytes (%.1f%%)\n",
           (size_t)(used - requested),
           100.0 * (double)(used - requested) / (double)requested);

    // 분할 효과 측정 
    // 큰 블록을 여러 개 free해 free list에 큰 조각을 쌓은 뒤,
    // free된 블록 수보다 더 많은 작은 요청을 던진다.
    // 분할 X: 작은 요청 하나가 큰 블록을 통째로 먹음 -> 큰 블록 소진 후 sbrk 폭증
    // 분할 O: 큰 블록을 쪼개 여러 작은 요청에 나눠줌 -> sbrk 증가 거의 없음
    #define BIG_N   200    // free시켜둘 큰 블록 개수
    #define BIG_SZ  128    // 큰 블록 요청 크기
    #define SMALL_SZ 16    // 작은 요청 크기
    #define SMALL_N 600    // 작은 요청 개수 (BIG_N보다 크게)

    void *bigs[BIG_N];
    for (int i = 0; i < BIG_N; i++) {
        bigs[i] = my_malloc(BIG_SZ);
        assert(bigs[i] != NULL);
    }
    for (int i = 0; i < BIG_N; i++) my_free(bigs[i]);  // 큰 free 블록 BIG_N개 free

    uintptr_t brk_before = (uintptr_t)sbrk(0);          // 작은 요청 직전 break
    for (int i = 0; i < SMALL_N; i++) {
        void *p = my_malloc(SMALL_SZ);
        assert(p != NULL);
    }
    uintptr_t grew = (uintptr_t)sbrk(0) - brk_before;   // 작은 요청이 유발한 힙 증가

    printf("\n분할 효과 측정\n");
    printf("free된 %dB 블록 %d개 위에서 %dB 요청 %d개 처리\n",
           BIG_SZ, BIG_N, SMALL_SZ, SMALL_N);
    printf("작은 요청이 유발한 힙 증가(sbrk) : %zu bytes\n", (size_t)grew);
    printf("(분할 O면 기존 free블록 재사용으로 0에 수렴, 분할 X면 폭증)\n");

    return 0;
}