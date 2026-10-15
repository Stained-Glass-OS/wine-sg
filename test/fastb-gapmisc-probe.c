/* gap batch probe: inkobj clipboard data object GetDataHere */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static const CLSID CLSID_InkDispP = { 0x937c1a34, 0x151d, 0x4610, { 0x9c, 0xa6, 0xa8, 0xcc, 0x9b, 0xdb, 0x5d, 0x83 } };

static void test_ink(void)
{
    IDispatch *ink;
    DISPID id;
    DISPPARAMS dp = { 0 };
    VARIANT args[3];
    VARIANT res;
    IDataObject *data;
    OLECHAR *name = (OLECHAR *)L"ClipboardCopy";
    HRESULT hr;
    FORMATETC fmt = { 0, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM med, med2;

    hr = CoCreateInstance(&CLSID_InkDispP, NULL, CLSCTX_INPROC_SERVER, &IID_IDispatch, (void **)&ink);
    if (hr != S_OK) { printf("SKIP ink: %#lx\n", hr); return; }
    hr = IDispatch_GetIDsOfNames(ink, &GUID_NULL, &name, 1, 0, &id);
    CHECK(hr == S_OK, "GetIDsOfNames %#lx", hr);
    VariantInit(&res);
    /* arguments are in reverse order: Modes, Formats, Strokes */
    V_VT(&args[0]) = VT_I4; V_I4(&args[0]) = 0;      /* ICB_Copy */
    V_VT(&args[1]) = VT_I4; V_I4(&args[1]) = 0x3ff;  /* ICF_Default */
    V_VT(&args[2]) = VT_DISPATCH; V_DISPATCH(&args[2]) = NULL;
    dp.rgvarg = args;
    dp.cArgs = 3;
    hr = IDispatch_Invoke(ink, id, &GUID_NULL, 0, DISPATCH_METHOD, &dp, &res, NULL, NULL);
    CHECK(hr == S_OK, "ClipboardCopy %#lx", hr);
    if (hr != S_OK) goto done;
    CHECK((V_VT(&res) == VT_UNKNOWN || V_VT(&res) == VT_DISPATCH) && V_UNKNOWN(&res), "result vt %d", V_VT(&res));
    hr = IUnknown_QueryInterface(V_UNKNOWN(&res), &IID_IDataObject, (void **)&data);
    CHECK(hr == S_OK, "QI IDataObject %#lx", hr);
    if (hr != S_OK) goto done;

    fmt.cfFormat = RegisterClipboardFormatW(L"Ink Serialized Format");
    memset(&med, 0, sizeof(med));
    hr = IDataObject_GetData(data, &fmt, &med);
    CHECK(hr == S_OK, "GetData %#lx", hr);
    if (hr == S_OK)
    {
        SIZE_T size = GlobalSize(med.hGlobal);

        /* GetDataHere into a big enough block */
        memset(&med2, 0, sizeof(med2));
        med2.tymed = TYMED_HGLOBAL;
        med2.hGlobal = GlobalAlloc(GMEM_MOVEABLE, size + 16);
        hr = IDataObject_GetDataHere(data, &fmt, &med2);
        CHECK(hr == S_OK, "GetDataHere %#lx", hr);
        if (hr == S_OK)
        {
            BYTE *a = GlobalLock(med.hGlobal), *b = GlobalLock(med2.hGlobal);
            CHECK(!memcmp(a, b, size), "GetDataHere data differs");
            GlobalUnlock(med.hGlobal);
            GlobalUnlock(med2.hGlobal);
        }
        GlobalFree(med2.hGlobal);

        /* too small */
        if (size > 1)
        {
            med2.hGlobal = GlobalAlloc(GMEM_MOVEABLE, size - 1);
            hr = IDataObject_GetDataHere(data, &fmt, &med2);
            CHECK(hr == STG_E_MEDIUMFULL, "small block: %#lx", hr);
            GlobalFree(med2.hGlobal);
        }
        /* wrong medium type */
        med2.tymed = TYMED_ISTREAM;
        hr = IDataObject_GetDataHere(data, &fmt, &med2);
        CHECK(hr == DV_E_TYMED, "stream medium: %#lx", hr);
        ReleaseStgMedium(&med);
    }
    IDataObject_Release(data);
done:
    VariantClear(&res);
    IDispatch_Release(ink);
}

int main(void)
{
    CoInitialize(NULL);
    test_ink();
    CoUninitialize();
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
