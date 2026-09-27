#include <ddk/ddkdefs.h>
size_t kernel_slot(void) { return __get_reserved(0); }
void kernel_set(size_t value) { __set_reserved(0, value); }
