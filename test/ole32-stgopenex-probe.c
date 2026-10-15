/* StgOpenStorageEx hands back the interface asked for (IID_IStorage, IID_IPropertySetStorage); it used to
 * ignore riid and return the IStorage pointer for any of them, so an IPropertySetStorage call went to the
 * wrong method.  An interface the storage lacks is E_NOINTERFACE, a NULL out pointer STG_E_INVALIDPOINTER. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <propidl.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID fmtid = {0x5ee0e9c1, 0x6001, 0x4c01, {1, 2, 3, 4, 5, 6, 7, 8}};

int main(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    IStorage *stg;
    IPropertySetStorage *pss;
    IPropertyStorage *ps;
    PROPSPEC spec;
    PROPVARIANT value, out;
    HRESULT hr;
    void *obj;
    IUnknown *unk;
    static const struct { DWORD stgfmt; const char *name; } formats[] =
    {
        {STGFMT_DOCFILE, "STGFMT_DOCFILE"}, {STGFMT_STORAGE, "STGFMT_STORAGE"}, {STGFMT_ANY, "STGFMT_ANY"},
    };
    unsigned int i;
    char text[120];

    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%ssg_openex_%lu.stg", dir, GetCurrentProcessId());
    DeleteFileW(path);

    /* a storage with a property set in it */
    hr = StgCreateDocfile(path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    check(hr == S_OK, "create a compound file");
    hr = IStorage_QueryInterface(stg, &IID_IPropertySetStorage, (void **)&pss);
    check(hr == S_OK, "the storage has IPropertySetStorage");
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = 2;
    hr = IPropertySetStorage_Create(pss, &fmtid, NULL, PROPSETFLAG_DEFAULT, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, &ps);
    check(hr == S_OK, "create a property set");
    PropVariantInit(&value);
    value.vt = VT_I4;
    value.lVal = 0x5ee0;
    hr = IPropertyStorage_WriteMultiple(ps, 1, &spec, &value, 2);
    check(hr == S_OK, "write a property");
    IPropertyStorage_Commit(ps, STGC_DEFAULT);
    IPropertyStorage_Release(ps);
    IPropertySetStorage_Release(pss);
    IStorage_Release(stg);

    for (i = 0; i < sizeof(formats) / sizeof(formats[0]); i++)
    {
        /* IPropertySetStorage */
        obj = (void *)0xdeadbeef;
        hr = StgOpenStorageEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, formats[i].stgfmt, 0, NULL, NULL,
                              &IID_IPropertySetStorage, &obj);
        sprintf(text, "%s: IPropertySetStorage is returned (hr %#lx)", formats[i].name, hr);
        check(hr == S_OK, text);
        if (hr == S_OK)
        {
            pss = obj;
            ps = NULL;
            hr = IPropertySetStorage_Open(pss, &fmtid, STGM_READ | STGM_SHARE_EXCLUSIVE, &ps);
            sprintf(text, "%s: the property set opens through it (hr %#lx)", formats[i].name, hr);
            check(hr == S_OK, text);
            if (hr == S_OK)
            {
                PropVariantInit(&out);
                hr = IPropertyStorage_ReadMultiple(ps, 1, &spec, &out);
                sprintf(text, "%s: the property is read back (hr %#lx)", formats[i].name, hr);
                check(hr == S_OK && out.vt == VT_I4 && out.lVal == 0x5ee0, text);
                IPropertyStorage_Release(ps);
            }
            IPropertySetStorage_Release(pss);
        }

        /* IStorage */
        obj = NULL;
        hr = StgOpenStorageEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, formats[i].stgfmt, 0, NULL, NULL, &IID_IStorage, &obj);
        sprintf(text, "%s: IStorage is returned (hr %#lx)", formats[i].name, hr);
        check(hr == S_OK && obj, text);
        if (hr == S_OK)
        {
            STATSTG st;
            stg = obj;
            hr = IStorage_Stat(stg, &st, STATFLAG_NONAME);
            check(hr == S_OK && st.type == STGTY_STORAGE, "it is a working IStorage");
            IStorage_Release(stg);
        }
    }

    /* the other requests */
    obj = (void *)0xdeadbeef;
    hr = StgOpenStorageEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, STGFMT_DOCFILE, 0, NULL, NULL, &IID_IUnknown, &obj);
    check(hr == S_OK && obj, "IID_IUnknown");
    if (hr == S_OK)
    {
        unk = obj;
        IUnknown_Release(unk);
    }
    obj = (void *)0xdeadbeef;
    hr = StgOpenStorageEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, STGFMT_DOCFILE, 0, NULL, NULL, &IID_IStream, &obj);
    check(hr == E_NOINTERFACE && !obj, "IID_IStream is E_NOINTERFACE with a NULL result");
    hr = StgOpenStorageEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, STGFMT_DOCFILE, 0, NULL, NULL, &IID_IStorage, NULL);
    check(hr == STG_E_INVALIDPOINTER, "a NULL out pointer is STG_E_INVALIDPOINTER");
    obj = (void *)0xdeadbeef;
    hr = StgOpenStorageEx(L"Z:\\no\\such\\file.stg", STGM_READ | STGM_SHARE_DENY_WRITE, STGFMT_DOCFILE, 0, NULL, NULL,
                          &IID_IPropertySetStorage, &obj);
    check(FAILED(hr) && !obj, "a missing file fails with a NULL result");
    obj = (void *)0xdeadbeef;
    hr = StgOpenStorageEx(path, STGM_READ | STGM_SHARE_DENY_WRITE, STGFMT_FILE, 0, NULL, NULL, &IID_IStorage, &obj);
    check(hr == STG_E_INVALIDPARAMETER, "STGFMT_FILE is STG_E_INVALIDPARAMETER");

    DeleteFileW(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
