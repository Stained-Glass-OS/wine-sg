/* riched20 batch (patches/sg/2041), run by test/richesel-gate.sh. Families:
 * the selection (SetRange with the anchor and the active end, GetType, the
 * movement methods that act like the arrow, Home and End keys, TypeText) and
 * ITextFont (SetDuplicate, IsEqual, CanChange).
 *
 *   richesel-probe.exe */
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
static void checkhr(HRESULT hr, HRESULT want, const char *what)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s (hr %08lx, want %08lx)", what, (unsigned long)hr, (unsigned long)want);
    check(hr == want, buf);
}

static HWND edit;
static ITextDocument *doc;
static ITextSelection *sel;

static int sel_is(LONG a, LONG b)
{
    CHARRANGE cr;
    SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&cr);
    if (cr.cpMin != a || cr.cpMax != b) printf("   selection is %ld-%ld\n", cr.cpMin, cr.cpMax);
    return cr.cpMin == a && cr.cpMax == b;
}

static void reset(const WCHAR *text)
{
    SendMessageW(edit, EM_SETREADONLY, FALSE, 0);
    SetWindowTextW(edit, text);
    SendMessageW(edit, EM_SETSEL, 0, 0);
}

static int text_is(const WCHAR *want)
{
    WCHAR buf[300], *crlf;
    GetWindowTextW(edit, buf, 300);
    while ((crlf = wcsstr(buf, L"\r\n"))) memmove(crlf + 1, crlf + 2, (wcslen(crlf + 2) + 1) * sizeof(WCHAR));
    if (wcscmp(buf, want)) printf("   text is [%ls]\n", buf);
    return !wcscmp(buf, want);
}

static void test_range_and_type(void)
{
    LONG t = 99, s, e;
    HRESULT hr;

    reset(L"hello world foo\rsecond line");
    hr = ITextSelection_SetRange(sel, 2, 5);
    check(hr == S_OK && sel_is(2, 5), "SetRange(2, 5)");
    check(ITextSelection_GetType(sel, &t) == S_OK && t == tomSelectionNormal, "a range is a normal selection");
    hr = ITextSelection_SetRange(sel, 3, 3);
    check(hr == S_OK && sel_is(3, 3), "SetRange(3, 3)");
    check(ITextSelection_GetType(sel, &t) == S_OK && t == tomSelectionIP, "an empty range is an insertion point");
    check(ITextSelection_GetType(sel, NULL) == E_INVALIDARG, "GetType(NULL)");

    /* anchor after active: the selection is the same, the active end is at the start */
    hr = ITextSelection_SetRange(sel, 8, 3);
    check(hr == S_OK && sel_is(3, 8), "SetRange(8, 3) selects 3-8");
    ITextSelection_GetStart(sel, &s);
    ITextSelection_GetEnd(sel, &e);
    check(s == 3 && e == 8, "start and end follow");
    /* the active end is the one that extends: shift-right moves the start end */
    ITextSelection_MoveLeft(sel, tomCharacter, 1, tomExtend, NULL);
    check(sel_is(2, 8), "extending to the left moves the active end at the start");

    hr = ITextSelection_SetRange(sel, 0, 1000);
    ITextSelection_GetStart(sel, &s);
    check(hr == S_OK && s == 0, "SetRange to beyond the end is clamped");
    hr = ITextSelection_SetRange(sel, -5, 4);
    check(hr == S_OK && sel_is(0, 4), "a negative end is the start");

    /* TypeText */
    reset(L"hello world foo\rsecond line");
    ITextSelection_SetRange(sel, 2, 4);
    hr = ITextSelection_TypeText(sel, SysAllocString(L"XYZ"));
    check(hr == S_OK && text_is(L"heXYZo world foo\rsecond line") && sel_is(5, 5), "TypeText replaces the selection, the point follows");
    hr = ITextSelection_TypeText(sel, SysAllocString(L"!"));
    check(hr == S_OK && text_is(L"heXYZ!o world foo\rsecond line") && sel_is(6, 6), "TypeText at a point inserts");
    SendMessageW(edit, EM_SETREADONLY, TRUE, 0);
    hr = ITextSelection_TypeText(sel, SysAllocString(L"?"));
    check(hr == E_ACCESSDENIED && text_is(L"heXYZ!o world foo\rsecond line"), "TypeText in a read-only control");
    SendMessageW(edit, EM_SETREADONLY, FALSE, 0);
}

