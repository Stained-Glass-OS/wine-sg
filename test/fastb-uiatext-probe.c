/* uiautomationcore, fast mode (patch 3513): the TextPattern_* and TextRange_* client exports against an
 * in-process text provider ("Hello, wide world" as one document). */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <uiautomation.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const WCHAR doc_text[] = L"Hello, wide world";
#define DOC_LEN 17

struct range { ITextRangeProvider ITextRangeProvider_iface; LONG ref; int start, end; };
struct doc { IRawElementProviderSimple simple; ITextProvider text; LONG ref; };
static struct doc the_doc;
static int ranges_alive;

static struct range *impl_range(ITextRangeProvider *i) { return (struct range *)i; }

static HRESULT WINAPI r_QI(ITextRangeProvider *i, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ITextRangeProvider)) { *out = i; ITextRangeProvider_AddRef(i); return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI r_AddRef(ITextRangeProvider *i) { return InterlockedIncrement(&impl_range(i)->ref); }
static ULONG WINAPI r_Release(ITextRangeProvider *i)
{
    LONG r = InterlockedDecrement(&impl_range(i)->ref);
    if (!r) { InterlockedDecrement((LONG *)&ranges_alive); HeapFree(GetProcessHeap(), 0, i); }
    return r;
}
static ITextRangeProvider *new_range(int start, int end)
{
    struct range *r = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*r));
    extern const ITextRangeProviderVtbl range_vtbl;
    r->ITextRangeProvider_iface.lpVtbl = (void *)&range_vtbl;
    r->ref = 1; r->start = start; r->end = end;
    InterlockedIncrement((LONG *)&ranges_alive);
    return &r->ITextRangeProvider_iface;
}
static HRESULT WINAPI r_Clone(ITextRangeProvider *i, ITextRangeProvider **out)
{ *out = new_range(impl_range(i)->start, impl_range(i)->end); return S_OK; }
static HRESULT WINAPI r_Compare(ITextRangeProvider *i, ITextRangeProvider *o, BOOL *out)
{ *out = impl_range(i)->start == impl_range(o)->start && impl_range(i)->end == impl_range(o)->end; return S_OK; }
static HRESULT WINAPI r_CompareEndpoints(ITextRangeProvider *i, enum TextPatternRangeEndpoint e, ITextRangeProvider *o,
        enum TextPatternRangeEndpoint oe, int *out)
{
    int a = e == TextPatternRangeEndpoint_Start ? impl_range(i)->start : impl_range(i)->end;
    int b = oe == TextPatternRangeEndpoint_Start ? impl_range(o)->start : impl_range(o)->end;
    *out = a - b; return S_OK;
}
static HRESULT WINAPI r_Expand(ITextRangeProvider *i, enum TextUnit u)
{ impl_range(i)->start = 0; impl_range(i)->end = DOC_LEN; return S_OK; }
static HRESULT WINAPI r_FindAttribute(ITextRangeProvider *i, TEXTATTRIBUTEID id, VARIANT v, BOOL b, ITextRangeProvider **out)
{ *out = NULL; return S_OK; }
static HRESULT WINAPI r_FindText(ITextRangeProvider *i, BSTR text, BOOL back, BOOL ic, ITextRangeProvider **out)
{
    int len = lstrlenW(text), s;
    *out = NULL;
    for (s = impl_range(i)->start; s + len <= impl_range(i)->end; s++)
        if (!wcsncmp(doc_text + s, text, len)) { *out = new_range(s, s + len); break; }
    return S_OK;
}
static HRESULT WINAPI r_GetAttributeValue(ITextRangeProvider *i, TEXTATTRIBUTEID id, VARIANT *v)
{ V_VT(v) = VT_I4; V_I4(v) = id + 1000; return S_OK; }
static HRESULT WINAPI r_GetBoundingRectangles(ITextRangeProvider *i, SAFEARRAY **out)
{
    double rc[4] = { 1, 2, 3, 4 }; LONG k;
    *out = SafeArrayCreateVector(VT_R8, 0, 4);
    for (k = 0; k < 4; k++) SafeArrayPutElement(*out, &k, &rc[k]);
    return S_OK;
}
static HRESULT WINAPI r_GetEnclosingElement(ITextRangeProvider *i, IRawElementProviderSimple **out)
{ *out = &the_doc.simple; IRawElementProviderSimple_AddRef(*out); return S_OK; }
static HRESULT WINAPI r_GetText(ITextRangeProvider *i, int max, BSTR *out)
{
    int len = impl_range(i)->end - impl_range(i)->start;
    if (max >= 0 && len > max) len = max;
    *out = SysAllocStringLen(doc_text + impl_range(i)->start, len); return S_OK;
}
static HRESULT WINAPI r_Move(ITextRangeProvider *i, enum TextUnit u, int count, int *out)
{
    struct range *r = impl_range(i);
    int len = r->end - r->start, ns = r->start + count;
    if (ns < 0) ns = 0;
    if (ns + len > DOC_LEN) ns = DOC_LEN - len;
    *out = ns - r->start; r->start = ns; r->end = ns + len; return S_OK;
}
static HRESULT WINAPI r_MoveEndpointByUnit(ITextRangeProvider *i, enum TextPatternRangeEndpoint e, enum TextUnit u, int count, int *out)
{
    struct range *r = impl_range(i);
    int *p = e == TextPatternRangeEndpoint_Start ? &r->start : &r->end, np = *p + count;
    if (np < 0) np = 0;
    if (np > DOC_LEN) np = DOC_LEN;
    *out = np - *p; *p = np;
    if (r->start > r->end) { if (e == TextPatternRangeEndpoint_Start) r->end = r->start; else r->start = r->end; }
    return S_OK;
}
static HRESULT WINAPI r_MoveEndpointByRange(ITextRangeProvider *i, enum TextPatternRangeEndpoint e, ITextRangeProvider *o, enum TextPatternRangeEndpoint oe)
{
    struct range *r = impl_range(i);
    int v = oe == TextPatternRangeEndpoint_Start ? impl_range(o)->start : impl_range(o)->end;
    if (e == TextPatternRangeEndpoint_Start) r->start = v; else r->end = v;
    return S_OK;
}
static int sel_start = -1, sel_end = -1, scroll_top = -1;
static HRESULT WINAPI r_Select2(ITextRangeProvider *i) { sel_start = impl_range(i)->start; sel_end = impl_range(i)->end; return S_OK; }
static HRESULT WINAPI r_AddToSelection(ITextRangeProvider *i) { return S_OK; }
static HRESULT WINAPI r_RemoveFromSelection(ITextRangeProvider *i) { return S_OK; }
static HRESULT WINAPI r_ScrollIntoView(ITextRangeProvider *i, BOOL top) { scroll_top = top; return S_OK; }
static HRESULT WINAPI r_GetChildren(ITextRangeProvider *i, SAFEARRAY **out)
{
    VARIANT v; LONG k = 0;
    *out = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)&the_doc.simple; IUnknown_AddRef(V_UNKNOWN(&v));
    SafeArrayPutElement(*out, &k, &v); VariantClear(&v);
    return S_OK;
}
const ITextRangeProviderVtbl range_vtbl = {
    r_QI, r_AddRef, r_Release, r_Clone, r_Compare, r_CompareEndpoints, r_Expand, r_FindAttribute, r_FindText,
    r_GetAttributeValue, r_GetBoundingRectangles, r_GetEnclosingElement, r_GetText, r_Move, r_MoveEndpointByUnit,
    r_MoveEndpointByRange, r_Select2, r_AddToSelection, r_RemoveFromSelection, r_ScrollIntoView, r_GetChildren };

