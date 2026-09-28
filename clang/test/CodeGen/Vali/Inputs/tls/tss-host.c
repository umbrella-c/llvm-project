// Exercise actual Vali TSS and hashtable code on concurrent host workers.
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <threads.h>
static _Thread_local unsigned identity;
static _Thread_local int token, calls;
unsigned __crt_thread_id(void) { return 1000; }
void *__tls_current(void) { return &token; }
void tss_cleanup(void);
static tss_t key, plain;
static pthread_barrier_t barrier;
static atomic_int first_done;
static void destructor(void *p) {
  assert(p == &token && tss_get(key) == NULL);
  ++calls;
  // Key creation/deletion and reassignment must be safe from a destructor.
  tss_t temporary;
  assert(tss_create(&temporary, NULL) == thrd_success);
  tss_delete(temporary);
  assert(tss_set(key, p) == thrd_success);
}
static void *worker(void *p) {
  identity = (uintptr_t)p;
  assert(tss_get(key) == NULL);
  assert(tss_set(key, &token) == thrd_success);
  assert(tss_set(plain, &token) == thrd_success);
  pthread_barrier_wait(&barrier);
  if (identity == 2) {
    while (!atomic_load(&first_done)) assert(tss_get(key) == &token);
    assert(tss_get(plain) == &token);
  }
  tss_cleanup();
  assert(calls == TSS_DTOR_ITERATIONS);
  assert(tss_get(key) == NULL && tss_get(plain) == NULL);
  if (identity == 1) atomic_store(&first_done, 1);
  return NULL;
}
int main(void) {
  tss_t extra;
  assert(tss_create(&key, destructor) == thrd_success);
  assert(tss_create(&plain, NULL) == thrd_success);
  assert(tss_create(&extra, NULL) == thrd_success && extra != plain && extra != key);
  tss_delete(extra);
  pthread_t workers[2];
  assert(pthread_barrier_init(&barrier, NULL, 2) == 0);
  assert(pthread_create(&workers[0], NULL, worker, (void*)1) == 0);
  assert(pthread_create(&workers[1], NULL, worker, (void*)2) == 0);
  assert(pthread_join(workers[0], NULL) == 0);
  assert(pthread_join(workers[1], NULL) == 0);
  tss_delete(key); tss_delete(plain);
  pthread_barrier_destroy(&barrier);
  return 0;
}
