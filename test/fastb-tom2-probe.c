/* riched20: ITextDocument2Old / ITextDocument2 accessors (patch 3731) */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define HR(x) ((unsigned)(x))

/* ITextDocument2Old is not in the mingw headers: call through the vtable. Slots follow ITextDocument (26 with IUnknown/IDispatch). */
static const GUID IID_Doc2 = {0xc241f5e0, 0x7206, 0x11d8, {0xa2, 0xc7, 0x00, 0xa0, 0xd1, 0xd6, 0xc6, 0xb3}};
static const GUID IID_Doc1 = {0x8cc497c0, 0xa1df, 0x11ce, {0x80, 0x98, 0x00, 0xaa, 0x00, 0x47, 0xbe, 0x5d}};
static const GUID IID_Old = {0x01c25500, 0x4268, 0x11d1, {0x88, 0x3a, 0x3c, 0x8b, 0x00, 0xc1, 0x00, 0x00}};
#define SLOT(o, n) ((*(void ***)(o))[n])
#define D2_GETCARET 26
#define D2_SETNOTIF 37
#define D2_GETNOTIF 36
#define D2_GETVERSION 41
#define D2_GETWINDOW 42
#define D2_CHECKLIMIT 44
#define D2_CLIENTRECT 46
#define D2_GETEFFECT 47
#define D2_GETIMM 48
#define D2_GETPROP 50
#define D2_RELIMM 56
#define D2_SETEFFECT 57
#define D2_SETPROP 58
#define OLD_ATTACH 26
#define OLD_SETEFFECT 27
#define OLD_GETEFFECT 28
#define OLD_GETCARET 29
#define OLD_SETCARET 30
#define OLD_GETIMM 31
#define OLD_RELIMM 32
#define OLD_GETNOTIF 34
#define OLD_SETNOTIF 35
#define OLD_CLIENTRECT 36
#define OLD_SELEX 37
#define OLD_GETWINDOW 38
#define OLD_FEFLAGS 39
#define OLD_UPDATEWINDOW 40
#define OLD_CHECKLIMIT 41
#define OLD_IMEPROG 42
#define OLD_SYSBEEP 43
#define OLD_UPDATE 44
#define OLD_NOTIFY 45
#define OLD_TAB_GET 13
#define OLD_TAB_SET 14
#define OLD_STORYCOUNT 9
#define OLD_STORYRANGES 10
#define OLD_BEGINEDIT 20
#define OLD_ENDEDIT 21
#define OLD_RANGEFROMPOINT 25
typedef HRESULT (WINAPI *p0)(void *);
typedef HRESULT (WINAPI *p1)(void *, void *);
typedef HRESULT (WINAPI *pl)(void *, LONG);
typedef HRESULT (WINAPI *pll)(void *, LONG, LONG);
typedef HRESULT (WINAPI *plp)(void *, LONG, void *);
typedef HRESULT (WINAPI *pf)(void *, float);

#define C0(o, n) ((p0)SLOT(o, n))(o)
#define C1(o, n, a) ((p1)SLOT(o, n))(o, (void *)(a))
#define CL(o, n, a) ((pl)SLOT(o, n))(o, a)
#define CLP(o, n, a, b) ((plp)SLOT(o, n))(o, a, (void *)(b))
#define CLL(o, n, a, b) ((pll)SLOT(o, n))(o, a, b)

static HWND make_edit(HWND parent)
{
    HWND e = CreateWindowExW(0, L"RichEdit20W", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOHSCROLL,
            10, 20, 300, 150, parent, NULL, GetModuleHandleW(NULL), NULL);
    return e;
}

