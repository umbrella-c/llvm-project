// Real CRT registration, allocation, callbacks and logical-job destruction.
#include <internal/_tls.h>
#include <ddk/ddkdefs.h>
#include <stdlib.h>
#include <threads.h>
extern void __cxa_module_tls_global_init(void);
extern void __cxa_module_global_init(void), __cxa_module_global_finit(void);
extern void __cxa_module_tls_thread_init(void), __cxa_module_tls_thread_finit(void);
extern void __cxa_runinitializers(const uintptr_t*, void (*)(void), void (*)(void), void (*)(void), void (*)(void));
extern void __cxa_threadinitialize(void), __cxa_threadfinalize(void);
extern int __cxa_thread_atexit_impl(void (*)(void*), void*, void*);
__declspec(dllimport) void __CrtLibraryEntry(int);
__declspec(dllimport) int *dll_value(void);
int *local_initialized(void), *local_zero(void);
char *local_aligned(void);
static thread_storage_t jobs[2];
static uint64_t slots[2][12];
extern int live_allocations, fail_allocation;
__attribute__((noreturn)) void test_exit(int);
#define CHECK(x) do { if (!(x)) test_exit(__LINE__); } while (0)
static int order;
static void last(void* arg) {
  CHECK(order == 2 && *local_initialized() == 101 && *dll_value() == 104);
  order = 3;
}
static void middle(void* arg) {
  CHECK(order == 1);
  order = 2;
}
static void reentrant(void* arg) {
  CHECK(order == 0); order = 1;
  CHECK(__cxa_thread_atexit_impl(middle, 0, (void*)2) == 0);
}
static tss_t plain_key, repeated_key;
static int repeats;
static void repeated(void* arg) {
  CHECK(tss_get(repeated_key) == NULL);
  CHECK(*local_initialized() == 101);
  ++repeats;
  CHECK(tss_set(repeated_key, arg) == thrd_success);
}
void entry(void) {
  __asm__ volatile("msr tpidr_el0, %0" : : "r"(slots[0]) : "memory");
  jobs[0].job_id = 1000; jobs[1].job_id = 1000; // numeric IDs can collide
  __tls_switch(&jobs[0]);
  __cxa_module_tls_global_init();
  static const unsigned char payload[] = {1, 2, 3};
  unsigned long synthetic, duplicate;
  CHECK(__tls_register_module((void*)10, payload, 3, 13, 8192, &synthetic) == 0);
  CHECK(__tls_register_module((void*)10, payload, 3, 13, 8192, &duplicate) == 0 && synthetic == duplicate);
  CHECK(__tls_register_module((void*)11, payload, 3, 0, 3, &duplicate) == -1);
  CHECK(__tls_register_module((void*)11, payload, (size_t)-1, 13, 16, &duplicate) == -1);
  CHECK(__tls_register_module((void*)10, payload, 2, 13, 8192, &duplicate) == -1);
#ifdef CAPACITY
  for (uintptr_t i = 0; i < 61; ++i)
    CHECK(__tls_register_module((void*)(100 + i), NULL, 0, 0, 16, &duplicate) == 0);
  __CrtLibraryEntry(DLL_ACTION_TLSREGISTER);
  CHECK(__tls_register_module((void*)999, NULL, 0, 0, 16, &duplicate) == -1);
#endif
  uintptr_t libraries[] = {(uintptr_t)&__CrtLibraryEntry, 0};
  __cxa_runinitializers(libraries, __cxa_module_global_init, __cxa_module_global_finit,
                       __cxa_module_tls_thread_init, __cxa_module_tls_thread_finit);
  unsigned char *block = __vali_tls_get_block(synthetic);
  CHECK(((uintptr_t)block & 8191) == 0 && block[0] == 1 && block[2] == 3);
  for (int i = 3; i < 16; ++i) CHECK(block[i] == 0);
  CHECK(*local_initialized() == 41 && *local_zero() == 0 && *dll_value() == 73);
  CHECK(((uintptr_t)local_aligned() & 63) == 0);
  CHECK(tss_create(&plain_key, NULL) == thrd_success);
  tss_t another;
  CHECK(tss_create(&another, NULL) == thrd_success && another != plain_key);
  tss_delete(another);
  CHECK(tss_create(&repeated_key, repeated) == thrd_success);
  CHECK(tss_set(plain_key, (void*)100) == thrd_success);
  CHECK(tss_set(repeated_key, (void*)200) == thrd_success);
  CHECK(tss_get(plain_key) == (void*)100);
  int *address = local_initialized(), *dll_address = dll_value();
  *address = 101; *local_zero() = 102; *dll_address = 104;
  CHECK(__cxa_thread_atexit_impl(last, 0, (void*)1) == 0);
  CHECK(__cxa_thread_atexit_impl(reentrant, 0, (void*)2) == 0);
  __tls_switch(&jobs[1]);
  CHECK(tss_get(plain_key) == NULL);
  CHECK(tss_set(plain_key, (void*)300) == thrd_success);
  // Allocation failure after the first module must roll it back entirely.
  int before = live_allocations;
  fail_allocation = 1;
  CHECK(__tls_prepare_modules() == -1);
  CHECK(live_allocations == before && !jobs[1].tls_modules_prepared_count);
  for (int i = 0; i < TLS_NUMBER_ENTRIES; ++i) CHECK(!jobs[1].tls_array[i]);
  fail_allocation = -1;
  __cxa_threadinitialize();
  CHECK(*local_initialized() == 41 && *local_zero() == 0 && *dll_value() == 73);
  CHECK(local_initialized() != address && dll_value() != dll_address);
  *local_initialized() = 201;
  __asm__ volatile("msr tpidr_el0, %0" : : "r"(slots[1]) : "memory");
  __tls_switch(&jobs[0]);
  CHECK(local_initialized() == address && *address == 101);
#ifndef CAPACITY
  unsigned long late, later;
  unsigned int prepared = jobs[0].tls_modules_prepared_count;
  CHECK(__tls_register_module((void*)99, payload, 3, 13, 64, &late) == 0);
  CHECK(__tls_register_module((void*)98, payload, 3, 13, 64, &later) == 0);
  before = live_allocations;
  fail_allocation = 1;
  CHECK(__tls_prepare_modules() == -1);
  CHECK(live_allocations == before && jobs[0].tls_modules_prepared_count == prepared);
  CHECK(!jobs[0].tls_array[late] && !jobs[0].tls_array[later]);
  CHECK(local_initialized() == address && *address == 101);
  fail_allocation = -1;
  CHECK(__tls_prepare_modules() == 0);
  before = live_allocations;
  CHECK(__tls_prepare_modules() == 0 && live_allocations == before);
  CHECK(((unsigned char*)__vali_tls_get_block(late))[2] == 3);
  CHECK(!jobs[1].tls_array[late]); // Registration alone does not update other jobs.
#endif
  __cxa_threadfinalize();
  CHECK(order == 3 && !jobs[0].tls_destructors);
  CHECK(repeats == TSS_DTOR_ITERATIONS && tss_get(repeated_key) == NULL);
  for (int i = 0; i < TLS_NUMBER_ENTRIES; ++i) CHECK(!jobs[0].tls_array[i]);
  __tls_switch(&jobs[1]);
  CHECK(*local_initialized() == 201 && *dll_value() == 73);
#ifndef CAPACITY
  CHECK(__tls_prepare_modules() == 0);
  CHECK(((unsigned char*)__vali_tls_get_block(late))[2] == 3);
#endif
  CHECK(tss_get(plain_key) == (void*)300);
  __cxa_threadfinalize();
  tss_delete(plain_key); tss_delete(repeated_key);
  CHECK(live_allocations == 0);
  unsigned long index;
#ifdef CAPACITY
  CHECK(__tls_register_module((void*)99, 0, 0, 0, 16, &index) == -1);
#else
  CHECK(__tls_register_module((void*)9999, 0, 0, 0, 16, &index) == 0);
#endif
  test_exit(0);
}
