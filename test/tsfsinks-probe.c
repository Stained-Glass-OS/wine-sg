/* Text Services Framework context notifications (patches/sg/1661), run by
 * test/tsfsinks-gate.sh. The probe is a text store (ITextStoreACP) in a
 * context of its own, and advises the context's sinks. As the text store
 * tells the context what changed (ITextStoreACPSink), the sinks are told:
 * the layout sink with the view (whose window, screen extent, text extent
 * and range at a point come from the text store), the status sink, the
 * edit transaction sink, and the text edit sink with an edit record of the
 * changed text and the selection. The context's active view, its view
 * enumeration and InWriteSession work. These were stubs (E_NOTIMPL or an
 * S_OK doing nothing). */
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

/* --- an edit session: views, InWriteSession ---------------------------- */
static BOOL in_write, ext_ok, point_ok, range_ok;
static HRESULT WINAPI es_QI(ITfEditSession *iface, REFIID riid, void **ppv) { *ppv = iface; return S_OK; }
static ULONG WINAPI es_AddRef(ITfEditSession *iface) { return 2; }
static ULONG WINAPI es_Release(ITfEditSession *iface) { return 1; }
static HRESULT WINAPI es_DoEditSession(ITfEditSession *iface, TfEditCookie ec)
{
    ITfContextView *view;
    ITfRange *range;
    RECT rc;
    BOOL clipped;
    POINT pt = { 55, 10 };

    ITfContext_InWriteSession(ctx, tid, &in_write);
    if (SUCCEEDED(ITfContext_GetActiveView(ctx, &view)))
    {
        if (SUCCEEDED(ITfContext_GetStart(ctx, ec, &range)))
        {
            LONG moved = 0;
            ITfRange_ShiftEnd(range, ec, 4, &moved, NULL);
            ext_ok = moved == 4 && SUCCEEDED(ITfContextView_GetTextExt(view, ec, range, &rc, &clipped)) && rc.left == 0 && rc.right == 40;
            ITfRange_Release(range);
        }
        if (SUCCEEDED(ITfContext_GetStart(ctx, ec, &range)))
        {
            WCHAR text[16] = {0};
            ULONG got = 0;
            LONG moved = 0, cmp = 9;
            BOOL empty = FALSE, equal = FALSE;
            ITfRange *clone = NULL;
            ITfRange_ShiftEnd(range, ec, 5, &moved, NULL);
            ITfRange_GetText(range, ec, 0, text, 15, &got);
            if (SUCCEEDED(ITfRange_Clone(range, &clone)))
            {
                ITfRange_CompareStart(clone, ec, range, TF_ANCHOR_START, &cmp);
                ITfRange_Collapse(clone, ec, TF_ANCHOR_END);
                ITfRange_IsEmpty(clone, ec, &empty);
                ITfRange_IsEqualStart(clone, ec, range, TF_ANCHOR_END, &equal);
                ITfRange_Release(clone);
            }
            printf("  range text \"%ls\" (%lu) cmp %ld empty %d equal %d\n", text, got, cmp, empty, equal);
            range_ok = got == 5 && !wcscmp(text, L"Hello") && cmp == 0 && empty && equal;
            ITfRange_Release(range);
        }
        if (SUCCEEDED(ITfContextView_GetRangeFromPoint(view, ec, &pt, 0, &range)))
        {
            ITfRangeACP *acp;
            LONG s = -1, l = -1;
            if (SUCCEEDED(ITfRange_QueryInterface(range, &IID_ITfRangeACP, (void **)&acp)))
            {
                ITfRangeACP_GetExtent(acp, &s, &l);
                ITfRangeACP_Release(acp);
            }
            point_ok = s == 5 && l == 0;
            ITfRange_Release(range);
        }
        ITfContextView_Release(view);
    }
    return S_OK;
}
static ITfEditSessionVtbl es_vtbl = { es_QI, es_AddRef, es_Release, es_DoEditSession };
static ITfEditSession session = { &es_vtbl };

