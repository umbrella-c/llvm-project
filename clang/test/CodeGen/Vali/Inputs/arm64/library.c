__declspec(dllexport) int exported_data = 37;
__declspec(dllexport) int *exported_pointer = &exported_data;
__declspec(dllexport) int exported_function(int x) { return x + exported_data; }
