#include "allocator.h"

void *my_malloc(size_t size){
    if(size==0) return NULL;
    size_t aligend = size; 
    uintptr_t cur = (uintptr_t)sbrk(0);
    uintptr_t pad = ((uintptr_t)12 - cur) & 15; // 하위 4비트만 가져오기
    if(pad){
        if(sbrk(pad) == (void*)(-1)) return NULL;
    }
    void* p = sbrk(4);
    if(p == (void*)(-1)) return NULL;
    if (aligend % 16 < 12){
        aligend = aligend + (12 - aligend%16);
    }
    else if(aligend % 16 > 12){
        aligend = aligend + (16 - aligend%16) + 12;
    }
    *(uint32_t*)p = (uint32_t)((aligend+4)) | 1 ; // flag 추가. LSB가 1이면 사용중

    void *start = sbrk(aligend);
    // 실패하면 sbrk는 (void*)(-1)을 반환하므로 NULL을 대입해야한다.
    if(start == (void*)(-1)){
        start = NULL;
    }
    return start;
}

void my_free(void *ptr){
    if(ptr == NULL) return;
    uint32_t *header = (uint32_t*)ptr - 1; 
    *header = *header & (~1u);
}