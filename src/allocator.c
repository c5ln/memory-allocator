#include "allocator.h"

static void *free_head = NULL; // free list 머리

void *my_malloc(size_t size){
    if(size==0) return NULL;
    size_t aligend = size; 

    if (aligend % 16 < 12){
        aligend = aligend + (12 - aligend%16);
    }
    else if(aligend % 16 > 12){
        aligend = aligend + (16 - aligend%16) + 12;
    }
    void **link = &free_head;
    while(*link)
    {
        uint32_t *h = (uint32_t*)(*link);
        size_t chunk = *h & ~15u;
        if(chunk >= aligend+4){
            *h |= 1;
            void *payload = (char*)(*link) + 4;
            *link = *(void**)payload;
            return payload;
        }
        link = (void**)((char*)(*link)+4);
    }
    uintptr_t cur = (uintptr_t)sbrk(0);
    uintptr_t pad = ((uintptr_t)12 - cur) & 15; // 하위 4비트만 가져오기

     // flag 추가. LSB가 1이면 사용중
    void *p = sbrk(aligend+pad+4);
    if(p==(void*)(-1)){
        return NULL;
    }
    char *header = (char*)p + pad;
    char *payload = header+4;
    *(uint32_t*)header = (uint32_t)((aligend+4)) | 1 ;

    return (void*)payload;
}

void my_free(void *ptr){
    if(ptr == NULL) return;
    uint32_t *header = (uint32_t*)ptr - 1; 
    *header = *header & (~1u);

    *(void**)ptr = free_head;
    free_head = (void*)header;
}