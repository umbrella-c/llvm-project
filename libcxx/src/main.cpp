// Library entry point
#ifdef _DLL
extern "C" void dllmain(int action)
{
    (void)action;
}
#else
void __libcpp_not_used()
{
    
}
#endif
