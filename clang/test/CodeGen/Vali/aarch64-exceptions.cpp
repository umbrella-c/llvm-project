// REQUIRES: aarch64-registered-target
// RUN: %clang_cc1 -triple aarch64-uml-vali -fexceptions -fcxx-exceptions -emit-llvm -o - %s | FileCheck %s --check-prefix=IR
// RUN: %clang_cc1 -triple aarch64-uml-vali -fexceptions -fcxx-exceptions -funwind-tables=2 -S -o - %s | FileCheck %s --check-prefix=ASM

extern void may_throw();
extern void cleanup() noexcept;
struct Guard { ~Guard() { cleanup(); } };
int catches() {
  try { Guard guard; may_throw(); }
  catch (int n) { return n; }
  return 0;
}

// PE frame tables wrap an Itanium personality and LSDA, not a Windows SEH
// dispatcher personality or MSVC funclets.
// IR: define{{.*}} @_Z7catchesv(){{.*}} personality ptr @__gxx_personality_v0
// IR: landingpad { ptr, i32 }
// IR: catch ptr @_ZTIi
// IR: call ptr @__cxa_begin_catch
// ASM: .seh_proc _Z7catchesv
// ASM: .seh_handler __gxx_personality_v0, @unwind, @except
// ASM: .seh_handlerdata
// ASM: GCC_except_table
