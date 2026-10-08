/* textrange2-probe.c (test/textrange2-gate.sh, wine-sg 1510): a RichEdit's
 * ITextDocument2 as Office's text boxes use it -- the main story, Range2, the
 * selection as ITextSelection2 -- reading and replacing the box's text (Word's
 * start screen: typing went after its placeholder instead of replacing it).
 *
 * mingw's tom.h predates TOM 2, so the methods are called by their vtable
 * slots (include/tom.idl): IUnknown 3 + IDispatch 4 + ITextDocument 19 = 26
 * before ITextDocument2's own; ITextRange's 51 and ITextSelection's 10 before
 * ITextRange2's own; ITextStory is an IUnknown. */
#include <windows.h>
#include <richedit.h>
#include <richole.h>

static const GUID IID_IRichEditOle_ = { 0x00020d00, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID IID_ITextDocument2_ = { 0xc241f5e0, 0x7206, 0x11d8, { 0xa2, 0xc7, 0x00, 0xa0, 0xd1, 0xd6, 0xc6, 0xb3 } };

enum { DOC2_GetSelection2 = 38, DOC2_Range2 = 53, DOC2_GetMainStory = 67,
       RANGE_GetStart = 14, RANGE_GetEnd = 16,
       R2_GetCch = 68, R2_GetDuplicate2 = 72, R2_GetChar2 = 90, R2_GetText2 = 96, R2_SetText2 = 103,
       STORY_GetType = 7, STORY_GetRange = 10, STORY_GetText = 11, STORY_SetText = 14 };

#define SLOT(obj, n) (((void ***)(obj))[0][n])
typedef HRESULT (WINAPI *fn_p)(void *, void **);
typedef HRESULT (WINAPI *fn_lp)(void *, LONG *);
typedef HRESULT (WINAPI *fn_llp)(void *, LONG, LONG, void **);
typedef HRESULT (WINAPI *fn_lbp)(void *, LONG, BSTR *);
typedef HRESULT (WINAPI *fn_lb)(void *, LONG, BSTR);
typedef HRESULT (WINAPI *fn_lpl)(void *, LONG *, LONG);
static void release(void *obj) { if (obj) ((IUnknown *)obj)->lpVtbl->Release((IUnknown *)obj); }

static HANDLE out;
static int failures;
static void say(const char *fmt, ...)
{
    char buf[512]; DWORD n; va_list ap;
    va_start(ap, fmt); n = wvsprintfA(buf, fmt, ap); va_end(ap);
    WriteFile(out, buf, n, &n, NULL);
}
static void check(BOOL ok, const char *what) { say("%s  %s\r\n", ok ? "PASS" : "FAIL", what); if (!ok) failures++; }
static BOOL bstr_is(BSTR b, const WCHAR *s) { return b && !lstrcmpW(b, s); }

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    IRichEditOle *ole = NULL;
    void *doc = NULL, *story = NULL, *range = NULL, *dup = NULL, *r3 = NULL, *sel = NULL;
    BSTR b = NULL; LONG n = -1, ch = 0, start = -1, end = -1; HRESULT hr; WCHAR text[256];
    HWND w;

    out = GetStdHandle(STD_OUTPUT_HANDLE);
    OleInitialize(NULL);
    LoadLibraryA("msftedit.dll");
    w = CreateWindowExW(0, L"RICHEDIT50W", L"Describe the document", WS_POPUP | ES_MULTILINE, 0, 0, 300, 100, 0, 0, inst, 0);
    SendMessageW(w, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    if (!ole || FAILED(ole->lpVtbl->QueryInterface(ole, &IID_ITextDocument2_, &doc)) || !doc)
    { say("FAIL  the RichEdit answers no ITextDocument2\r\nRESULT: FAIL\r\n"); return 1; }

    hr = ((fn_p)SLOT(doc, DOC2_GetMainStory))(doc, &story);
    check(SUCCEEDED(hr) && story, "GetMainStory gives the main story");
    if (story)
    {
        hr = ((fn_lp)SLOT(story, STORY_GetType))(story, &n);
        check(hr == S_OK && n == 1 /* tomMainTextStory */, "...of type tomMainTextStory");
        hr = ((fn_lbp)SLOT(story, STORY_GetText))(story, 0, &b);
        check(hr == S_OK && b && !wcsncmp(b, L"Describe the document", 21), "...its text is the box's text");
        SysFreeString(b); b = NULL;
    }

    hr = ((fn_llp)SLOT(doc, DOC2_Range2))(doc, 8, 0, &range);
    check(SUCCEEDED(hr) && range, "Range2(8, 0) gives a range");
    if (range)
    {
        hr = ((fn_lp)SLOT(range, R2_GetCch))(range, &n);
        check(hr == S_OK && n == 8, "...8 characters long (GetCch)");
        hr = ((fn_lbp)SLOT(range, R2_GetText2))(range, 0, &b);
        check(hr == S_OK && bstr_is(b, L"Describe"), "...GetText2: \"Describe\"");
        SysFreeString(b); b = NULL;
        hr = ((fn_lpl)SLOT(range, R2_GetChar2))(range, &ch, 1);
        check(hr == S_OK && ch == 'e', "...GetChar2(1): 'e'");
        hr = ((fn_p)SLOT(range, R2_GetDuplicate2))(range, &dup);
        n = -1;
        if (dup) ((fn_lp)SLOT(dup, R2_GetCch))(dup, &n);
        check(SUCCEEDED(hr) && dup && n == 8, "GetDuplicate2: an ITextRange2 over the same text");
    }

    SendMessageW(w, EM_SETSEL, 0, -1);
    hr = ((fn_p)SLOT(doc, DOC2_GetSelection2))(doc, &sel);
    check(SUCCEEDED(hr) && sel, "GetSelection2 gives the selection");
    if (sel)
    {
        ((fn_lp)SLOT(sel, RANGE_GetStart))(sel, &start);
        ((fn_lp)SLOT(sel, RANGE_GetEnd))(sel, &end);
        hr = ((fn_lp)SLOT(sel, R2_GetCch))(sel, &n);
        check(hr == S_OK && start == 0 && end >= 21 && n == end - start, "...its length (GetCch) is its end less its start, over the whole text");
        hr = ((fn_lb)SLOT(sel, R2_SetText2))(sel, 0, (BSTR)L"Hello");
        GetWindowTextW(w, text, 256);
        check(hr == S_OK && !lstrcmpW(text, L"Hello"), "...SetText2 replaces the text (the placeholder replaced, not added to)");
    }
    if (story)
    {
        hr = ((fn_lb)SLOT(story, STORY_SetText))(story, 0, (BSTR)L"New text");
        GetWindowTextW(w, text, 256);
        check(hr == S_OK && !lstrcmpW(text, L"New text"), "the story's SetText replaces all of it");
        hr = ((fn_llp)SLOT(story, STORY_GetRange))(story, 3, 0, &r3);
        if (r3) ((fn_lbp)SLOT(r3, R2_GetText2))(r3, 0, &b);
        check(SUCCEEDED(hr) && r3 && bstr_is(b, L"New"), "the story's GetRange(3, 0): \"New\"");
        SysFreeString(b);
    }
    release(r3); release(dup); release(range); release(sel); release(story); release(doc);
    ole->lpVtbl->Release(ole);
    DestroyWindow(w);
    say("RESULT: %s\r\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
