#include "allocator.h"

static void *free_head = NULL; // free list 머리

void *my_malloc(size_t size){
    if(size==0) return NULL;
    size_t need = (size + 4 + 15) & ~(size_t)15;  // 16의 배수로 올림 연산. 이진수의 관점으로 보면 된다.
    void **link = &free_head;
    while(*link)
    {
        uint32_t* h = (uint32_t*)(*link);
        size_t chunk_size = *h & ~15u;
        if(chunk_size >= need){
            *h = *h | 1;
            void *payload = (char*)(*link)+4;
            *link = *(void**)payload;
            return payload;
        }
        link = (void**)((char*)(*link) + 4);
    }
    uintptr_t cur = (uintptr_t)sbrk(0);
    uintptr_t pad = ((uintptr_t)12 - cur) & 15; // 하위 4비트만 가져오기

     // flag 추가. LSB가 1이면 사용중
    void *p = sbrk(need+pad);
    if(p==(void*)(-1)){
        return NULL;
    }
    char *header = (char*)p + pad;
    char *payload = header+4;
    *(uint32_t*)header = (uint32_t)need | 1 ;

    return (void*)payload;
}

void my_free(void *ptr){
    if(ptr == NULL) return;
    uint32_t *header = (uint32_t*)ptr - 1; 
    *header = *header & (~1u);

    *(void**)ptr = free_head;
    free_head = (void*)header;
}