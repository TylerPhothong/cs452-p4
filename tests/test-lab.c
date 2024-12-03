#include <assert.h>
#include <stdlib.h>
#include <time.h>
#ifdef __APPLE__
#include <sys/errno.h>
#else
#include <errno.h>
#endif
#include "harness/unity.h"
#include "../src/lab.h"

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

/**
 * Check the pool to ensure it is full.
 */
void check_buddy_pool_full(struct buddy_pool *pool)
{
  for (size_t i = 0; i < pool->kval_m; i++)
  {
    assert(pool->avail[i].next == &pool->avail[i]);
    assert(pool->avail[i].prev == &pool->avail[i]);
    assert(pool->avail[i].tag == BLOCK_UNUSED);
    assert(pool->avail[i].kval == i);
  }

  assert(pool->avail[pool->kval_m].next->tag == BLOCK_AVAIL);
  assert(pool->avail[pool->kval_m].next->next == &pool->avail[pool->kval_m]);
  assert(pool->avail[pool->kval_m].prev->prev == &pool->avail[pool->kval_m]);
  assert(pool->avail[pool->kval_m].next == pool->base);
}

/**
 * Check the pool to ensure it is empty.
 */
void check_buddy_pool_empty(struct buddy_pool *pool)
{
  for (size_t i = 0; i <= pool->kval_m; i++)
  {
    assert(pool->avail[i].next == &pool->avail[i]);
    assert(pool->avail[i].prev == &pool->avail[i]);
    assert(pool->avail[i].tag == BLOCK_UNUSED);
    assert(pool->avail[i].kval == i);
  }
}

/**
 * Test allocating 1 byte.
 */
