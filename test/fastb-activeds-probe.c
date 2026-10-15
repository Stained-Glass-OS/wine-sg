/* activeds gate: IADsPathname::Retrieve in every ADS_FORMAT */
#include <windows.h>
#include <oleauto.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct IPath IPath;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IPath *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IPath *);
    ULONG (WINAPI *Release)(IPath *);
    void *disp[4];
    HRESULT (WINAPI *Set)(IPath *, BSTR, LONG);
    HRESULT (WINAPI *SetDisplayType)(IPath *, LONG);
    HRESULT (WINAPI *Retrieve)(IPath *, LONG, BSTR *);
} PathVtbl;
struct IPath { const PathVtbl *v; };

static const GUID clsid = { 0x080d0d78, 0xf421, 0x11d0, { 0xa3, 0x6e, 0x00, 0xc0, 0x4f, 0xb9, 0x50, 0xdc } };
static const GUID iid = { 0xd592aed4, 0xf420, 0x11d0, { 0xa3, 0x6e, 0x00, 0xc0, 0x4f, 0xb9, 0x50, 0xdc } };

static void check(IPath *p, int format, const WCHAR *expect, const char *name)
{
    BSTR b = NULL;
    HRESULT hr = p->v->Retrieve(p, format, &b);
    CHECK(hr == S_OK && b && !wcscmp(b, expect), "%s (format %d): hr %#lx got %ls expected %ls", name, format, hr, b ? b : L"(null)", expect);
    SysFreeString(b);
}

int main(void)
{
    IPath *p = NULL;
    BSTR path;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &iid, (void **)&p);
    CHECK(hr == S_OK && p, "CoCreateInstance %#lx", hr);
    if (!p) return 1;

    path = SysAllocString(L"LDAP://server.example/cn=bar,ou=mid,o=internet");
    hr = p->v->Set(p, path, 1 /* ADS_SETTYPE_FULL */);
    CHECK(hr == S_OK, "Set %#lx", hr);
    SysFreeString(path);

    check(p, 1, L"LDAP://server.example/o=internet/ou=mid/cn=bar", "WINDOWS");
    check(p, 2, L"LDAP://o=internet/ou=mid/cn=bar", "WINDOWS_NO_SERVER");
    check(p, 3, L"o=internet/ou=mid/cn=bar", "WINDOWS_DN");
    check(p, 4, L"o=internet/ou=mid", "WINDOWS_PARENT");
    check(p, 5, L"LDAP://server.example/cn=bar,ou=mid,o=internet", "X500");
    check(p, 6, L"LDAP://cn=bar,ou=mid,o=internet", "X500_NO_SERVER");
    check(p, 7, L"cn=bar,ou=mid,o=internet", "X500_DN");
    check(p, 8, L"ou=mid,o=internet", "X500_PARENT");
    check(p, 9, L"server.example", "SERVER");
    check(p, 10, L"LDAP", "PROVIDER");
    check(p, 11, L"cn=bar", "LEAF");

    path = SysAllocString(L"LDAP://x/cn=a\\,b,o=internet");
    p->v->Set(p, path, 1);
    SysFreeString(path);
    check(p, 3, L"o=internet/cn=a\\,b", "WINDOWS_DN with an escaped comma");
    check(p, 8, L"o=internet", "X500_PARENT with an escaped comma");

    path = SysAllocString(L"LDAP://x/o=internet");
    p->v->Set(p, path, 1);
    SysFreeString(path);
    check(p, 4, L"", "WINDOWS_PARENT of a single element");
    check(p, 8, L"", "X500_PARENT of a single element");
    check(p, 2, L"LDAP://o=internet", "WINDOWS_NO_SERVER single");

    p->v->Release(p);
    CoUninitialize();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
