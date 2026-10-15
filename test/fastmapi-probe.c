#include <windows.h>
#include <stdio.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapitags.h>
#include <mapicode.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", #c, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

typedef SCODE (WINAPI *ALLOC)(ULONG, LPVOID *);
static ALLOC pAlloc;
static SCODE (WINAPI *pFree)(LPVOID);
static SCODE (WINAPI *pMore)(ULONG, LPVOID, LPVOID *);
static HRESULT (WINAPI *pQueryAll)(LPMAPITABLE, LPSPropTagArray, LPSRestriction, LPSSortOrderSet, LONG, LPSRowSet *);
static void (WINAPI *pFreeProws)(LPSRowSet);
static const GUID IID_PropData = { 0x0002031a, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static HRESULT (WINAPI *pCreateIProp)(LPCIID, ALLOCATEBUFFER *, ALLOCATEMORE *, FREEBUFFER *, LPVOID, LPPROPDATA *);

/* a table of 70 rows; remembers what was asked of it */
typedef struct { IMAPITable iface; ULONG next, nrows, setcols, restrict_, sort, seek, maxq; } mock_table;
static mock_table *mt(IMAPITable *i) { return (mock_table *)i; }
static HRESULT WINAPI t_QI(IMAPITable *t, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG WINAPI t_AddRef(IMAPITable *t) { return 2; }
static ULONG WINAPI t_Release(IMAPITable *t) { return 1; }
static HRESULT WINAPI t_GetLastError(IMAPITable *t, HRESULT h, ULONG f, LPMAPIERROR *e) { return E_NOTIMPL; }
static HRESULT WINAPI t_Advise(IMAPITable *t, ULONG m, LPMAPIADVISESINK s, ULONG *c) { return E_NOTIMPL; }
static HRESULT WINAPI t_Unadvise(IMAPITable *t, ULONG c) { return E_NOTIMPL; }
static HRESULT WINAPI t_GetStatus(IMAPITable *t, ULONG *s, ULONG *tt) { return E_NOTIMPL; }
static HRESULT WINAPI t_SetColumns(IMAPITable *t, LPSPropTagArray c, ULONG f) { mt(t)->setcols++; return S_OK; }
static HRESULT WINAPI t_QueryColumns(IMAPITable *t, ULONG f, LPSPropTagArray *c) { return E_NOTIMPL; }
static HRESULT WINAPI t_GetRowCount(IMAPITable *t, ULONG f, ULONG *c) { return E_NOTIMPL; }
static HRESULT WINAPI t_SeekRow(IMAPITable *t, BOOKMARK b, LONG n, LONG *r) { mt(t)->seek++; mt(t)->next = 0; return b == BOOKMARK_BEGINNING ? S_OK : E_FAIL; }
static HRESULT WINAPI t_SeekRowApprox(IMAPITable *t, ULONG n, ULONG d) { return E_NOTIMPL; }
static HRESULT WINAPI t_QueryPosition(IMAPITable *t, ULONG *r, ULONG *n, ULONG *d) { return E_NOTIMPL; }
static HRESULT WINAPI t_FindRow(IMAPITable *t, LPSRestriction r, BOOKMARK b, ULONG f) { return E_NOTIMPL; }
static HRESULT WINAPI t_Restrict(IMAPITable *t, LPSRestriction r, ULONG f) { mt(t)->restrict_++; return S_OK; }
static HRESULT WINAPI t_CreateBookmark(IMAPITable *t, BOOKMARK *b) { return E_NOTIMPL; }
static HRESULT WINAPI t_FreeBookmark(IMAPITable *t, BOOKMARK b) { return E_NOTIMPL; }
static HRESULT WINAPI t_SortTable(IMAPITable *t, LPSSortOrderSet s, ULONG f) { mt(t)->sort++; return S_OK; }
static HRESULT WINAPI t_QuerySortOrder(IMAPITable *t, LPSSortOrderSet *s) { return E_NOTIMPL; }
static HRESULT WINAPI t_QueryRows(IMAPITable *t, LONG count, ULONG flags, LPSRowSet *rows)
{
    mock_table *m = mt(t);
    SRowSet *set;
    ULONG n = m->nrows - m->next, i;

    if ((ULONG)count < n) n = count;
    if (n > m->maxq) m->maxq = n;
    pAlloc(CbNewSRowSet(n), (void **)&set);
    set->cRows = n;
    for (i = 0; i < n; i++)
    {
        SPropValue *p;
        pAlloc(sizeof(*p), (void **)&p);
        p->ulPropTag = PR_ROWID;
        p->dwAlignPad = 0;
        p->Value.l = m->next + i;
        set->aRow[i].cValues = 1;
        set->aRow[i].ulAdrEntryPad = 0;
        set->aRow[i].lpProps = p;
    }
    m->next += n;
    *rows = set;
    return S_OK;
}
static HRESULT WINAPI t_Abort(IMAPITable *t) { return E_NOTIMPL; }
static HRESULT WINAPI t_ExpandRow(IMAPITable *t, ULONG a, LPBYTE b, ULONG c, ULONG d, LPSRowSet *e, ULONG *f) { return E_NOTIMPL; }
static HRESULT WINAPI t_CollapseRow(IMAPITable *t, ULONG a, LPBYTE b, ULONG c, ULONG *d) { return E_NOTIMPL; }
static HRESULT WINAPI t_WaitForCompletion(IMAPITable *t, ULONG a, ULONG b, ULONG *c) { return E_NOTIMPL; }
static HRESULT WINAPI t_GetCollapseState(IMAPITable *t, ULONG a, ULONG b, LPBYTE c, ULONG *d, LPBYTE *e) { return E_NOTIMPL; }
static HRESULT WINAPI t_SetCollapseState(IMAPITable *t, ULONG a, ULONG b, LPBYTE c, BOOKMARK *d) { return E_NOTIMPL; }

static IMAPITableVtbl tvtbl = {
    t_QI, t_AddRef, t_Release, t_GetLastError, t_Advise, t_Unadvise, t_GetStatus, t_SetColumns, t_QueryColumns,
    t_GetRowCount, t_SeekRow, t_SeekRowApprox, t_QueryPosition, t_FindRow, t_Restrict, t_CreateBookmark,
    t_FreeBookmark, t_SortTable, t_QuerySortOrder, t_QueryRows, t_Abort, t_ExpandRow, t_CollapseRow,
    t_WaitForCompletion, t_GetCollapseState, t_SetCollapseState,
};

int main(void)
{
    HMODULE m = LoadLibraryA("mapi32.dll");
    IMAPITable *table;
    mock_table mock = { { &tvtbl }, 0, 70 };
    SRowSet *rows;
    SPropTagArray cols = { 1, { PR_ROWID } };
    SSortOrderSet sort = { 0 };
    SRestriction restr = { 0 };
    HRESULT hr;
    ULONG i;

    pAlloc = (ALLOC)GetProcAddress(m, "MAPIAllocateBuffer@8");
    pFree = (void *)GetProcAddress(m, "MAPIFreeBuffer@4");
    pMore = (void *)GetProcAddress(m, "MAPIAllocateMore@12");
    pQueryAll = (void *)GetProcAddress(m, "HrQueryAllRows@24");
    pCreateIProp = (void *)GetProcAddress(m, "CreateIProp@24");
    pFreeProws = (void *)GetProcAddress(m, "FreeProws@4");
    if (!pAlloc || !pQueryAll || !pCreateIProp) { printf("FAIL exports\nRESULT: FAIL\n"); return 1; }
    table = &mock.iface;

    /* HrQueryAllRows */
    hr = pQueryAll(table, &cols, &restr, &sort, 0, &rows);
    CHECK(hr == S_OK && rows && rows->cRows == 70, "all rows hr %lx rows %lu", hr, rows ? rows->cRows : 0);
    CHECK(mock.setcols == 1 && mock.restrict_ == 1 && mock.sort == 1 && mock.seek == 1, "table calls %lu %lu %lu %lu",
          mock.setcols, mock.restrict_, mock.sort, mock.seek);
    if (hr == S_OK && rows)
    {
        for (i = 0; i < rows->cRows; i++)
            if (rows->aRow[i].cValues != 1 || rows->aRow[i].lpProps[0].Value.l != (LONG)i) { CHECK(0, "row %lu content", i); break; }
        pFreeProws(rows);
    }
    memset(&mock.setcols, 0, sizeof(ULONG) * 5);
    hr = pQueryAll(table, NULL, NULL, NULL, 40, &rows);
    CHECK(hr == S_OK && rows && rows->cRows == 40, "limit 40: hr %lx rows %lu", hr, rows ? rows->cRows : 0);
    CHECK(mock.setcols == 0 && mock.restrict_ == 0 && mock.sort == 0, "optional calls skipped");
    if (hr == S_OK && rows) pFreeProws(rows);
    mock.nrows = 0;
    hr = pQueryAll(table, NULL, NULL, NULL, 0, &rows);
    CHECK(hr == S_OK && rows && rows->cRows == 0, "empty table hr %lx", hr);
    if (hr == S_OK && rows) pFreeProws(rows);
    rows = (SRowSet *)1;
    hr = pQueryAll(NULL, NULL, NULL, NULL, 0, &rows);
    CHECK(hr == MAPI_E_INVALID_PARAMETER && !rows, "null table %lx", hr);

    /* IPropData string width */
    {
        IPropData *pd = NULL;
        SPropValue v[2], *got = NULL;
        SPropTagArray *list = NULL, req;
        ULONG cnt;

        hr = pCreateIProp(&IID_PropData, (void *)pAlloc, (void *)pMore, (void *)pFree, NULL, &pd);
        CHECK(hr == S_OK, "CreateIProp %lx", hr);
        if (hr == S_OK)
        {
            v[0].ulPropTag = PROP_TAG(PT_STRING8, 0x3001); v[0].dwAlignPad = 0; v[0].Value.lpszA = (char *)"abc";
            v[1].ulPropTag = PROP_TAG(PT_UNICODE, 0x3002); v[1].dwAlignPad = 0; v[1].Value.lpszW = (WCHAR *)L"xyz";
            hr = pd->lpVtbl->SetProps(pd, 2, v, NULL);
            CHECK(hr == S_OK, "SetProps %lx", hr);

            req.cValues = 1; req.aulPropTag[0] = PROP_TAG(PT_UNSPECIFIED, 0x3001);
            cnt = 0;
            hr = pd->lpVtbl->GetProps(pd, &req, 0, &cnt, &got);
            CHECK(hr == S_OK && cnt == 1 && PROP_TYPE(got[0].ulPropTag) == PT_STRING8 && !strcmp(got[0].Value.lpszA, "abc"), "ansi->ansi %lx", hr);
            if (got) pFree(got);
            got = NULL; cnt = 0;
            hr = pd->lpVtbl->GetProps(pd, &req, MAPI_UNICODE, &cnt, &got);
            CHECK(hr == S_OK && cnt == 1 && PROP_TYPE(got[0].ulPropTag) == PT_UNICODE && !lstrcmpW(got[0].Value.lpszW, L"abc"), "ansi->unicode %lx tag %lx", hr, got ? got[0].ulPropTag : 0);
            if (got) pFree(got);
            req.aulPropTag[0] = PROP_TAG(PT_UNSPECIFIED, 0x3002);
            got = NULL; cnt = 0;
            hr = pd->lpVtbl->GetProps(pd, &req, 0, &cnt, &got);
            CHECK(hr == S_OK && cnt == 1 && PROP_TYPE(got[0].ulPropTag) == PT_STRING8 && !strcmp(got[0].Value.lpszA, "xyz"), "unicode->ansi %lx tag %lx", hr, got ? got[0].ulPropTag : 0);
            if (got) pFree(got);

            hr = pd->lpVtbl->GetPropList(pd, 0, &list);
            CHECK(hr == S_OK && list && list->cValues == 2 && PROP_TYPE(list->aulPropTag[0]) == PT_STRING8 &&
                  PROP_TYPE(list->aulPropTag[1]) == PT_STRING8, "list ansi %lx", hr);
            if (list) pFree(list);
            list = NULL;
            hr = pd->lpVtbl->GetPropList(pd, MAPI_UNICODE, &list);
            CHECK(hr == S_OK && list && PROP_TYPE(list->aulPropTag[0]) == PT_UNICODE &&
                  PROP_TYPE(list->aulPropTag[1]) == PT_UNICODE, "list unicode %lx", hr);
            if (list) pFree(list);
            pd->lpVtbl->Release(pd);
        }
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
