#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mshtml.h>
#include <stdio.h>
static const IID IID_IHTMLEventObj3_ = {0x3050f680,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj3 IID_IHTMLEventObj3_
static const IID IID_IHTMLEventObj4_ = {0x3050f814,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj4 IID_IHTMLEventObj4_
static const IID IID_IHTMLEventObj5_ = {0x30510478,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj5 IID_IHTMLEventObj5_
static const IID IID_IHTMLEventObj6_ = {0x30510734,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLEventObj6 IID_IHTMLEventObj6_
static const IID IID_IHTMLDocument4_ = {0x3050f69a,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IHTMLDocument4 IID_IHTMLDocument4_
static const IID IID_IMarkupPointer2_ = {0x3050f675,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IMarkupPointer2 IID_IMarkupPointer2_
static const IID IID_IMarkupServices_ = {0x3050f4a0,0x98b5,0x11cf,{0xbb,0x82,0x00,0xaa,0x00,0xbd,0xce,0x0b}};
#define IID_IMarkupServices IID_IMarkupServices_
#define P(...) printf(__VA_ARGS__)
static const CLSID CLSID_HTMLDocument_ = {0x25336920,0x03f9,0x11cf,{0x8f,0xd0,0x00,0xaa,0x00,0x68,0x6f,0x13}};



static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}




static const WCHAR page[] =
L"<!DOCTYPE html>\n<html><head><meta http-equiv=\"x-ua-compatible\" content=\"IE=11\"></head><body>"
L"<div id=d1>Hello <b id=b1>World</b>!</div><p id=p1>x</p><br id=br1><i id=i1>ab cd</i>"
L"</body></html>";

static IHTMLDocument2 *doc;
static IHTMLDocument3 *doc3;
static IMarkupServices *ms;

static IHTMLElement *byid(const WCHAR *id)
{
    IHTMLElement *e = NULL; BSTR b = SysAllocString(id);
    IHTMLDocument3_getElementById(doc3, b, &e); SysFreeString(b);
    return e;
}

static IMarkupPointer *mkptr(const WCHAR *id, ELEMENT_ADJACENCY adj)
{
    IMarkupPointer *p = NULL; IHTMLElement *e = byid(id);
    IMarkupServices_CreateMarkupPointer(ms, &p);
    if (e) { IMarkupPointer_MoveAdjacentToElement(p, e, adj); IHTMLElement_Release(e); }
    return p;
}

static void inner(const WCHAR *id, const char *tag)
{
    IHTMLElement *e = byid(id); BSTR b = NULL;
    if (e) IHTMLElement_get_innerHTML(e, &b);
    P("%s %ls\n", tag, b ? b : L"(null)");
    SysFreeString(b);
    if (e) IHTMLElement_Release(e);
}

/* walk with Right/Left and print the tokens */
static void walk(IMarkupPointer *p, BOOL right, const char *tag, int max)
{
    char out[1024]; int n = 0, i; out[0] = 0;
    for (i = 0; i < max; i++) {
        MARKUP_CONTEXT_TYPE ctx = 99; IHTMLElement *e = NULL; LONG cch = 64; OLECHAR buf[65]; HRESULT hr; BSTR tn = NULL;
        buf[0] = 0;
        hr = right ? IMarkupPointer_Right(p, TRUE, &ctx, &e, &cch, buf) : IMarkupPointer_Left(p, TRUE, &ctx, &e, &cch, buf);
        if (FAILED(hr) || ctx == CONTEXT_TYPE_None) { n += sprintf(out + n, "%sNone", n ? "|" : ""); if (e) IHTMLElement_Release(e); break; }
        if (e) IHTMLElement_get_tagName(e, &tn);
        if (ctx == CONTEXT_TYPE_Text) { buf[cch] = 0; n += sprintf(out + n, "%sT:%ls", n ? "|" : "", buf); }
        else n += sprintf(out + n, "%s%c:%ls", n ? "|" : "", ctx == CONTEXT_TYPE_EnterScope ? 'E' : ctx == CONTEXT_TYPE_ExitScope ? 'X' : 'N', tn ? tn : L"?");
        SysFreeString(tn); if (e) IHTMLElement_Release(e);
    }
    P("%s %s\n", tag, out);
}

static BOOL eq(IMarkupPointer *a, IMarkupPointer *b)
{
    BOOL r = 2; IMarkupPointer_IsEqualTo(a, b, &r); return r;
}

int main(void)
{
    HRESULT hr; BOOL bl; LONG l;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_HTMLDocument_,NULL,CLSCTX_INPROC_SERVER,&IID_IHTMLDocument2,(void**)&doc);
    P("DOC %08lx\n", hr);
    {
        IPersistStreamInit *psi; IStream *stream; HGLOBAL mem; char *ascii; int len;
        len = WideCharToMultiByte(CP_ACP, 0, page, -1, NULL, 0, NULL, NULL) - 1;
        mem = GlobalAlloc(0, len); ascii = GlobalLock(mem);
        WideCharToMultiByte(CP_ACP, 0, page, -1, ascii, len, NULL, NULL); GlobalUnlock(mem);
        CreateStreamOnHGlobal(mem, TRUE, &stream);
        IHTMLDocument2_QueryInterface(doc, &IID_IPersistStreamInit, (void**)&psi);
        IPersistStreamInit_InitNew(psi);
        IPersistStreamInit_Load(psi, stream);
        IPersistStreamInit_Release(psi); IStream_Release(stream);
    }
    pump(1500);
    IHTMLDocument2_QueryInterface(doc, &IID_IHTMLDocument3, (void**)&doc3);
    hr = IHTMLDocument2_QueryInterface(doc, &IID_IMarkupServices, (void**)&ms);
    P("MS %08lx\n", hr);

    /* pointer state */
    {
        IMarkupPointer *p = NULL; IMarkupContainer *c = NULL; IHTMLDocument2 *od = NULL; POINTER_GRAVITY g = 9; BOOL cl = 9;
        IMarkupServices_CreateMarkupPointer(ms, &p);
        bl = 9; IMarkupPointer_IsPositioned(p, &bl); P("POS0 %d", bl);
        IMarkupPointer_MoveToContainer(p, NULL, TRUE);
        { IHTMLElement *e = byid(L"d1"); hr = IMarkupPointer_MoveAdjacentToElement(p, e, ELEM_ADJ_AfterBegin); IHTMLElement_Release(e); }
        bl = 9; IMarkupPointer_IsPositioned(p, &bl); P(" %08lx %d", hr, bl);
        hr = IMarkupPointer_GetContainer(p, &c); P(" %08lx %d", hr, c != NULL);
        hr = IMarkupPointer_OwningDoc(p, &od); P(" %08lx %d\n", hr, od != NULL);
        IMarkupPointer_Gravity(p, &g); IMarkupPointer_SetGravity(p, POINTER_GRAVITY_Right); { POINTER_GRAVITY g2 = 9; IMarkupPointer_Gravity(p, &g2); P("GRAV %d %d\n", g, g2); }
        IMarkupPointer_SetCling(p, TRUE); IMarkupPointer_Cling(p, &cl); P("CLING %d\n", cl);
        IMarkupPointer_Unposition(p); bl = 9; IMarkupPointer_IsPositioned(p, &bl); P("UNPOS %d\n", bl);
        if (c) IMarkupContainer_Release(c); if (od) IHTMLDocument2_Release(od);
        IMarkupPointer_Release(p);
    }
    /* walking */
    { IMarkupPointer *p = mkptr(L"d1", ELEM_ADJ_AfterBegin); walk(p, TRUE, "RIGHT", 10); IMarkupPointer_Release(p); }
    { IMarkupPointer *p = mkptr(L"d1", ELEM_ADJ_AfterEnd); walk(p, FALSE, "LEFT", 10); IMarkupPointer_Release(p); }
    { IMarkupPointer *p = mkptr(L"br1", ELEM_ADJ_BeforeBegin); walk(p, TRUE, "NOSCOPE", 1); IMarkupPointer_Release(p); }
    /* comparisons */
    {
        IMarkupPointer *a = mkptr(L"d1", ELEM_ADJ_AfterEnd), *b = mkptr(L"p1", ELEM_ADJ_BeforeBegin), *c = mkptr(L"d1", ELEM_ADJ_AfterBegin), *d = mkptr(L"d1", ELEM_ADJ_BeforeEnd);
        BOOL l1 = 9, l2 = 9, r1 = 9, e1 = 9, le = 9, re = 9;
        IMarkupPointer_IsLeftOf(c, d, &l1); IMarkupPointer_IsLeftOf(d, c, &l2); IMarkupPointer_IsRightOf(d, c, &r1);
        IMarkupPointer_IsLeftOfOrEqualTo(a, b, &le); IMarkupPointer_IsRightOfOrEqualTo(a, b, &re);
        e1 = eq(a, b);
        P("CMP %d %d %d %d %d %d\n", l1, l2, r1, e1, le, re);
        IMarkupPointer_Release(a); IMarkupPointer_Release(b); IMarkupPointer_Release(c); IMarkupPointer_Release(d);
    }
    /* the end of a text and the position after its node are one position */
    {
        IMarkupPointer *p = mkptr(L"d1", ELEM_ADJ_AfterBegin), *q = mkptr(L"b1", ELEM_ADJ_BeforeBegin);
        MARKUP_CONTEXT_TYPE ctx; LONG cch = 64; OLECHAR buf[65];
        IMarkupPointer_Right(p, TRUE, &ctx, NULL, &cch, buf);
        P("TEXTEND %d %ld %d\n", ctx, cch, eq(p, q));
        IMarkupPointer_Release(p); IMarkupPointer_Release(q);
    }
    /* markup positions */
    {
        IMarkupPointer2 *p2 = NULL; IMarkupPointer *a = mkptr(L"d1", ELEM_ADJ_AfterBegin), *b = mkptr(L"d1", ELEM_ADJ_AfterEnd), *c;
        LONG pa = -1, pb = -1; IMarkupContainer *cont = NULL; IMarkupPointer2 *c2 = NULL;
        IMarkupPointer_QueryInterface(a, &IID_IMarkupPointer2, (void**)&p2);
        hr = IMarkupPointer2_GetMarkupPosition(p2, &pa);
        IMarkupPointer_QueryInterface(b, &IID_IMarkupPointer2, (void**)&c2);
        IMarkupPointer2_GetMarkupPosition(c2, &pb);
        P("MP %08lx %ld\n", hr, pb - pa);
        IMarkupServices_CreateMarkupPointer(ms, &c);
        IMarkupPointer_GetContainer(a, &cont);
        { IMarkupPointer2 *cc = NULL; IMarkupPointer_QueryInterface(c, &IID_IMarkupPointer2, (void**)&cc);
          hr = IMarkupPointer2_MoveToMarkupPosition(cc, cont, pa); P("MOVEMP %08lx %d\n", hr, eq(a, c));
          hr = IMarkupPointer2_MoveToMarkupPosition(cc, cont, pb); P("MOVEMP2 %08lx %d\n", hr, eq(b, c)); IMarkupPointer2_Release(cc); }
        if (cont) IMarkupContainer_Release(cont);
        IMarkupPointer2_Release(p2); IMarkupPointer2_Release(c2); IMarkupPointer_Release(a); IMarkupPointer_Release(b); IMarkupPointer_Release(c);
    }
    /* tag ids and element creation */
    {
        ELEMENT_TAG_ID id = 0; BSTR n = NULL, nm = SysAllocString(L"DIV"); IHTMLElement *e = NULL, *clone = NULL; BSTR s = NULL, idv = NULL, cls = NULL; VARIANT v;
        IMarkupServices_GetTagIDForName(ms, nm, &id); P("TAG1 %d\n", id == TAGID_DIV); SysFreeString(nm);
        hr = IMarkupServices_GetNameForTagID(ms, TAGID_TABLE, &n); P("TAG2 %08lx %ls\n", hr, n ? n : L"(null)"); SysFreeString(n);
        e = byid(L"d1"); IMarkupServices_GetElementTagId(ms, e, &id); P("TAG3 %d\n", id == TAGID_DIV); IHTMLElement_Release(e);
        e = byid(L"br1"); bl = 9; IMarkupServices_IsScopedElement(ms, e, &bl); P("SCOPED %d", bl); IHTMLElement_Release(e);
        e = byid(L"d1"); bl = 9; IMarkupServices_IsScopedElement(ms, e, &bl); P(" %d\n", bl); IHTMLElement_Release(e);
        hr = IMarkupServices_CreateElement(ms, TAGID_SPAN, (OLECHAR*)L"id=s1 class='k m' title=t", &e);
        if (e) { IHTMLElement_get_tagName(e, &s); IHTMLElement_get_id(e, &idv); IHTMLElement_get_className(e, &cls); }
        P("CREATE %08lx %ls %ls %ls\n", hr, s ? s : L"?", idv ? idv : L"?", cls ? cls : L"?");
        SysFreeString(s); SysFreeString(idv); SysFreeString(cls);
        if (e) { VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"inner"); IHTMLElement_put_innerText(e, V_BSTR(&v)); SysFreeString(V_BSTR(&v));
                 hr = IMarkupServices_CloneElement(ms, e, &clone); s = NULL; if (clone) IHTMLElement_get_innerHTML(clone, &s); idv = NULL; if (clone) IHTMLElement_get_id(clone, &idv);
                 P("CLONE %08lx [%ls] %ls\n", hr, s ? s : L"(null)", idv ? idv : L"?"); SysFreeString(s); SysFreeString(idv); if (clone) IHTMLElement_Release(clone); IHTMLElement_Release(e); }
        hr = IMarkupServices_CreateElement(ms, TAGID_UNKNOWN, NULL, &e); P("CREATEBAD %08lx\n", hr);
    }
    /* editing */
    {
        IMarkupPointer *s = mkptr(L"d1", ELEM_ADJ_BeforeEnd);
        IHTMLElement *span = NULL;
        hr = IMarkupServices_InsertText(ms, (OLECHAR*)L"??", -1, s); P("INSTEXT %08lx\n", hr); inner(L"d1", "D1A");
        IMarkupPointer_Release(s);
        IMarkupServices_CreateElement(ms, TAGID_SPAN, (OLECHAR*)L"id=s1", &span);
        { IMarkupPointer *a = mkptr(L"b1", ELEM_ADJ_BeforeBegin), *b = mkptr(L"b1", ELEM_ADJ_AfterEnd);
          hr = IMarkupServices_InsertElement(ms, span, a, b); P("INSELEM %08lx\n", hr); inner(L"d1", "D1B");
          IMarkupPointer_Release(a); IMarkupPointer_Release(b); }
        hr = IMarkupServices_RemoveElement(ms, span); P("REMELEM %08lx\n", hr); inner(L"d1", "D1C");
        IHTMLElement_Release(span);
        { IMarkupPointer *a = mkptr(L"d1", ELEM_ADJ_AfterBegin), *b = mkptr(L"b1", ELEM_ADJ_BeforeBegin), *t = mkptr(L"p1", ELEM_ADJ_AfterBegin);
          hr = IMarkupServices_Copy(ms, a, b, t); P("COPY %08lx\n", hr); inner(L"p1", "P1A"); inner(L"d1", "D1D");
          hr = IMarkupServices_Move(ms, a, b, t); P("MOVE %08lx\n", hr); inner(L"p1", "P1B"); inner(L"d1", "D1E");
          hr = IMarkupServices_Copy(ms, a, b, a); P("COPYSELF %08lx\n", hr);
          IMarkupPointer_Release(a); IMarkupPointer_Release(b); IMarkupPointer_Release(t); }
        { IMarkupPointer *a = mkptr(L"b1", ELEM_ADJ_AfterBegin), *b = mkptr(L"b1", ELEM_ADJ_BeforeEnd);
          hr = IMarkupServices_Remove(ms, b, a); P("REMOVEBAD %08lx\n", hr);
          hr = IMarkupServices_Remove(ms, a, b); P("REMOVE %08lx\n", hr); inner(L"d1", "D1F");
          IMarkupPointer_Release(a); IMarkupPointer_Release(b); }
    }
    /* ranges */
    {
        IHTMLBodyElement *body = NULL; IHTMLElement *be = NULL; IHTMLTxtRange *rng = NULL; IMarkupPointer *a, *b; BSTR t = NULL;
        IHTMLDocument2_get_body(doc, &be); IHTMLElement_QueryInterface(be, &IID_IHTMLBodyElement, (void**)&body);
        IHTMLBodyElement_createTextRange(body, &rng);
        { IHTMLElement *e = byid(L"i1"); IHTMLTxtRange_moveToElementText(rng, e); IHTMLElement_Release(e); }
        IMarkupServices_CreateMarkupPointer(ms, &a); IMarkupServices_CreateMarkupPointer(ms, &b);
        hr = IMarkupServices_MovePointersToRange(ms, rng, a, b);
        { IMarkupPointer *ea = mkptr(L"i1", ELEM_ADJ_AfterBegin), *eb = mkptr(L"i1", ELEM_ADJ_BeforeEnd);
          P("PTR2RANGE %08lx %d %d\n", hr, eq(a, ea), eq(b, eb)); IMarkupPointer_Release(ea); IMarkupPointer_Release(eb); }
        IMarkupPointer_Release(a); IMarkupPointer_Release(b);
        a = mkptr(L"i1", ELEM_ADJ_AfterBegin); b = mkptr(L"i1", ELEM_ADJ_BeforeEnd);
        hr = IMarkupServices_MoveRangeToPointers(ms, a, b, rng); IHTMLTxtRange_get_text(rng, &t);
        P("RANGE2PTR %08lx [%ls]\n", hr, t ? t : L"(null)"); SysFreeString(t);
        IMarkupPointer_Release(a); IMarkupPointer_Release(b); IHTMLTxtRange_Release(rng); IHTMLBodyElement_Release(body); IHTMLElement_Release(be);
    }
    /* FindText, MoveUnit */
    {
        IMarkupPointer *p = mkptr(L"i1", ELEM_ADJ_AfterBegin), *e = mkptr(L"i1", ELEM_ADJ_AfterBegin);
        hr = IMarkupPointer_FindText(p, (OLECHAR*)L"cd", 0, e, NULL);
        { MARKUP_CONTEXT_TYPE ctx; LONG cch = 8; OLECHAR buf[9] = {0}; IMarkupPointer_Right(p, FALSE, &ctx, NULL, &cch, buf); buf[cch] = 0;
          P("FIND %08lx %ls", hr, buf); cch = 8; IMarkupPointer_Left(e, FALSE, &ctx, NULL, &cch, buf); buf[cch] = 0; P(" %ls\n", buf); }
        hr = IMarkupPointer_FindText(p, (OLECHAR*)L"zz", 0, NULL, NULL); P("FINDNO %08lx\n", hr);
        hr = IMarkupPointer_FindText(p, (OLECHAR*)L"CD", 4, NULL, NULL); P("FINDCASE %08lx\n", hr);
        IMarkupPointer_Release(p); IMarkupPointer_Release(e);
        p = mkptr(L"i1", ELEM_ADJ_AfterBegin);
        hr = IMarkupPointer_MoveUnit(p, MOVEUNIT_NEXTCHAR);
        { MARKUP_CONTEXT_TYPE ctx; LONG cch = 2; OLECHAR buf[3] = {0}; IMarkupPointer_Right(p, FALSE, &ctx, NULL, &cch, buf); buf[cch] = 0; P("UNIT %08lx %ls\n", hr, buf); }
        IMarkupPointer_Release(p);
    }
    /* a parsed container */
    {
        IMarkupContainer *c = NULL; IHTMLDocument2 *od = NULL; IMarkupPointer *a, *b; MARKUP_CONTEXT_TYPE ctx; LONG cch = 32; OLECHAR buf[33] = {0}; IHTMLElement *e = NULL; BSTR tn = NULL;
        IMarkupServices_CreateMarkupPointer(ms, &a); IMarkupServices_CreateMarkupPointer(ms, &b);
        hr = IMarkupServices_ParseString(ms, (OLECHAR*)L"<u>zz</u>t", 0, &c, a, b);
        P("PARSE %08lx", hr);
        hr = IMarkupContainer_OwningDoc(c, &od); P(" %08lx %d", hr, od != NULL);
        IMarkupPointer_Right(a, FALSE, &ctx, &e, &cch, buf); if (e) IHTMLElement_get_tagName(e, &tn); P(" %d %ls", ctx, tn ? tn : L"?");
        bl = 9; IMarkupPointer_IsLeftOf(a, b, &bl); P(" %d\n", bl);
        { HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE, 64); WCHAR *w = GlobalLock(g); IMarkupContainer *c2 = NULL; lstrcpyW(w, L"<s>q</s>"); GlobalUnlock(g);
          hr = IMarkupServices_ParseGlobal(ms, g, 0, &c2, NULL, NULL); P("PARSEGLOBAL %08lx %d\n", hr, c2 != NULL); if (c2) IMarkupContainer_Release(c2); GlobalFree(g); }
        SysFreeString(tn); if (e) IHTMLElement_Release(e);
        if (od) IHTMLDocument2_Release(od);
        IMarkupContainer_Release(c); IMarkupPointer_Release(a); IMarkupPointer_Release(b);
    }
    P("DONE\n");
    (void)l;
    return 0;
}
