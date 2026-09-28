#include <crtdefs.h>
__thread int value = 73;
__declspec(dllexport) int *dll_value(void) { return &value; }
void dllmain(int action) {
  if ((action == DLL_ACTION_INITIALIZE || action == DLL_ACTION_THREADATTACH) && value != 73)
    __builtin_trap();
}
