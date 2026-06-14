#include "allocator.h"


void *my_malloc(size_t size){
    if(size==0) return NULL;
    // 블록 시작점 16배수로 맞추기
    void *cur = sbrk(0);
    uintptr_t misalign = (uintptr_t)cur % 16;
    if(misalign != 0){
        sbrk(16-misalign);
    }
    void *start = NULL;
    size_t aligend = size;
    if (aligend % 16 != 0){
        aligend = aligend + (16 - aligend%16);
    }
    start = sbrk(aligend);
    // 실패하면 sbrk는 (void*)(-1)을 반환하므로 NULL을 대입해야한다.
    if(start == (void*)(-1)){
        start = NULL;
    }
    return start;
}