/* Text Services Framework context properties (patches/sg/1531), run by
 * test/tsfprops-gate.sh. The probe is a text store in a context of its
 * own (the scaffolding of test/tsfsinks-probe.c). In a read/write edit
 * session it asks the context for a property: Windows gives one for any
 * GUID, the same object each time, with values set on spans of the text
 * and read back with GetValue, FindRange and EnumRanges, cut by Clear.
 * GetProperty was E_NOTIMPL, and Word went on with nothing. */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <msctf.h>
#include <textstor.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

/* the sinks this context tells (public TSF interfaces) */
DEFINE_GUID(IID_ITfTextLayoutSink_, 0x2af2d06a, 0xdd5b, 0x4927, 0xa0, 0xb4, 0x54, 0xf1, 0x9c, 0x91, 0xfa, 0xde);
DEFINE_GUID(IID_ITfStatusSink_, 0x6b7d8d73, 0xb267, 0x4f69, 0xb3, 0x2e, 0x1c, 0xa3, 0x21, 0xce, 0x4f, 0x45);
DEFINE_GUID(IID_ITfEditTransactionSink_, 0x708fbf70, 0xb520, 0x416b, 0xb0, 0x6c, 0x2c, 0x41, 0xab, 0x44, 0xf8, 0xba);

static HWND store_hwnd;
static ITextStoreACPSink *acp_sink;
static ITfContext *ctx;
static TfClientId tid;

/* --- a sink object answering any of the three, counting calls --------- */
typedef struct { void *vtbl; LONG ref; } Obj;
static int layout_calls, layout_code = -1, status_calls, status_flags, trans_start, trans_end, endedit_calls;
static BOOL layout_view_ok, endedit_sel;
static LONG endedit_start = -1, endedit_len = -1;

static HRESULT WINAPI unk_QI(IUnknown *iface, REFIID riid, void **ppv) { *ppv = iface; IUnknown_AddRef(iface); return S_OK; }
static ULONG WINAPI unk_AddRef(IUnknown *iface) { return InterlockedIncrement(&((Obj *)iface)->ref); }
static ULONG WINAPI unk_Release(IUnknown *iface) { return InterlockedDecrement(&((Obj *)iface)->ref); }

static HRESULT WINAPI layout_OnLayoutChange(IUnknown *iface, ITfContext *pic, int lcode, ITfContextView *view)
{
    HWND hwnd = NULL;
    RECT rc = {0};
    layout_calls++;
    layout_code = lcode;
    if (view && SUCCEEDED(ITfContextView_GetWnd(view, &hwnd)) && SUCCEEDED(ITfContextView_GetScreenExt(view, &rc)))
        layout_view_ok = hwnd == store_hwnd && rc.left == 10 && rc.bottom == 400;
    return S_OK;
}
static void *layout_vtbl[] = { unk_QI, unk_AddRef, unk_Release, layout_OnLayoutChange };
static Obj layout_obj = { layout_vtbl, 1 };

static HRESULT WINAPI status_OnStatusChange(IUnknown *iface, ITfContext *pic, DWORD flags)
{
    status_calls++;
    status_flags = flags;
    return S_OK;
}
static void *status_vtbl[] = { unk_QI, unk_AddRef, unk_Release, status_OnStatusChange };
static Obj status_obj = { status_vtbl, 1 };

static HRESULT WINAPI trans_Start(IUnknown *iface, ITfContext *pic) { trans_start++; return S_OK; }
static HRESULT WINAPI trans_End(IUnknown *iface, ITfContext *pic) { trans_end++; return S_OK; }
static void *trans_vtbl[] = { unk_QI, unk_AddRef, unk_Release, trans_Start, trans_End };
static Obj trans_obj = { trans_vtbl, 1 };

static HRESULT WINAPI edit_OnEndEdit(ITfTextEditSink *iface, ITfContext *pic, TfEditCookie ec, ITfEditRecord *rec)
{
    IEnumTfRanges *ranges;
    ITfRange *range;
    ITfRangeACP *acp;
    ULONG n = 0;

    endedit_calls++;
    if (!rec) return S_OK;
    ITfEditRecord_GetSelectionStatus(rec, &endedit_sel);
    if (SUCCEEDED(ITfEditRecord_GetTextAndPropertyUpdates(rec, TF_GTP_INCL_TEXT, NULL, 0, &ranges)))
    {
        if (IEnumTfRanges_Next(ranges, 1, &range, &n) == S_OK && n == 1)
        {
            if (SUCCEEDED(ITfRange_QueryInterface(range, &IID_ITfRangeACP, (void **)&acp)))
            {
                ITfRangeACP_GetExtent(acp, &endedit_start, &endedit_len);
                ITfRangeACP_Release(acp);
            }
            ITfRange_Release(range);
        }
        else endedit_start = endedit_len = -1;
        IEnumTfRanges_Release(ranges);
    }
    return S_OK;
}
static void *edit_vtbl[] = { unk_QI, unk_AddRef, unk_Release, edit_OnEndEdit };
static Obj edit_obj = { edit_vtbl, 1 };

