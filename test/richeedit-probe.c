/* riched20 ITextRange batch (patches/sg/2029), run by test/richeedit-gate.sh.
 * Families: Delete (range text, counted characters, ranges kept in step),
 * CanEdit and the read-only error, SetChar, ChangeCase (five kinds, formatting
 * kept), InStory, on a range and on the selection.
 *
 *   richeedit-probe.exe */
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

static ITextRange *rng(LONG s, LONG e)
{
    ITextRange *r = NULL;
    ITextDocument_Range(doc, s, e, &r);
    return r;
}

static int at(ITextRange *r, LONG s, LONG e)
{
    LONG a = -1, b = -1;
    ITextRange_GetStart(r, &a);
    ITextRange_GetEnd(r, &b);
    return a == s && b == e;
}

static void settext(const WCHAR *t)
{
    SendMessageW(edit, EM_SETREADONLY, FALSE, 0);
    SetWindowTextW(edit, t);
}

static int text_is(const WCHAR *want)
{
    WCHAR buf[300];
    WCHAR *crlf;
    GetWindowTextW(edit, buf, 300);
    while ((crlf = wcsstr(buf, L"\r\n"))) memmove(crlf + 1, crlf + 2, (wcslen(crlf + 2) + 1) * sizeof(WCHAR));
    if (wcscmp(buf, want))
        printf("   text is [%ls]\n", buf);
    return !wcscmp(buf, want);
}

static void test_delete(void)
{
    ITextRange *r, *other, *later;
    LONG delta;
    HRESULT hr;

    settext(L"hello world. second one");
    r = rng(1, 3); other = rng(0, 5); later = rng(6, 11);
    delta = 99;
    hr = ITextRange_Delete(r, tomSentence, 0, &delta);
    check(hr == S_OK && delta == 2 && text_is(L"hlo world. second one"), "delete the text of a range");
    check(at(r, 1, 1) && at(other, 0, 3) && at(later, 4, 9), "delete: ranges follow");
    delta = 99;
    hr = ITextRange_Delete(r, tomCharacter, 0, &delta);
    check(hr == S_FALSE && delta == 0, "delete an empty range is S_FALSE");

    ITextRange_SetStart(r, 2); ITextRange_SetEnd(r, 2);
    delta = 0;
    hr = ITextRange_Delete(r, tomCharacter, 3, &delta);
    check(hr == S_OK && delta == 3 && text_is(L"hlorld. second one") && at(r, 2, 2), "delete 3 characters after a point");
    hr = ITextRange_Delete(r, tomCharacter, -2, &delta);
    check(hr == S_OK && delta == -2 && text_is(L"orld. second one") && at(r, 0, 0), "delete 2 characters before a point");
    hr = ITextRange_Delete(r, tomCharacter, -2, &delta);
    check(hr == S_FALSE && delta == 0, "nothing before the start");
    hr = ITextRange_Delete(r, tomCharacter, tomForward, &delta);
    check(hr == S_OK && delta == 16 && text_is(L""), "delete forward stops before the last paragraph mark");
    ITextRange_Release(r); ITextRange_Release(other); ITextRange_Release(later);

    settext(L"abcdef");
    r = rng(1, 3);
    hr = ITextRange_Delete(r, tomCharacter, 2, &delta);
    check(hr == E_NOTIMPL && text_is(L"abcdef"), "counted delete of a longer range is not done");
    ITextRange_SetEnd(r, 1);
    hr = ITextRange_Delete(r, tomWord, 1, &delta);
    check(hr == E_NOTIMPL && text_is(L"abcdef"), "counted delete of words is not done");
    ITextRange_Release(r);

    settext(L"abcdef");
    SendMessageW(edit, EM_SETSEL, 2, 5);
    {
        ITextSelection *sel = NULL;
        ITextDocument_GetSelection(doc, &sel);
        hr = ITextSelection_Delete(sel, tomCharacter, 0, &delta);
        check(hr == S_OK && delta == 3 && text_is(L"abf"), "selection delete");
        {
            CHARRANGE cr;
            SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&cr);
            check(cr.cpMin == 2 && cr.cpMax == 2, "selection delete leaves a point");
        }
        ITextSelection_Release(sel);
    }
}

