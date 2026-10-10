/* riched20 ITextPara batch (patches/sg/2015), run by test/richepara-gate.sh.
 * Families: paragraph alignment, indents, spacing, line spacing, the yes/no
 * paragraph flags, numbering, tab stops and style through ITextPara, checked
 * against EM_GETPARAFORMAT; ranges across paragraphs; the selection left
 * alone; the selection left alone.
 *
 *   richepara-probe.exe */
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

static void test_alignment_and_flags(void)
{
    ITextPara *p1 = para_of(0, 3), *p2 = para_of(4, 7), *both = para_of(0, 6);
    LONG v = 99;
    PARAFORMAT2 f;

    check(p1 && p2 && both, "paragraph objects");
    check(ITextPara_GetAlignment(p1, &v) == S_OK && v == tomAlignLeft, "default alignment is left");
    checkhr(ITextPara_SetAlignment(p1, tomAlignCenter), S_OK, "SetAlignment center");
    check(ITextPara_GetAlignment(p1, &v) == S_OK && v == tomAlignCenter, "alignment reads back");
    f = fmt_at(1);
    check(f.wAlignment == PFA_CENTER, "the editor has the centred paragraph");
    f = fmt_at(5);
    check(f.wAlignment == PFA_LEFT, "the next paragraph is untouched");
    check(sel_is(8, 8), "the selection is left alone");
    check(ITextPara_GetAlignment(both, &v) == S_OK && v == tomUndefined, "mixed alignment is undefined");
    checkhr(ITextPara_SetAlignment(both, tomAlignRight), S_OK, "SetAlignment over two paragraphs");
    f = fmt_at(1); check(f.wAlignment == PFA_RIGHT, "first paragraph of the range");
    f = fmt_at(5); check(f.wAlignment == PFA_RIGHT, "second paragraph of the range");
    f = fmt_at(9); check(f.wAlignment == PFA_LEFT, "paragraph after the range");
    checkhr(ITextPara_SetAlignment(p2, tomAlignJustify), S_OK, "SetAlignment justify");
    check(ITextPara_GetAlignment(p2, &v) == S_OK && v == tomAlignJustify, "justify reads back");
    checkhr(ITextPara_SetAlignment(p2, 9), E_INVALIDARG, "SetAlignment of a bad value");
    checkhr(ITextPara_SetAlignment(p2, tomUndefined), S_OK, "SetAlignment(undefined) changes nothing");
    check(ITextPara_GetAlignment(p2, &v) == S_OK && v == tomAlignJustify, "still justify");
    checkhr(ITextPara_GetAlignment(p2, NULL), E_INVALIDARG, "GetAlignment(NULL)");

    /* yes/no flags */
    check(ITextPara_GetKeepTogether(p1, &v) == S_OK && v == tomFalse, "KeepTogether false at first");
    checkhr(ITextPara_SetKeepTogether(p1, tomTrue), S_OK, "SetKeepTogether");
    check(ITextPara_GetKeepTogether(p1, &v) == S_OK && v == tomTrue, "KeepTogether reads back");
    f = fmt_at(1); check(f.wEffects & PFE_KEEP, "the editor has PFE_KEEP");
    checkhr(ITextPara_SetKeepWithNext(p1, tomTrue), S_OK, "SetKeepWithNext");
    f = fmt_at(1); check((f.wEffects & PFE_KEEPNEXT) && (f.wEffects & PFE_KEEP), "KeepWithNext does not undo KeepTogether");
    check(ITextPara_GetKeepWithNext(p1, &v) == S_OK && v == tomTrue, "KeepWithNext reads back");
    checkhr(ITextPara_SetPageBreakBefore(p1, tomTrue), S_OK, "SetPageBreakBefore");
    check(ITextPara_GetPageBreakBefore(p1, &v) == S_OK && v == tomTrue, "PageBreakBefore reads back");
    checkhr(ITextPara_SetNoLineNumber(p1, tomTrue), S_OK, "SetNoLineNumber");
    check(ITextPara_GetNoLineNumber(p1, &v) == S_OK && v == tomTrue, "NoLineNumber reads back");
    f = fmt_at(1); check((f.wEffects & PFE_NOLINENUMBER) && (f.wEffects & PFE_PAGEBREAKBEFORE), "the editor has both flags");
    check(ITextPara_GetWidowControl(p1, &v) == S_OK && v == tomTrue, "widow control on at first");
    checkhr(ITextPara_SetWidowControl(p1, tomFalse), S_OK, "SetWidowControl off");
    f = fmt_at(1); check(f.wEffects & PFE_NOWIDOWCONTROL, "off is PFE_NOWIDOWCONTROL");
    check(ITextPara_GetWidowControl(p1, &v) == S_OK && v == tomFalse, "widow control reads back off");
    check(ITextPara_GetHyphenation(p1, &v) == S_OK && v == tomTrue, "hyphenation on at first");
    checkhr(ITextPara_SetHyphenation(p1, tomFalse), S_OK, "SetHyphenation off");
    f = fmt_at(1); check(f.wEffects & PFE_DONOTHYPHEN, "off is PFE_DONOTHYPHEN");
    checkhr(ITextPara_SetHyphenation(p1, tomToggle), S_OK, "SetHyphenation toggle");
    check(ITextPara_GetHyphenation(p1, &v) == S_OK && v == tomTrue, "toggle turns it back on");
    checkhr(ITextPara_SetKeepTogether(p1, tomFalse), S_OK, "SetKeepTogether off");
    check(ITextPara_GetKeepTogether(p1, &v) == S_OK && v == tomFalse, "KeepTogether off reads back");
    checkhr(ITextPara_SetKeepTogether(p1, tomUndefined), S_OK, "flag set to undefined changes nothing");
    checkhr(ITextPara_SetKeepTogether(p1, 5), E_INVALIDARG, "flag set to a bad value");
    ITextPara_SetKeepTogether(p1, tomTrue);
    check(ITextPara_GetKeepTogether(both, &v) == S_OK && v == tomUndefined, "a flag that differs over the range is undefined");
    ITextPara_SetKeepTogether(p1, tomFalse);

    ITextPara_Release(p1); ITextPara_Release(p2); ITextPara_Release(both);
}

