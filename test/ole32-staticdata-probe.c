/* OleCreateStaticFromData with OLERENDER_DRAW (the data object chooses the picture format) and the
 * render options the docs call an error (OLERENDER_NONE, OLERENDER_ASIS); they used to be passed on to
 * OleCreateFromData, which makes a different object. */
#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID CLSID_PicDib = {0x00000316, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID CLSID_PicEmf = {0x00000319, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

/* a data object offering a configurable set of formats */
struct data_object
{
    IDataObject IDataObject_iface;
    BOOL has_dib, has_emf, has_text;
    LONG get_calls;
};
static struct data_object *impl(IDataObject *i) { return CONTAINING_RECORD(i, struct data_object, IDataObject_iface); }
static HRESULT WINAPI d_QI(IDataObject *i, REFIID r, void **p)
{
    if (IsEqualIID(r, &IID_IUnknown) || IsEqualIID(r, &IID_IDataObject)) { *p = i; return S_OK; }
    *p = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI d_AddRef(IDataObject *i) { return 2; }
static ULONG WINAPI d_Release(IDataObject *i) { return 1; }
static HRESULT WINAPI d_QueryGetData(IDataObject *i, FORMATETC *f)
{
    struct data_object *d = impl(i);
    if (f->cfFormat == CF_DIB && d->has_dib && f->tymed & TYMED_HGLOBAL) return S_OK;
    if (f->cfFormat == CF_ENHMETAFILE && d->has_emf && f->tymed & TYMED_ENHMF) return S_OK;
    if (f->cfFormat == CF_TEXT && d->has_text && f->tymed & TYMED_HGLOBAL) return S_OK;
    return DV_E_FORMATETC;
}
static HRESULT WINAPI d_GetData(IDataObject *i, FORMATETC *f, STGMEDIUM *m)
{
    struct data_object *d = impl(i);
    HRESULT hr = d_QueryGetData(i, f);

    if (hr != S_OK) return hr;
    d->get_calls++;
    memset(m, 0, sizeof(*m));
    if (f->cfFormat == CF_DIB)
    {
        BITMAPINFOHEADER *h;
        m->tymed = TYMED_HGLOBAL;
        m->hGlobal = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(*h) + 16);
        h = GlobalLock(m->hGlobal);
        h->biSize = sizeof(*h); h->biWidth = 2; h->biHeight = 2; h->biPlanes = 1; h->biBitCount = 24;
        GlobalUnlock(m->hGlobal);
    }
    else if (f->cfFormat == CF_ENHMETAFILE)
    {
        HDC dc = CreateEnhMetaFileA(NULL, NULL, NULL, NULL);
        Rectangle(dc, 0, 0, 5, 5);
        m->tymed = TYMED_ENHMF;
        m->hEnhMetaFile = CloseEnhMetaFile(dc);
    }
    else
    {
        m->tymed = TYMED_HGLOBAL;
        m->hGlobal = GlobalAlloc(GMEM_MOVEABLE, 8);
    }
    return S_OK;
}
static HRESULT WINAPI d_GetDataHere(IDataObject *i, FORMATETC *f, STGMEDIUM *m) { return E_NOTIMPL; }
static HRESULT WINAPI d_GetCanonicalFormatEtc(IDataObject *i, FORMATETC *a, FORMATETC *b) { return E_NOTIMPL; }
static HRESULT WINAPI d_SetData(IDataObject *i, FORMATETC *f, STGMEDIUM *m, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI d_EnumFormatEtc(IDataObject *i, DWORD d, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI d_DAdvise(IDataObject *i, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI d_DUnadvise(IDataObject *i, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI d_EnumDAdvise(IDataObject *i, IEnumSTATDATA **e) { return E_NOTIMPL; }
static const IDataObjectVtbl d_vtbl =
{
    d_QI, d_AddRef, d_Release, d_GetData, d_GetDataHere, d_QueryGetData, d_GetCanonicalFormatEtc, d_SetData,
    d_EnumFormatEtc, d_DAdvise, d_DUnadvise, d_EnumDAdvise,
};

int main(void)
{
    static const struct
    {
        BOOL dib, emf, text;
        DWORD renderopt;
        HRESULT hr;
        const GUID *clsid;
        const char *name;
    } tests[] =
    {
        {1, 1, 0, OLERENDER_DRAW, S_OK, &CLSID_PicDib, "DRAW with a DIB and an EMF: the DIB"},
        {0, 1, 0, OLERENDER_DRAW, S_OK, &CLSID_PicEmf, "DRAW with only an EMF"},
        {1, 0, 0, OLERENDER_DRAW, S_OK, &CLSID_PicDib, "DRAW with only a DIB"},
        {0, 0, 1, OLERENDER_DRAW, DV_E_CLIPFORMAT, NULL, "DRAW with only text"},
        {0, 0, 0, OLERENDER_DRAW, DV_E_CLIPFORMAT, NULL, "DRAW with nothing"},
        {1, 1, 0, OLERENDER_NONE, E_INVALIDARG, NULL, "NONE is an error"},
        {1, 1, 0, OLERENDER_ASIS, E_INVALIDARG, NULL, "ASIS is an error"},
        {1, 1, 0, 9, E_INVALIDARG, NULL, "a render option out of range"},
    };
    struct data_object data;
    FORMATETC dib_fmt = { CF_DIB, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    unsigned int i;
    char text[160];

    OleInitialize(NULL);
    data.IDataObject_iface.lpVtbl = &d_vtbl;

    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        IStorage *stg;
        IOleObject *obj = (IOleObject *)0xdeadbeef;
        IPersist *persist;
        HRESULT hr;
        CLSID clsid;

        data.has_dib = tests[i].dib; data.has_emf = tests[i].emf; data.has_text = tests[i].text; data.get_calls = 0;
        StgCreateDocfile(NULL, STGM_READWRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg);
        hr = OleCreateStaticFromData(&data.IDataObject_iface, &IID_IOleObject, tests[i].renderopt,
                                     tests[i].renderopt == OLERENDER_DRAW ? NULL : &dib_fmt, NULL, stg, (void **)&obj);
        sprintf(text, "%s (hr %#lx)", tests[i].name, hr);
        check(hr == tests[i].hr, text);
        if (hr == S_OK)
        {
            memset(&clsid, 0, sizeof(clsid));
            IOleObject_QueryInterface(obj, &IID_IPersist, (void **)&persist);
            IPersist_GetClassID(persist, &clsid);
            check(IsEqualGUID(&clsid, tests[i].clsid), "  the object has the class of the format");
            {
                STATSTG st;
                IStream *stream;
                IStorage_Stat(stg, &st, STATFLAG_NONAME);
                check(IsEqualGUID(&st.clsid, tests[i].clsid), "  the storage has it too");
                check(IStorage_OpenStream(stg, L"CONTENTS", NULL, STGM_READ | STGM_SHARE_EXCLUSIVE, 0, &stream) == S_OK,
                      "  the picture is saved in CONTENTS");
                if (stream) IStream_Release(stream);
            }
            IPersist_Release(persist);
            IOleObject_Release(obj);
        }
        IStorage_Release(stg);
    }

    OleUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
