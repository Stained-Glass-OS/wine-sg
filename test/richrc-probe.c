/* riched20 ITextPara Reset/SetDuplicate and ITextFont::Reset(tomDefault) on a range (patch 3283), run by test/richrc-gate.sh.
 *
 * (set-up as richepara-probe.c)
 *
 *   richrc-probe.exe */
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

static ITextPara *para_of(LONG start, LONG end)
{
    ITextRange *range = NULL;
    ITextPara *para = NULL;
    ITextDocument_Range(doc, start, end, &range);
    if (range) { ITextRange_GetPara(range, &para); ITextRange_Release(range); }
    return para;
}

/* the format of the paragraph holding a position, read the old way */
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

static int sel_is(LONG a, LONG b)
{
    CHARRANGE cr;
    SendMessageW(edit, EM_EXGETSEL, 0, (LPARAM)&cr);
    return cr.cpMin == a && cr.cpMax == b;
}

static int flt(FLOAT a, FLOAT b) { return a > b - 0.01f && a < b + 0.01f; }


static void test_para(void)
{
    ITextPara *p1 = para_of(0, 3), *p2 = para_of(4, 7), *p3 = para_of(8, 12);
    LONG v; FLOAT f1; PARAFORMAT2 f;

    /* Reset(tomDefault) puts the paragraph format back to the RTF \\pard defaults, for the paragraphs of the range only */
    ITextPara_SetAlignment(p1, tomAlignRight);
    ITextPara_SetSpaceBefore(p1, 12);
    ITextPara_SetKeepTogether(p1, tomTrue);
    ITextPara_SetListType(p1, tomListBullet);
    ITextPara_SetIndents(p1, 10, 30, 5);
    ITextPara_SetAlignment(p2, tomAlignCenter);
    checkhr(ITextPara_Reset(p1, tomDefault), S_OK, "Reset(tomDefault)");
    check(ITextPara_GetAlignment(p1, &v) == S_OK && v == tomAlignLeft, "alignment is left again");
    check(ITextPara_GetSpaceBefore(p1, &f1) == S_OK && f1 == 0, "space before is 0 again");
    check(ITextPara_GetKeepTogether(p1, &v) == S_OK && v == tomFalse, "keep together is off again");
    check(ITextPara_GetListType(p1, &v) == S_OK && v == tomNone, "no list again");
    check(ITextPara_GetLeftIndent(p1, &f1) == S_OK && f1 == 0 && ITextPara_GetRightIndent(p1, &f1) == S_OK && f1 == 0, "indents are 0 again");
    f = fmt_at(1);
    check(f.wAlignment == PFA_LEFT && f.dxStartIndent == 0 && f.dxOffset == 0 && f.wNumbering == 0, "the editor has the default format");
    check(ITextPara_GetAlignment(p2, &v) == S_OK && v == tomAlignCenter, "the next paragraph is untouched");
    checkhr(ITextPara_Reset(p1, tomUndefined), E_INVALIDARG, "Reset(tomUndefined) on a range's paragraph");
    checkhr(ITextPara_Reset(p1, 12345), E_INVALIDARG, "Reset(bad value)");

    /* SetDuplicate applies what the source has defined */
    ITextPara_SetAlignment(p2, tomAlignJustify);
    ITextPara_SetSpaceAfter(p2, 6);
    ITextPara_SetIndents(p2, 8, 20, 4);
    ITextPara_SetHyphenation(p2, tomTrue);
    ITextPara_SetListType(p2, tomListNumberAsArabic);
    ITextPara_SetListStart(p2, 3);
    ITextPara_SetLineSpacing(p2, tomLineSpaceExactly, 18);
    checkhr(ITextPara_SetDuplicate(p3, p2), S_OK, "SetDuplicate");
    check(ITextPara_GetAlignment(p3, &v) == S_OK && v == tomAlignJustify, "duplicate: alignment");
    check(ITextPara_GetSpaceAfter(p3, &f1) == S_OK && flt(f1, 6), "duplicate: space after");
    check(ITextPara_GetFirstLineIndent(p3, &f1) == S_OK && flt(f1, 8) && ITextPara_GetLeftIndent(p3, &f1) == S_OK && flt(f1, 20)
          && ITextPara_GetRightIndent(p3, &f1) == S_OK && flt(f1, 4), "duplicate: indents");
    check(ITextPara_GetHyphenation(p3, &v) == S_OK && v == tomTrue, "duplicate: hyphenation");
    check(ITextPara_GetListType(p3, &v) == S_OK && v == tomListNumberAsArabic && ITextPara_GetListStart(p3, &v) == S_OK && v == 3, "duplicate: list");
    check(ITextPara_GetLineSpacingRule(p3, &v) == S_OK && v == tomLineSpaceExactly && ITextPara_GetLineSpacing(p3, &f1) == S_OK && flt(f1, 18), "duplicate: line spacing");
    checkhr(ITextPara_SetDuplicate(p3, NULL), E_INVALIDARG, "SetDuplicate(NULL)");
    ITextPara_Release(p1); ITextPara_Release(p2); ITextPara_Release(p3);
}

static void test_font(void)
{
    ITextRange *range = NULL; ITextFont *font = NULL; LONG v; CHARFORMAT2W cf; HRESULT hr;
    ITextDocument_Range(doc, 0, 3, &range);
    ITextRange_GetFont(range, &font);
    check(font != NULL, "range font");
    if (!font) return;
    ITextFont_SetBold(font, tomTrue);
    ITextFont_SetItalic(font, tomTrue);
    ITextFont_SetSize(font, 20);
    memset(&cf, 0, sizeof(cf)); cf.cbSize = sizeof(cf);
    SendMessageW(edit, EM_SETSEL, 0, 3);
    SendMessageW(edit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    check((cf.dwEffects & CFE_BOLD) && (cf.dwEffects & CFE_ITALIC), "the editor has the bold italic text");
    hr = ITextFont_Reset(font, tomDefault);
    checkhr(hr, S_OK, "Reset(tomDefault) on a range font");
    check(ITextFont_GetBold(font, &v) == S_OK && v == tomFalse, "bold is off in the font");
    memset(&cf, 0, sizeof(cf)); cf.cbSize = sizeof(cf);
    SendMessageW(edit, EM_SETSEL, 0, 3);
    SendMessageW(edit, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    check(!(cf.dwEffects & CFE_BOLD) && !(cf.dwEffects & CFE_ITALIC), "the editor text is not bold or italic any more");
    checkhr(ITextFont_Reset(font, tomUndefined), E_INVALIDARG, "Reset(tomUndefined) on a range font");
    ITextFont_Release(font); ITextRange_Release(range);
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

    test_para();
    test_font();

    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)ole);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