static void test_moves(void)
{
    LONG d = 99;
    HRESULT hr;

    reset(L"hello world foo\rsecond line");
    hr = ITextSelection_MoveRight(sel, tomCharacter, 3, tomMove, &d);
    check(hr == S_OK && d == 3 && sel_is(3, 3), "MoveRight 3 characters");
    hr = ITextSelection_MoveLeft(sel, tomCharacter, 2, tomMove, &d);
    check(hr == S_OK && d == 2 && sel_is(1, 1), "MoveLeft 2 characters");
    hr = ITextSelection_MoveLeft(sel, tomCharacter, 5, tomMove, &d);
    check(hr == S_FALSE && d == 1 && sel_is(0, 0), "MoveLeft stops at the start");
    hr = ITextSelection_MoveRight(sel, tomCharacter, 0, tomMove, &d);
    check(hr == S_FALSE && d == 0, "a count of 0 moves nothing");
    hr = ITextSelection_MoveLeft(sel, tomCharacter, -2, tomMove, &d);
    check(hr == S_OK && d == -2 && sel_is(2, 2), "a negative count goes the other way");

    /* extending, and collapsing */
    ITextSelection_SetRange(sel, 2, 2);
    hr = ITextSelection_MoveRight(sel, tomCharacter, 3, tomExtend, &d);
    check(hr == S_OK && d == 3 && sel_is(2, 5), "MoveRight extends");
    hr = ITextSelection_MoveRight(sel, tomCharacter, 1, tomMove, &d);
    check(hr == S_OK && d == 1 && sel_is(5, 5), "MoveRight collapses a selection to its end");
    ITextSelection_SetRange(sel, 2, 5);
    hr = ITextSelection_MoveLeft(sel, tomCharacter, 1, tomMove, &d);
    check(hr == S_OK && d == 1 && sel_is(2, 2), "MoveLeft collapses a selection to its start");

    /* words and lines */
    ITextSelection_SetRange(sel, 0, 0);
    hr = ITextSelection_MoveRight(sel, tomWord, 1, tomMove, &d);
    check(hr == S_OK && d == 1 && sel_is(6, 6), "MoveRight one word");
    ITextSelection_SetRange(sel, 2, 2);
    hr = ITextSelection_MoveDown(sel, tomLine, 1, tomMove, &d);
    check(hr == S_OK && d == 1, "MoveDown one line");
    {
        LONG s;
        ITextSelection_GetStart(sel, &s);
        check(s > 15, "the point is on the second line");
    }
    hr = ITextSelection_MoveUp(sel, tomLine, 1, tomMove, &d);
    {
        LONG s;
        ITextSelection_GetStart(sel, &s);
        check(hr == S_OK && d == 1 && s < 16, "MoveUp one line goes back");
    }
    hr = ITextSelection_MoveRight(sel, tomSentence, 1, tomMove, &d);
    check(hr == E_NOTIMPL && d == 0, "sentences are not a unit it moves by");

    /* Home and End */
    ITextSelection_SetRange(sel, 5, 5);
    hr = ITextSelection_HomeKey(sel, tomLine, tomMove, &d);
    check(hr == S_OK && d == 5 && sel_is(0, 0), "Home goes to the start of the line");
    hr = ITextSelection_EndKey(sel, tomLine, tomMove, &d);
    check(hr == S_OK && d == 15 && sel_is(15, 15), "End goes to the end of the line");
    ITextSelection_SetRange(sel, 20, 20);
    hr = ITextSelection_HomeKey(sel, tomStory, tomMove, &d);
    check(hr == S_OK && d == 20 && sel_is(0, 0), "Home of the story");
    hr = ITextSelection_EndKey(sel, tomStory, tomMove, &d);
    check(hr == S_OK && sel_is(27, 27), "End of the story");
    ITextSelection_SetRange(sel, 5, 5);
    hr = ITextSelection_HomeKey(sel, tomLine, tomExtend, &d);
    check(hr == S_OK && sel_is(0, 5), "Home extending selects back to the line start");
    hr = ITextSelection_HomeKey(sel, tomParagraph, tomMove, &d);
    check(hr == E_INVALIDARG && d == 0, "a unit Home does not know");
}

