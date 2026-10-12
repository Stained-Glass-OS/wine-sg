#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <mlang.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

DEFINE_GUID(IID_IMultiLanguage3, 0x4e5868ab,0xb157,0x4623,0x9a,0xcc,0x6a,0x1d,0x9c,0xae,0xbe,0x04);

int main(void)
{
    IMultiLanguage3 *ml; IEnumCodePage *e, *e2; MIMECPINFO info, info2; ULONG got;
    DetectEncodingInfo det[4]; INT n, size; UINT out[4], nout; HRESULT hr;
    static const char utf8[] = "caf\xc3\xa9 \xe2\x82\xac";
    static const WCHAR ascii[] = L"hello";

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_CMultiLanguage, NULL, CLSCTX_INPROC_SERVER, &IID_IMultiLanguage3, (void **)&ml);
    CHECK(hr == S_OK, "create %lx", hr);
    if (hr != S_OK) return 1;

    IMultiLanguage3_EnumCodePages(ml, MIMECONTF_MIME_LATEST, 0, &e);
    IEnumCodePage_Next(e, 1, &info, &got);
    hr = IEnumCodePage_Clone(e, &e2); CHECK(hr == S_OK, "clone %lx", hr);
    IEnumCodePage_Next(e, 1, &info, &got);
    IEnumCodePage_Next(e2, 1, &info2, &got);
    CHECK(info.uiCodePage == info2.uiCodePage, "clone position");
    IEnumCodePage_Release(e2); IEnumCodePage_Release(e);

    size = sizeof(utf8) - 1; n = 4;
    hr = IMultiLanguage3_DetectInputCodepage(ml, 0, 1252, (char *)utf8, &size, det, &n);
    CHECK(hr == S_OK && n >= 1 && det[0].nCodePage == 65001, "detect %lx n=%d cp=%u", hr, n, det[0].nCodePage);
    size = 5; n = 4;
    hr = IMultiLanguage3_DetectInputCodepage(ml, 0, 0, (char *)"hello", &size, det, &n);
    CHECK(hr == S_OK && n >= 1 && det[0].nCodePage == 20127, "detect ascii %lx cp=%u", hr, det[0].nCodePage);

    nout = 4;
    hr = IMultiLanguage3_DetectOutboundCodePage(ml, 0, ascii, 5, NULL, 0, out, &nout, NULL);
    CHECK(hr == S_OK && nout == 3 && out[0] == 65001 && out[1] == 65000 && out[2] == 1200, "outbound %lx n=%u", hr, nout);
    hr = IMultiLanguage3_ConvertStringReset(ml); CHECK(hr == S_OK, "reset %lx", hr);

    IMultiLanguage3_Release(ml);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