/* the document: a simple provider that has the text pattern */
static HRESULT WINAPI s_QI(IRawElementProviderSimple *i, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IRawElementProviderSimple)) { *out = i; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI s_AddRef(IRawElementProviderSimple *i) { return 2; }
static ULONG WINAPI s_Release(IRawElementProviderSimple *i) { return 1; }
static HRESULT WINAPI s_Opts(IRawElementProviderSimple *i, enum ProviderOptions *o) { *o = ProviderOptions_ServerSideProvider; return S_OK; }
static HRESULT WINAPI s_GetPattern(IRawElementProviderSimple *i, PATTERNID id, IUnknown **out)
{
    *out = NULL;
    if (id == UIA_TextPatternId) *out = (IUnknown *)&the_doc.text;
    return S_OK;
}
static HRESULT WINAPI s_GetProp(IRawElementProviderSimple *i, PROPERTYID id, VARIANT *v) { VariantInit(v); return S_OK; }
static HRESULT WINAPI s_Host(IRawElementProviderSimple *i, IRawElementProviderSimple **o) { *o = NULL; return S_OK; }
static const IRawElementProviderSimpleVtbl simple_vtbl = { s_QI, s_AddRef, s_Release, s_Opts, s_GetPattern, s_GetProp, s_Host };

static HRESULT WINAPI t_QI(ITextProvider *i, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ITextProvider)) { *out = i; return S_OK; }
    *out = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI t_AddRef(ITextProvider *i) { return 2; }
