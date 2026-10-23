/* displaysvc-probe: IDisplayServices' display pointers, caret and computed styles (Stained Glass OS
 * wine-sg 6051), in a document shown in a window (ATL's AtlAxWin with "mshtml:" text):
 * hit testing a character, line moves, line start and IsAtBOL, line breaks, line info, the flow
 * element, comparisons; the caret moved to a display pointer, read back, located, inserting text;
 * computed styles of bold underlined red text against plain text.
 * Prints NAME value lines; see test/displaysvc-gate.sh. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <initguid.h>
#include <mshtml.h>
#include <stdio.h>
#include <stdlib.h>
#define P(...) printf(__VA_ARGS__)

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while ((int)(end - GetTickCount()) > 0) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static IHTMLDocument2 *doc;
static IHTMLDocument3 *doc3;
static IMarkupServices *ms;

static IHTMLElement *byid(const WCHAR *id)
{
    IHTMLElement *e = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e); SysFreeString(b);
    return e;
}

/* the character right of a display pointer */
static WCHAR right_char(IDisplayPointer *dp)
{
    IMarkupPointer *mp = NULL;
    MARKUP_CONTEXT_TYPE ctx = CONTEXT_TYPE_None;
    WCHAR ch[2] = { 0 };
    LONG cch = 1;

    IMarkupServices_CreateMarkupPointer(ms, &mp);
    if (FAILED(IDisplayPointer_PositionMarkupPointer(dp, mp))) { IMarkupPointer_Release(mp); return '?'; }
    IMarkupPointer_Right(mp, FALSE, &ctx, NULL, &cch, ch);
    IMarkupPointer_Release(mp);
    return ctx == CONTEXT_TYPE_Text && cch >= 1 ? ch[0] : '#';
}

static WCHAR mp_right_char(IMarkupPointer *mp)
{
    MARKUP_CONTEXT_TYPE ctx = CONTEXT_TYPE_None;
    WCHAR ch[2] = { 0 };
    LONG cch = 1;
    IMarkupPointer_Right(mp, FALSE, &ctx, NULL, &cch, ch);
    return ctx == CONTEXT_TYPE_Text && cch >= 1 ? ch[0] : '#';
}

static IHTMLComputedStyle *style_in(IDisplayServices *ds, IHTMLElement *e)
{
    IMarkupPointer *mp = NULL;
    IHTMLComputedStyle *cs = NULL;
    IMarkupServices_CreateMarkupPointer(ms, &mp);
    IMarkupPointer_MoveAdjacentToElement(mp, e, ELEM_ADJ_AfterBegin);
    IDisplayServices_GetComputedStyle(ds, mp, &cs);
    IMarkupPointer_Release(mp);
    return cs;
}

