// tree-sitter를 내 allocator 위에서 돌리는 benchmark driver
// ts_set_allocator()로 내 malloc/calloc/realloc/free를 tree-sitter에 주입하고,
// 인자로 받은 파일들을 파싱한다. 래퍼로 감싸 할당 통계를 찍어
// tree-sitter가 실제로 내 allocator를 거치는지 확인한다.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <tree_sitter/api.h>
#include "../src/allocator.h"

extern const TSLanguage *tree_sitter_c(void);

//할당 통계 카운터
static unsigned long n_malloc, n_calloc, n_realloc, n_free;
static long live;        // 현재 살아있는 할당 개수 (malloc/calloc - free)
static long live_peak;   // 최고치

// per-op 훅: 매 연산 후 힙 무결성 검사 
static unsigned long op_count;        // 전역 연산 번호 (모든 래퍼 통과 시 +1)
static unsigned long chk_every;       // N연산마다 검사 (0=off)  <- TS_CHECK_EVERY
static unsigned long chk_from;        // 이 연산부터 검사 시작   <- TS_CHECK_FROM

// 각 래퍼 끝에서 호출. 검사할 차례면 연산 정보 찍고 check_invariant() 실행.
// check_invariant 안의 assert가 터지면, 직전에 찍힌 [op ...] 줄이 범인 연산이다.
static void post_op(const char *kind, size_t arg1, size_t arg2, void *ret) {
    op_count++;
    if (chk_every == 0 || op_count < chk_from) return;
    if ((op_count - chk_from) % chk_every != 0) return;
    fprintf(stderr, "[op %lu] %-7s arg=(%zu,%zu) -> %p  checking...\n",
            op_count, kind, arg1, arg2, ret);
    check_invariant();
}

static void *w_malloc(size_t n)           { n_malloc++;  live++; if(live>live_peak)live_peak=live; void *r=my_malloc(n);    post_op("malloc",  n, 0, r); return r; }
static void *w_calloc(size_t c, size_t n) { n_calloc++;  live++; if(live>live_peak)live_peak=live; void *r=my_calloc(c, n); post_op("calloc",  c, n, r); return r; }
static void *w_realloc(void *p, size_t n) { n_realloc++; if(p==NULL)live++;                        void *r=my_realloc(p, n);post_op("realloc", (size_t)p, n, r); return r; }
static void  w_free(void *p)              { if(p){ n_free++; live--; } my_free(p);                                          post_op("free",    (size_t)p, 0, p); }

// 파일 전체를 읽어 malloc 버퍼로 반환 (glibc malloc 사용. 파일 IO는 벤치 대상 아님)
static char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    fclose(f);
    buf[rd] = '\0';
    if (out_len) *out_len = rd;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.c> [more.c ...]\n", argv[0]);
        return 2;
    }

    // 내 allocator 주입. TS_GLIBC 설정 시 기본(glibc) allocator로 대조.
    int use_glibc = getenv("TS_GLIBC") != NULL;
    if (!use_glibc)
        ts_set_allocator(w_malloc, w_calloc, w_realloc, w_free);
    printf("[allocator: %s]\n", use_glibc ? "glibc (default)" : "MY allocator");

    // per-op 훅 설정: TS_CHECK_EVERY=N (N연산마다 검사), TS_CHECK_FROM=M (M연산부터)
    const char *e = getenv("TS_CHECK_EVERY");
    if (e) chk_every = strtoul(e, NULL, 10);
    const char *ff = getenv("TS_CHECK_FROM");
    if (ff) chk_from = strtoul(ff, NULL, 10);
    if (chk_every) fprintf(stderr, "[per-op check] every=%lu from=%lu\n", chk_every, chk_from);

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());

    long total_bytes = 0, total_nodes = 0, errors = 0;
    double parse_secs = 0;   // 순수 파싱 시간 누적 (파일 IO 제외)
    struct timespec t0, t1;
    for (int i = 1; i < argc; i++) {
        size_t len = 0;
        void *brk_before = getenv("TS_PROBE") ? sbrk(0) : NULL;
        char *src = read_file(argv[i], &len);
        if (!src) { fprintf(stderr, "  [skip] cannot read %s\n", argv[i]); continue; }
        if (getenv("TS_PROBE")) {
            void *brk_after = sbrk(0);
            fprintf(stderr, "[probe] read_file(%s, %zuB via glibc malloc): "
                    "brk %p -> %p (moved %+ld B), src=%p\n",
                    argv[i], len, brk_before, brk_after,
                    (long)((char*)brk_after - (char*)brk_before), (void*)src);
        }

        clock_gettime(CLOCK_MONOTONIC, &t0);
        TSTree *tree = ts_parser_parse_string(parser, NULL, src, (uint32_t)len);
        TSNode root = ts_tree_root_node(tree);

        // 트리를 순회하며 노드 수를 세고 에러 유무 확인
        long nodes = 0;
        TSTreeCursor cur = ts_tree_cursor_new(root);
        int desc = 1;
        while (1) {
            nodes++;
            if (desc && ts_tree_cursor_goto_first_child(&cur)) continue;
            if (ts_tree_cursor_goto_next_sibling(&cur)) { desc = 1; continue; }
            desc = 0;
            if (!ts_tree_cursor_goto_parent(&cur)) break;
        }
        ts_tree_cursor_delete(&cur);
        if (ts_node_has_error(root)) errors++;

        ts_tree_delete(tree);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        parse_secs += (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;

        total_bytes += len;
        total_nodes += nodes;
        free(src);

        // TS_CHECK 설정 시: 매 파일 후 힙 무결성 검사(이 시점 live==0, 전 힙이 free)
        if (!use_glibc && getenv("TS_CHECK")) {
            fprintf(stderr, "  [check_invariant] after file %d (%s) ...\n",
                    i, argv[i]);
            check_invariant();
            fprintf(stderr, "  [check_invariant] OK\n");
        }
    }

    ts_parser_delete(parser);

    printf("=== parse summary ===\n");
    printf("files parsed : %d\n", argc - 1);
    printf("total bytes  : %ld\n", total_bytes);
    printf("total nodes  : %ld\n", total_nodes);
    printf("files w/error: %ld\n", errors);
    printf("parse time   : %.4f s  (%.2f MB/s)\n",
           parse_secs, parse_secs > 0 ? (total_bytes / 1e6) / parse_secs : 0);
    printf("=== allocator traffic (via my allocator) ===\n");
    printf("malloc : %lu\n", n_malloc);
    printf("calloc : %lu\n", n_calloc);
    printf("realloc: %lu\n", n_realloc);
    printf("free   : %lu\n", n_free);
    printf("live at end : %ld  (leak if != 0)\n", live);
    printf("live peak   : %ld\n", live_peak);
    return 0;
}
