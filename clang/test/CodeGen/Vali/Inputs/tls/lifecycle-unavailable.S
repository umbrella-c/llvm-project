// Unused services: accidental invocation is fatal, never a success stub.
.text
.globl __crt_environment, calloc, strdup, _assert_panic
.globl SHMBuffer, OSHandleDestroy, SHMCreate, __at_exit_impl
.globl __at_quick_exit_impl, __cxa_at_exit_run
__crt_environment:
calloc:
strdup:
_assert_panic:
SHMBuffer:
OSHandleDestroy:
SHMCreate:
__at_exit_impl:
__at_quick_exit_impl:
__cxa_at_exit_run:
  brk #0