static void test_indents_spacing(void)
{
    ITextPara *p = para_of(8, 12);
    FLOAT f;
    PARAFORMAT2 pf;
    LONG v;

    check(ITextPara_GetLeftIndent(p, &f) == S_OK && flt(f, 0), "left indent 0 at first");
    checkhr(ITextPara_SetIndents(p, 10, 20, 5), S_OK, "SetIndents(10, 20, 5)");
    check(ITextPara_GetLeftIndent(p, &f) == S_OK && flt(f, 20), "left indent");
    check(ITextPara_GetFirstLineIndent(p, &f) == S_OK && flt(f, 10), "first line indent");
    check(ITextPara_GetRightIndent(p, &f) == S_OK && flt(f, 5), "right indent");
    pf = fmt_at(9);
    check(pf.dxStartIndent == 600 && pf.dxOffset == -200 && pf.dxRightIndent == 100, "the editor has start 600, offset -200, right 100");
    checkhr(ITextPara_SetIndents(p, tomUndefined, 40, tomUndefined), S_OK, "SetIndents changing only the left");
    check(ITextPara_GetLeftIndent(p, &f) == S_OK && flt(f, 40), "left indent now 40");
    check(ITextPara_GetFirstLineIndent(p, &f) == S_OK && flt(f, 10), "first line indent kept");
    check(ITextPara_GetRightIndent(p, &f) == S_OK && flt(f, 5), "right indent kept");
    checkhr(ITextPara_SetRightIndent(p, 7.5f), S_OK, "SetRightIndent");
    check(ITextPara_GetRightIndent(p, &f) == S_OK && flt(f, 7.5f), "right indent 7.5");
    checkhr(ITextPara_SetIndents(p, -15, 30, tomUndefined), S_OK, "a hanging first line");
    check(ITextPara_GetFirstLineIndent(p, &f) == S_OK && flt(f, -15) && ITextPara_GetLeftIndent(p, &f) == S_OK && flt(f, 30), "hanging indent reads back");

    check(ITextPara_GetSpaceBefore(p, &f) == S_OK && flt(f, 0), "space before 0");
    checkhr(ITextPara_SetSpaceBefore(p, 12.5f), S_OK, "SetSpaceBefore");
    check(ITextPara_GetSpaceBefore(p, &f) == S_OK && flt(f, 12.5f), "space before reads back");
    pf = fmt_at(9); check(pf.dySpaceBefore == 250, "the editor has 250 twips");
    checkhr(ITextPara_SetSpaceAfter(p, 6), S_OK, "SetSpaceAfter");
    check(ITextPara_GetSpaceAfter(p, &f) == S_OK && flt(f, 6), "space after reads back");
    checkhr(ITextPara_SetSpaceAfter(p, -1), E_INVALIDARG, "negative space");

    check(ITextPara_GetLineSpacingRule(p, &v) == S_OK && v == tomLineSpaceSingle, "single spacing at first");
    checkhr(ITextPara_SetLineSpacing(p, tomLineSpaceExactly, 18), S_OK, "SetLineSpacing exactly 18");
    check(ITextPara_GetLineSpacingRule(p, &v) == S_OK && v == tomLineSpaceExactly, "rule reads back");
    check(ITextPara_GetLineSpacing(p, &f) == S_OK && flt(f, 18), "spacing reads back");
    pf = fmt_at(9); check(pf.bLineSpacingRule == 4 && pf.dyLineSpacing == 360, "the editor has rule 4, 360 twips");
    checkhr(ITextPara_SetLineSpacing(p, tomLineSpaceMultiple, 1.5f), S_OK, "SetLineSpacing multiple 1.5");
    pf = fmt_at(9); check(pf.bLineSpacingRule == 5 && pf.dyLineSpacing == 30, "the editor has rule 5, 30");
    checkhr(ITextPara_SetLineSpacing(p, 9, 1), E_INVALIDARG, "SetLineSpacing with a bad rule");
    checkhr(ITextPara_SetLineSpacing(p, -1, 1), E_INVALIDARG, "SetLineSpacing with a negative rule");

    ITextPara_Release(p);
}

