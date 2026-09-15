// RUN: %clang_cc1 -triple x86_64-uml-vali -emit-llvm -o - %s | FileCheck %s --check-prefix=X64
// RUN: %clang_cc1 -triple x86_64-uml-vali -fexperimental-abi-lowering -emit-llvm -o - %s | FileCheck %s --check-prefix=X64
// RUN: %clang_cc1 -triple i386-uml-vali -emit-llvm -o - %s | FileCheck %s --check-prefix=X32
// RUN: %clang_cc1 -triple x86_64-uml-vali -emit-obj -o %t64.obj %s
// RUN: %clang_cc1 -triple i386-uml-vali -emit-obj -o %t32.obj %s

_Static_assert(sizeof(long) == 4, "Vali uses 32-bit long");
_Static_assert(sizeof(__WCHAR_TYPE__) == 2, "Vali wchar_t width");
struct Pair { long long x, y; };
struct Small { int x; };

// X64-LABEL: define{{.*}} void @pair(ptr{{.*}} sret(%struct.Pair){{.*}}, ptr{{.*}} %
// X32-LABEL: define{{.*}} void @pair(ptr{{.*}} sret(%struct.Pair){{.*}}, ptr noundef align 8
struct Pair pair(struct Pair v) { return v; }

// X64-LABEL: define{{.*}} i32 @small(i32
// X32-LABEL: define{{.*}} void @small(ptr{{.*}} sret(%struct.Small)
struct Small small(struct Small v) { return v; }

#ifdef __VALI64__
_Static_assert(_Alignof(__int128) == 16, "Vali i128 alignment");
struct WideMember { char tag; __int128 value; };
_Static_assert(__builtin_offsetof(struct WideMember, value) == 16,
               "Vali i128 member alignment");
_Static_assert(sizeof(struct WideMember) == 32, "Vali i128 aggregate size");
// X64-LABEL: define{{.*}} <2 x i64> @wide(ptr
__int128 wide(__int128 value) { return value; }
#endif

// X32-LABEL: define{{.*}} x86_stdcallcc i32 @{{.*}}std_call@8"(
// X64-LABEL: define{{.*}} i32 @std_call(
int __attribute__((stdcall)) std_call(int a, int b) { return a + b; }

// X32-LABEL: define{{.*}} x86_fastcallcc i32 @{{.*}}fast_call@8"(i32 inreg
// X64-LABEL: define{{.*}} i32 @fast_call(
int __attribute__((fastcall)) fast_call(int a, int b) { return a + b; }

// X64-LABEL: define{{.*}} i32 @vararg(
// X32-LABEL: define{{.*}} i32 @vararg(
int vararg(int unused, ...) {
  __builtin_va_list args;
  __builtin_va_start(args, unused);
  int result = __builtin_va_arg(args, int);
  __builtin_va_end(args);
  return result;
}

extern void sink(int tag, ...);
// X64-LABEL: define{{.*}} void @null_vararg(
// X64: call void (i32, ...) @sink(i32{{.*}} 1, i64{{.*}} 0)
// X32-LABEL: define{{.*}} void @null_vararg(
// X32: call void (i32, ...) @sink(i32{{.*}} 1, i32{{.*}} 0)
void null_vararg(void) { sink(1, 0); }

#ifdef __VALI64__
// X64-LABEL: define{{.*}} x86_64_sysvcc { i64, i64 } @sysv_pair(i64{{.*}}, i64
struct Pair __attribute__((sysv_abi)) sysv_pair(struct Pair v) { return v; }
#endif