int main(void)
{
    HWND top, edit;
    IUnknown *ole = NULL;
    void *old = NULL;
    void *doc2 = NULL;
    ITextDocument *doc = NULL;
    HRESULT hr;
    LONG l, t, r, b, v;
    float fl;
    COLORREF cr;
    RECT rc;
    POINT pt;
    INT64 w64;
    int xa, xb;
    ITextSelection *sel1 = NULL, *sel2 = NULL;
    ITextRange *range = NULL;
    LRESULT lr;
    int i;

    LoadLibraryW(L"riched20.dll");
    top = CreateWindowExW(0, L"static", L"top", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 50, 60, 400, 300, NULL, NULL, GetModuleHandleW(NULL), NULL);
    edit = make_edit(top);
    CHECK(edit != NULL, "no richedit window");
    if (!edit) { printf("RESULT: FAIL\n"); return 1; }
    SendMessageW(edit, EM_SETEVENTMASK, 0, 0);
    SetWindowTextW(edit, L"a\tb Hello world");
    lr = SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    CHECK(lr && ole, "EM_GETOLEINTERFACE");
    hr = IUnknown_QueryInterface(ole, &IID_Old, &old);
    CHECK(hr == S_OK && old, "ITextDocument2Old %#x", HR(hr));
    hr = IUnknown_QueryInterface(ole, &IID_Doc2, &doc2);
    CHECK(hr == S_OK && doc2, "ITextDocument2 %#x", HR(hr));
    hr = IUnknown_QueryInterface(ole, &IID_Doc1, (void **)&doc);
    if (!old || !doc2 || !doc) { printf("RESULT: FAIL\n"); return 1; }

    /* ---- default tab stop, also seen in the layout */
    fl = 0; hr = (CLP(old, OLD_TAB_GET, 0, 0), 0);
    hr = ((p1)SLOT(old, OLD_TAB_GET))(old, &fl);
    CHECK(hr == S_OK && fl == 36.0f, "default tab %f hr %#x", fl, HR(hr));
    CHECK(((p1)SLOT(old, OLD_TAB_GET))(old, NULL) == E_INVALIDARG, "tab NULL");
    {
        POINTL p;
        SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)&p, 2);
        xb = p.x;
        SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)&p, 0);
        xa = p.x;
        CHECK(xb > xa, "tab did not move text: %d %d", xa, xb);
        hr = ((pf)SLOT(old, OLD_TAB_SET))(old, 100.0f);
        CHECK(hr == S_OK, "set tab %#x", HR(hr));
        fl = 0; ((p1)SLOT(old, OLD_TAB_GET))(old, &fl);
        CHECK(fl == 100.0f, "tab get %f", fl);
        SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)&p, 2);
        CHECK(p.x - xa > xb - xa + 30, "tab layout unchanged: %d vs %d", p.x - xa, xb - xa);
        CHECK(((pf)SLOT(old, OLD_TAB_SET))(old, 0.0f) == E_INVALIDARG, "tab 0");
        CHECK(((pf)SLOT(old, OLD_TAB_SET))(old, -3.0f) == E_INVALIDARG, "tab negative");
        ((pf)SLOT(old, OLD_TAB_SET))(old, 36.0f);
        SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)&p, 2);
        CHECK(p.x == xb, "tab restore: %d vs %d", (int)p.x, xb);
        hr = ((p1)SLOT(doc2, OLD_TAB_GET))(doc2, &fl);
        CHECK(hr == S_OK && fl == 36.0f, "doc2 tab");
    }

    /* ---- effect colours */
    hr = ((plp)SLOT(old, OLD_GETEFFECT))(old, 2, &cr);
    CHECK(hr == S_OK && cr == RGB(0, 0, 255), "effect 2 default %#lx", (unsigned long)cr);
    hr = ((plp)SLOT(old, OLD_GETEFFECT))(old, 16, &cr);
    CHECK(hr == S_OK && cr == RGB(192, 192, 192), "effect 16 default %#lx", (unsigned long)cr);
    hr = ((plp)SLOT(old, OLD_GETEFFECT))(old, 0, &cr);
    CHECK(hr == S_OK && cr == GetSysColor(COLOR_WINDOWTEXT), "effect 0 default %#lx", (unsigned long)cr);
    hr = ((pll)SLOT(old, OLD_SETEFFECT))(old, 3, RGB(1, 2, 3));
    CHECK(hr == S_OK, "set effect");
    ((plp)SLOT(old, OLD_GETEFFECT))(old, 3, &cr);
    CHECK(cr == RGB(1, 2, 3), "effect 3 %#lx", (unsigned long)cr);
    hr = ((plp)SLOT(doc2, D2_GETEFFECT))(doc2, 3, &cr);
    CHECK(hr == S_OK && cr == RGB(1, 2, 3), "doc2 effect 3");
    hr = ((pll)SLOT(doc2, D2_SETEFFECT))(doc2, 4, RGB(9, 8, 7));
    ((plp)SLOT(old, OLD_GETEFFECT))(old, 4, &cr);
    CHECK(hr == S_OK && cr == RGB(9, 8, 7), "set via doc2");
    ((plp)SLOT(old, OLD_GETEFFECT))(old, 5, &cr);
    CHECK(cr == RGB(255, 0, 255), "effect 5 not default after others set");
    CHECK(((plp)SLOT(old, OLD_GETEFFECT))(old, 17, &cr) == E_INVALIDARG, "effect 17");
    CHECK(((plp)SLOT(old, OLD_GETEFFECT))(old, -1, &cr) == E_INVALIDARG, "effect -1");
    CHECK(((plp)SLOT(old, OLD_GETEFFECT))(old, 1, NULL) == E_INVALIDARG, "effect NULL");
    CHECK(((pll)SLOT(old, OLD_SETEFFECT))(old, 17, 0) == E_INVALIDARG, "set effect 17");

    /* ---- caret type / notification mode are shared by both interfaces */
    CHECK(((pl)SLOT(old, OLD_SETCARET))(old, tomKoreanBlockCaret) == S_OK, "set caret");
    hr = ((p1)SLOT(doc2, D2_GETCARET))(doc2, &v);
    CHECK(hr == S_OK && v == tomKoreanBlockCaret, "caret via doc2 %ld", v);
    v = -5; hr = ((p1)SLOT(old, OLD_GETCARET))(old, &v);
    CHECK(hr == S_OK && v == tomKoreanBlockCaret, "caret via old %ld", v);
    CHECK(((p1)SLOT(old, OLD_GETCARET))(old, NULL) == E_INVALIDARG, "caret NULL");
    ((pl)SLOT(doc2, D2_SETNOTIF))(doc2, 0x15);
    v = 0; hr = ((p1)SLOT(old, OLD_GETNOTIF))(old, &v);
    CHECK(hr == S_OK && v == 0x15, "notification via old %#lx", v);
    CHECK(((pl)SLOT(old, OLD_SETNOTIF))(old, 7) == S_OK, "set notif");
    ((p1)SLOT(doc2, D2_GETNOTIF))(doc2, &v);
    CHECK(v == 7, "notification via doc2 %ld", v);

    /* ---- client rectangle and window */
    GetClientRect(edit, &rc);
    {
        LONG a[4];
        typedef HRESULT (WINAPI *pcr)(void *, LONG, LONG *, LONG *, LONG *, LONG *);
        hr = ((pcr)SLOT(old, OLD_CLIENTRECT))(old, tomClientCoord, &a[0], &a[1], &a[2], &a[3]);
        CHECK(hr == S_OK && a[0] - rc.left >= 0 && a[0] - rc.left <= 2 && a[1] == rc.top && rc.right - a[2] >= 0 && rc.right - a[2] <= 2 && a[3] == rc.bottom,
                "client rect %ld %ld %ld %ld vs %ld %ld %ld %ld", a[0], a[1], a[2], a[3], rc.left, rc.top, rc.right, rc.bottom);
        pt.x = a[0]; pt.y = a[1]; ClientToScreen(edit, &pt);
        hr = ((pcr)SLOT(old, OLD_CLIENTRECT))(old, 0, &l, &t, &r, &b);
        CHECK(hr == S_OK && l == pt.x && t == pt.y, "screen rect %ld %ld vs %ld %ld", l, t, (long)pt.x, (long)pt.y);
        CHECK(r - l == a[2] - a[0] && b - t == a[3] - a[1], "screen rect size");
        hr = ((HRESULT (WINAPI *)(void *, LONG, LONG *, LONG *, LONG *, LONG *))SLOT(doc2, D2_CLIENTRECT))(doc2, tomClientCoord, &l, &t, &r, &b);
        CHECK(hr == S_OK && l == a[0] && r == a[2] && b == a[3], "doc2 client rect");
        CHECK(((pcr)SLOT(old, OLD_CLIENTRECT))(old, 0, NULL, &a[1], &a[2], &a[3]) == E_INVALIDARG, "client rect NULL");
    }
    w64 = 0; hr = ((p1)SLOT(doc2, D2_GETWINDOW))(doc2, &w64);
    CHECK(hr == S_OK && (HWND)(ULONG_PTR)w64 == edit, "doc2 window %p vs %p", (void *)(ULONG_PTR)w64, edit);
    v = 0; hr = ((p1)SLOT(old, OLD_GETWINDOW))(old, &v);
    CHECK(hr == S_OK && (HWND)(LONG_PTR)v == edit, "old window");
    CHECK(((p1)SLOT(old, OLD_GETWINDOW))(old, NULL) == E_INVALIDARG, "window NULL");
    v = 5; hr = ((p1)SLOT(old, OLD_FEFLAGS))(old, &v);
    CHECK(hr == S_OK && v == 0, "FE flags");

    /* ---- text limit */
    SendMessageW(edit, EM_EXLIMITTEXT, 0, 30);
    SetWindowTextW(edit, L"0123456789");
    v = -1; hr = ((plp)SLOT(old, OLD_CHECKLIMIT))(old, 5, &v);
    CHECK(hr == S_OK && v == 0, "limit 5: %ld", v);
    v = -1; hr = ((plp)SLOT(old, OLD_CHECKLIMIT))(old, 25, &v);
    CHECK(hr == S_OK && v == 5, "limit 25: %ld", v);
    hr = ((plp)SLOT(doc2, D2_CHECKLIMIT))(doc2, 25, &v);
    CHECK(hr == S_OK && v == 5, "doc2 limit");
    CHECK(((plp)SLOT(old, OLD_CHECKLIMIT))(old, 1, NULL) == E_INVALIDARG, "limit NULL");

    /* ---- small calls */
    CHECK(((pl)SLOT(old, OLD_IMEPROG))(old, tomTrue) == S_OK, "IMEInProgress");
    CHECK(C0(old, OLD_SYSBEEP) == S_OK, "SysBeep");
    CHECK(CL(old, OLD_UPDATE, tomTrue) == S_OK, "Update");
    CHECK(C0(old, OLD_UPDATEWINDOW) == S_OK, "UpdateWindow");
    CHECK(CL(old, OLD_NOTIFY, 1) == S_OK, "Notify");
    v = -1; hr = ((p1)SLOT(old, OLD_GETIMM))(old, &v);
    CHECK(hr == S_OK, "GetImmContext %#x", HR(hr));
    if (hr == S_OK) CHECK(CL(old, OLD_RELIMM, v) == S_OK, "ReleaseImmContext");
    {
        INT64 ctx = -1;
        hr = ((p1)SLOT(doc2, D2_GETIMM))(doc2, &ctx);
        CHECK(hr == S_OK, "doc2 GetImmContext %#x", HR(hr));
        if (hr == S_OK) ((HRESULT (WINAPI *)(void *, INT64))SLOT(doc2, D2_RELIMM))(doc2, ctx);
    }

    /* ---- stories */
    v = 0; hr = ITextDocument_GetStoryCount(doc, &v);
    CHECK(hr == S_OK && v == 1, "story count %ld", v);
    CHECK(ITextDocument_GetStoryCount(doc, NULL) == E_INVALIDARG, "story count NULL");
    {
        ITextStoryRanges *sr = (ITextStoryRanges *)1;
        hr = ITextDocument_GetStoryRanges(doc, &sr);
        CHECK(hr == E_NOTIMPL, "story ranges %#x", HR(hr));
    }
    hr = ITextDocument_GetSelection(doc, &sel1);
    hr = ((p1)SLOT(old, OLD_SELEX))(old, &sel2);
    CHECK(hr == S_OK && sel1 && sel1 == sel2, "GetSelectionEx %p %p", sel1, sel2);
    if (sel1) ITextSelection_Release(sel1);
    if (sel2) ITextSelection_Release(sel2);

    /* ---- range from a screen point */
    SetWindowTextW(edit, L"0123456789");
    {
        POINTL p;
        SendMessageW(edit, EM_POSFROMCHAR, (WPARAM)&p, 6);
        pt.x = p.x + 1; pt.y = p.y + 3;
        ClientToScreen(edit, &pt);
        hr = ITextDocument_RangeFromPoint(doc, pt.x, pt.y, &range);
        CHECK(hr == S_OK && range, "RangeFromPoint %#x", HR(hr));
        if (range)
        {
            ITextRange_GetStart(range, &l); ITextRange_GetEnd(range, &r);
            CHECK(l == 6 && r == 6, "range from point %ld %ld", l, r);
            ITextRange_Release(range);
        }
        CHECK(ITextDocument_RangeFromPoint(doc, 1, 1, NULL) == E_INVALIDARG, "RangeFromPoint NULL");
    }

    /* ---- edit collection: undo grouping */
    SetWindowTextW(edit, L"");
    SendMessageW(edit, EM_EMPTYUNDOBUFFER, 0, 0);
    SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"A");
    SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"B");
    SendMessageW(edit, EM_UNDO, 0, 0);
    CHECK(GetWindowTextLengthW(edit) == 1, "without a collection one undo removes one edit: len %d", GetWindowTextLengthW(edit));
    SendMessageW(edit, EM_EMPTYUNDOBUFFER, 0, 0);
    SetWindowTextW(edit, L"");
    SendMessageW(edit, EM_EMPTYUNDOBUFFER, 0, 0);
    hr = C0(old, OLD_BEGINEDIT);
    CHECK(hr == S_OK, "BeginEditCollection %#x", HR(hr));
    hr = C0(old, OLD_BEGINEDIT);
    CHECK(hr == S_OK, "nested BeginEditCollection");
    SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"A");
    SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"B");
    SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"C");
    CHECK(C0(old, OLD_ENDEDIT) == S_OK, "inner EndEditCollection");
    SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"D");
    CHECK(C0(old, OLD_ENDEDIT) == S_OK, "outer EndEditCollection");
    CHECK(C0(old, OLD_ENDEDIT) == S_FALSE, "unmatched EndEditCollection");
    SendMessageW(edit, EM_UNDO, 0, 0);
    CHECK(GetWindowTextLengthW(edit) == 0, "one undo removes the whole collection: len %d", GetWindowTextLengthW(edit));

    /* ---- properties */
    SetWindowTextW(edit, L"hello");
    SendMessageW(edit, EM_EMPTYUNDOBUFFER, 0, 0);
    SendMessageW(edit, EM_SETSEL, 0, 0);
    hr = ((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x89, &v);
    CHECK(hr == S_OK && v == tomFalse, "CanCopy without selection %ld", v);
    SendMessageW(edit, EM_SETSEL, 0, 3);
    hr = ((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x89, &v);
    CHECK(hr == S_OK && v == tomTrue, "CanCopy with selection %ld", v);
    hr = ((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x8b, &v);
    CHECK(hr == S_OK && v == tomFalse, "CanUndo empty %ld", v);
    SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"X");
    hr = ((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x8b, &v);
    CHECK(hr == S_OK && v == tomTrue, "CanUndo after edit %ld", v);
    hr = ((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x8a, &v);
    CHECK(hr == S_OK && v == tomFalse, "CanRedo %ld", v);
    SendMessageW(edit, EM_UNDO, 0, 0);
    hr = ((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x8a, &v);
    CHECK(hr == S_OK && v == tomTrue, "CanRedo after undo %ld", v);
    hr = ((pll)SLOT(doc2, D2_SETPROP))(doc2, 0x8c, 17);
    CHECK(hr == S_OK, "set undo limit %#x", HR(hr));
    hr = ((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x8c, &v);
    CHECK(hr == S_OK && v == 17, "undo limit %ld", v);
    CHECK(((pll)SLOT(doc2, D2_SETPROP))(doc2, 0x89, 1) == E_INVALIDARG, "set CanCopy");
    CHECK(((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x89, NULL) == E_INVALIDARG, "property NULL");
    CHECK(FAILED(((plp)SLOT(doc2, D2_GETPROP))(doc2, 0x7777, &v)), "unknown property");

    /* ---- version */
    hr = ((p1)SLOT(doc2, D2_GETVERSION))(doc2, &v);
    CHECK(hr == S_OK && ((v >> 24) & 0xff) == 4, "version %#lx", (unsigned long)v);
    CHECK(((p1)SLOT(doc2, D2_GETVERSION))(doc2, NULL) == E_INVALIDARG, "version NULL");

    ITextDocument_Release(doc);
    ((ULONG (WINAPI *)(void *))SLOT(doc2, 2))(doc2);
    ((ULONG (WINAPI *)(void *))SLOT(old, 2))(old);
    IUnknown_Release(ole);
    DestroyWindow(top);
    (void)i; (void)l; (void)t;
    printf("RESULT: %s\n", f ? "FAIL" : "PASS");
    return f ? 1 : 0;
}