static void test_readonly(void)
{
    ITextRange *r;
    LONG v = 99, delta;
    HRESULT hr;

    settext(L"abcdef");
    r = rng(1, 3);
    hr = ITextRange_CanEdit(r, &v);
    check(hr == S_OK && v == tomTrue, "an editable range can be edited");
    check(ITextRange_CanEdit(r, NULL) == E_INVALIDARG, "CanEdit(NULL)");
    SendMessageW(edit, EM_SETREADONLY, TRUE, 0);
    hr = ITextRange_CanEdit(r, &v);
    check(hr == S_FALSE && v == tomFalse, "a read-only range cannot");
    hr = ITextRange_Delete(r, tomCharacter, 0, &delta);
    check(hr == E_ACCESSDENIED && text_is(L"abcdef"), "delete in a read-only control");
    check(ITextRange_SetChar(r, 'x') == E_ACCESSDENIED && text_is(L"abcdef"), "SetChar in a read-only control");
    check(ITextRange_ChangeCase(r, tomUpperCase) == E_ACCESSDENIED && text_is(L"abcdef"), "ChangeCase in a read-only control");
    SendMessageW(edit, EM_SETREADONLY, FALSE, 0);
    ITextRange_Release(r);
}

static int italic_at(LONG pos)
{
    ITextRange *r = rng(pos, pos + 1);
    ITextFont *font = NULL;
    LONG v = 99;
    ITextRange_GetFont(r, &font);
    ITextFont_GetItalic(font, &v);
    ITextFont_Release(font);
    ITextRange_Release(r);
    return v == tomTrue;
}

static void test_setchar(void)
{
    ITextRange *r;
    ITextSelection *sel = NULL;
    ITextFont *font = NULL;

    settext(L"hello");
    r = rng(1, 3);
    ITextRange_GetFont(r, &font);
    ITextFont_SetItalic(font, tomTrue);
    ITextFont_Release(font);
    ITextRange_Release(r);
    check(!italic_at(0) && italic_at(1) && italic_at(2) && !italic_at(3), "the middle characters are italic");
    SendMessageW(edit, EM_SETSEL, 4, 4);

    r = rng(0, 3);
    check(ITextRange_SetChar(r, 'J') == S_OK && text_is(L"Jello"), "SetChar replaces the first character");
    check(at(r, 0, 3), "SetChar leaves the range");
    check(!italic_at(0), "SetChar of a plain character stays plain");
    ITextRange_SetStart(r, 2); ITextRange_SetEnd(r, 2);
    check(ITextRange_SetChar(r, 'L') == S_OK && text_is(L"JeLlo"), "SetChar in the middle");
    check(italic_at(1) && italic_at(2) && !italic_at(3), "SetChar keeps the formatting");
    ITextRange_Release(r);

    r = rng(5, 5);
    check(ITextRange_SetChar(r, 'x') == E_INVALIDARG && text_is(L"JeLlo"), "SetChar at the end of the story");
    ITextRange_Release(r);

    SendMessageW(edit, EM_SETSEL, 4, 4);
    ITextDocument_GetSelection(doc, &sel);
    check(ITextSelection_SetChar(sel, 'y') == S_OK && text_is(L"JeLly"), "selection SetChar");
    ITextSelection_Release(sel);
}

