/* Object inspection probe; never supply dummy implementations of helpers. */
struct pair { unsigned long long a, b; };
struct big { unsigned long long a, b, c; };
struct hfa { double a, b, c, d; };
__declspec(dllexport) struct pair pair_echo(struct pair x) { return x; }
__declspec(dllexport) struct big big_echo(struct big x) { return x; }
__declspec(dllexport) struct hfa hfa_echo(struct hfa x) { return x; }
__declspec(dllexport) double fp(double a, float b) { return a + b; }
__declspec(dllexport) double variadic(int n, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, n);
    double sum = 0;
    for (int i = 0; i < n; ++i) sum += __builtin_va_arg(ap, double);
    __builtin_va_end(ap);
    return sum;
}
__declspec(dllexport) double call_variadic(void) { return variadic(2, 1.25, 2.5); }
__declspec(thread) int tls_value = 17;
__declspec(dllexport) int *tls_address(void) { return &tls_value; }
__declspec(dllexport) volatile int constructed;
__attribute__((constructor)) static void initialize(void) { constructed = 23; }
extern void escape(volatile void *);
__declspec(dllexport) int large_frame(int i) {
    volatile unsigned char frame[32768];
    frame[0] = 7; frame[32767] = 9; escape(frame);
    return frame[(unsigned)i & 32767];
}
__declspec(dllexport) unsigned long long atomic_add(unsigned long long *p) {
    return __atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST);
}
__declspec(dllexport) __int128 divide128(__int128 a, __int128 b) { return a / b; }
