/* langstatics-probe: Windows.Globalization.Language statics
 * (patches/sg/1529). Publisher asks for ILanguageStatics as a text box takes
 * the caret. Prints "statics=<hr> wf=<en-US><x-priv><bad1><bad2> tag=<tag>
 * statics2=<hr> tryset=<0|1>" */
#include <windows.h>
#include <stdio.h>
#include <roapi.h>
#include <winstring.h>

static const GUID IID_ILanguageStatics = {0xb23cd557,0x0865,0x46d4,{0x89,0xb8,0xd5,0x9b,0xe8,0x99,0x0f,0x0d}};
static const GUID IID_ILanguageStatics2 = {0x30199f6e,0x914b,0x4b2a,{0x9d,0x6e,0xe3,0xb0,0xe2,0x7d,0xbe,0x4f}};

typedef struct statics statics;
struct statics_vtbl
{
    HRESULT (WINAPI *QueryInterface)(statics *, REFIID, void **);
    ULONG (WINAPI *AddRef)(statics *);
    ULONG (WINAPI *Release)(statics *);
    HRESULT (WINAPI *GetIids)(statics *, ULONG *, IID **);
    HRESULT (WINAPI *GetRuntimeClassName)(statics *, HSTRING *);
    HRESULT (WINAPI *GetTrustLevel)(statics *, int *);
    HRESULT (WINAPI *Method1)(statics *, HSTRING, BOOLEAN *);
    HRESULT (WINAPI *Method2)(statics *, HSTRING *);
};
struct statics { const struct statics_vtbl *lpVtbl; };

static int well_formed(statics *s, const WCHAR *tag)
{
    HSTRING h; BOOLEAN r = 2;
    WindowsCreateString(tag, wcslen(tag), &h);
    if (FAILED(s->lpVtbl->Method1(s, h, &r))) r = 9;
    WindowsDeleteString(h);
    return r;
}

int main(void)
{
    HSTRING cls, tag = NULL, zz;
    statics *s = NULL, *s2 = NULL;
    HRESULT hr, hr2 = E_FAIL;
    BOOLEAN tried = 2;

    RoInitialize(RO_INIT_MULTITHREADED);
    WindowsCreateString(L"Windows.Globalization.Language", 30, &cls);
    hr = RoGetActivationFactory(cls, &IID_ILanguageStatics, (void **)&s);
    if (FAILED(hr)) { printf("statics=%lx\n", hr); return 0; }
    printf("statics=0 wf=%d%d%d%d", well_formed(s, L"en-US"), well_formed(s, L"x-private"),
           well_formed(s, L"en--US"), well_formed(s, L"toolongtag-US"));
    s->lpVtbl->Method2(s, &tag);
    printf(" tag=%ls", tag ? WindowsGetStringRawBuffer(tag, NULL) : L"(null)");
    hr2 = s->lpVtbl->QueryInterface(s, &IID_ILanguageStatics2, (void **)&s2);
    WindowsCreateString(L"zz-ZZ", 5, &zz);
    if (SUCCEEDED(hr2)) s2->lpVtbl->Method1(s2, zz, &tried);
    printf(" statics2=%lx tryset=%d\n", hr2, tried);
    return 0;
}