int main(void)
{
    static const WCHAR html[] = L"mshtml:<html><body style=\"margin:0;font:20px monospace\">"
        L"<p id=p1 style=\"margin:0\"><span id=s1>abcdef</span><br>ghijkl</p>"
        L"<p id=p2 style=\"margin:0;font-weight:bold;color:rgb(255,0,0)\"><u id=u1>bold</u></p></body></html>";
    BOOL (WINAPI *pAtlAxWinInit)(void);
    HRESULT (WINAPI *pAtlAxGetControl)(HWND, IUnknown **);
    HMODULE atl;
    HWND hwnd;
    IUnknown *unk = NULL;
    IDisplayServices *ds = NULL;
    IDisplayPointer *dp = NULL, *dp2 = NULL;
    IHTMLCaret *caret = NULL;
    IHTMLElement *s1, *p1, *p2, *u1, *flow = NULL;
    IHTMLComputedStyle *cs_bold, *cs_plain, *cs_plain2;
    IMarkupPointer *mp = NULL;
    ILineInfo *li = NULL;
    LONG left = 0, top = 0, width = 0, height = 0, lx = -1, lh = 0;
    double cw;
    POINT pt;
    DWORD res = 0, breaks = 0, breaks_mid = 0;
    BOOL bol_mid = TRUE, bol_start = FALSE, left_of = FALSE, equal = FALSE;
    VARIANT_BOOL vb, eq1 = 0, eq2 = 0, bold = 0, under = 0;
    LONG weight = 0;
    DWORD color = 0;
    WCHAR face[64] = { 0 };
    BSTR text = NULL, id = NULL;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    atl = LoadLibraryW(L"atl.dll");
    pAtlAxWinInit = (void *)GetProcAddress(atl, "AtlAxWinInit");
    pAtlAxGetControl = (void *)GetProcAddress(atl, "AtlAxGetControl");
    if (!pAtlAxWinInit || !pAtlAxGetControl || !pAtlAxWinInit()) { P("ATL 0\nDONE\n"); return 0; }
    hwnd = CreateWindowExW(0, L"AtlAxWin", html, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 640, 480,
                           NULL, NULL, NULL, NULL);
    pump(3000);
    pAtlAxGetControl(hwnd, &unk);
    if (unk) IUnknown_QueryInterface(unk, &IID_IHTMLDocument2, (void **)&doc);
    P("DOC %d\n", doc != NULL);
    if (!doc) { P("DONE\n"); return 0; }
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void **)&doc3);
    IHTMLDocument2_QueryInterface(doc, &IID_IMarkupServices, (void **)&ms);
    IHTMLDocument2_QueryInterface(doc, &IID_IDisplayServices, (void **)&ds);
    s1 = byid(L"s1"); p1 = byid(L"p1"); p2 = byid(L"p2"); u1 = byid(L"u1");
    if (!ds || !ms || !s1) { P("SETUP 0\nDONE\n"); return 0; }
    IHTMLElement_get_offsetLeft(s1, &left);
    IHTMLElement_get_offsetTop(s1, &top);
    IHTMLElement_get_offsetWidth(s1, &width);
    IHTMLElement_get_offsetHeight(s1, &height);
    cw = width / 6.0;  /* the font's characters are not a whole pixel wide */
    P("LAYOUT %ld %ld %.2f %ld\n", left, top, cw, height);

    hr = IDisplayServices_CreateDisplayPointer(ds, &dp);
    P("CREATE %08lx\n", hr);
    if (!dp) { P("DONE\n"); return 0; }
    IDisplayServices_CreateDisplayPointer(ds, &dp2);

    /* on the third character of the first line, a little right of its left edge */
    pt.x = (LONG)(left + cw * 2.25); pt.y = top + height / 2;
    hr = IDisplayPointer_MoveToPoint(dp, pt, COORD_SYSTEM_CONTENT, NULL, 0, &res);
    P("HIT %08lx %lc %lu\n", hr, right_char(dp), res);
    /* past the middle of the fourth: after it */
    pt.x = (LONG)(left + cw * 3.75);
    IDisplayPointer_MoveToPoint(dp2, pt, COORD_SYSTEM_CONTENT, NULL, 0, &res);
    P("HITAFTER %lc\n", right_char(dp2));
    IDisplayPointer_IsLeftOf(dp, dp2, &left_of);
    IDisplayPointer_IsEqualTo(dp, dp, &equal);
    P("COMPARE %d %d\n", left_of, equal);

    hr = IDisplayPointer_MoveUnit(dp, DISPLAY_MOVEUNIT_NextLine, -1);
    P("NEXTLINE %08lx %lc\n", hr, right_char(dp));
    IDisplayPointer_IsAtBOL(dp, &bol_mid);
    IDisplayPointer_QueryBreaks(dp, &breaks_mid);
    hr = IDisplayPointer_MoveUnit(dp, DISPLAY_MOVEUNIT_CurrentLineStart, -1);
    P("LINESTART %08lx %lc\n", hr, right_char(dp));
    IDisplayPointer_IsAtBOL(dp, &bol_start);
    IDisplayPointer_QueryBreaks(dp, &breaks);
    P("BOL %d %d\n", bol_mid, bol_start);
    P("BREAKS %lu %lu\n", breaks_mid, breaks);
    if (SUCCEEDED(IDisplayPointer_GetLineInfo(dp, &li)) && li) {
        ILineInfo_get_x(li, &lx);
        ILineInfo_get_textHeight(li, &lh);
    }
    P("LINEINFO %d %d\n", lx == left, lh > 0 && lh <= height + 4);
    hr = IDisplayPointer_MoveUnit(dp, DISPLAY_MOVEUNIT_CurrentLineEnd, -1);
    P("LINEEND %08lx %lc\n", hr, right_char(dp));
    hr = IDisplayPointer_MoveUnit(dp, DISPLAY_MOVEUNIT_PreviousLine, (LONG)(left + cw / 4));
    P("PREVLINE %08lx %lc\n", hr, right_char(dp));
    if (SUCCEEDED(IDisplayPointer_GetFlowElement(dp, &flow)) && flow) IHTMLElement_get_id(flow, &id);
    P("FLOW %ls\n", id ? id : L"(none)");

    /* the caret */
    hr = IDisplayServices_GetCaret(ds, &caret);
    P("CARET %08lx\n", hr);
    if (caret) {
        pt.x = (LONG)(left + cw * 2.25); pt.y = top + height / 2;
        IDisplayPointer_MoveToPoint(dp, pt, COORD_SYSTEM_CONTENT, NULL, 0, &res);
        hr = IHTMLCaret_MoveCaretToPointer(caret, dp, FALSE, CARET_DIRECTION_FORWARD);
        IMarkupServices_CreateMarkupPointer(ms, &mp);
        IHTMLCaret_MoveMarkupPointerToCaret(caret, mp);
        P("CARETAT %08lx %lc\n", hr, mp_right_char(mp));
        pt.x = pt.y = -1;
        IHTMLCaret_GetLocation(caret, &pt, FALSE);
        P("CARETLOC %d\n", labs(pt.x - (LONG)(left + cw * 2)) <= 2 && labs(pt.y - top) <= height);
        hr = IHTMLCaret_InsertText(caret, (OLECHAR *)L"XY", 2);
        IHTMLElement_get_innerText(s1, &text);
        IHTMLCaret_MoveMarkupPointerToCaret(caret, mp);
        P("INSERT %08lx %ls %lc\n", hr, text ? text : L"(null)", mp_right_char(mp));
        IHTMLCaret_Hide(caret);
        IHTMLCaret_IsVisible(caret, &equal);
        P("HIDE %d\n", !equal);
    }

    /* computed styles */
    cs_bold = style_in(ds, u1);
    cs_plain = style_in(ds, s1);
    cs_plain2 = style_in(ds, p1);
    if (cs_bold && cs_plain && cs_plain2) {
        IHTMLComputedStyle_get_bold(cs_bold, &bold);
        IHTMLComputedStyle_get_underline(cs_bold, &under);
        IHTMLComputedStyle_get_fontWeight(cs_bold, &weight);
        IHTMLComputedStyle_get_textColor(cs_bold, &color);
        IHTMLComputedStyle_get_fontName(cs_bold, (void *)face);
        P("STYLE %d %d %ld %06lx %ls\n", bold == VARIANT_TRUE, under == VARIANT_TRUE, weight, color, face);
        vb = 1;
        IHTMLComputedStyle_get_bold(cs_plain, &vb);
        IHTMLComputedStyle_IsEqual(cs_bold, cs_plain, &eq1);
        IHTMLComputedStyle_IsEqual(cs_plain, cs_plain2, &eq2);
        P("PLAIN %d %d %d\n", vb == VARIANT_FALSE, eq1 == VARIANT_FALSE, eq2 == VARIANT_TRUE);
    }
    else P("STYLE none\n");
    P("DONE\n");
    DestroyWindow(hwnd);
    return 0;
}
