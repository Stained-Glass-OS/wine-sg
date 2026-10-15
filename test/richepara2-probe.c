/* riched20 ITextPara duplicates, comparison, CanChange and Reset (patch 3606), run by test/richepara2-gate.sh.
 *
 *   richepara2-probe.exe */
#define COBJMACROS
#include <windows.h>

#include <richedit.h>
#include <richole.h>
#include <tom.h>
#include <stdio.h>
#include <string.h>

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

static ITextPara *para_of(LONG start, LONG end)
{
    ITextRange *range = NULL;
    ITextPara *para = NULL;
    ITextDocument_Range(doc, start, end, &range);
    if (range) { ITextRange_GetPara(range, &para); ITextRange_Release(range); }
    return para;
}

static PARAFORMAT2 fmt_at(LONG pos)
{
    PARAFORMAT2 f;
    CHARRANGE old;
    SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&old);
    SendMessageW(edit, EM_SETSEL, pos, pos);
    memset(&f, 0, sizeof(f));
    f.cbSize = sizeof(f);
    SendMessageW(edit, EM_GETPARAFORMAT, 0, (LPARAM)&f);
    SendMessageW(edit, EM_EXSETSEL, 0, (LPARAM)&old);
    return f;
}

static int flt(FLOAT a, FLOAT b) { return a > b - 0.01f && a < b + 0.01f; }

static void test_duplicates(void)
{
    ITextPara *p1 = para_of(0, 3), *p2 = para_of(4, 7), *dup = NULL, *dup2 = NULL;
    LONG v = 99;
    FLOAT f = 0;
    PARAFORMAT2 pf;

    check(p1 && p2, "paragraph objects");
    checkhr(ITextPara_SetAlignment(p1, tomAlignCenter), S_OK, "p1 centred");
    checkhr(ITextPara_SetIndents(p1, 0.0f, 20.0f, 0.0f), S_OK, "p1 left indent 20");
    checkhr(ITextPara_SetSpaceAfter(p1, 6.0f), S_OK, "p1 space after 6");

    checkhr(ITextPara_GetDuplicate(p1, NULL), E_INVALIDARG, "GetDuplicate(NULL)");
    checkhr(ITextPara_GetDuplicate(p1, &dup), S_OK, "GetDuplicate");
    check(dup != NULL && dup != p1, "the duplicate is another object");
    check(ITextPara_GetAlignment(dup, &v) == S_OK && v == tomAlignCenter, "duplicate keeps the alignment");
    check(ITextPara_GetLeftIndent(dup, &f) == S_OK && flt(f, 20.0f), "duplicate keeps the left indent");
    check(ITextPara_GetSpaceAfter(dup, &f) == S_OK && flt(f, 6.0f), "duplicate keeps the space after");

    /* changing the duplicate leaves the paragraph alone, and the paragraph changing leaves the duplicate */
    checkhr(ITextPara_SetAlignment(dup, tomAlignRight), S_OK, "duplicate right-aligned");
    check(ITextPara_GetAlignment(dup, &v) == S_OK && v == tomAlignRight, "duplicate reads back");
    check(ITextPara_GetAlignment(p1, &v) == S_OK && v == tomAlignCenter, "p1 unchanged by the duplicate");
    pf = fmt_at(1);
    check(pf.wAlignment == PFA_CENTER, "the editor is unchanged by the duplicate");
    checkhr(ITextPara_SetIndents(p1, 0.0f, 30.0f, 0.0f), S_OK, "p1 left indent 30");
    check(ITextPara_GetLeftIndent(dup, &f) == S_OK && flt(f, 20.0f), "duplicate unchanged by the paragraph");

    /* equal and not equal */
    checkhr(ITextPara_GetDuplicate(p1, &dup2), S_OK, "second duplicate");
    v = 99;
    checkhr(ITextPara_IsEqual(p1, dup2, &v), S_OK, "IsEqual with its own copy");
    check(v == tomTrue, "... tomTrue");
    v = 99;
    checkhr(ITextPara_IsEqual(p1, dup, &v), S_FALSE, "IsEqual with the changed duplicate");
    check(v == tomFalse, "... tomFalse");
    v = 99;
    checkhr(ITextPara_IsEqual(p1, NULL, &v), S_FALSE, "IsEqual(NULL)");
    check(v == tomFalse, "... tomFalse for NULL");
    v = 99;
    checkhr(ITextPara_IsEqual(p1, p2, &v), S_FALSE, "IsEqual of different paragraphs");

    /* SetDuplicate applies the format to the paragraphs of the range */
    checkhr(ITextPara_SetDuplicate(p2, NULL), E_INVALIDARG, "SetDuplicate(NULL)");
    checkhr(ITextPara_SetDuplicate(p2, dup2), S_OK, "SetDuplicate");
    pf = fmt_at(5);
    check(pf.wAlignment == PFA_CENTER && pf.dxStartIndent == 600, "the editor has the duplicated format");
    pf = fmt_at(9);
    check(pf.wAlignment == PFA_LEFT && pf.dxStartIndent == 0, "the third paragraph is untouched");
    v = 99;
    checkhr(ITextPara_IsEqual(p1, p2, &v), S_OK, "the two paragraphs are equal now");

    /* CanChange */
    v = 99;
    checkhr(ITextPara_CanChange(p1, &v), S_OK, "CanChange");
    check(v == tomTrue, "... tomTrue");
    checkhr(ITextPara_CanChange(p1, NULL), E_INVALIDARG, "CanChange(NULL)");
    SendMessageW(edit, EM_SETREADONLY, TRUE, 0);
    v = 99;
    ITextPara_CanChange(p1, &v);
    check(v == tomFalse, "CanChange is tomFalse in a read-only control");
    v = 99;
    ITextPara_CanChange(dup, &v);
    check(v == tomTrue, "CanChange of a duplicate is tomTrue");
    SendMessageW(edit, EM_SETREADONLY, FALSE, 0);

    /* Reset */
    checkhr(ITextPara_Reset(dup, tomDefault), S_OK, "Reset(tomDefault) of a duplicate");
    check(ITextPara_GetAlignment(dup, &v) == S_OK && v == tomAlignLeft, "duplicate is left-aligned again");
    check(ITextPara_GetLeftIndent(dup, &f) == S_OK && flt(f, 0.0f), "duplicate has no indent again");
    check(ITextPara_GetSpaceAfter(dup, &f) == S_OK && flt(f, 0.0f), "duplicate has no space after again");
    checkhr(ITextPara_Reset(p1, tomDefault), S_OK, "Reset(tomDefault) of a paragraph");
    pf = fmt_at(1);
    check(pf.wAlignment == PFA_LEFT && pf.dxStartIndent == 0 && pf.dySpaceAfter == 0, "the editor has the default format");
    pf = fmt_at(5);
    check(pf.wAlignment == PFA_CENTER, "the next paragraph is untouched by the Reset");

    ITextPara_Release(dup2);
    ITextPara_Release(dup);
    ITextPara_Release(p2);
    ITextPara_Release(p1);
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
    SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)L"one\rtwo\rthree");
    SendMessageW(edit, EM_SETSEL, 8, 8);
    SendMessageW(edit, EM_GETOLEINTERFACE, 0, (LPARAM)&ole);
    check(ole != NULL, "IRichEditOle");
    IUnknown_QueryInterface((IUnknown *)ole, &SG_IID_ITextDocument, (void **)&doc);
    check(doc != NULL, "ITextDocument");
    if (!doc) { printf("RESULT: FAIL\n"); return 1; }

    test_duplicates();

    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)ole);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
