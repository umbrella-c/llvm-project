__declspec(dllimport) extern int exported_data;
__declspec(dllimport) int exported_function(int);
int entry(void) { return exported_function(exported_data) == 74 ? 0 : 1; }
