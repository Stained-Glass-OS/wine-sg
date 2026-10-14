/* riched20 TOM text ranges by line, screen, window, character/paragraph format, table and object (patch 3698) */
#define COBJMACROS
#include <windows.h>
#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
#include <string.h>

static const GUID IID_ITextDocument_ = {0x8cc497c0, 0xa1df, 0x11ce, {0x80, 0x98, 0x00, 0xaa, 0x00, 0x47, 0xbe, 0x5d}};
#define IID_ITextDocument IID_ITextDocument_
static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ITextDocument *doc;
static IRichEditOle *ole;
static HWND hwnd;

static ITextRange *range(LONG a, LONG b)
{
    ITextRange *r = NULL;
    ITextDocument_Range(doc, a, b, &r);
    return r;
}

static void expect(ITextRange *r, LONG s, LONG e, const char *what)
{
    LONG a = -1, b = -1;
    ITextRange_GetStart(r, &a);
    ITextRange_GetEnd(r, &b);
    CHECK(a == s && b == e, "%s: [%ld,%ld] want [%ld,%ld]", what, a, b, s, e);
}

static void open_edit(const WCHAR *cls, int w, int h, const WCHAR *text)
{
    if (doc) { ITextDocument_Release(doc); doc = NULL; }
    if (ole) { ole->lpVtbl->Release(ole); ole = NULL; }
    if (hwnd) DestroyWindow(hwnd);
    hwnd = CreateWindowExW(0, cls, NULL, WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, w, h, NULL, NULL, NULL, NULL);
    SetWindowTextW(hwnd, text);
    SendMessageW(hwnd, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    ole->lpVtbl->QueryInterface(ole, &IID_ITextDocument, (void **)&doc);
}

/* a narrow control that wraps the text into several lines */
static void test_lines(void)
{
    LONG a, b, i, n, d;
    ITextRange *r, *r2;
    HRESULT hr;
    int lines;

    open_edit(L"RichEdit20W", 80, 400, L"alpha beta gamma delta epsilon zeta eta theta\rshort\rlast line of the text here");
    lines = SendMessageW(hwnd, EM_GETLINECOUNT, 0, 0);
    CHECK(lines >= 5, "wrapped into %d lines", lines);
    /* every line start from the control must be a line boundary of the unit */
    r = range(0, 0);
    n = 0;
    for (i = 0; i < lines; i++)
    {
        LONG ofs = SendMessageW(hwnd, EM_LINEINDEX, i, 0), len = SendMessageW(hwnd, EM_LINELENGTH, ofs, 0);

        r2 = range(ofs + (len > 2 ? 2 : 0), ofs + (len > 2 ? 2 : 0));
        d = 99;
        hr = ITextRange_Expand(r2, tomLine, &d);
        ITextRange_GetStart(r2, &a);
        ITextRange_GetEnd(r2, &b);
        CHECK(hr == S_OK && a == ofs, "line %ld: Expand hr %#lx start %ld want %ld", i, hr, a, ofs);
        if (i + 1 < lines) CHECK(b == SendMessageW(hwnd, EM_LINEINDEX, i + 1, 0), "line %ld: end %ld", i, b);
        ITextRange_Release(r2);
        n++;
    }
    /* Move by lines */
    d = 99;
    hr = ITextRange_Move(r, tomLine, 1, &d);
    CHECK(hr == S_OK && d == 1, "Move line hr %#lx d %ld", hr, d);
    expect(r, SendMessageW(hwnd, EM_LINEINDEX, 1, 0), SendMessageW(hwnd, EM_LINEINDEX, 1, 0), "Move line 1");
    hr = ITextRange_Move(r, tomLine, 2, &d);
    CHECK(d == 2, "Move line 2: %ld", d);
    expect(r, SendMessageW(hwnd, EM_LINEINDEX, 3, 0), SendMessageW(hwnd, EM_LINEINDEX, 3, 0), "Move line 3");
    hr = ITextRange_Move(r, tomLine, -3, &d);
    CHECK(d == -3, "Move line -3: %ld", d);
    expect(r, 0, 0, "Move line back");
    hr = ITextRange_Move(r, tomLine, -1, &d);
    CHECK(hr == S_FALSE && d == 0, "Move line before the start %#lx %ld", hr, d);
    ITextRange_Release(r);

    /* StartOf, EndOf, MoveEnd */
    r = range(7, 7);
    hr = ITextRange_StartOf(r, tomLine, tomExtend, &d);
    a = SendMessageW(hwnd, EM_LINEINDEX, SendMessageW(hwnd, EM_LINEFROMCHAR, 7, 0), 0);
    CHECK(hr == S_OK && d == a - 7, "StartOf line extend hr %#lx d %ld", hr, d);
    expect(r, a, 7, "StartOf line extend");
    hr = ITextRange_EndOf(r, tomLine, tomExtend, &d);
    b = SendMessageW(hwnd, EM_LINEINDEX, SendMessageW(hwnd, EM_LINEFROMCHAR, 7, 0) + 1, 0);
    expect(r, a, b, "EndOf line extend");
    ITextRange_MoveEnd(r, tomLine, 1, &d);
    CHECK(d == 1, "MoveEnd line %ld", d);
    ITextRange_Release(r);
    r = range(0, 3);
    ITextRange_MoveStart(r, tomLine, 1, &d);
    b = SendMessageW(hwnd, EM_LINEINDEX, 1, 0);
    CHECK(d == 1, "MoveStart line %ld", d);
    expect(r, b, b, "MoveStart past the end drags the end");
    ITextRange_Release(r);

    /* GetIndex of a line */
    r = range(0, 0);
    ITextRange_SetIndex(r, tomLine, 3, 0);
    a = SendMessageW(hwnd, EM_LINEINDEX, 2, 0);
    ITextRange_GetStart(r, &b);
    CHECK(b == a, "SetIndex line 3 at %ld want %ld", b, a);
    ITextRange_GetIndex(r, tomLine, &d);
    CHECK(d == 3, "GetIndex line %ld", d);
    ITextRange_Release(r);
}

static void test_formats(void)
{
    ITextRange *r, *r2;
    ITextPara *para;
    LONG a, b, d;
    HRESULT hr;

    open_edit(L"RichEdit20W", 300, 200, L"plain bold more plain\rsecond\rthird\rfourth");
    {
        CHARFORMAT2W cf = { sizeof(cf) };

        cf.dwMask = cf.dwEffects = CFM_ITALIC;
        SendMessageW(hwnd, EM_SETSEL, 6, 10);
        SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }

    r = range(7, 7);
    d = 99;
    hr = ITextRange_Expand(r, tomCharFormat, &d);
    CHECK(hr == S_OK && d == 4, "Expand char format hr %#lx d %ld", hr, d);
    expect(r, 6, 10, "Expand char format");
    hr = ITextRange_Expand(r, tomCharFormat, &d);
    CHECK(hr == S_FALSE && d == 0, "Expand char format again %#lx %ld", hr, d);
    ITextRange_Release(r);

    r = range(2, 2);
    ITextRange_EndOf(r, tomCharFormat, tomExtend, &d);
    CHECK(d == 4, "EndOf char format d %ld", d);
    expect(r, 2, 6, "EndOf char format extend");
    ITextRange_Move(r, tomCharFormat, 1, &d);
    CHECK(d == 1, "Move char format %ld", d);
    expect(r, 10, 10, "Move char format");
    ITextRange_Move(r, tomCharFormat, -2, &d);
    CHECK(d == -2, "Move char format back %ld", d);
    expect(r, 0, 0, "Move char format back");
    ITextRange_Release(r);
    r = range(7, 7);
    ITextRange_StartOf(r, tomCharFormat, tomMove, &d);
    expect(r, 6, 6, "StartOf char format");
    ITextRange_MoveEnd(r, tomCharFormat, 1, &d);
    CHECK(d == 1, "MoveEnd char format %ld", d);
    expect(r, 6, 10, "MoveEnd char format");
    ITextRange_Release(r);

    /* paragraph formats: the second and third paragraph are centred */
    r = range(22, 22);
    ITextRange_GetPara(r, &para);
    ITextPara_SetAlignment(para, tomAlignCenter);
    ITextPara_Release(para);
    r2 = range(30, 30);
    ITextRange_GetPara(r2, &para);
    ITextPara_SetAlignment(para, tomAlignCenter);
    ITextPara_Release(para);
    ITextRange_Release(r2);
    ITextRange_Expand(r, tomParaFormat, &d);
    a = b = -1;
    ITextRange_GetStart(r, &a); ITextRange_GetEnd(r, &b);
    CHECK(a == 22 && b == 35, "Expand para format [%ld,%ld]", a, b);
    ITextRange_Release(r);
    r = range(0, 0);
    ITextRange_Move(r, tomParaFormat, 1, &d);
    CHECK(d == 1, "Move para format %ld", d);
    expect(r, 22, 22, "Move para format");
    ITextRange_Release(r);
}

static void test_screen_window(void)
{
    ITextRange *r;
    LONG a, b, d, first;
    HRESULT hr;
    int i;
    WCHAR text[1200] = L"";

    for (i = 0; i < 60; i++) wcscat(text, i < 59 ? L"line of text\r" : L"end");
    open_edit(L"RichEdit20W", 200, 100, text);
    SendMessageW(hwnd, EM_LINESCROLL, 0, 0);
    r = range(0, 0);
    d = 99;
    hr = ITextRange_Expand(r, tomScreen, &d);
    ITextRange_GetStart(r, &a);
    ITextRange_GetEnd(r, &b);
    CHECK(hr == S_OK && a == 0 && b > 13 && b < 13 * 20, "Expand screen hr %#lx [%ld,%ld]", hr, a, b);
    first = b;
    ITextRange_Release(r);
    r = range(0, 0);
    hr = ITextRange_Move(r, tomScreen, 1, &d);
    CHECK(hr == S_OK && d == 1, "Move screen %#lx %ld", hr, d);
    expect(r, first, first, "Move screen 1");
    ITextRange_Move(r, tomScreen, -1, &d);
    CHECK(d == -1, "Move screen back %ld", d);
    expect(r, 0, 0, "Move screen back");
    ITextRange_Move(r, tomScreen, 1000, &d);
    CHECK(d > 1 && d < 60, "Move many screens %ld", d);
    ITextRange_Release(r);

    /* the window is what shows: scrolled down, StartOf goes to the first visible line */
    SendMessageW(hwnd, EM_LINESCROLL, 0, 20);
    r = range(0, 0);
    hr = ITextRange_StartOf(r, tomWindow, tomMove, &d);
    ITextRange_GetStart(r, &a);
    CHECK(hr == S_FALSE || a == 0, "window start before scrolled text");
    ITextRange_Release(r);
    r = range(20 * 13 + 3, 20 * 13 + 3);
    hr = ITextRange_StartOf(r, tomWindow, tomMove, &d);
    ITextRange_GetStart(r, &a);
    first = SendMessageW(hwnd, EM_LINEINDEX, SendMessageW(hwnd, EM_GETFIRSTVISIBLELINE, 0, 0), 0);
    CHECK(hr == S_OK && a == first, "StartOf window %ld want %ld (hr %#lx)", a, first, hr);
    ITextRange_EndOf(r, tomWindow, tomExtend, &d);
    ITextRange_GetEnd(r, &b);
    CHECK(b > first && b < first + 13 * 20, "EndOf window %ld", b);
    ITextRange_Release(r);
}


struct rtf_src { const char *p; };
static DWORD CALLBACK rtf_cb(DWORD_PTR cookie, LPBYTE buf, LONG cb, LONG *pcb)
{
    struct rtf_src *src = (struct rtf_src *)cookie;
    LONG n = (LONG)strlen(src->p);

    if (n > cb) n = cb;
    memcpy(buf, src->p, n);
    src->p += n;
    *pcb = n;
    return 0;
}

static void test_table(void)
{
    static const char rtf[] = "{\\rtf1 before\\par "
        "\\trowd \\cellx1000\\cellx2000 \\intbl a1\\cell b1\\cell\\row "
        "\\trowd \\cellx1000\\cellx2000 \\intbl a2\\cell b2\\cell\\row "
        "after\\par}";
    struct rtf_src src = { rtf };
    EDITSTREAM es = { (DWORD_PTR)&src, 0, rtf_cb };
    ITextRange *r;
    LONG d, len;
    HRESULT hr;

    open_edit(L"RICHEDIT50W", 400, 200, L"");
    SendMessageW(hwnd, EM_STREAMIN, SF_RTF, (LPARAM)&es);
    ITextRange_GetStoryLength((r = range(0, 0)), &len);
    CHECK(len == 28, "table story length %ld", len);
    ITextRange_Release(r);

    r = range(12, 12);
    ITextRange_Expand(r, tomRow, &d); expect(r, 7, 17, "Expand row");
    ITextRange_Release(r);
    r = range(20, 20);
    ITextRange_Expand(r, tomRow, &d); expect(r, 17, 27, "Expand second row");
    ITextRange_Release(r);
    r = range(12, 12);
    d = 99;
    hr = ITextRange_Expand(r, tomCell, &d);
    CHECK(hr == S_OK && d == 3, "Expand cell hr %#lx d %ld", hr, d);
    expect(r, 12, 15, "Expand cell");
    ITextRange_Release(r);
    r = range(20, 20);
    ITextRange_Expand(r, tomTable, &d); expect(r, 7, 27, "Expand table");
    ITextRange_Release(r);
    r = range(3, 3);
    ITextRange_Expand(r, tomTable, &d); expect(r, 0, 7, "no table before it");
    ITextRange_Release(r);
    r = range(0, 0);
    hr = ITextRange_Move(r, tomTable, 1, &d);
    CHECK(d == 1, "Move table %ld", d); expect(r, 7, 7, "Move table");
    ITextRange_Move(r, tomTable, 1, &d); expect(r, 27, 27, "Move table past the end of the table");
    ITextRange_Move(r, tomRow, -1, &d);
    CHECK(d == -1, "Move row back %ld", d); expect(r, 17, 17, "Move row back");
    ITextRange_Move(r, tomCell, 1, &d);
    CHECK(d == 1, "Move cell %ld", d); { LONG s0; ITextRange_GetStart(r, &s0); CHECK(s0 > 17 && s0 < 27, "Move cell to %ld", s0); }
    ITextRange_SetStart(r, 12); ITextRange_SetEnd(r, 12);
    ITextRange_EndOf(r, tomCell, tomExtend, &d); expect(r, 12, 15, "EndOf cell extend");
    ITextRange_StartOf(r, tomRow, tomExtend, &d); expect(r, 7, 15, "StartOf row extend");
    ITextRange_Release(r);
}

static void test_objects(void)
{
    IOleClientSite *site = NULL;
    REOBJECT reo = { sizeof(reo) };
    ITextRange *r;
    LONG d, o1, o2;
    HRESULT hr;

    open_edit(L"RichEdit20W", 300, 200, L"abc def ghi jkl");
    hr = ole->lpVtbl->GetClientSite(ole, &site);
    CHECK(hr == S_OK, "GetClientSite %#lx", hr);
    reo.cp = 3;
    reo.polesite = site;
    reo.sizel.cx = reo.sizel.cy = 10;
    reo.dvaspect = DVASPECT_CONTENT;
    hr = ole->lpVtbl->InsertObject(ole, &reo);
    CHECK(hr == S_OK, "InsertObject %#lx", hr);
    reo.cp = 9;
    hr = ole->lpVtbl->InsertObject(ole, &reo);
    CHECK(hr == S_OK, "InsertObject 2 %#lx", hr);
    IOleClientSite_Release(site);
    {
        REOBJECT got = { sizeof(got) };

        CHECK(ole->lpVtbl->GetObjectCount(ole) == 2, "object count");
        hr = ole->lpVtbl->GetObject(ole, 0, &got, REO_GETOBJ_NO_INTERFACES);
        o1 = hr == S_OK ? got.cp : -1;
        hr = ole->lpVtbl->GetObject(ole, 1, &got, REO_GETOBJ_NO_INTERFACES);
        o2 = hr == S_OK ? got.cp : -1;
    }
    CHECK(o1 == 3 && o2 == 9, "objects at %ld and %ld", o1, o2);

    r = range(o1, o1);
    d = 99;
    hr = ITextRange_Expand(r, tomObject, &d);
    CHECK(hr == S_OK && d == 1, "Expand object hr %#lx d %ld", hr, d);
    expect(r, o1, o1 + 1, "Expand object");
    hr = ITextRange_Expand(r, tomObject, &d);
    CHECK(hr == S_FALSE && d == 0, "Expand object again %#lx %ld", hr, d);
    ITextRange_Release(r);
    r = range(5, 5);
    hr = ITextRange_Expand(r, tomObject, &d);
    CHECK(hr == S_FALSE && d == 0, "Expand object between objects %#lx %ld", hr, d);
    expect(r, 5, 5, "no object at 5");
    ITextRange_Release(r);

    r = range(0, 0);
    hr = ITextRange_Move(r, tomObject, 1, &d);
    CHECK(hr == S_OK && d == 1, "Move object %#lx %ld", hr, d); expect(r, o1, o1, "Move object 1");
    ITextRange_Move(r, tomObject, 1, &d);
    CHECK(d == 1, "Move object 2 %ld", d); expect(r, o2, o2, "Move object 2");
    hr = ITextRange_Move(r, tomObject, 1, &d);
    CHECK(hr == S_FALSE && d == 0, "no object after the last %#lx %ld", hr, d); expect(r, o2, o2, "Move object stays");
    ITextRange_Move(r, tomObject, -5, &d);
    CHECK(d == -1, "Move object back past the first %ld", d); expect(r, o1, o1, "Move back to the first");
    ITextRange_Move(r, tomObject, 7, &d);
    CHECK(d == 1, "Move object forward past the last %ld", d); expect(r, o2, o2, "Move forward to the last");
    ITextRange_Move(r, tomObject, -5, &d);
    ITextRange_SetStart(r, 0); ITextRange_SetEnd(r, 0);
    ITextRange_Move(r, tomObject, 2, &d);
    CHECK(d == 2, "Move 2 objects %ld", d); expect(r, o2, o2, "Move 2 objects");
    ITextRange_Move(r, tomObject, -5, &d);
    CHECK(d == -1, "Move object back %ld", d); expect(r, o1, o1, "Move object back to the first");
    ITextRange_Release(r);

    r = range(0, 0);
    hr = ITextRange_EndOf(r, tomObject, tomExtend, &d);
    CHECK(hr == S_OK && d == o1 + 1, "EndOf object %#lx %ld", hr, d); expect(r, 0, o1 + 1, "EndOf object extend");
    ITextRange_Release(r);
    r = range(o2 + 4, o2 + 4);
    hr = ITextRange_StartOf(r, tomObject, tomMove, &d);
    CHECK(hr == S_OK && d == o2 - (o2 + 4), "StartOf object %#lx %ld", hr, d); expect(r, o2, o2, "StartOf object");
    ITextRange_Release(r);
    r = range(0, 1);
    ITextRange_MoveEnd(r, tomObject, 1, &d);
    CHECK(d == 1, "MoveEnd object %ld", d); expect(r, 0, o1, "MoveEnd object");
    ITextRange_MoveStart(r, tomObject, 1, &d);
    CHECK(d == 1, "MoveStart object %ld", d); expect(r, o1, o1, "MoveStart object drags the end");
    ITextRange_Release(r);
}

int main(void)
{
    LoadLibraryA("riched20.dll");
    LoadLibraryA("msftedit.dll");
    OleInitialize(NULL);
    test_lines();
    test_formats();
    test_screen_window();
    test_table();
    test_objects();
    if (doc) ITextDocument_Release(doc);
    if (ole) ole->lpVtbl->Release(ole);
    if (hwnd) DestroyWindow(hwnd);
    printf(failed ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failed);
    return failed != 0;
}
