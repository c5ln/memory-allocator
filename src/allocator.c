#include "allocator.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void *free_head = NULL; // free list 머리


// invariant 체크용
static void *heap_lo = NULL;
static void *heap_hi = NULL;


void *my_malloc(size_t size){
    if(size==0) return NULL;

    size_t need = (size + 4 + 4 + 15) & ~(size_t)15;  // 16의 배수로 올림 연산. 이진수의 관점으로 보면 된다.
    void **link = &free_head;
    while(*link)
    {
        uint32_t* h = (uint32_t*)(*link);
        size_t chunk_size = *h & ~15u;
        if(chunk_size >= need + 16 ) {
            *h = need | 1; // 활용 표기
            *(uint32_t*)((char*)h+need-4) = need | 1; // footer도 표기

            void *payload = (char*)(*link)+4; // payload 위치
            
            void *splited_chunk_payload = (char*)(*link)+need+4;
            uint32_t *splited_chunk_header = (uint32_t*)splited_chunk_payload-1;   
            *splited_chunk_header = (uint32_t)(chunk_size - need); // 자르고 남은 영역만큼 header에 표기

            void *splited_chunk_footer = (void*)((char*)splited_chunk_header + *splited_chunk_header -4); 
            *(uint32_t*)splited_chunk_footer = *splited_chunk_header; // footer 붙이기

            *link = *(void**)payload;
            *(void**)splited_chunk_payload = free_head;
            free_head = (void*)splited_chunk_header;

            return payload;
        }
        if(chunk_size >= need){
            *h = *h | 1; //header
            *((char*)h + (*h & ~15u) - 4) |= 1; //footer

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
    
    // heap 천장 체크
    heap_hi = sbrk(0);

    if(p==(void*)(-1)){
        return NULL;
    }
    char *header = (char*)p + pad;
    char *payload = header+4;
    char *footer = header + need - 4;
    *(uint32_t*)header = (uint32_t)need | 1 ;
    *(uint32_t*)footer = (uint32_t)need | 1 ;
    
    
    //heap 시작점 체크
    if(heap_lo == NULL) heap_lo = (void*)header;

    return (void*)payload;
}

void my_free(void *ptr){
    if(ptr == NULL) return;
    uint32_t *header = (uint32_t*)ptr - 1; 
    if(!(*header&1)) return; // double free 방지
    
    *header = *header & (~1u);
    
    // coalescing logic
    // 오른쪽 블록 찾기
    size_t size = *header & ~15u;
    uint32_t* right_header = (uint32_t*)((char*)header + size);
    // 오른쪽 블럭도 free 영역인 경우
    if ((char*)right_header < (char*)heap_hi && (*right_header & 1) == 0)
    {
        void **link = &free_head; 
        while(*link){
            //free list의 block과 righ_header의 주소가 같으면
            if(*link == (void*)right_header){
                //link 해제 
                *link = *(void**)((char*)right_header + 4); // right header가 원래 가리키던 곳의 주소 -> 즉, right_header는 unlink
                break;
            }
            link = (void**)((char*)(*link)+4);
        }
        *header = size + (*right_header & ~15u); // header 더하기 
        *(uint32_t*)((char*)header + *header - 4) = *header;
    }
    // 왼쪽 블럭 찾기
    uint32_t* left_footer = (uint32_t*)((char*)header - 4);
    size_t left_size = *left_footer & ~15u;
    // 왼쪽 블럭도 free인 경우
    if ((char*)left_footer > (char*)heap_lo && (*left_footer & 1) == 0)
    {
        uint32_t* left_header = (uint32_t*)((char*)left_footer-left_size+4);
        void** link = &free_head;
        while(*link)
        {
            if(*link == (void*)left_header)
            {
                *link = *(void**)((char*)left_header + 4);
                break;
            }
            link = (void**)((char*)(*link)+4);
        }
        *left_header = (*header&~15u) + (*left_header & ~15u); // header 더하기
        
        *(uint32_t*)((char*)left_header+(*left_header & ~15u) - 4) = *left_header;

        *(void**)((char*)left_header + 4) = free_head;
        free_head = (void*)left_header;
        return;
    }

    *(void**)ptr = free_head;
    free_head = (void*)header;
}


void *my_calloc(size_t nmemb, size_t size){
    if(nmemb == 0 || size == 0) return NULL;
    if(nmemb > SIZE_MAX / size) return NULL; // 곱셈 오버플로 차단

    size_t total = nmemb * size;
    void *p = my_malloc(total);
    if(p) memset(p, 0, total);
    return p;
}
void *my_realloc(void *ptr, size_t size)
{
    if(ptr == NULL) return my_malloc(size);
    if(size==0 && ptr != NULL) {
        my_free(ptr);
        return NULL;
    }
    // 기존의 block size
    size_t old_size = *((uint32_t*)ptr-1) & ~15u;
    // 새로 필요한 size
    size_t new_size = (size + 8 + 15) & ~15u; 

    // 새로운 size > old size
    if(new_size > old_size){
        // 잔여 공간 여부 파악. 어떻게?
        uint32_t *right_header = (uint32_t*)((char*)ptr - 4 + old_size);
        // 1. 이웃이 존재하고 free 상태이며 공간 충분하면 위치 그대로 확장
        if((char*)right_header < (char*)heap_hi && (*right_header & 1)==0 &&old_size + (size_t)(*right_header & ~15u) >= new_size)
        {   
            size_t combined = old_size + *right_header;
            // 합치고 남은 공간이 최소 크기인 16byte보다 크다면, 16byte를 spliting
            if(combined >= new_size + 16)
            {
                uint32_t *spliting_header = (uint32_t*)((char*)ptr + new_size - 4);
                *spliting_header = (combined - (size_t)new_size) & ~15u;
                uint32_t *spliting_footer = (uint32_t*)((char*)ptr + combined - 8);
                *spliting_footer = *spliting_header;

                // free list 갱신
                void **link = &free_head;
                while(*link)
                {
                    if((void*)right_header == *link)
                    {
                        //free list 갱신
                        *link = *(void**)((char*)right_header+4); // right_header의 payload에 있던 다음 node에 대한 주소로 link 갱신
                        // free list에 spliting된 block 추가
                        *(void**)((char*)spliting_header + 4) = free_head;
                        // head 갱신
                        free_head = (void*)spliting_header;
                        break;
                    }
                    // 못 찾았으므로 다음 노드로 
                    link = (void**)((char*)*link + 4); 
                }
                // 앞 블록 header 갱신
                *((uint32_t*)ptr - 1) = new_size | 1;
                // 앞 블록 footer 갱신                
                *(uint32_t*)((char*)ptr + new_size - 8) = new_size | 1; 
            }
            // 합치고 남은 공간이 16byte보다 작을 예정이라면 spliting 하지 않는다.
            else {
                //header 와 footer 갱신
                *((uint32_t*)ptr - 1) = combined | 1;
                *(uint32_t*)((char*)ptr + combined-8) = combined | 1;
                // free list 에서 합쳐진 공간 삭제
                void **link = &free_head;
                while(*link)
                {
                    if((void*)right_header == *link)
                    {
                        //free list 갱신
                        *link = *(void**)((char*)right_header+4); // right_header의 payload에 있던 다음 node에 대한 주소로 link 갱신
                        break;
                    }
                    // 못 찾았으므로 다음 노드로 
                    link = (void**)((char*)*link + 4); 
                }
            }
            return ptr;
        }
        // 2. block 위치 이동
        else {
        void *new_ptr = my_malloc(new_size);
        if(new_ptr == NULL) return NULL;
        memcpy(new_ptr, ptr, old_size-8);
        my_free(ptr);
        return new_ptr;
        }
    }
    // 새로운 size <= old size
    else if(new_size <= old_size)
    {
        size_t leftover = old_size - new_size;
        // 남는 공간이 최소 블록(16byte) 이상일 때만 spliting
        if(leftover >= 16)
        {
            // 앞 블록을 new_size로 갱신
            *((uint32_t*)ptr - 1) = new_size | (*((uint32_t*)ptr - 1) & 15u);
            *(uint32_t*)((char*)ptr + new_size - 8) = *((uint32_t*)ptr - 1);

            // 뒤쪽 남은 블록을 free 블록으로
            uint32_t *free_header = (uint32_t*)((char*)ptr + new_size - 4);
            *free_header = (uint32_t)leftover;
            *(uint32_t*)((char*)ptr + old_size - 8) = (uint32_t)leftover; // footer

            // free list에 넣기
            void *free_payload = (char*)free_header + 4;
            *(void**)free_payload = free_head;
            free_head = (void*)free_header;
        }
        // leftover < 16 이면 자르지 않고 old_size 그대로 둔다
        return ptr;
    }
    return NULL;
}

void check_invariant()
{
    #define MAX_NODES 10000
    static void *linear_free_list[MAX_NODES];
    int linear_count = 0;

    // 블록 단위 검사 + chunk 수집
    for(char* p = heap_lo; p < (char*)heap_hi;){

        size_t chunk = *(uint32_t*)p & ~15u;

        assert(chunk%16==0); // 메모리 정렬 확인
        assert(chunk >= 16); // 메모리 최소 크기 확인
        assert(p+chunk <= (char*)heap_hi); // chunk가 heap 안 넘는지
        assert((*(uint32_t*)(p + chunk - 4) & ~15u) == chunk); // header 크기 == footer 크기
     
        if ((*(uint32_t*)p & 1) == 0) { 
            linear_free_list[linear_count++] = p ;
        }

        p += chunk;  
    }
      

      // free list 탐색 + 교차 검증
    int freelist_count = 0;
    void *p = free_head;
    while (p) {
        assert((*(uint32_t*)p & 1) == 0); // free 영역인지 체크
        assert(p >= heap_lo && p <= heap_hi); // 힙 범위 체크
        assert(freelist_count < MAX_NODES);              // 사이클/폭주 감지

        int found = 0;
        for (int i = 0; i < linear_count; i++) {
            if (linear_free_list[i] == p) { found = 1; break; }
        }
        assert(found);                                   // 교차 검증

        freelist_count++;
        p = *(void**)((char*)p + 4); // free list의 다음 블록 체크
    }

    // 개수 일치 = 두 집합이 같음
    assert(linear_count == freelist_count);              // link 갱신 누락 검출
}