/* --- the text store ---------------------------------------------------- */
static HRESULT WINAPI ts_QI(ITextStoreACP *iface, REFIID riid, void **ppv)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ITextStoreACP)) { *ppv = iface; return S_OK; }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI ts_AddRef(ITextStoreACP *iface) { return 2; }
static ULONG WINAPI ts_Release(ITextStoreACP *iface) { return 1; }
static HRESULT WINAPI ts_AdviseSink(ITextStoreACP *iface, REFIID riid, IUnknown *punk, DWORD mask)
{
    if (!acp_sink) IUnknown_QueryInterface(punk, &IID_ITextStoreACPSink, (void **)&acp_sink);
    return S_OK;
}
static HRESULT WINAPI ts_UnadviseSink(ITextStoreACP *iface, IUnknown *punk) { return S_OK; }
static HRESULT WINAPI ts_RequestLock(ITextStoreACP *iface, DWORD flags, HRESULT *hr)
{
    *hr = ITextStoreACPSink_OnLockGranted(acp_sink, flags & TS_LF_READWRITE);
    return S_OK;
}
static HRESULT WINAPI ts_GetStatus(ITextStoreACP *iface, TS_STATUS *st) { st->dwDynamicFlags = 0; st->dwStaticFlags = 0; return S_OK; }
static HRESULT WINAPI ts_QueryInsert(ITextStoreACP *i, LONG a, LONG b, ULONG c, LONG *d, LONG *e) { return E_NOTIMPL; }
static HRESULT WINAPI ts_GetSelection(ITextStoreACP *i, ULONG idx, ULONG count, TS_SELECTION_ACP *sel, ULONG *fetched)
{
    sel->acpStart = 1; sel->acpEnd = 3; sel->style.ase = TS_AE_END; sel->style.fInterimChar = FALSE;
    *fetched = 1;
    return S_OK;
}
static HRESULT WINAPI ts_SetSelection(ITextStoreACP *i, ULONG c, const TS_SELECTION_ACP *s) { return S_OK; }
static const WCHAR doc_text[] = L"Hello world of text stores";
static HRESULT WINAPI ts_GetText(ITextStoreACP *i, LONG start, LONG end, WCHAR *buf, ULONG max, ULONG *got,
                                 TS_RUNINFO *runs, ULONG nruns, ULONG *gotruns, LONG *next)
{
    ULONG len = (end < 0 ? 20 : end) - start;
    if (len > max) len = max;
    memcpy(buf, doc_text + start, len * sizeof(WCHAR));
    *got = len;
    *gotruns = 0;
    *next = start + len;
    return S_OK;
}
static HRESULT WINAPI ts_SetText(ITextStoreACP *i, DWORD a, LONG b, LONG c, const WCHAR *d, ULONG e, TS_TEXTCHANGE *f) { return E_NOTIMPL; }
static HRESULT WINAPI ts_GetFormattedText(ITextStoreACP *i, LONG a, LONG b, IDataObject **c) { return E_NOTIMPL; }
static HRESULT WINAPI ts_GetEmbedded(ITextStoreACP *i, LONG a, REFGUID b, REFIID c, IUnknown **d) { return E_NOTIMPL; }
static HRESULT WINAPI ts_QueryInsertEmbedded(ITextStoreACP *i, const GUID *a, const FORMATETC *b, BOOL *c) { return E_NOTIMPL; }
static HRESULT WINAPI ts_InsertEmbedded(ITextStoreACP *i, DWORD a, LONG b, LONG c, IDataObject *d, TS_TEXTCHANGE *e) { return E_NOTIMPL; }
static HRESULT WINAPI ts_InsertTextAtSelection(ITextStoreACP *i, DWORD a, const WCHAR *b, ULONG c, LONG *d, LONG *e, TS_TEXTCHANGE *f) { return E_NOTIMPL; }
static HRESULT WINAPI ts_InsertEmbeddedAtSelection(ITextStoreACP *i, DWORD a, IDataObject *b, LONG *c, LONG *d, TS_TEXTCHANGE *e) { return E_NOTIMPL; }
static HRESULT WINAPI ts_RequestSupportedAttrs(ITextStoreACP *i, DWORD a, ULONG b, const TS_ATTRID *c) { return E_NOTIMPL; }
static HRESULT WINAPI ts_RequestAttrsAtPosition(ITextStoreACP *i, LONG a, ULONG b, const TS_ATTRID *c, DWORD d) { return E_NOTIMPL; }
static HRESULT WINAPI ts_RequestAttrsTransitioningAtPosition(ITextStoreACP *i, LONG a, ULONG b, const TS_ATTRID *c, DWORD d) { return E_NOTIMPL; }
static HRESULT WINAPI ts_FindNextAttrTransition(ITextStoreACP *i, LONG a, LONG b, ULONG c, const TS_ATTRID *d, DWORD e, LONG *f, BOOL *g, LONG *h) { return E_NOTIMPL; }
static HRESULT WINAPI ts_RetrieveRequestedAttrs(ITextStoreACP *i, ULONG a, TS_ATTRVAL *b, ULONG *c) { return E_NOTIMPL; }
static HRESULT WINAPI ts_GetEndACP(ITextStoreACP *i, LONG *a) { *a = 20; return S_OK; }
static HRESULT WINAPI ts_GetActiveView(ITextStoreACP *i, TsViewCookie *v) { *v = 7; return S_OK; }
static HRESULT WINAPI ts_GetACPFromPoint(ITextStoreACP *i, TsViewCookie v, const POINT *pt, DWORD f, LONG *acp)
{
    *acp = v == 7 ? pt->x / 10 : -1;
    return S_OK;
}
static HRESULT WINAPI ts_GetTextExt(ITextStoreACP *i, TsViewCookie v, LONG a, LONG b, RECT *rc, BOOL *clipped)
{
    SetRect(rc, a * 10, 5, b * 10, 25);
    *clipped = FALSE;
    return v == 7 ? S_OK : E_INVALIDARG;
}
static HRESULT WINAPI ts_GetScreenExt(ITextStoreACP *i, TsViewCookie v, RECT *rc) { SetRect(rc, 10, 20, 300, 400); return S_OK; }
static HRESULT WINAPI ts_GetWnd(ITextStoreACP *i, TsViewCookie v, HWND *h) { *h = store_hwnd; return S_OK; }