static ULONG WINAPI t_Release(ITextProvider *i) { return 1; }
static HRESULT ranges_array(SAFEARRAY **out, int start, int end)
{
    VARIANT v; LONG k = 0;
    *out = SafeArrayCreateVector(VT_VARIANT, 0, 1);
    V_VT(&v) = VT_UNKNOWN; V_UNKNOWN(&v) = (IUnknown *)new_range(start, end);
    SafeArrayPutElement(*out, &k, &v); VariantClear(&v);
    return S_OK;
}
static HRESULT WINAPI t_GetSelection(ITextProvider *i, SAFEARRAY **out) { return ranges_array(out, 7, 11); }
static HRESULT WINAPI t_GetVisible(ITextProvider *i, SAFEARRAY **out) { return ranges_array(out, 0, DOC_LEN); }
static HRESULT WINAPI t_FromChild(ITextProvider *i, IRawElementProviderSimple *c, ITextRangeProvider **out)
{ *out = new_range(2, 4); return S_OK; }
static HRESULT WINAPI t_FromPoint(ITextProvider *i, struct UiaPoint pt, ITextRangeProvider **out)
{ *out = new_range((int)pt.x, (int)pt.y); return S_OK; }
static HRESULT WINAPI t_Doc(ITextProvider *i, ITextRangeProvider **out) { *out = new_range(0, DOC_LEN); return S_OK; }
static HRESULT WINAPI t_Sel(ITextProvider *i, enum SupportedTextSelection *out) { *out = SupportedTextSelection_Multiple; return S_OK; }
static const ITextProviderVtbl text_vtbl = { t_QI, t_AddRef, t_Release, t_GetSelection, t_GetVisible, t_FromChild, t_FromPoint, t_Doc, t_Sel };

typedef HRESULT (WINAPI *pf_get_pattern)(HUIANODE, PATTERNID, HUIAPATTERNOBJECT *);
typedef HRESULT (WINAPI *pf_node_from_provider)(IRawElementProviderSimple *, HUIANODE *);

