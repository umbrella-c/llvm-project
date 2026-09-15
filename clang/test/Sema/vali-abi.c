// RUN: %clang_cc1 -triple i386-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple x86_64-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple armv7-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple thumbv7-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple armebv7-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple thumbebv7-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple aarch64-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple aarch64_be-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple mips-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple mipsel-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple mips64-uml-vali -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple mips64el-uml-vali -fsyntax-only -verify %s
// expected-no-diagnostics

// These are frontend checks. Success does not imply VPE object emission or
// OS runtime support on every architecture represented here.
#if !defined(VALI) || !defined(__VALI__) || !defined(MOLLENOS) || !defined(__MOLLENOS__)
#error missing Vali or legacy MollenOS platform definitions
#endif
#ifdef _WIN32
#error Vali must not define the Windows OS macro
#endif

_Static_assert(sizeof(__WCHAR_TYPE__) == 2, "Vali wchar_t is 16 bits");
_Static_assert((__WCHAR_TYPE__)-1 > 0, "Vali wchar_t is unsigned");
_Static_assert(sizeof(__WINT_TYPE__) == 2, "Vali wint_t is 16 bits");

#if defined(__i386__) || defined(__x86_64__)
_Static_assert(sizeof(long) == 4, "Vali long is 32 bits");
_Static_assert(_Alignof(double) == 8, "Vali double alignment");
_Static_assert(_Alignof(long long) == 8, "Vali long long alignment");
struct record {
  char tag;
  long long value;
};
_Static_assert(__builtin_offsetof(struct record, value) == 8, "record ABI");
_Static_assert(sizeof(struct record) == 16, "record tail padding");
#endif

#ifdef __x86_64__
_Static_assert(sizeof(void *) == 8, "Vali x86-64 pointers are 64 bits");
_Static_assert(sizeof(long double) == 8, "Vali x86-64 long double is double");
_Static_assert(__LDBL_MANT_DIG__ == 53, "long double precision");
_Static_assert(__builtin_types_compatible_p(__builtin_va_list, char *),
               "Vali x86-64 uses pointer-style va_list");
_Static_assert(__builtin_types_compatible_p(__SIZE_TYPE__, unsigned long long),
               "Vali x86-64 size_t ABI");
_Static_assert(__builtin_types_compatible_p(__PTRDIFF_TYPE__, long long),
               "Vali x86-64 ptrdiff_t ABI");
#endif