static void test_lists_tabs_style(void)
{
    ITextPara *p = para_of(0, 3);
    FLOAT pos, f;
    LONG v, align, leader;
    PARAFORMAT2 pf;

    check(ITextPara_GetListType(p, &v) == S_OK && v == tomListNone, "no list at first");
    checkhr(ITextPara_SetListType(p, tomListNumberAsArabic | tomListPeriod), S_OK, "SetListType arabic with period");
    check(ITextPara_GetListType(p, &v) == S_OK && v == (tomListNumberAsArabic | tomListPeriod), "list type reads back");
    pf = fmt_at(1); check(pf.wNumbering == PFN_ARABIC && pf.wNumberingStyle == PFNS_PERIOD, "the editor has arabic with period");
    checkhr(ITextPara_SetListType(p, tomListBullet), S_OK, "SetListType bullet");
    check(ITextPara_GetListType(p, &v) == S_OK && v == tomListBullet, "bullet reads back");
    checkhr(ITextPara_SetListType(p, 12), E_INVALIDARG, "SetListType of an unknown type");
    checkhr(ITextPara_SetListStart(p, 5), S_OK, "SetListStart");
    check(ITextPara_GetListStart(p, &v) == S_OK && v == 5, "list start reads back");
    checkhr(ITextPara_SetListStart(p, -3), E_INVALIDARG, "negative list start");
    checkhr(ITextPara_SetListTab(p, 18), S_OK, "SetListTab");
    check(ITextPara_GetListTab(p, &f) == S_OK && flt(f, 18), "list tab reads back");
    pf = fmt_at(1); check(pf.wNumberingTab == 360, "the editor has 360 twips");
    checkhr(ITextPara_SetListType(p, tomListNone), S_OK, "SetListType none");

    check(ITextPara_GetTabCount(p, &v) == S_OK && v == 0, "no tabs at first");
    checkhr(ITextPara_AddTab(p, 72, tomAlignLeft, tomSpaces), S_OK, "AddTab 72");
    checkhr(ITextPara_AddTab(p, 144, tomAlignRight, tomDashes), S_OK, "AddTab 144");
    checkhr(ITextPara_AddTab(p, 36, tomAlignCenter, tomDots), S_OK, "AddTab 36");
    check(ITextPara_GetTabCount(p, &v) == S_OK && v == 3, "three tabs");
    check(ITextPara_GetTab(p, 0, &pos, &align, &leader) == S_OK && flt(pos, 36) && align == tomAlignCenter && leader == tomDots, "tab 0 is the smallest");
    check(ITextPara_GetTab(p, 1, &pos, &align, &leader) == S_OK && flt(pos, 72) && align == tomAlignLeft && leader == tomSpaces, "tab 1");
    check(ITextPara_GetTab(p, 2, &pos, &align, &leader) == S_OK && flt(pos, 144) && align == tomAlignRight && leader == tomDashes, "tab 2");
    pf = fmt_at(1); check(pf.cTabCount == 3 && (pf.rgxTabs[0] & 0xffffff) == 720 && ((pf.rgxTabs[0] >> 24) & 15) == 1 && ((pf.rgxTabs[0] >> 28) & 15) == 1, "the editor has the packed tabs");
    checkhr(ITextPara_AddTab(p, 72, tomAlignDecimal, tomLines), S_OK, "AddTab at an existing place");
    check(ITextPara_GetTabCount(p, &v) == S_OK && v == 3, "still three tabs");
    check(ITextPara_GetTab(p, 1, &pos, &align, &leader) == S_OK && align == tomAlignDecimal && leader == tomLines, "the tab changed");
    checkhr(ITextPara_AddTab(p, 50, 9, tomSpaces), E_INVALIDARG, "AddTab with a bad alignment");
    checkhr(ITextPara_AddTab(p, 50, tomAlignLeft, 12), E_INVALIDARG, "AddTab with a bad leader");
    checkhr(ITextPara_AddTab(p, -5, tomAlignLeft, tomSpaces), E_INVALIDARG, "AddTab with a negative place");
    checkhr(ITextPara_GetTab(p, 3, &pos, &align, &leader), E_INVALIDARG, "GetTab past the end");
    checkhr(ITextPara_GetTab(p, -1, &pos, &align, &leader), E_INVALIDARG, "GetTab at -1");
    checkhr(ITextPara_DeleteTab(p, 36), S_OK, "DeleteTab");
    check(ITextPara_GetTabCount(p, &v) == S_OK && v == 2, "two tabs");
    check(ITextPara_GetTab(p, 0, &pos, &align, &leader) == S_OK && flt(pos, 72), "the others moved up");
    checkhr(ITextPara_ClearAllTabs(p), S_OK, "ClearAllTabs");
    check(ITextPara_GetTabCount(p, &v) == S_OK && v == 0, "no tabs left");
    {
        int i;
        HRESULT hr = S_OK;
        for (i = 1; i <= MAX_TAB_STOPS && hr == S_OK; i++) hr = ITextPara_AddTab(p, 10.0f * i, tomAlignLeft, tomSpaces);
        check(hr == S_OK, "32 tabs fit");
        hr = ITextPara_AddTab(p, 999, tomAlignLeft, tomSpaces);
        check(FAILED(hr), "the 33rd tab does not");
        ITextPara_ClearAllTabs(p);
    }

    checkhr(ITextPara_SetStyle(p, 2), S_OK, "SetStyle");
    check(ITextPara_GetStyle(p, &v) == S_OK && v == 2, "style reads back");
    ITextPara_Release(p);
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

    test_alignment_and_flags();
    test_indents_spacing();
    test_lists_tabs_style();

    ITextDocument_Release(doc);
    IUnknown_Release((IUnknown *)ole);
    printf("%d checks failed\n", failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