static void test_case(void)
{
    ITextRange *r;
    ITextSelection *sel = NULL;

    settext(L"hello World. second one\rnew para");
    r = rng(0, 5);
    check(ITextRange_ChangeCase(r, tomUpperCase) == S_OK && text_is(L"HELLO World. second one\rnew para"), "upper case");
    check(ITextRange_ChangeCase(r, tomLowerCase) == S_OK && text_is(L"hello World. second one\rnew para"), "lower case");
    ITextRange_SetEnd(r, 11);
    check(ITextRange_ChangeCase(r, tomToggleCase) == S_OK && text_is(L"HELLO wORLD. second one\rnew para"), "toggle case");
    ITextRange_Release(r);

    settext(L"hello world. second one\rnew para");
    r = rng(0, 31);
    check(ITextRange_ChangeCase(r, tomTitleCase) == S_OK && text_is(L"Hello World. Second One\rNew Para"), "title case");
    settext(L"hello world. second one\rnew para");
    check(ITextRange_ChangeCase(r, tomSentenceCase) == S_OK && text_is(L"Hello world. Second one\rNew para"), "sentence case");
    settext(L"hello world. second one");
    ITextRange_SetStart(r, 7); ITextRange_SetEnd(r, 8);
    check(ITextRange_ChangeCase(r, tomTitleCase) == S_OK && text_is(L"hello world. second one"), "title case leaves a letter inside a word");
    ITextRange_SetStart(r, 6); ITextRange_SetEnd(r, 7);
    check(ITextRange_ChangeCase(r, tomTitleCase) == S_OK && text_is(L"hello World. second one"), "title case of a word's first letter");
    check(ITextRange_ChangeCase(r, 99) == E_INVALIDARG, "an unknown case");
    ITextRange_Release(r);

    settext(L"abc def");
    SendMessageW(edit, EM_SETSEL, 0, 7);
    ITextDocument_GetSelection(doc, &sel);
    check(ITextSelection_ChangeCase(sel, tomUpperCase) == S_OK && text_is(L"ABC DEF"), "selection ChangeCase");
    ITextSelection_Release(sel);
}

static void test_instory(void)
{
    ITextRange *r = rng(0, 2), *r2 = rng(1, 4);
    ITextSelection *sel = NULL;
    HWND edit2;
    IRichEditOle *ole2 = NULL;
    ITextDocument *doc2 = NULL;
    ITextRange *foreign = NULL;
    LONG v = 99;

    check(ITextRange_InStory(r, r2, &v) == S_OK && v == tomTrue, "same story");
    v = 99;
    check(ITextRange_InStory(r, NULL, &v) == S_FALSE && v == tomFalse, "no range is not in the story");
    edit2 = CreateWindowExW(0, L"RichEdit50W", L"", WS_POPUP | ES_MULTILINE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    SendMessageW(edit2, EM_GETOLEINTERFACE, 0, (LPARAM)&ole2);
    IUnknown_QueryInterface((IUnknown *)ole2, &SG_IID_ITextDocument, (void **)&doc2);
    ITextDocument_Range(doc2, 0, 0, &foreign);
    v = 99;
    check(ITextRange_InStory(r, foreign, &v) == S_FALSE && v == tomFalse, "another control's range is not");
    ITextDocument_GetSelection(doc, &sel);
    v = 99;
    check(ITextSelection_InStory(sel, r, &v) == S_OK && v == tomTrue, "selection: same story");
    v = 99;
    check(ITextSelection_InStory(sel, foreign, &v) == S_FALSE && v == tomFalse, "selection: other story");
    ITextSelection_Release(sel);
    ITextRange_Release(foreign); ITextDocument_Release(doc2); IUnknown_Release((IUnknown *)ole2);
    DestroyWindow(edit2);
    ITextRange_Release(r); ITextRange_Release(r2);
}

int main(void)
{
    IRichEditOle *ole = NULL;
    HMODULE lib = LoadLibraryW(L"msftedit.dll");

    CoInitialize(NULL);
    if (!lib) { printf("FAIL  msftedit not loaded\nRESULT: FAIL\n"); return 1; }
    edit = CreateWindowExW(0, L"RichEdit50W", L"", WS_POPUP | ES_MULTILINE, 0, 0, 300, 200, NULL, NULL, NULL, NULL);
    check(edit != NULL, "rich edit window");
    if (!edit) { printf("RESULT: FAIL\n"); return 1; }
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    check(ole != NULL, "IRichEditOle");
    IUnknown_QueryInterface((IUnknown *)ole, &SG_IID_ITextDocument, (void **)&doc);
    check(doc != NULL, "ITextDocument");
    if (!doc) { printf("RESULT: FAIL\n"); return 1; }

    test_delete();
    test_readonly();
    test_setchar();
    test_case();
    test_instory();

    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)ole);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
