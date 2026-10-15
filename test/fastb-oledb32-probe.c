/* oledb32 gate: IDataConvert to DBTYPE_BYTES (DataConvert and GetConversionSize) */
#define COBJMACROS
#include <windows.h>
#include <oledb.h>
#include <msdadc.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { failures++; printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const GUID clsid = { 0xc8b522d1, 0x5cf3, 0x11ce, { 0xad, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0x77, 0x3d } };
static const GUID iid = { 0x0c733a8d, 0x2a1c, 0x11ce, { 0xad, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0x77, 0x3d } };

int main(void)
{
    IDataConvert *dc = NULL;
    HRESULT hr;
    DBLENGTH len;
    DBSTATUS st;
    BYTE out[16];
    DBLENGTH n;
    BSTR bstr;
    const BYTE raw[3] = { 1, 2, 3 };

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &iid, (void **)&dc);
    CHECK(hr == S_OK && dc, "create %#lx", hr);
    if (!dc) return 1;

    /* GetConversionSize */
    n = 3; len = 0;
    hr = IDataConvert_GetConversionSize(dc, DBTYPE_BYTES, DBTYPE_BYTES, &n, &len, (void *)raw);
    CHECK(hr == S_OK && len == 3, "BYTES->BYTES size: %#lx %Iu", hr, len);
    hr = IDataConvert_GetConversionSize(dc, DBTYPE_BYTES, DBTYPE_BYTES, NULL, &len, (void *)raw);
    CHECK(hr == E_INVALIDARG, "BYTES->BYTES without a length: %#lx", hr);
    n = 8; len = 0;
    hr = IDataConvert_GetConversionSize(dc, DBTYPE_STR, DBTYPE_BYTES, &n, &len, (void *)"A10B0c0d");
    CHECK(hr == S_OK && len == 4, "STR->BYTES size: %#lx %Iu", hr, len);
    n = 8 * sizeof(WCHAR); len = 0;
    hr = IDataConvert_GetConversionSize(dc, DBTYPE_WSTR, DBTYPE_BYTES, &n, &len, (void *)L"A10B0c0d");
    CHECK(hr == S_OK && len == 4, "WSTR->BYTES size: %#lx %Iu", hr, len);
    bstr = SysAllocString(L"aabbcc");
    len = 0;
    hr = IDataConvert_GetConversionSize(dc, DBTYPE_BSTR, DBTYPE_BYTES, NULL, &len, &bstr);
    CHECK(hr == S_OK && len == 3, "BSTR->BYTES size: %#lx %Iu", hr, len);

    /* DataConvert */
    memset(out, 0xee, sizeof(out));
    n = 3; len = 0; st = 0xff;
    hr = IDataConvert_DataConvert(dc, DBTYPE_BYTES, DBTYPE_BYTES, n, &len, (void *)raw, out, sizeof(out), DBSTATUS_S_OK, &st, 0, 0, 0);
    CHECK(hr == S_OK && len == 3 && st == DBSTATUS_S_OK && !memcmp(out, raw, 3), "BYTES->BYTES %#lx", hr);
    memset(out, 0xee, sizeof(out));
    len = 0; st = 0xff;
    hr = IDataConvert_DataConvert(dc, DBTYPE_STR, DBTYPE_BYTES, 8, &len, (void *)"A10B0c0d", out, sizeof(out), DBSTATUS_S_OK, &st, 0, 0, 0);
    CHECK(hr == S_OK && len == 4 && st == DBSTATUS_S_OK && out[0] == 0xa1 && out[1] == 0x0b && out[2] == 0x0c && out[3] == 0x0d && out[4] == 0xee,
          "STR->BYTES %#lx len %Iu status %lu bytes %02x%02x%02x%02x", hr, len, st, out[0], out[1], out[2], out[3]);
    memset(out, 0xee, sizeof(out));
    len = 0; st = 0xff;
    hr = IDataConvert_DataConvert(dc, DBTYPE_WSTR, DBTYPE_BYTES, 4 * sizeof(WCHAR), &len, (void *)L"ff10", out, sizeof(out), DBSTATUS_S_OK, &st, 0, 0, 0);
    CHECK(hr == S_OK && len == 2 && out[0] == 0xff && out[1] == 0x10, "WSTR->BYTES %#lx", hr);
    len = 0; st = 0xff;
    hr = IDataConvert_DataConvert(dc, DBTYPE_BSTR, DBTYPE_BYTES, 0, &len, &bstr, out, sizeof(out), DBSTATUS_S_OK, &st, 0, 0, 0);
    CHECK(hr == S_OK && len == 3 && out[0] == 0xaa && out[2] == 0xcc, "BSTR->BYTES %#lx", hr);
    len = 0; st = 0xff;
    hr = IDataConvert_DataConvert(dc, DBTYPE_STR, DBTYPE_BYTES, 8, &len, (void *)"A10B0c0d", out, 2, DBSTATUS_S_OK, &st, 0, 0, 0);
    CHECK(hr == S_OK && len == 4 && st == DBSTATUS_S_TRUNCATED, "truncation: %#lx len %Iu status %lu", hr, len, st);
    len = 0; st = 0xff;
    hr = IDataConvert_DataConvert(dc, DBTYPE_STR, DBTYPE_BYTES, 3, &len, (void *)"abc", out, sizeof(out), DBSTATUS_S_OK, &st, 0, 0, 0);
    CHECK(FAILED(hr) && st == DBSTATUS_E_CANTCONVERTVALUE, "odd digit count: %#lx status %lu", hr, st);
    len = 0; st = 0xff;
    hr = IDataConvert_DataConvert(dc, DBTYPE_STR, DBTYPE_BYTES, 4, &len, (void *)"zz00", out, sizeof(out), DBSTATUS_S_OK, &st, 0, 0, 0);
    CHECK(FAILED(hr) && st == DBSTATUS_E_CANTCONVERTVALUE, "non-hex digits: %#lx status %lu", hr, st);

    SysFreeString(bstr);
    IDataConvert_Release(dc);
    CoUninitialize();
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
