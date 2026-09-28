// Unused services in the real OS TLS/CRT source files.
.text
.globl __crt_environment, strdup, SHMBuffer, OSHandleDestroy, SHMCreate
.globl __at_exit_impl, __at_quick_exit_impl, __cxa_at_exit_run
__crt_environment:
strdup:
SHMBuffer:
OSHandleDestroy:
SHMCreate:
__at_exit_impl:
__at_quick_exit_impl:
__cxa_at_exit_run:
  brk #0
