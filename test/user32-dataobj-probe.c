/* Probe for patches/sg/5221: the data object a drag from another (X11) program hands to an OLE drop
 * target answers GetDataHere (copies into the caller's HGLOBAL, STG_E_MEDIUMFULL when it is too small,
 * DV_E_TYMED / DV_E_FORMATETC otherwise) and GetCanonicalFormatEtc (the input format without its
 * device, DATA_S_SAMEFORMATETC).  The drag is started the way the X11 driver does, with
 * NtUserMessageCall(WINE_DRAG_DROP_*).  Table driven; last line "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

typedef LRESULT (WINAPI *message_call_t)(HWND, UINT, WPARAM, LPARAM, void *, UINT, BOOL);
#define NT_USER_DRAG_DROP_CALL 0x0307
enum { DRAG_ENTER, DRAG_LEAVE, DRAG_DRAG, DRAG_DROP, DRAG_POST };

struct entry { UINT format; UINT size; char data[16]; };

static IDataObject *got_object;

static HRESULT WINAPI dt_QI(IDropTarget *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IDropTarget)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI dt_AddRef(IDropTarget *iface) { return 2; }
static ULONG WINAPI dt_Release(IDropTarget *iface) { return 1; }
static HRESULT WINAPI dt_DragEnter(IDropTarget *iface, IDataObject *obj, DWORD keys, POINTL pt, DWORD *effect)
{
    IDataObject_AddRef(obj);
    got_object = obj;
    *effect = DROPEFFECT_COPY;
    return S_OK;
}
static HRESULT WINAPI dt_DragOver(IDropTarget *iface, DWORD keys, POINTL pt, DWORD *effect) { *effect = DROPEFFECT_COPY; return S_OK; }
static HRESULT WINAPI dt_DragLeave(IDropTarget *iface) { return S_OK; }
static HRESULT WINAPI dt_Drop(IDropTarget *iface, IDataObject *obj, DWORD keys, POINTL pt, DWORD *effect) { return S_OK; }
static IDropTargetVtbl dt_vtbl = { dt_QI, dt_AddRef, dt_Release, dt_DragEnter, dt_DragOver, dt_DragLeave, dt_Drop };
static IDropTarget drop_target = { &dt_vtbl };

static STGMEDIUM hglobal_medium(SIZE_T size)
{
    STGMEDIUM m = {0};
    m.tymed = TYMED_HGLOBAL;
    m.hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
    return m;
}

int main(void)
{
    message_call_t pMessageCall = (message_call_t)GetProcAddress(GetModuleHandleA("win32u.dll"), "NtUserMessageCall");
    struct entry entries[2];
    ULONG ret_ptr = 0;
    HWND hwnd;
    HRESULT hr;
    FORMATETC fmt = {CF_TEXT, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL}, out;
    STGMEDIUM m;
    char *p;

    setvbuf(stdout, NULL, _IONBF, 0);
    if (!pMessageCall) { printf("ok   no NtUserMessageCall\nRESULT: PASS\n"); return 0; }
    OleInitialize(NULL);
    hwnd = CreateWindowA("static", "t", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, NULL, NULL, GetModuleHandleA(NULL), NULL);
    hr = RegisterDragDrop(hwnd, &drop_target);
    check(hr == S_OK, "drop target registered %#lx", hr);

    memset(entries, 0, sizeof(entries));
    entries[0].format = CF_TEXT;  entries[0].size = 6;  memcpy(entries[0].data, "hello", 6);
    entries[1].format = CF_UNICODETEXT; entries[1].size = 10; memcpy(entries[1].data, L"wide", 10);
    pMessageCall(0, DRAG_ENTER, sizeof(entries), (LPARAM)entries, &ret_ptr, NT_USER_DRAG_DROP_CALL, FALSE);
    {
        POINT pt = {50, 50};
        ClientToScreen(hwnd, &pt);
        pMessageCall(hwnd, DRAG_DRAG, MAKELONG(pt.x, pt.y), DROPEFFECT_COPY, &ret_ptr, NT_USER_DRAG_DROP_CALL, FALSE);
    }
    check(got_object != NULL, "the drop target got the data object");
    if (!got_object) goto done;

    /* GetData still works (the reference) */
    m = (STGMEDIUM){0};
    hr = IDataObject_GetData(got_object, &fmt, &m);
    check(hr == S_OK && m.hGlobal, "GetData %#lx", hr);
    if (m.hGlobal) ReleaseStgMedium(&m);

    /* GetDataHere: into a big enough HGLOBAL */
    m = hglobal_medium(64);
    memset(GlobalLock(m.hGlobal), 0xcc, 64); GlobalUnlock(m.hGlobal);
    hr = IDataObject_GetDataHere(got_object, &fmt, &m);
    p = GlobalLock(m.hGlobal);
    check(hr == S_OK && !strcmp(p, "hello") && (BYTE)p[6] == 0xcc, "GetDataHere copies the data (%#lx '%s')", hr, p);
    GlobalUnlock(m.hGlobal);
    check(m.hGlobal != NULL && m.pUnkForRelease == NULL, "the caller's medium is kept");
    GlobalFree(m.hGlobal);

    /* the wide format */
    fmt.cfFormat = CF_UNICODETEXT;
    m = hglobal_medium(10);
    hr = IDataObject_GetDataHere(got_object, &fmt, &m);
    check(hr == S_OK && !wcscmp(GlobalLock(m.hGlobal), L"wide"), "GetDataHere, CF_UNICODETEXT (%#lx)", hr);
    GlobalUnlock(m.hGlobal);
    GlobalFree(m.hGlobal);

    /* too small */
    fmt.cfFormat = CF_TEXT;
    m = hglobal_medium(3);
    hr = IDataObject_GetDataHere(got_object, &fmt, &m);
    check(hr == STG_E_MEDIUMFULL, "a medium too small: %#lx", hr);
    GlobalFree(m.hGlobal);

    /* not an HGLOBAL, no medium handle, missing format */
    m = hglobal_medium(64);
    m.tymed = TYMED_ISTREAM;
    hr = IDataObject_GetDataHere(got_object, &fmt, &m);
    check(hr == DV_E_TYMED, "not an HGLOBAL medium: %#lx", hr);
    m.tymed = TYMED_HGLOBAL;
    fmt.cfFormat = CF_BITMAP;
    hr = IDataObject_GetDataHere(got_object, &fmt, &m);
    check(hr == DV_E_FORMATETC, "a format the object does not have: %#lx", hr);
    GlobalFree(m.hGlobal);
    fmt.cfFormat = CF_TEXT;
    m = (STGMEDIUM){0};
    m.tymed = TYMED_HGLOBAL;
    hr = IDataObject_GetDataHere(got_object, &fmt, &m);
    check(hr == DV_E_TYMED, "no HGLOBAL in the medium: %#lx", hr);

    /* GetCanonicalFormatEtc */
    memset(&out, 0xcc, sizeof(out));
    fmt.ptd = (DVTARGETDEVICE *)0x1234;
    fmt.tymed = TYMED_HGLOBAL;
    hr = IDataObject_GetCanonicalFormatEtc(got_object, &fmt, &out);
    check(hr == DATA_S_SAMEFORMATETC, "GetCanonicalFormatEtc %#lx", hr);
    check(out.cfFormat == CF_TEXT && out.ptd == NULL && out.dwAspect == DVASPECT_CONTENT && out.lindex == -1, "the format without its device (%u, %p)", out.cfFormat, out.ptd);
    fmt.cfFormat = CF_BITMAP;
    hr = IDataObject_GetCanonicalFormatEtc(got_object, &fmt, &out);
    check(hr == DV_E_FORMATETC, "canonical, format missing: %#lx", hr);
    fmt.cfFormat = CF_TEXT;
    hr = IDataObject_GetCanonicalFormatEtc(got_object, &fmt, NULL);
    check(hr == E_INVALIDARG, "canonical, no output: %#lx", hr);

    IDataObject_Release(got_object);
done:
    pMessageCall(0, DRAG_LEAVE, 0, 0, &ret_ptr, NT_USER_DRAG_DROP_CALL, FALSE);
    RevokeDragDrop(hwnd);
    DestroyWindow(hwnd);
    OleUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