int main(void)
{
    HMODULE m;
    pf_get_pattern get_pattern;
    pf_node_from_provider node_from_provider;
    HRESULT (WINAPI *doc_range)(HUIAPATTERNOBJECT, HUIATEXTRANGE *);
    HRESULT (WINAPI *sup_sel)(HUIAPATTERNOBJECT, enum SupportedTextSelection *);
    HRESULT (WINAPI *get_sel)(HUIAPATTERNOBJECT, SAFEARRAY **);
    HRESULT (WINAPI *get_vis)(HUIAPATTERNOBJECT, SAFEARRAY **);
    HRESULT (WINAPI *from_child)(HUIAPATTERNOBJECT, HUIANODE, HUIATEXTRANGE *);
    HRESULT (WINAPI *from_point)(HUIAPATTERNOBJECT, struct UiaPoint, HUIATEXTRANGE *);
    BOOL (WINAPI *range_release)(HUIATEXTRANGE);
    HRESULT (WINAPI *range_from_variant)(VARIANT *, HUIATEXTRANGE *);
    HRESULT (WINAPI *r_clone)(HUIATEXTRANGE, HUIATEXTRANGE *);
    HRESULT (WINAPI *r_compare)(HUIATEXTRANGE, HUIATEXTRANGE, BOOL *);
    HRESULT (WINAPI *r_cmpend)(HUIATEXTRANGE, enum TextPatternRangeEndpoint, HUIATEXTRANGE, enum TextPatternRangeEndpoint, int *);
    HRESULT (WINAPI *r_expand)(HUIATEXTRANGE, enum TextUnit);
    HRESULT (WINAPI *r_attr)(HUIATEXTRANGE, TEXTATTRIBUTEID, VARIANT *);
    HRESULT (WINAPI *r_findtext)(HUIATEXTRANGE, BSTR, BOOL, BOOL, HUIATEXTRANGE *);
    HRESULT (WINAPI *r_rects)(HUIATEXTRANGE, SAFEARRAY **);
    HRESULT (WINAPI *r_encl)(HUIATEXTRANGE, HUIANODE *);
    HRESULT (WINAPI *r_text)(HUIATEXTRANGE, int, BSTR *);
    HRESULT (WINAPI *r_move)(HUIATEXTRANGE, enum TextUnit, int, int *);
    HRESULT (WINAPI *r_moveu)(HUIATEXTRANGE, enum TextPatternRangeEndpoint, enum TextUnit, int, int *);
    HRESULT (WINAPI *r_mover)(HUIATEXTRANGE, enum TextPatternRangeEndpoint, HUIATEXTRANGE, enum TextPatternRangeEndpoint);
    HRESULT (WINAPI *r_select)(HUIATEXTRANGE);
    HRESULT (WINAPI *r_scroll)(HUIATEXTRANGE, BOOL);
    HRESULT (WINAPI *r_children)(HUIATEXTRANGE, SAFEARRAY **);
    HUIANODE node = NULL;
    HUIAPATTERNOBJECT pat = NULL;
    HUIATEXTRANGE doc, r1, r2;
    HRESULT hr;
    BSTR text;
    BOOL same;
    int n;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    the_doc.simple.lpVtbl = (void *)&simple_vtbl;
    the_doc.text.lpVtbl = (void *)&text_vtbl;
    m = LoadLibraryA("uiautomationcore.dll");
#define GP(var, name) var = (void *)GetProcAddress(m, name); check(var != NULL, name " is exported")
    GP(get_pattern, "UiaGetPatternProvider");
    GP(node_from_provider, "UiaNodeFromProvider");
    GP(doc_range, "TextPattern_get_DocumentRange"); GP(sup_sel, "TextPattern_get_SupportedTextSelection");
    GP(get_sel, "TextPattern_GetSelection"); GP(get_vis, "TextPattern_GetVisibleRanges");
    GP(from_child, "TextPattern_RangeFromChild"); GP(from_point, "TextPattern_RangeFromPoint");
    GP(range_release, "UiaTextRangeRelease"); GP(range_from_variant, "UiaHTextRangeFromVariant");
    GP(r_clone, "TextRange_Clone"); GP(r_compare, "TextRange_Compare"); GP(r_cmpend, "TextRange_CompareEndpoints");
    GP(r_expand, "TextRange_ExpandToEnclosingUnit"); GP(r_attr, "TextRange_GetAttributeValue");
    GP(r_findtext, "TextRange_FindText"); GP(r_rects, "TextRange_GetBoundingRectangles");
    GP(r_encl, "TextRange_GetEnclosingElement"); GP(r_text, "TextRange_GetText"); GP(r_move, "TextRange_Move");
    GP(r_moveu, "TextRange_MoveEndpointByUnit"); GP(r_mover, "TextRange_MoveEndpointByRange");
    GP(r_select, "TextRange_Select"); GP(r_scroll, "TextRange_ScrollIntoView"); GP(r_children, "TextRange_GetChildren");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    hr = node_from_provider(&the_doc.simple, &node);
    check(hr == S_OK && node, "node from the text provider");
    hr = get_pattern(node, UIA_TextPatternId, &pat);
    check(hr == S_OK && pat, "text pattern object");
    if (!pat) { printf("RESULT: FAIL\n"); return 1; }

    { enum SupportedTextSelection sel = 0; hr = sup_sel(pat, &sel); check(hr == S_OK && sel == SupportedTextSelection_Multiple, "supported selection"); }
    hr = doc_range(pat, &doc);
    check(hr == S_OK && doc, "document range");
    text = NULL; hr = r_text(doc, -1, &text);
    check(hr == S_OK && text && !wcscmp(text, doc_text), "document text");
    SysFreeString(text);
    text = NULL; hr = r_text(doc, 5, &text);
    check(hr == S_OK && text && !wcscmp(text, L"Hello"), "text limited to 5");
    SysFreeString(text);

    hr = r_clone(doc, &r1);
    check(hr == S_OK && r1 && r1 != doc, "clone");
    same = FALSE; hr = r_compare(doc, r1, &same);
    check(hr == S_OK && same, "clone compares equal");
    n = 0; hr = r_moveu(r1, TextPatternRangeEndpoint_End, TextUnit_Character, -12, &n);
    check(hr == S_OK && n == -12, "MoveEndpointByUnit moved 12 back");
    text = NULL; r_text(r1, -1, &text);
    check(text && !wcscmp(text, L"Hello"), "the clone is now Hello");
    SysFreeString(text);
    same = TRUE; r_compare(doc, r1, &same);
    check(!same, "and differs from the document");
    n = 99; hr = r_cmpend(doc, TextPatternRangeEndpoint_End, r1, TextPatternRangeEndpoint_End, &n);
    check(hr == S_OK && n == 12, "CompareEndpoints (17 - 5)");
    n = 0; hr = r_move(r1, TextUnit_Character, 7, &n);
    check(hr == S_OK && n == 7, "Move by 7");
    text = NULL; r_text(r1, -1, &text);
    check(text && !wcscmp(text, L"wide "), "Hello moved to the word 'wide '");
    SysFreeString(text);
    hr = r_mover(r1, TextPatternRangeEndpoint_Start, doc, TextPatternRangeEndpoint_Start);
    check(hr == S_OK, "MoveEndpointByRange");
    text = NULL; r_text(r1, -1, &text);
    check(text && !wcscmp(text, L"Hello, wide "), "start moved to the document's");
    SysFreeString(text);
    hr = r_expand(r1, TextUnit_Document);
    text = NULL; r_text(r1, -1, &text);
    check(hr == S_OK && text && !wcscmp(text, doc_text), "ExpandToEnclosingUnit");
    SysFreeString(text);

    { BSTR needle = SysAllocString(L"wide"); hr = r_findtext(doc, needle, FALSE, FALSE, &r2); SysFreeString(needle); }
    check(hr == S_OK && r2, "FindText finds a range");
    text = NULL; if (r2) r_text(r2, -1, &text);
    check(text && !wcscmp(text, L"wide"), "that is the word");
    SysFreeString(text);
    if (r2) { range_release(r2); r2 = NULL; }
    { BSTR needle = SysAllocString(L"absent"); r2 = (HUIATEXTRANGE)1; hr = r_findtext(doc, needle, FALSE, FALSE, &r2); SysFreeString(needle); }
    check(hr == S_OK && r2 == NULL, "FindText of absent text gives no range");

    { VARIANT v; VariantInit(&v); hr = r_attr(doc, 40005, &v);
      check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 40005 + 1000, "GetAttributeValue passes the value through"); }
    { SAFEARRAY *sa = NULL; double d = 0; LONG k = 2; hr = r_rects(doc, &sa);
      check(hr == S_OK && sa && SUCCEEDED(SafeArrayGetElement(sa, &k, &d)) && d == 3.0, "bounding rectangles");
      if (sa) SafeArrayDestroy(sa); }
    { HUIANODE enc = NULL; hr = r_encl(doc, &enc); check(hr == S_OK && enc, "enclosing element is a node"); if (enc) UiaNodeRelease(enc); }
    hr = r_select(r1);
    check(hr == S_OK && sel_start == 0 && sel_end == DOC_LEN, "Select reached the provider's range");
    hr = r_scroll(doc, TRUE);
    check(hr == S_OK && scroll_top == 1, "ScrollIntoView");
    { SAFEARRAY *sa = NULL; hr = r_children(doc, &sa); check(hr == S_OK && sa, "children"); if (sa) SafeArrayDestroy(sa); }

    { SAFEARRAY *sa = NULL; VARIANT v; LONG k = 0; HUIATEXTRANGE sel = NULL;
      hr = get_sel(pat, &sa);
      check(hr == S_OK && sa, "GetSelection");
      if (sa)
      {
          VariantInit(&v);
          SafeArrayGetElement(sa, &k, &v);
          hr = range_from_variant(&v, &sel);
          check(hr == S_OK && sel, "the array holds a range handle");
          text = NULL; if (sel) r_text(sel, -1, &text);
          check(text && !wcscmp(text, L"wide"), "the selection is 'wide'");
          SysFreeString(text);
          if (sel) range_release(sel);
          SafeArrayDestroy(sa);
      }
      sa = NULL; hr = get_vis(pat, &sa); check(hr == S_OK && sa, "GetVisibleRanges");
      if (sa)
      {
          HUIATEXTRANGE vis = NULL;
          VariantInit(&v); SafeArrayGetElement(sa, &k, &v); range_from_variant(&v, &vis);
          if (vis) range_release(vis);
          SafeArrayDestroy(sa);
      } }
    hr = from_point(pat, (struct UiaPoint){ 3, 6 }, &r2);
    text = NULL; if (hr == S_OK && r2) r_text(r2, -1, &text);
    check(text && !wcscmp(text, L"lo,"), "RangeFromPoint passes the point");
    SysFreeString(text);
    if (r2) range_release(r2);
    hr = from_child(pat, node, &r2);
    text = NULL; if (hr == S_OK && r2) r_text(r2, -1, &text);
    check(text && !wcscmp(text, L"ll"), "RangeFromChild reaches the provider");
    SysFreeString(text);
    if (r2) range_release(r2);

    check(!range_release(NULL), "releasing NULL fails");
    range_release(r1); range_release(doc);
    check(ranges_alive == 0, "every provider range was released");
    { HUIATEXTRANGE bad = NULL; VARIANT v; VariantInit(&v); hr = range_from_variant(&v, &bad); check(hr == E_INVALIDARG, "bad variant"); }

    UiaPatternRelease(pat);
    UiaNodeRelease(node);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