int main(void)
{
    ITfThreadMgr *tm;
    ITfDocumentMgr *dm;
    ITfSource *source;
    ITfContextView *view;
    IEnumTfContextViews *views;
    TS_TEXTCHANGE change = { 2, 4, 7 };
    TfEditCookie ec;
    DWORD cookie;
    HRESULT hr, hrs;
    BOOL w = TRUE;
    ULONG n = 0;

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
    ITfContext_QueryInterface(ctx, &IID_ITfSource, (void **)&source);
    check(ITfSource_AdviseSink(source, &IID_ITfTextLayoutSink_, (IUnknown *)&layout_obj, &cookie) == S_OK,
          "AdviseSink(ITfTextLayoutSink)");
    check(ITfSource_AdviseSink(source, &IID_ITfStatusSink_, (IUnknown *)&status_obj, &cookie) == S_OK,
          "AdviseSink(ITfStatusSink)");
    check(ITfSource_AdviseSink(source, &IID_ITfEditTransactionSink_, (IUnknown *)&trans_obj, &cookie) == S_OK,
          "AdviseSink(ITfEditTransactionSink)");
    ITfSource_AdviseSink(source, &IID_ITfTextEditSink, (IUnknown *)&edit_obj, &cookie);

    ITextStoreACPSink_OnLayoutChange(acp_sink, TS_LC_CHANGE, 7);
    check(layout_calls == 1 && layout_code == 1, "OnLayoutChange tells the layout sink (TF_LC_CHANGE)");
    check(layout_view_ok, "... with the view: the text store's window and screen extent");

    ITextStoreACPSink_OnStatusChange(acp_sink, TS_SD_READONLY);
    check(status_calls == 1 && status_flags == TS_SD_READONLY, "OnStatusChange tells the status sink");

    ITextStoreACPSink_OnStartEditTransaction(acp_sink);
    ITextStoreACPSink_OnEndEditTransaction(acp_sink);
    check(trans_start == 1 && trans_end == 1, "edit transactions are told to the transaction sink");

    ITextStoreACPSink_OnTextChange(acp_sink, 0, &change);
    printf("  OnEndEdit calls %d, changed %ld+%ld, selection %d\n", endedit_calls, endedit_start, endedit_len, endedit_sel);
    check(endedit_calls == 1 && endedit_start == 2 && endedit_len == 5 && !endedit_sel,
          "OnTextChange: the text edit sink gets an edit record with the changed range");
    ITextStoreACPSink_OnSelectionChange(acp_sink);
    check(endedit_calls == 2 && endedit_sel && endedit_start == -1, "OnSelectionChange: the record's selection status");

    hr = ITfContext_GetActiveView(ctx, &view);
    check(hr == S_OK && view, "GetActiveView");
    if (view) ITfContextView_Release(view);
    hr = ITfContext_EnumViews(ctx, &views);
    if (hr == S_OK)
    {
        ITfContextView *v[2];
        IEnumTfContextViews_Next(views, 2, v, &n);
        while (n) { n--; ITfContextView_Release(v[n]); }
        IEnumTfContextViews_Reset(views);
        IEnumTfContextViews_Next(views, 2, v, &n);
        check(n == 1, "EnumViews: one view");
        while (n) { n--; ITfContextView_Release(v[n]); }
        IEnumTfContextViews_Release(views);
    }
    else check(0, "EnumViews");

    check(ITfContext_InWriteSession(ctx, tid, &w) == S_OK && !w, "InWriteSession outside a session: FALSE");
    ITextStoreACPSink_OnStatusChange(acp_sink, 0);   /* writable again */
    hr = ITfContext_RequestEditSession(ctx, tid, &session, TF_ES_SYNC | TF_ES_READWRITE, &hrs);
    printf("  edit session %lx %lx\n", hr, hrs);
    check(in_write, "InWriteSession inside a read/write session: TRUE");
    check(ext_ok, "the view's GetTextExt: the text store's extent of the range");
    check(point_ok, "the view's GetRangeFromPoint");
    check(range_ok, "ranges: GetText from the text store, Clone, CompareStart, Collapse, IsEmpty, IsEqualStart");

    ITfSource_Release(source);
    ITfDocumentMgr_Pop(dm, TF_POPF_ALL);
    ITfThreadMgr_Deactivate(tm);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
