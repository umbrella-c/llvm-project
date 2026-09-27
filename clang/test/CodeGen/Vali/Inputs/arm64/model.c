/* No SDK or host headers: measure the compiler itself. */
#define FACT(T, N) const unsigned N[] = {sizeof(T), _Alignof(T)}
FACT(char, abi_char);
FACT(short, abi_short);
FACT(int, abi_int);
FACT(long, abi_long);
FACT(long long, abi_long_long);
FACT(void *, abi_pointer);
FACT(float, abi_float);
FACT(double, abi_double);
FACT(long double, abi_long_double);
FACT(__WCHAR_TYPE__, abi_wchar);
FACT(__SIZE_TYPE__, abi_size);
FACT(__builtin_va_list, abi_va_list);
struct padded { char a; double b; short c; };
FACT(struct padded, abi_padded);
#ifdef VALI_ABI_CONTRACT
_Static_assert(sizeof(long) == 4, "Vali ARM64 requires LLP64");
_Static_assert(sizeof(long double) == 8 && __LDBL_MANT_DIG__ == 53,
               "Vali ARM64 requires binary64 long double");
_Static_assert(sizeof(__WCHAR_TYPE__) == 2 && (__WCHAR_TYPE__)-1 > 0,
               "Vali ARM64 requires unsigned 16-bit wchar_t");
_Static_assert(sizeof(void *) == 8 && sizeof(int) == 4, "integer model");
_Static_assert((char)-1 < 0, "Vali ARM64 requires signed plain char");
_Static_assert(_Alignof(double) == 8 && _Alignof(long double) == 8,
               "floating alignment");
_Static_assert(sizeof(struct padded) == 24, "aggregate layout");
#endif
