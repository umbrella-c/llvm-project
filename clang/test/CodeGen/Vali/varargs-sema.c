// RUN: %clang_cc1 -triple x86_64-uml-vali -fsyntax-only -verify %s

void default_abi(int n, ...) {
  __builtin_va_list a;
  __builtin_va_start(a, n);
  __builtin_va_end(a);
  __builtin_ms_va_list b;
  __builtin_ms_va_start(b, n);
  __builtin_ms_va_end(b);
}

void __attribute__((sysv_abi)) sysv(int n, ...) {
  __builtin_ms_va_list a;
  __builtin_ms_va_start(a, n); // expected-error {{'__builtin_ms_va_start' used in System V ABI function}}
  __builtin_va_list b;
  __builtin_va_start(b, n); // expected-error {{'va_start' used in System V ABI function}}
}

void __attribute__((ms_abi)) explicit_ms(int n, ...) {
  __builtin_ms_va_list a;
  __builtin_ms_va_start(a, n);
  __builtin_ms_va_end(a);
}
