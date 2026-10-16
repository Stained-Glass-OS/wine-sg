/* Probe for patches/sg/4544: CRT internals of the DLL named on the command
 * line, each where the DLL exports it: _dosmaperr, __get_app_type,
 * _query_app_type, __p__osplatform, _get_winver, __get_tlsindex/
 * __get_flsindex, the __fls_* wrappers and __set_flsgetvalue,
 * _getdcwd_nolock, _ValidateRead/_ValidateWrite, type_info::_Name_base and
 * _Type_info_dtor, _set_new_handler(0), _set_se_translator(0), the debug
 * placement delete, __ExceptionPtrSwap. Lines "ok"/"FAIL", last "RESULT". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN64
#define N(a64, a32) a64
#else
#define N(a64, a32) a32
#endif

static int failures, checked;
#define check(c, ...) do { checked++; if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
static HMODULE mod;
#define P(name) GetProcAddress(mod, name)

static int __cdecl newh(size_t s) { return 0; }
static void __cdecl setr(unsigned int c, EXCEPTION_POINTERS *e) {}

int main(int argc, char **argv)
{
    static const char ro[] = "read only";
    void *f;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }

    if ((f = P("_dosmaperr")))
    {
        int *(__cdecl *p_errno)(void) = (void *)P("_errno");
        unsigned long *(__cdecl *p_doserrno)(void) = (void *)P("__doserrno");
        ((void (__cdecl *)(unsigned long))f)(ERROR_FILE_NOT_FOUND);
        check(*p_errno() == ENOENT && *p_doserrno() == ERROR_FILE_NOT_FOUND, "_dosmaperr: errno %d doserrno %lu", *p_errno(), *p_doserrno());
        ((void (__cdecl *)(unsigned long))f)(ERROR_ACCESS_DENIED);
        check(*p_errno() == EACCES, "_dosmaperr(ERROR_ACCESS_DENIED): %d", *p_errno());
    }
    if ((f = P("__get_app_type")))
    {
        void (__cdecl *set)(int) = (void *)P("__set_app_type");
        set(2);
        check(((int (__cdecl *)(void))f)() == 2, "__get_app_type after __set_app_type(2)");
        set(1);
        check(((int (__cdecl *)(void))f)() == 1, "__get_app_type after __set_app_type(1)");
    }
    if ((f = P("_query_app_type")))
    {
        void (__cdecl *set)(int) = (void *)P("_set_app_type");
        set(2);
        check(((int (__cdecl *)(void))f)() == 2, "_query_app_type");
        set(1);
    }
    if ((f = P("__p__osplatform")))
    {
        unsigned int *(__cdecl *winver)(void) = (void *)P("__p__winver");
        int (__cdecl *get_winver)(unsigned int *) = (void *)P("_get_winver");
        unsigned int v = 0;
        check(*((unsigned int *(__cdecl *)(void))f)() == VER_PLATFORM_WIN32_NT, "__p__osplatform");
        check(get_winver && !get_winver(&v) && (!winver || v == *winver()) && v >= 0x500, "_get_winver %#x", v);
    }
    if ((f = P("__get_tlsindex")))
    {
        DWORD idx = ((DWORD (__cdecl *)(void))f)();
        ((int *(__cdecl *)(void))P("_errno"))();  /* makes the thread data */
        check(idx != TLS_OUT_OF_INDEXES && TlsGetValue(idx) != NULL, "__get_tlsindex %lu holds the thread data", idx);
        check(P("__get_flsindex") && ((DWORD (__cdecl *)(void))P("__get_flsindex"))() == idx, "__get_flsindex");
    }
    if ((f = P("__fls_setvalue")))
    {
        DWORD idx = FlsAlloc(NULL);
        void *(__cdecl *get)(DWORD) = (void *)P("__fls_getvalue");
        void *(__cdecl *getter)(void) = (void *)P("__set_flsgetvalue");
        check(((BOOL (__cdecl *)(DWORD, void *))f)(idx, (void *)0x77) && FlsGetValue(idx) == (void *)0x77 && get(idx) == (void *)0x77,
              "__fls_setvalue/__fls_getvalue");
        if (getter)
            check(((void *(WINAPI *)(DWORD))getter())(idx) == (void *)0x77, "__set_flsgetvalue gives a getter");
        if (P("___fls_setvalue@8"))
        {
            check(((BOOL (WINAPI *)(DWORD, void *))P("___fls_setvalue@8"))(idx, (void *)0x88) &&
                  ((void *(WINAPI *)(DWORD))P("___fls_getvalue@4"))(idx) == (void *)0x88, "___fls_setvalue@8/___fls_getvalue@4");
        }
        FlsFree(idx);
    }
    if ((f = P("_getdcwd_nolock")))
    {
        char a[MAX_PATH], b[MAX_PATH];
        char *(__cdecl *g)(int, char *, int) = (void *)P("_getdcwd");
        check(((char *(__cdecl *)(int, char *, int))f)(0, a, MAX_PATH) && g(0, b, MAX_PATH) && !strcmp(a, b), "_getdcwd_nolock: %s", a);
    }
    if ((f = P(N("?_ValidateRead@@YAHPEBXI@Z", "?_ValidateRead@@YAHPBXI@Z"))))
    {
        int (__cdecl *w)(void *, unsigned int) = (void *)P(N("?_ValidateWrite@@YAHPEAXI@Z", "?_ValidateWrite@@YAHPAXI@Z"));
        char buf[8];
        check(((int (__cdecl *)(const void *, unsigned int))f)(ro, 4) && !((int (__cdecl *)(const void *, unsigned int))f)(NULL, 4),
              "_ValidateRead");
        check(w && w(buf, 8) && !w((void *)ro, 4) && !w(NULL, 1), "_ValidateWrite");
    }
    if ((f = P(N("?_Name_base@type_info@@CAPEBDPEBV1@PEAU__type_info_node@@@Z", "?_Name_base@type_info@@CAPBDPBV1@PAU__type_info_node@@@Z"))))
    {
        struct { const void *vtable; char *name; char mangled[32]; } ti = {NULL, NULL, ".?AVwidget@@"};
        const char *n = ((const char *(__cdecl *)(void *, void *))f)(&ti, NULL);
        void (__cdecl *dtor)(void *) = (void *)P(N("?_Type_info_dtor@type_info@@CAXPEAV1@@Z", "?_Type_info_dtor@type_info@@CAXPAV1@@Z"));
        check(n && !strcmp(n, "class widget"), "type_info::_Name_base: %s", n ? n : "(null)");
        if (dtor) dtor(&ti);
    }
    if ((f = P(N("?_set_new_handler@@YAP6AH_K@ZH@Z", "?_set_new_handler@@YAP6AHI@ZH@Z"))))
    {
        void *(__cdecl *set)(void *) = (void *)P(N("?_set_new_handler@@YAP6AH_K@ZP6AH0@Z@Z", "?_set_new_handler@@YAP6AHI@ZP6AHI@Z@Z"));
        set(newh);
        check(((void *(__cdecl *)(int))f)(0) == (void *)newh && set(NULL) == NULL, "_set_new_handler(0)");
    }
    if ((f = P(N("?_set_se_translator@@YAP6AXIPEAU_EXCEPTION_POINTERS@@@ZH@Z", "?_set_se_translator@@YAP6AXIPAU_EXCEPTION_POINTERS@@@ZH@Z"))))
    {
        void *(__cdecl *set)(void *) = (void *)P(N("?_set_se_translator@@YAP6AXIPEAU_EXCEPTION_POINTERS@@@ZP6AXI0@Z@Z", "?_set_se_translator@@YAP6AXIPAU_EXCEPTION_POINTERS@@@ZP6AXI0@Z@Z"));
        if (set)
        {
            set(setr);
            check(((void *(__cdecl *)(int))f)(0) == (void *)setr && set(NULL) == NULL, "_set_se_translator(0)");
        }
    }
    if ((f = P(N("??3@YAXPEAXHPEBDH@Z", "??3@YAXPAXHPBDH@Z"))))
    {
        void *(__cdecl *m)(size_t) = (void *)P("malloc");
        intptr_t (__cdecl *heap)(void) = (void *)P("_get_heap_handle");
        void *p = m(32);
        ((void (__cdecl *)(void *, int, const char *, int))f)(p, 1, "file.c", 12);
        check(!HeapValidate((HANDLE)heap(), 0, p), "operator delete(void*, int, const char*, int) frees");
    }
    if ((f = P(N("?__ExceptionPtrSwap@@YAXPEAX0@Z", "?__ExceptionPtrSwap@@YAXPAX0@Z"))))
    {
        void *a[2] = {(void *)1, (void *)2}, *b[2] = {(void *)3, (void *)4};
        ((void (__cdecl *)(void *, void *))f)(a, b);
        check(a[0] == (void *)3 && a[1] == (void *)4 && b[0] == (void *)1 && b[1] == (void *)2, "__ExceptionPtrSwap");
    }
    check(checked > 0, "%d checks", checked);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
