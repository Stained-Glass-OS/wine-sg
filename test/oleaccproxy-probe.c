/* oleacc (patches/sg/2655): CreateStdAccessibleProxyA/W give the standard accessible object of a window. */
#include <windows.h>
#include <oleacc.h>
#include <stdio.h>
#include <stdarg.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

typedef HRESULT (WINAPI *PFN_W)(HWND, const WCHAR *, LONG, REFIID, void **);
typedef HRESULT (WINAPI *PFN_A)(HWND, const char *, LONG, REFIID, void **);

static LONG role_of(void *acc)
{
    VARIANT self, role;

    VariantInit(&self);
    V_VT(&self) = VT_I4;
    V_I4(&self) = CHILDID_SELF;
    VariantInit(&role);
    if (FAILED(((HRESULT (STDMETHODCALLTYPE *)(void *, VARIANT, VARIANT *))(*(void ***)acc)[13])(acc, self, &role)) || V_VT(&role) != VT_I4)
        return -1;
    return V_I4(&role);
}

int main(void)
{
    HMODULE mod = LoadLibraryA("oleacc.dll");
    PFN_W proxyW = mod ? (PFN_W)GetProcAddress(mod, "CreateStdAccessibleProxyW") : NULL;
    PFN_A proxyA = mod ? (PFN_A)GetProcAddress(mod, "CreateStdAccessibleProxyA") : NULL;
    HWND hwnd;
    void *obj;
    HRESULT hr;

    CoInitialize(NULL);
    if (!proxyW || !proxyA) { printf("FAIL  the exports are missing\nRESULT: FAIL\n"); return 1; }
    hwnd = CreateWindowA("button", "ok", WS_CHILD | BS_PUSHBUTTON, 0, 0, 80, 24, HWND_MESSAGE, NULL, NULL, NULL);
    check(hwnd != NULL, "a button window");

    obj = NULL;
    hr = proxyW(hwnd, L"Button", OBJID_CLIENT, &IID_IAccessible, &obj);
    check(hr == S_OK && obj, "W: Button proxy for the client (hr %#lx)", hr);
    if (obj)
    {
        check(role_of(obj) == ROLE_SYSTEM_PUSHBUTTON, "W: the role is a push button (%ld)", role_of(obj));
        ((ULONG (STDMETHODCALLTYPE *)(void *))(*(void ***)obj)[2])(obj);
    }
    obj = NULL;
    hr = proxyA(hwnd, "Button", OBJID_CLIENT, &IID_IAccessible, &obj);
    check(hr == S_OK && obj, "A: Button proxy for the client (hr %#lx)", hr);
    if (obj) ((ULONG (STDMETHODCALLTYPE *)(void *))(*(void ***)obj)[2])(obj);

    obj = (void *)1;
    hr = proxyW(hwnd, NULL, OBJID_CLIENT, &IID_IAccessible, &obj);
    check(hr == E_INVALIDARG, "W: no class name is E_INVALIDARG (%#lx)", hr);
    check(obj == NULL, "the result is cleared on failure");
    hr = proxyW(hwnd, L"", OBJID_CLIENT, &IID_IAccessible, &obj);
    check(hr == E_INVALIDARG, "W: an empty class name is E_INVALIDARG (%#lx)", hr);
    hr = proxyA(hwnd, NULL, OBJID_CLIENT, &IID_IAccessible, &obj);
    check(hr == E_INVALIDARG, "A: no class name is E_INVALIDARG (%#lx)", hr);
    hr = proxyW(hwnd, L"Button", OBJID_CLIENT, &IID_IAccessible, NULL);
    check(hr == E_INVALIDARG, "W: no result pointer is E_INVALIDARG (%#lx)", hr);
    hr = proxyA(hwnd, "Button", OBJID_CLIENT, &IID_IAccessible, NULL);
    check(hr == E_INVALIDARG, "A: no result pointer is E_INVALIDARG (%#lx)", hr);
    hr = proxyW(hwnd, L"Button", 0x7fff, &IID_IAccessible, &obj);
    check(hr == E_INVALIDARG, "W: an unknown object id is E_INVALIDARG (%#lx)", hr);

    DestroyWindow(hwnd);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
