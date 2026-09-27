// Separate calls model execution of jobs, each with its own stable TLS context.
__thread int initialized = 41;
__thread int zero;
__thread __attribute__((aligned(64))) char aligned[64];
int *local_initialized(void) { return &initialized; }
int *local_zero(void) { return &zero; }
char *local_aligned(void) { return aligned; }