static ITextStoreACPVtbl ts_vtbl =
{
    ts_QI, ts_AddRef, ts_Release, ts_AdviseSink, ts_UnadviseSink, ts_RequestLock, ts_GetStatus, ts_QueryInsert,
    ts_GetSelection, ts_SetSelection, ts_GetText, ts_SetText, ts_GetFormattedText, ts_GetEmbedded,
    ts_QueryInsertEmbedded, ts_InsertEmbedded, ts_InsertTextAtSelection, ts_InsertEmbeddedAtSelection,
    ts_RequestSupportedAttrs, ts_RequestAttrsAtPosition, ts_RequestAttrsTransitioningAtPosition,
    ts_FindNextAttrTransition, ts_RetrieveRequestedAttrs, ts_GetEndACP, ts_GetActiveView, ts_GetACPFromPoint,
    ts_GetTextExt, ts_GetScreenExt, ts_GetWnd
};
static ITextStoreACP store = { &ts_vtbl };

/* --- an edit session: properties ------------------------------------- */
DEFINE_GUID(GUID_probe_prop, 0x1531aaaa, 0x1111, 0x2222, 0x33, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66);
static HRESULT get_hr = E_FAIL;
static BOOL same_obj, type_ok, value_ok, empty_ok, find_ok, find_none_ok, enum_ok, clear_ok, context_ok;

static ITfRange *span_range(TfEditCookie ec, LONG start, LONG len)
{
    ITfRange *r = NULL;
    LONG moved;
    if (FAILED(ITfContext_GetStart(ctx, ec, &r))) return NULL;
    ITfRange_ShiftEnd(r, ec, start + len, &moved, NULL);
    ITfRange_ShiftStart(r, ec, start, &moved, NULL);
    return r;
}