void test_buddy_malloc_one_byte(void)
{
  fprintf(stderr, "->Test allocating and freeing 1 byte\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << MIN_K;
  buddy_init(&pool, size);
  void *mem = buddy_malloc(&pool, 1);
  assert(mem != NULL);
  buddy_free(&pool, mem);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

/**
 * Test allocating a large block that consumes the entire pool.
 */
void test_buddy_malloc_one_large(void)
{
  fprintf(stderr, "->Testing size that will consume entire memory pool\n");
  struct buddy_pool pool;
  size_t bytes = UINT64_C(1) << MIN_K;
  buddy_init(&pool, bytes);

  size_t ask = bytes - sizeof(struct avail);
  void *mem = buddy_malloc(&pool, ask);
  assert(mem != NULL);

  struct avail *tmp = (struct avail *)mem - 1;
  assert(tmp->kval == MIN_K);
  assert(tmp->tag == BLOCK_RESERVED);
  check_buddy_pool_empty(&pool);

  void *fail = buddy_malloc(&pool, 5);
  assert(fail == NULL);
  assert(errno == ENOMEM);

  buddy_free(&pool, mem);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

/**
 * Test initializing the pool with different sizes.
 */
void test_buddy_init(void)
{
  fprintf(stderr, "->Testing buddy init\n");
  for (size_t i = MIN_K; i <= DEFAULT_K; i++)
  {
    size_t size = UINT64_C(1) << i;
    struct buddy_pool pool;
    buddy_init(&pool, size);
    check_buddy_pool_full(&pool);
    buddy_destroy(&pool);
  }
}

/**
 * Test allocating and freeing multiple blocks.
 */
void test_buddy_malloc_multiple_blocks(void)
{
  fprintf(stderr, "->Testing multiple block allocations and frees\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  void *block1 = buddy_malloc(&pool, 64);
  void *block2 = buddy_malloc(&pool, 128);
  void *block3 = buddy_malloc(&pool, 256);

  assert(block1 != NULL);
  assert(block2 != NULL);
  assert(block3 != NULL);

  buddy_free(&pool, block1);
  buddy_free(&pool, block2);
  buddy_free(&pool, block3);

  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

/**
 * Test allocating zero bytes.
 */
void test_buddy_malloc_zero(void)
{
  fprintf(stderr, "->Testing zero byte allocation\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << MIN_K;
  buddy_init(&pool, size);

  void *block = buddy_malloc(&pool, 0);
  assert(block == NULL);

  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

/**
 * Test buddy merge after freeing blocks.
 */
void test_buddy_merge(void)
{
  fprintf(stderr, "->Testing buddy merge after freeing blocks\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  void *block1 = buddy_malloc(&pool, 64);
  void *block2 = buddy_malloc(&pool, 64);

  assert(block1 != NULL);
  assert(block2 != NULL);

  buddy_free(&pool, block1);
  buddy_free(&pool, block2);

  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

/**
 * Test reallocating memory with data preservation.
 */
void test_buddy_realloc(void)
{
  fprintf(stderr, "->Testing realloc with data preservation\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  int *block = (int *)buddy_malloc(&pool, sizeof(int) * 4);
  assert(block != NULL);

  block[0] = 42;
  block[1] = 84;

  int *new_block = (int *)buddy_realloc(&pool, block, sizeof(int) * 8);
  assert(new_block != NULL);
  assert(new_block[0] == 42);
  assert(new_block[1] == 84);

  buddy_free(&pool, new_block);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

void test_buddy_malloc_invalid(void)
{
  fprintf(stderr, "->Testing invalid malloc\n");
  void *block = buddy_malloc(NULL, 64);
  assert(block == NULL);
}

void test_buddy_free_invalid(void)
{
  fprintf(stderr, "->Testing invalid free\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  // Freeing NULL should do nothing
  buddy_free(&pool, NULL);

  // Freeing an invalid pointer
  int invalid_block;
  buddy_free(&pool, &invalid_block);

  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

void test_buddy_realloc_smaller(void)
{
  fprintf(stderr, "->Testing realloc to smaller size\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  int *block = (int *)buddy_malloc(&pool, sizeof(int) * 8);
  assert(block != NULL);

  block[0] = 42;

  int *new_block = (int *)buddy_realloc(&pool, block, sizeof(int) * 4);
  assert(new_block != NULL);
  assert(new_block[0] == 42);

  buddy_free(&pool, new_block);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

void test_buddy_realloc_null(void)
{
  fprintf(stderr, "->Testing realloc with NULL pointer\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  int *block = (int *)buddy_realloc(&pool, NULL, sizeof(int) * 4);
  assert(block != NULL);

  block[0] = 42;

  buddy_free(&pool, block);
  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}

void test_buddy_realloc_zero(void)
{
  fprintf(stderr, "->Testing realloc to size zero\n");
  struct buddy_pool pool;
  size_t size = UINT64_C(1) << DEFAULT_K;
  buddy_init(&pool, size);

  int *block = (int *)buddy_malloc(&pool, sizeof(int) * 4);
  assert(block != NULL);

  buddy_realloc(&pool, block, 0);

  check_buddy_pool_full(&pool);
  buddy_destroy(&pool);
}


/**
 * Main test runner.
 */
int main(void)
{
  time_t t;
  unsigned seed = (unsigned)time(&t);
  fprintf(stderr, "Random seed:%d\n", seed);
  srand(seed);
  printf("Running memory tests.\n");

  UNITY_BEGIN();
  RUN_TEST(test_buddy_init);
  RUN_TEST(test_buddy_malloc_one_byte);
  RUN_TEST(test_buddy_malloc_one_large);
  RUN_TEST(test_buddy_malloc_multiple_blocks);
  RUN_TEST(test_buddy_malloc_zero);
  RUN_TEST(test_buddy_merge);
  RUN_TEST(test_buddy_realloc);
  RUN_TEST(test_buddy_malloc_invalid);
  RUN_TEST(test_buddy_free_invalid);
  RUN_TEST(test_buddy_realloc_smaller);
  RUN_TEST(test_buddy_realloc_null);
  RUN_TEST(test_buddy_realloc_zero);
  return UNITY_END();
}

