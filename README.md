# memory-allocator

직접 구현한 동적 메모리 할당기. `malloc`/`free`/`calloc`/`realloc`을 헤더·풋터 기반
free list 위에서 구현하고, split(분할)·coalesce(병합)으로 단편화를 줄인다.

## 설계 결정 (Design Decisions)

### 1. 16바이트 정렬
모든 반환 포인터를 16바이트 경계에 맞춘다.

- **표준 malloc**: C 표준은 malloc 반환 포인터가 *어떤 타입이든* 담을 수 있게 정렬되길 요구한다. 그 상한이 `max_align_t`이고, x86-64(System V ABI)에서는 **16**이다
  (`long double`이 16정렬). 8정렬만 하면 `long double`·SIMD 타입에서 표준 위반.
- **대체 가능성**: glibc malloc도 x86-64에서 16정렬을 보장한다. 내 할당기를
  `LD_PRELOAD`로 실제 프로그램에 끼우려면(로드맵 STEP 4) 16정렬이 필수다. 덜 정렬된
  포인터를 주면 SIMD 정렬 로드(`movaps` 등)를 쓰는 프로그램이 정렬 폴트로 죽는다.
- **추가적인 이득**: 크기가 항상 16배수 → 헤더 하위 4비트가 항상 0 → 그 비트를 **플래그**로 재활용할 수 있다.

> 확인: `_Alignof(max_align_t) == 16` (x86-64 Linux).

### 2. 4바이트 헤더
헤더는 `uint32_t` 한 개(크기 | 플래그)로 충분하다.

- 16정렬을 전제로 하고 `header_simul.py`로 시각화했다(4B vs 8B 헤더의 실제 할당 크기 비교).
- 그 결과 malloc 요청하는 size가 uniform distribution을 따른다는 가정하에 약 25% 상황에서 4byte 헤더를 사용했을 때, 16byte만큼 공간 이득이다. 즉 평균 4byte 이득이다. 하지만, 실제로는 uniform distribution이 아니고, 프로그램마다 자주 원하는 크기가 있을 것이다. 그래서 실제로는 8byte 헤더이든 4byte 헤더이든 오버헤드가 같을 수도 있다. 재밌는 지점이다. 

### 3. 블록 레이아웃 (경계 태그 / boundary tag)
```
[ header 4B ][ payload ... ][ footer 4B ]
  size|flag    16정렬 영역     size|flag
```
- **헤더·풋터에 같은 값**(크기 | in-use 비트)을 둔다. 풋터가 있어야 "물리적으로 앞에 있는
  블록"을 `header - 4`로 O(1)에 찾을 수 있다(coalescing에 사용한다)
- 최소 블록 = 16B. 이때 payload = **8B**, 즉 64비트 포인터 하나가 딱 들어간다 →
  free 블록은 payload 자리에 free list의 next 포인터를 저장한다.

### 4. free list — LIFO 단일 연결 리스트
- free된 블록을 `free_head` 앞에 끼운다(LIFO). next 포인터는 각 블록 payload에 저장.
- 자료구조가 단순하고 free/병합 시 갱신이 빠르다. 

### 5. split (분할)
- 요청보다 충분히 큰 free 블록을 찾으면 잘라서 쓰고 나머지를 free 블록으로 되돌린다.
- 임계값: `chunk_size >= need + 16` — 남는 조각이 최소 블록(헤더+풋터+정렬)을 담을 수
  있을 때만 분할한다. 못 담으면 통째로 내준다.

### 6. coalesce (병합)
- `my_free`에서 인접 free 블록과 즉시 병합해 단편화를 줄인다.
- **forward**: 뒤 블록(`header + size`)이 free면 합친다.
- **backward**: 풋터(`header - 4`)로 앞 블록을 찾아 free면 합친다.
- 병합 시 해당 블록을 free list에서 unlink 후 크기를 합산하고 풋터를 갱신한다.

### 7. calloc / realloc
- `my_calloc(n, size)`: `n > SIZE_MAX/size` 곱셈 오버플로를 먼저 차단하고 `memset`으로 0 초기화.
- `my_realloc(ptr, size)`:
  - 축소/현상유지 -> 남는 조각이 16B 이상이면 분할 반납.
  - 확대 -> 물리적으로 뒤에 붙은 free 블록과 **in-place 병합**해 복사를 회피. 불가하면 malloc + copy + free.
  - `realloc(NULL, n) == malloc(n)`, `realloc(p, 0) == free(p)` 표준 의미 처리.

### 8. 무결성 검사 — `check_invariant()`
디버깅/검증 자산. 힙을 스캔하며 (정렬·최소 크기·헤더==풋터·힙 범위) 확인하고,
선형 스캔으로 모은 free 집합과 free list를 **교차 검증**(개수·구성원 일치)해 링크 누락이나 사이클을 잡는다. 
`-DCHECK`로 빌드하면 테스트의 매 연산마다 호출된다.

## 빌드 & 테스트
```sh
make            # -DCHECK 포함 빌드 (매 연산마다 invariant 검사)
./myallocator.out
```

## Extension: allocsitter

이 allocator를 실제 workload에 적용한 후속 프로젝트.
tree-sitter로 Redis 소스를 파싱하는 벤치마크(정확성 게이트 + 입력 크기 스윕)로
glibc malloc과 비교·분석하고, workload 특화 최적화를 진행한다.

[AllocSitter](https://github.com/c5ln/AllocSitter)

이 repo는 범용 구현(위 설계 결정들)을 다루고, workload 분석과 특화 튜닝은
allocsitter에서 이어진다.
