#include <internal/_tls.h>
#include <os/usched/mutex.h>
#include <stdint.h>
#include <setjmp.h>
#include <stddef.h>
_Static_assert(sizeof(jmp_buf) == 192, "ARM64 jump buffer size");
_Static_assert(offsetof(_JUMP_BUFFER, Sp) == 96, "ARM64 SP offset");
_Static_assert(offsetof(_JUMP_BUFFER, D8_D15) == 128, "ARM64 D-register offset");
extern void __cxa_module_tls_global_init(void);
extern void __cxa_module_global_init(void), __cxa_module_global_finit(void);
extern void __cxa_module_tls_thread_init(void), __cxa_module_tls_thread_finit(void);
extern void __cxa_runinitializers(const uintptr_t*, void (*)(void), void (*)(void), void (*)(void), void (*)(void));
extern void __cxa_threadinitialize(void), __cxa_threadfinalize(void);
extern _Noreturn void __usched_task_start(void*, void*, void (*)(void*));
extern void job_body(int);
extern void test_exit(int);
static thread_storage_t main_tls, jobs[2];
static uint64_t slots[12];
static jmp_buf main_context, contexts[2];
static _Alignas(16) char stacks[2][65536];
static int active, done[2];
void test_job_yield(void) {
  if (setjmp(contexts[active])) return;
  __tls_switch(&main_tls);
  longjmp(main_context, 1);
}
static void start_job(void* unused) {
  __cxa_threadinitialize();
  job_body(active);
  __cxa_threadfinalize();
  done[active] = 1;
  test_job_yield();
  __builtin_trap();
}
void threaded_setup(void) {
  __asm__ volatile("msr tpidr_el0, %0" : : "r"(slots) : "memory");
  __tls_switch(&main_tls);
  __cxa_module_tls_global_init();
  __cxa_runinitializers(0, __cxa_module_global_init, __cxa_module_global_finit,
                       __cxa_module_tls_thread_init, __cxa_module_tls_thread_finit);
  for (int i = 0; i < 2; ++i) {
    jobs[i].job_id = i + 1;

  }
  jmp_buf probe;
  int value = setjmp(probe);
  if (!value) longjmp(probe, 0);
  if (value != 1) test_exit(88);
  value = setjmp(probe);
  if (!value) longjmp(probe, -7);
  if (value != -7) test_exit(89);
  // Both jobs must suspend inside a catch before either one resumes.
  for (int round = 0; round < 2; ++round) {
    for (active = 0; active < 2; ++active) {
      if (!setjmp(main_context)) {
        __tls_switch(&jobs[active]);
        if (!round)
          __usched_task_start(stacks[active] + sizeof(stacks[active]), 0, start_job);
        longjmp(contexts[active], 1);
      }
      if (done[active] != round) test_exit(80);
    }
  }
}
// Cooperative fixture locks: no job yields while holding a runtime lock.
// Kernel-worker contention and production scheduler mutexes remain unqualified.
void usched_mtx_lock(struct usched_mtx* m) { spinlock_acquire(&m->lock); }
void usched_mtx_unlock(struct usched_mtx* m) { spinlock_release(&m->lock); }


#include <internal/_tls.h>
#include <stdlib.h>
void *dsalloc(size_t n) { return malloc(n); }
void dsfree(void *p) { free(p); }
int *__errno(void) { return &__tls_current()->err_no; }
uuid_t __crt_thread_id(void) { return __tls_current()->job_id; }
