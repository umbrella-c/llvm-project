/* Link-only PE TLS directory fixture, not a CRT implementation. */
#pragma section(".tls$AAA", read, write)
#pragma section(".tls$ZZZ", read, write)
__declspec(allocate(".tls$AAA")) char tls_start;
__declspec(allocate(".tls$ZZZ")) char tls_end;
unsigned int _tls_index;
struct tls_directory {
    const void *start, *end;
    unsigned int *index;
    const void *callbacks;
    unsigned int zero_fill, characteristics;
};
const struct tls_directory _tls_used = {
    &tls_start, &tls_end, &_tls_index, 0, 0, 0
};