static void test_font(void)
{
    ITextRange *r = NULL, *r2 = NULL;
    ITextFont *attached = NULL, *dup = NULL, *dup2 = NULL, *first = NULL;
    LONG v = 99;
    HRESULT hr;

    reset(L"hello world");
    ITextDocument_Range(doc, 0, 5, &r);
    ITextDocument_Range(doc, 6, 11, &r2);
    ITextRange_GetFont(r, &attached);
    ITextFont_GetDuplicate(attached, &dup);
    ITextFont_GetDuplicate(attached, &dup2);

    check(ITextFont_IsEqual(dup, dup2, &v) == S_OK && v == tomTrue, "two copies of one font are equal");
    ITextFont_SetItalic(dup, tomTrue);
    v = 99;
    check(ITextFont_IsEqual(dup, dup2, &v) == S_FALSE && v == tomFalse, "an italic one is not");
    check(ITextFont_IsEqual(dup, NULL, &v) == S_FALSE && v == tomFalse, "equal to nothing is false");
    check(ITextFont_SetDuplicate(dup2, dup) == S_OK, "SetDuplicate");
    v = 99;
    check(ITextFont_IsEqual(dup, dup2, &v) == S_OK && v == tomTrue, "equal after SetDuplicate");
    v = 99;
    ITextFont_GetItalic(dup2, &v);
    check(v == tomTrue, "and italic");
    check(ITextFont_SetDuplicate(dup2, NULL) == E_INVALIDARG, "SetDuplicate(NULL)");

    /* an attached font applies the font to its range */
    hr = ITextFont_SetDuplicate(attached, dup);
    check(hr == S_OK, "SetDuplicate on a font of a range");
    ITextRange_GetFont(r, &first);
    v = 99;
    ITextFont_GetItalic(first, &v);
    check(v == tomTrue, "the range text is italic");
    ITextFont_Release(first);
    {
        ITextRange *other = NULL;
        ITextFont *f2 = NULL;
        ITextDocument_Range(doc, 6, 11, &other);
        ITextRange_GetFont(other, &f2);
        v = 99;
        ITextFont_GetItalic(f2, &v);
        check(v == tomFalse, "the rest is not");
        ITextFont_Release(f2);
        ITextRange_Release(other);
    }

    v = 99;
    check(ITextFont_CanChange(attached, &v) == S_OK && v == tomTrue, "an editable font can change");
    check(ITextFont_CanChange(attached, NULL) == E_INVALIDARG, "CanChange(NULL)");
    SendMessageW(edit, EM_SETREADONLY, TRUE, 0);
    v = 99;
    check(ITextFont_CanChange(attached, &v) == S_FALSE && v == tomFalse, "a read-only one cannot");
    v = 99;
    check(ITextFont_CanChange(dup, &v) == S_OK && v == tomTrue, "a copy can");
    SendMessageW(edit, EM_SETREADONLY, FALSE, 0);

    ITextFont_Release(attached); ITextFont_Release(dup); ITextFont_Release(dup2);
    ITextRange_Release(r); ITextRange_Release(r2);
}

int main(void)
{
    IRichEditOle *ole = NULL;
    HMODULE lib = LoadLibraryW(L"msftedit.dll");

    CoInitialize(NULL);
    if (!lib) { printf("FAIL  msftedit not loaded\nRESULT: FAIL\n"); return 1; }
    edit = CreateWindowExW(0, L"RichEdit50W", L"", WS_POPUP | WS_VISIBLE | ES_MULTILINE, 0, 0, 400, 200, NULL, NULL, NULL, NULL);
    check(edit != NULL, "rich edit window");
    if (!edit) { printf("RESULT: FAIL\n"); return 1; }
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    IUnknown_QueryInterface((IUnknown *)ole, &SG_IID_ITextDocument, (void **)&doc);
    check(doc != NULL, "ITextDocument");
    if (!doc) { printf("RESULT: FAIL\n"); return 1; }
    ITextDocument_GetSelection(doc, &sel);
    check(sel != NULL, "the selection");
    if (!sel) { printf("RESULT: FAIL\n"); return 1; }

    test_range_and_type();
    test_moves();
    test_font();

    ITextSelection_Release(sel);
    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)ole);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
