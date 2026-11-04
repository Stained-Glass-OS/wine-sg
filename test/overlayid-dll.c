/* overlayid: a test icon overlay handler (patch 6245).  Three classes: "a" marks *.ova (priority 10),
 * "b" marks *.ovb (priority 0), "f" marks nothing (a filler); last_path() is the path last asked. */
#define COBJMACROS
#include <windows.h>
#include <shlwapi.h>

static const GUID IID_Identifier = { 0x0c6c4200, 0xc589, 0x11d0, { 0x99, 0x9a, 0x00, 0xc0, 0x4f, 0xd6, 0x55, 0xe1 } };
static const GUID CLSID_A = { 0x7b1d0e11, 0x2b6d, 0x4c11, { 0x9a, 0x01, 0x0e, 0x57, 0xa1, 0x10, 0x0e, 0x0a } };
static const GUID CLSID_B = { 0x7b1d0e11, 0x2b6d, 0x4c11, { 0x9a, 0x01, 0x0e, 0x57, 0xa1, 0x10, 0x0e, 0x0b } };

static WCHAR last[MAX_PATH];
__declspec(dllexport) const WCHAR *last_path(void) { return last; }

struct handler { void **vtbl; char kind; };

static HRESULT WINAPI h_qi(struct handler *h, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_Identifier)) { *o = h; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI h_ar(struct handler *h) { return 2; }
static ULONG WINAPI h_rl(struct handler *h) { return 1; }
static HRESULT WINAPI h_member(struct handler *h, const WCHAR *path, DWORD attrib)
{
    const WCHAR *ext = PathFindExtensionW(path);
    lstrcpynW(last, path, MAX_PATH);
    if (h->kind == 'a') return !lstrcmpiW(ext, L".ova") ? S_OK : S_FALSE;
    if (h->kind == 'b') return !lstrcmpiW(ext, L".ovb") ? S_OK : S_FALSE;
    return S_FALSE;
}
static HRESULT WINAPI h_info(struct handler *h, WCHAR *file, int max, int *index, DWORD *flags)
{
    GetSystemDirectoryW(file, max);
    lstrcatW(file, L"\\shell32.dll");
    *index = h->kind == 'a' ? 1 : h->kind == 'b' ? 2 : 3;
    *flags = 3;  /* ISIOI_ICONFILE | ISIOI_ICONINDEX */
    return S_OK;
}
static HRESULT WINAPI h_priority(struct handler *h, int *p) { *p = h->kind == 'a' ? 10 : h->kind == 'b' ? 0 : 50; return S_OK; }
static void *h_vtbl[] = { h_qi, h_ar, h_rl, h_member, h_info, h_priority };
static struct handler ha = { h_vtbl, 'a' }, hb = { h_vtbl, 'b' }, hf = { h_vtbl, 'f' };

struct factory { void **vtbl; struct handler *h; };
static HRESULT WINAPI f_qi(struct factory *f, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IClassFactory)) { *o = f; return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI f_ar(struct factory *f) { return 2; }
static ULONG WINAPI f_rl(struct factory *f) { return 1; }
static HRESULT WINAPI f_ci(struct factory *f, IUnknown *outer, REFIID r, void **o)
{
    return h_qi(f->h, r, o);
}
static HRESULT WINAPI f_lock(struct factory *f, BOOL l) { return S_OK; }
static void *f_vtbl[] = { f_qi, f_ar, f_rl, f_ci, f_lock };
static struct factory fa = { f_vtbl, &ha }, fb = { f_vtbl, &hb }, ff = { f_vtbl, &hf };

HRESULT WINAPI DllGetClassObject(REFCLSID c, REFIID r, void **o)
{
    struct factory *f = IsEqualGUID(c, &CLSID_A) ? &fa : IsEqualGUID(c, &CLSID_B) ? &fb : &ff;
    return f_qi(f, r, o);
}
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
