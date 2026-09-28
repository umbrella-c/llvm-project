#include <cxxabi.h>
#include "cxa_exception.h"
#include <exception>
#include <typeinfo>
extern "C" void test_job_yield();
extern "C" void test_exit(int);
extern "C" void message(const char*);
static void *globals[2];
static int destroyed[2];
struct Local {
  int id = -1;
  ~Local() {
    if (id < 0 || std::uncaught_exceptions() != 0) test_exit(81);
    ++destroyed[id];
  }
};
static thread_local Local local;
extern "C" void job_body(int id) {
  local.id = id;
  globals[id] = __cxxabiv1::__cxa_get_globals();
  try {
    if (id) throw 2.5;
    throw 42;
  } catch (...) {
    const std::type_info* type = __cxxabiv1::__cxa_current_exception_type();
    if (!type || *type != (id ? typeid(double) : typeid(int))) test_exit(82);
    unsigned long long fpcr = (unsigned long long)(id + 1) << 22;
    unsigned long long fpsr = 1u << id;
    __asm__ volatile("msr fpcr, %0; msr fpsr, %1" : : "r"(fpcr), "r"(fpsr) : "memory");
    register unsigned long long gp __asm__("x19") = 0x12345000 + id;
    register double fp __asm__("d8") = 10.5 + id;
    __asm__ volatile("" : "+r"(gp), "+w"(fp));
    test_job_yield();
    __asm__ volatile("" : "+r"(gp), "+w"(fp));
    unsigned long long actual_cr, actual_sr;
    __asm__ volatile("mrs %0, fpcr; mrs %1, fpsr" : "=r"(actual_cr), "=r"(actual_sr));
    if (actual_cr != fpcr || actual_sr != fpsr || gp != 0x12345000 + id || fp != 10.5 + id)
      test_exit(90);
    if (globals[0] == globals[1] || __cxxabiv1::__cxa_get_globals() != globals[id] ||
        local.id != id || std::uncaught_exceptions() != 0 ||
        __cxxabiv1::__cxa_current_exception_type() != type) test_exit(83);
    try { throw; }
    catch (int value) { if (id || value != 42) test_exit(84); }
    catch (double value) { if (!id || value != 2.5) test_exit(85); }
  }
  if (__cxxabiv1::__cxa_current_exception_type()) test_exit(86);
}
extern "C" void threaded_verify() {
  if (destroyed[0] != 1 || destroyed[1] != 1) test_exit(87);
  message("logical-job TLS, suspended catches, rethrow, C++ TLS destructors: PASS\n");
}
