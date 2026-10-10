/* riched20 batch (patches/sg/2053), run by test/richpaste-gate.sh: ITextRange
 * and ITextSelection Paste, CanPaste (from the clipboard, a data object and
 * a given format), GetPoint, and the selection's ScrollIntoView.
 *
 *   richpaste-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID SG_IID_ITextDocument = { 0x8cc497c0, 0xa1df, 0x11ce, { 0x80, 0x98, 0x00, 0xaa, 0x00, 0x47, 0xbe, 0x5d } };
static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(expr) check((expr), #expr)

static HWND edit;

/* a data object holding one text, wide or narrow */
typedef struct { IDataObject iface; const WCHAR *text; } TextData;
static HRESULT WINAPI td_qi(IDataObject *i, REFIID r, void **o)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDataObject)) { *o = i; return S_OK; }
    *o = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI td_addref(IDataObject *i) { return 2; }
static ULONG WINAPI td_release(IDataObject *i) { return 1; }
static HRESULT WINAPI td_getdata(IDataObject *i, FORMATETC *f, STGMEDIUM *m)
{
    TextData *d = (TextData *)i;
    HGLOBAL h;
    WCHAR *p;
    if (f->cfFormat != CF_UNICODETEXT || !(f->tymed & TYMED_HGLOBAL)) return DV_E_FORMATETC;
    h = GlobalAlloc(GMEM_MOVEABLE, (wcslen(d->text) + 1) * sizeof(WCHAR));
    p = GlobalLock(h);
    wcscpy(p, d->text);
    GlobalUnlock(h);
    m->tymed = TYMED_HGLOBAL; m->hGlobal = h; m->pUnkForRelease = NULL;
    return S_OK;
}
static HRESULT WINAPI td_getdatahere(IDataObject *i, FORMATETC *f, STGMEDIUM *m) { return E_NOTIMPL; }
static HRESULT WINAPI td_query(IDataObject *i, FORMATETC *f) { return f->cfFormat == CF_UNICODETEXT ? S_OK : DV_E_FORMATETC; }
static HRESULT WINAPI td_canon(IDataObject *i, FORMATETC *a, FORMATETC *b) { return E_NOTIMPL; }
static HRESULT WINAPI td_set(IDataObject *i, FORMATETC *f, STGMEDIUM *m, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI td_enum(IDataObject *i, DWORD d, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI td_advise(IDataObject *i, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI td_unadvise(IDataObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI td_enumadvise(IDataObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static IDataObjectVtbl td_vtbl = { td_qi, td_addref, td_release, td_getdata, td_getdatahere, td_query, td_canon, td_set,
                                   td_enum, td_advise, td_unadvise, td_enumadvise };

static int text_is(const WCHAR *want)
{
    WCHAR buf[300];
    GetWindowTextW(edit, buf, 300);
    if (wcscmp(buf, want)) printf("   text is [%ls]\n", buf);
    return !wcscmp(buf, want);
}

static void set_clipboard_w(const WCHAR *text)
{
    HGLOBAL h;
    WCHAR *p;
    OpenClipboard(edit);
    EmptyClipboard();
    if (text)
    {
        h = GlobalAlloc(GMEM_MOVEABLE, (wcslen(text) + 1) * sizeof(WCHAR));
        p = GlobalLock(h);
        wcscpy(p, text);
        GlobalUnlock(h);
        SetClipboardData(CF_UNICODETEXT, h);
    }
    CloseClipboard();
}

static void set_clipboard_a(const char *text)
{
    HGLOBAL h;
    char *p;
    OpenClipboard(edit);
    EmptyClipboard();
    h = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
    p = GlobalLock(h);
    strcpy(p, text);
    GlobalUnlock(h);
    SetClipboardData(CF_TEXT, h);
    CloseClipboard();
}

int main(void)
{
    IRichEditOle *ole = NULL;
    ITextDocument *doc = NULL;
    ITextSelection *sel = NULL;
    ITextRange *range;
    HMODULE lib = LoadLibraryW(L"msftedit.dll");
    VARIANT v;
    IDataObject *data;
    LONG can, x1, y1, x2, y2, cx, cy;
    POINT origin = { 0, 0 };

    OleInitialize(NULL);
    if (!lib) { printf("FAIL  msftedit not loaded\nRESULT: FAIL\n"); return 1; }
    edit = CreateWindowExW(0, L"RichEdit50W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 400, 200, NULL, NULL, NULL, NULL);
    if (!edit) { printf("FAIL  window\nRESULT: FAIL\n"); return 1; }
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    IUnknown_QueryInterface((IUnknown *)ole, &SG_IID_ITextDocument, (void **)&doc);
    ITextDocument_GetSelection(doc, &sel);
    if (!sel) { printf("FAIL  selection\nRESULT: FAIL\n"); return 1; }

    SetWindowTextW(edit, L"hello world");
    VariantInit(&v);

    /* from the clipboard */
    set_clipboard_w(L"ABC");
    ITextDocument_Range(doc, 0, 5, &range);
    can = 99;
    CHECK(ITextRange_CanPaste(range, NULL, 0, &can) == S_OK && can == tomTrue);
    can = 99;
    CHECK(ITextRange_CanPaste(range, NULL, CF_UNICODETEXT, &can) == S_OK && can == tomTrue);
    can = 99;
    CHECK(ITextRange_CanPaste(range, NULL, CF_BITMAP, &can) == S_OK && can == tomFalse);
    CHECK(ITextRange_CanPaste(range, NULL, 0, NULL) == E_INVALIDARG);
    CHECK(ITextRange_Paste(range, NULL, 0) == S_OK);
    CHECK(text_is(L"ABC world"));
    CHECK(ITextRange_Paste(range, NULL, CF_BITMAP) == DV_E_FORMATETC);
    ITextRange_Release(range);

    /* ANSI text, and nothing on the clipboard */
    set_clipboard_a("xyz");
    ITextDocument_Range(doc, 0, 3, &range);
    CHECK(ITextRange_Paste(range, NULL, 0) == S_OK);
    CHECK(text_is(L"xyz world"));
    set_clipboard_w(NULL);
    can = 99;
    CHECK(ITextRange_CanPaste(range, NULL, 0, &can) == S_OK && can == tomFalse);
    CHECK(ITextRange_Paste(range, NULL, 0) == DV_E_FORMATETC);
    CHECK(text_is(L"xyz world"));
    ITextRange_Release(range);

    /* from a data object */
    {
        static TextData td = { { &td_vtbl }, L"QRS" };
        data = &td.iface;
    }
    set_clipboard_w(L"other");
    V_VT(&v) = VT_UNKNOWN;
    V_UNKNOWN(&v) = (IUnknown *)data;
    ITextSelection_SetRange(sel, 0, 3);              /* a selection somewhere else stays out of it */
    ITextDocument_Range(doc, 4, 9, &range);
    can = 99;
    CHECK(ITextRange_CanPaste(range, &v, 0, &can) == S_OK && can == tomTrue);
    CHECK(ITextRange_Paste(range, &v, 0) == S_OK);
    CHECK(text_is(L"xyz QRS"));
    {
        LONG s0 = 0, e0 = 0;
        ITextRange_GetStart(range, &s0);
        ITextRange_GetEnd(range, &e0);
        CHECK(s0 == 4 && e0 == 7);                         /* the range is what was put in */
    }
    ITextRange_Release(range);

    /* the selection */
    set_clipboard_w(L"++");
    ITextSelection_SetRange(sel, 0, 3);
    can = 99;
    CHECK(ITextSelection_CanPaste(sel, NULL, 0, &can) == S_OK && can == tomTrue);
    CHECK(ITextSelection_Paste(sel, NULL, 0) == S_OK);
    CHECK(text_is(L"++ QRS"));
    CHECK(ITextSelection_ScrollIntoView(sel, tomStart) == S_OK);

    /* points */
    SetWindowTextW(edit, L"hello world");
    ClientToScreen(edit, &origin);
    ITextDocument_Range(doc, 0, 5, &range);
    CHECK(ITextRange_GetPoint(range, tomStart | tomClientCoord, &x1, &y1) == S_OK);
    CHECK(ITextRange_GetPoint(range, tomEnd | tomClientCoord, &x2, &y2) == S_OK);
    CHECK(x2 > x1 && y1 == y2 && y1 >= 0 && x1 >= 0);
    CHECK(ITextRange_GetPoint(range, tomStart, &cx, &cy) == S_OK && cx == x1 + origin.x && cy == y1 + origin.y);
    CHECK(ITextRange_GetPoint(range, tomStart, NULL, &cy) == E_INVALIDARG);
    ITextRange_Release(range);
    ITextSelection_SetRange(sel, 6, 11);
    CHECK(ITextSelection_GetPoint(sel, tomStart | tomClientCoord, &cx, &cy) == S_OK && cx > x2 && cy == y1);

    ITextSelection_Release(sel);
    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)ole);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
