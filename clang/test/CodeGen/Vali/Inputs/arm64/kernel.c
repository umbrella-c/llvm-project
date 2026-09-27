unsigned long long kernel_atomic(unsigned long long *p) {
    return __atomic_fetch_add(p, 1, __ATOMIC_SEQ_CST);
}
