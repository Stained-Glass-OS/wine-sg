/* ole32 odds and ends (patches/sg/1673), run by test/ole32misc-gate.sh:
 * OBJREF monikers (display name and MkParseDisplayName back, Save and
 * Load, binding); OleGetIconOfClass and OleGetIconOfFile (icon and label
 * pictures); OleRegEnumFormatEtc (a class's registered formats by
 * direction); OleQueryLinkFromData; property set enumerators' Skip and
 * Clone; OleInitializeWOW. These were stubs. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

HRESULT WINAPI OleInitializeWOW(DWORD, DWORD);
static const CLSID CLSID_ObjrefMoniker = {0x00000327,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const CLSID CLSID_Probe = {0x5347aaaa,0x1673,0x4c00,{1,2,3,4,5,6,7,8}};
static const FMTID fmtid_probe = {0x5347bbbb,0x1673,0x4c00,{1,2,3,4,5,6,7,8}};

/* a data object that offers what it is told */
struct data { IDataObject iface; CLIPFORMAT offered; DWORD tymed; };
static HRESULT WINAPI d_qi(IDataObject *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI d_addref(IDataObject *iface) { return 2; }
static ULONG WINAPI d_release(IDataObject *iface) { return 1; }
static HRESULT WINAPI d_get(IDataObject *iface, FORMATETC *f, STGMEDIUM *m) { return DV_E_FORMATETC; }
static HRESULT WINAPI d_gethere(IDataObject *iface, FORMATETC *f, STGMEDIUM *m) { return E_NOTIMPL; }
static HRESULT WINAPI d_query(IDataObject *iface, FORMATETC *f)
{
    struct data *d = (struct data *)iface;
    return f->cfFormat == d->offered && (f->tymed & d->tymed) ? S_OK : DV_E_FORMATETC;
}
static HRESULT WINAPI d_canon(IDataObject *iface, FORMATETC *in, FORMATETC *out) { return E_NOTIMPL; }
static HRESULT WINAPI d_set(IDataObject *iface, FORMATETC *f, STGMEDIUM *m, BOOL r) { return E_NOTIMPL; }
static HRESULT WINAPI d_enum(IDataObject *iface, DWORD dir, IEnumFORMATETC **e) { return E_NOTIMPL; }
static HRESULT WINAPI d_dadvise(IDataObject *iface, FORMATETC *f, DWORD a, IAdviseSink *s, DWORD *c) { return E_NOTIMPL; }
static HRESULT WINAPI d_dunadvise(IDataObject *iface, DWORD c) { return E_NOTIMPL; }
static HRESULT WINAPI d_enumadvise(IDataObject *iface, IEnumSTATDATA **e) { return E_NOTIMPL; }
static IDataObjectVtbl d_vtbl = { d_qi, d_addref, d_release, d_get, d_gethere, d_query, d_canon, d_set, d_enum,
                                  d_dadvise, d_dunadvise, d_enumadvise };

/* does a metafile picture draw this text? */
static BOOL picture_has_text(HGLOBAL hpict, const char *text)
{
    METAFILEPICT *mfp;
    BYTE *bits;
    UINT size, i;
    BOOL found = FALSE;

    if (!hpict || !(mfp = GlobalLock(hpict))) return FALSE;
    size = GetMetaFileBitsEx(mfp->hMF, 0, NULL);
    if ((bits = malloc(size)) && GetMetaFileBitsEx(mfp->hMF, size, bits))
        for (i = 0; i + strlen(text) <= size && !found; i++)
            found = !memcmp(bits + i, text, strlen(text));
    free(bits);
    GlobalUnlock(hpict);
    return found;
}

static void set_reg(const WCHAR *key, const WCHAR *name, const WCHAR *value)
{
    HKEY h;
    RegCreateKeyW(HKEY_CLASSES_ROOT, key, &h);
    RegSetValueExW(h, name, 0, REG_SZ, (const BYTE *)value, (lstrlenW(value) + 1) * sizeof(WCHAR));
    RegCloseKey(h);
}

int main(void)
{
    IMoniker *moniker, *parsed, *loaded;
    IBindCtx *bc;
    IUnknown *object, *bound;
    WCHAR *name, path[MAX_PATH], clsid_str[64], key[256];
    ULONG eaten;
    IStream *stream;
    HGLOBAL pict;
    IEnumFORMATETC *formats;
    FORMATETC fmt[4];
    ULONG got;
    struct data data = { { &d_vtbl } };
    IStorage *stg;
    IPropertySetStorage *pss;
    IPropertyStorage *ps;
    IEnumSTATPROPSTG *props, *props2;
    IEnumSTATPROPSETSTG *sets, *sets2;
    STATPROPSTG stat;
    STATPROPSETSTG setstat;
    PROPSPEC spec[3];
    PROPVARIANT var[3];
    HANDLE file;
    HRESULT hr;
    int i;

    hr = OleInitializeWOW(0, 0);
    check(SUCCEEDED(hr), "OleInitializeWOW initializes OLE");

    /* OBJREF monikers */
    CreateStreamOnHGlobal(NULL, TRUE, (IStream **)&object);
    CreateObjrefMoniker(object, &moniker);
    CreateBindCtx(0, &bc);
    hr = IMoniker_GetDisplayName(moniker, bc, NULL, &name);
    printf("      GetDisplayName %08lx\n", hr);
    check(hr == S_OK && !wcsncmp(name, L"objref:", 7) && name[lstrlenW(name) - 1] == ':', "an objref: display name");
    hr = MkParseDisplayName(bc, name, &eaten, &parsed);
    check(hr == S_OK && eaten == lstrlenW(name), "MkParseDisplayName takes it back");
    if (hr == S_OK)
    {
        bound = NULL;
        hr = IMoniker_BindToObject(parsed, bc, NULL, &IID_IUnknown, (void **)&bound);
        check(hr == S_OK && bound == object, "and binds to the object");
        if (bound) IUnknown_Release(bound);
        IMoniker_Release(parsed);
    }
    CoTaskMemFree(name);
    CreateStreamOnHGlobal(NULL, TRUE, &stream);
    check(IMoniker_Save(moniker, stream, TRUE) == S_OK, "Save");
    IStream_Seek(stream, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
    hr = CoCreateInstance(&CLSID_ObjrefMoniker, NULL, CLSCTX_INPROC_SERVER, &IID_IMoniker, (void **)&loaded);
    if (FAILED(hr)) hr = CreateObjrefMoniker(NULL, &loaded);
    check(hr == S_OK && IMoniker_Load(loaded, stream) == S_OK, "Load");
    bound = NULL;
    check(IMoniker_BindToObject(loaded, bc, NULL, &IID_IUnknown, (void **)&bound) == S_OK && bound == object,
          "the loaded moniker binds to the object");
    if (bound) IUnknown_Release(bound);
    IMoniker_Release(loaded);
    IStream_Release(stream);
    IMoniker_Release(moniker);
    IBindCtx_Release(bc);

    /* a class to describe */
    StringFromGUID2(&CLSID_Probe, clsid_str, 64);
    swprintf(key, 256, L"CLSID\\%ls", clsid_str);
    set_reg(key, NULL, L"Probe Thing Full");
    swprintf(key, 256, L"CLSID\\%ls\\AuxUserType\\2", clsid_str);
    set_reg(key, NULL, L"ProbeThing");
    swprintf(key, 256, L"CLSID\\%ls\\DefaultIcon", clsid_str);
    set_reg(key, NULL, L"shell32.dll,3");
    swprintf(key, 256, L"CLSID\\%ls\\DataFormats\\GetSet", clsid_str);
    set_reg(key, L"0", L"1,1,1,3");
    set_reg(key, L"1", L"Rich Text Format,1,1,1");
    set_reg(key, L"2", L"2,1,16,2");

    pict = OleGetIconOfClass(&CLSID_Probe, NULL, TRUE);
    check(pict != NULL, "OleGetIconOfClass");
    check(picture_has_text(pict, "ProbeThing"), "labelled with the short type name");
    pict = OleGetIconOfClass(&CLSID_Probe, (LPOLESTR)L"My Label", FALSE);
    check(picture_has_text(pict, "My Label"), "or with the label given");

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"oleicon-probe.txt");
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    CloseHandle(file);
    pict = OleGetIconOfFile(path, TRUE);
    check(pict != NULL && picture_has_text(pict, "oleicon-probe.txt"), "OleGetIconOfFile, labelled with the file");
    DeleteFileW(path);

    hr = OleRegEnumFormatEtc(&CLSID_Probe, DATADIR_GET, &formats);
    got = 0;
    if (hr == S_OK) IEnumFORMATETC_Next(formats, 4, fmt, &got);
    check(hr == S_OK && got == 2 && fmt[0].cfFormat == CF_TEXT &&
          fmt[1].cfFormat == RegisterClipboardFormatW(L"Rich Text Format"), "OleRegEnumFormatEtc: the get formats");
    if (hr == S_OK) IEnumFORMATETC_Release(formats);
    hr = OleRegEnumFormatEtc(&CLSID_Probe, DATADIR_SET, &formats);
    got = 0;
    if (hr == S_OK) IEnumFORMATETC_Next(formats, 4, fmt, &got);
    check(hr == S_OK && got == 2 && fmt[1].cfFormat == CF_BITMAP && fmt[1].tymed == TYMED_GDI, "and the set formats");
    if (hr == S_OK) IEnumFORMATETC_Release(formats);
    check(OleRegEnumFormatEtc(&fmtid_probe, DATADIR_GET, &formats) == REGDB_E_KEYMISSING,
          "a class without formats: REGDB_E_KEYMISSING");
    swprintf(key, 256, L"CLSID\\%ls", clsid_str);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, key);

    /* OleQueryLinkFromData */
    data.offered = RegisterClipboardFormatW(L"Link Source");
    data.tymed = TYMED_ISTREAM;
    check(OleQueryLinkFromData(&data.iface) == S_OK, "OleQueryLinkFromData: a link source");
    data.offered = CF_TEXT;
    check(OleQueryLinkFromData(&data.iface) == S_FALSE, "and text only: S_FALSE");

    /* property set enumerators */
    StgCreateDocfile(NULL, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE | STGM_DELETEONRELEASE, 0, &stg);
    IStorage_QueryInterface(stg, &IID_IPropertySetStorage, (void **)&pss);
    IPropertySetStorage_Create(pss, &fmtid_probe, NULL, PROPSETFLAG_DEFAULT, STGM_CREATE | STGM_READWRITE |
                               STGM_SHARE_EXCLUSIVE, &ps);
    for (i = 0; i < 3; i++)
    {
        spec[i].ulKind = PRSPEC_PROPID;
        spec[i].propid = 2 + i;
        var[i].vt = VT_I4;
        var[i].lVal = 10 + i;
    }
    IPropertyStorage_WriteMultiple(ps, 3, spec, var, 2);
    IPropertyStorage_Enum(ps, &props);
    check(IEnumSTATPROPSTG_Skip(props, 1) == S_OK, "IEnumSTATPROPSTG::Skip");
    memset(&stat, 0, sizeof(stat));
    IEnumSTATPROPSTG_Next(props, 1, &stat, NULL);
    check(stat.propid == 3, "skipped to the second property");
    check(IEnumSTATPROPSTG_Clone(props, &props2) == S_OK, "IEnumSTATPROPSTG::Clone");
    {
        STATPROPSTG next = { 0 };
        /* the order is the storage's own; the clone and the original go on alike */
        memset(&stat, 0, sizeof(stat));
        IEnumSTATPROPSTG_Next(props2, 1, &stat, NULL);
        IEnumSTATPROPSTG_Next(props, 1, &next, NULL);
        check(stat.propid >= 2 && stat.propid <= 4 && stat.propid != 3 && stat.propid == next.propid,
              "the clone goes on from there");
    }
    check(IEnumSTATPROPSTG_Skip(props2, 5) == S_FALSE, "skipping past the end: S_FALSE");
    IEnumSTATPROPSTG_Release(props2);
    IEnumSTATPROPSTG_Release(props);
    IPropertyStorage_Release(ps);
    IPropertySetStorage_Create(pss, &FMTID_SummaryInformation, NULL, PROPSETFLAG_DEFAULT, STGM_CREATE |
                               STGM_READWRITE | STGM_SHARE_EXCLUSIVE, &ps);
    IPropertyStorage_Release(ps);
    IPropertySetStorage_Enum(pss, &sets);
    check(IEnumSTATPROPSETSTG_Skip(sets, 1) == S_OK && IEnumSTATPROPSETSTG_Clone(sets, &sets2) == S_OK,
          "IEnumSTATPROPSETSTG::Skip and Clone");
    got = 0;
    IEnumSTATPROPSETSTG_Next(sets2, 1, &setstat, &got);
    check(got == 1 && IEnumSTATPROPSETSTG_Skip(sets2, 1) == S_FALSE, "the clone has the second set, then none");
    IEnumSTATPROPSETSTG_Release(sets2);
    IEnumSTATPROPSETSTG_Release(sets);
    IPropertySetStorage_Release(pss);
    IStorage_Release(stg);

    OleUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
