#include "allocator.h"
#include <assert.h>
#include <stdlib.h>

static void *free_head = NULL; // free list 머리


// invariant 체크용
static void *heap_lo = NULL;
static void *heap_hi = NULL;


void *my_malloc(size_t size){
    if(size==0) return NULL;

    size_t need = (size + 4 + 15) & ~(size_t)15;  // 16의 배수로 올림 연산. 이진수의 관점으로 보면 된다.
    void **link = &free_head;
    while(*link)
    {
        uint32_t* h = (uint32_t*)(*link);
        size_t chunk_size = *h & ~15u;
        if(chunk_size >= need + 16 ) {
            *h = need | 1;
            void *payload = (char*)(*link)+4;
            
            void *splited_chunk_payload = (char*)(*link)+need+4;
            uint32_t *splited_chunk_header = (uint32_t*)splited_chunk_payload-1;   
            *splited_chunk_header = (uint32_t)(chunk_size - need);
            
            *link = *(void**)payload;
            *(void**)splited_chunk_payload = free_head;
            free_head = (void*)splited_chunk_header;

            return payload;
        }
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
    
    // heap 천장 체크
    heap_hi = sbrk(0);

    if(p==(void*)(-1)){
        return NULL;
    }
    char *header = (char*)p + pad;
    char *payload = header+4;
    *(uint32_t*)header = (uint32_t)need | 1 ;
    
    //heap 시작점 체크
    if(heap_lo == NULL) heap_lo = p;

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
    }
    
    *(void**)ptr = free_head;
    free_head = (void*)header;
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

      // 3) 개수 일치 = 두 집합이 같음
      assert(linear_count == freelist_count);              // link 갱신 누락 검출
  }