static HRESULT WINAPI es_QI(ITfEditSession *iface, REFIID riid, void **ppv) { *ppv = iface; return S_OK; }
static ULONG WINAPI es_AddRef(ITfEditSession *iface) { return 2; }
static ULONG WINAPI es_Release(ITfEditSession *iface) { return 1; }
static HRESULT WINAPI es_DoEditSession(ITfEditSession *iface, TfEditCookie ec)
{
    ITfProperty *prop = NULL, *again = NULL;
    ITfRange *r0_5, *r1_2, *r8_2, *found = NULL;
    ITfContext *owner = NULL;
    IEnumTfRanges *e;
    VARIANT v;
    GUID g;

    get_hr = ITfContext_GetProperty(ctx, &GUID_probe_prop, &prop);
    if (FAILED(get_hr) || !prop) return S_OK;
    ITfContext_GetProperty(ctx, &GUID_probe_prop, &again);
    same_obj = again == prop;
    if (again) ITfProperty_Release(again);
    type_ok = SUCCEEDED(ITfProperty_GetType(prop, &g)) && IsEqualGUID(&g, &GUID_probe_prop);
    context_ok = SUCCEEDED(ITfProperty_GetContext(prop, &owner)) && owner == ctx;
    if (owner) ITfContext_Release(owner);

    r0_5 = span_range(ec, 0, 5);
    r1_2 = span_range(ec, 1, 2);
    r8_2 = span_range(ec, 8, 2);
    V_VT(&v) = VT_I4; V_I4(&v) = 42;
    ITfProperty_SetValue(prop, ec, r0_5, &v);
    VariantInit(&v);
    value_ok = ITfProperty_GetValue(prop, ec, r1_2, &v) == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 42;
    VariantInit(&v);
    empty_ok = ITfProperty_GetValue(prop, ec, r8_2, &v) == S_OK && V_VT(&v) == VT_EMPTY;
    if (ITfProperty_FindRange(prop, ec, r1_2, &found, TF_ANCHOR_START) == S_OK && found)
    {
        ITfRangeACP *acp;
        LONG s = -1, l = -1;
        if (SUCCEEDED(ITfRange_QueryInterface(found, &IID_ITfRangeACP, (void **)&acp)))
        {
            ITfRangeACP_GetExtent(acp, &s, &l);
            ITfRangeACP_Release(acp);
        }
        find_ok = s == 0 && l == 5;
        ITfRange_Release(found);
    }
    found = (ITfRange *)1;
    find_none_ok = ITfProperty_FindRange(prop, ec, r8_2, &found, TF_ANCHOR_START) == S_FALSE && !found;
    if (SUCCEEDED(ITfProperty_EnumRanges(prop, ec, &e, NULL)))
    {
        ITfRange *got[3];
        ULONG n = 0;
        IEnumTfRanges_Next(e, 3, got, &n);
        enum_ok = n == 1;
        while (n) { n--; ITfRange_Release(got[n]); }
        IEnumTfRanges_Release(e);
    }
    ITfProperty_Clear(prop, ec, r1_2);
    VariantInit(&v);
    clear_ok = ITfProperty_GetValue(prop, ec, r1_2, &v) == S_OK && V_VT(&v) == VT_EMPTY;
    if (SUCCEEDED(ITfProperty_EnumRanges(prop, ec, &e, NULL)))
    {
        ITfRange *got[3];
        ULONG n = 0;
        IEnumTfRanges_Next(e, 3, got, &n);
        clear_ok = clear_ok && n == 2;   /* 0-1 and 3-5 are left */
        while (n) { n--; ITfRange_Release(got[n]); }
        IEnumTfRanges_Release(e);
    }
    ITfRange_Release(r0_5); ITfRange_Release(r1_2); ITfRange_Release(r8_2);
    ITfProperty_Release(prop);
    return S_OK;
}
static ITfEditSessionVtbl es_vtbl = { es_QI, es_AddRef, es_Release, es_DoEditSession };
static ITfEditSession session = { &es_vtbl };

int main(void)
{
    ITfThreadMgr *tm;
    ITfDocumentMgr *dm;
    TfEditCookie ec;
    HRESULT hr, hrs;

    CoInitialize(NULL);
    store_hwnd = CreateWindowA("STATIC", "tsf", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    if (FAILED(CoCreateInstance(&CLSID_TF_ThreadMgr, NULL, CLSCTX_INPROC_SERVER, &IID_ITfThreadMgr, (void **)&tm)))
    {
        printf("FAIL  no thread manager\nRESULT: FAIL\n");
        return 1;
    }
    ITfThreadMgr_Activate(tm, &tid);
    ITfThreadMgr_CreateDocumentMgr(tm, &dm);
    hr = ITfDocumentMgr_CreateContext(dm, tid, 0, (IUnknown *)&store, &ctx, &ec);
    ITfDocumentMgr_Push(dm, ctx);
    check(SUCCEEDED(hr) && acp_sink, "a context over the text store");
    hr = ITfContext_RequestEditSession(ctx, tid, &session, TF_ES_SYNC | TF_ES_READWRITE, &hrs);
    printf("  edit session %lx %lx, GetProperty %lx\n", hr, hrs, get_hr);
    check(get_hr == S_OK, "GetProperty gives a property for any GUID");
    check(same_obj, "... the same property the second time");
    check(type_ok && context_ok, "GetType and GetContext");
    check(value_ok, "SetValue on 0-5, GetValue inside it");
    check(empty_ok, "GetValue where nothing is set: VT_EMPTY");
    check(find_ok, "FindRange: the span that holds the range");
    check(find_none_ok, "FindRange where nothing is set: S_FALSE");
    check(enum_ok, "EnumRanges: one span");
    check(clear_ok, "Clear 1-3 splits the span in two");
    ITfDocumentMgr_Pop(dm, TF_POPF_ALL);
    ITfThreadMgr_Deactivate(tm);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
