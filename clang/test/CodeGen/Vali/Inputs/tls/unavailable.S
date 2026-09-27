// Unused libc/OS dependencies of tls.c. Trap if this fixture accidentally calls
// initialization, allocation or SHM services; those paths are not qualified.
.text
.globl memset, __crt_environment, calloc, strdup, _assert_panic
.globl free, SHMBuffer, OSHandleDestroy, SHMCreate
memset:
__crt_environment:
calloc:
strdup:
_assert_panic:
free:
SHMBuffer:
OSHandleDestroy:
SHMCreate:
  brk #0
