/* windowscodecs (patches/sg/2651): the metadata readers of the PNG tEXt and gAMA chunks have no handler info
 * (WINCODEC_ERR_COMPONENTNOTFOUND), they still know their format; the other PNG readers have a handler info
 * (the Wine tests record Windows). */
#define INITGUID
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <stdio.h>
#include <stdarg.h>

static int failures;
static const GUID zero_guid = {0};

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}


int main(void)
{
    static const struct { const CLSID *clsid; HRESULT want; const char *name; } tests[] =
    {
        { &CLSID_WICPngTextMetadataReader, WINCODEC_ERR_COMPONENTNOTFOUND, "tEXt reader" },
        { &CLSID_WICPngGamaMetadataReader, WINCODEC_ERR_COMPONENTNOTFOUND, "gAMA reader" },
        { &CLSID_WICPngChrmMetadataReader, S_OK, "cHRM reader" },
    };
    unsigned int i;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
    {
        IWICMetadataReader *reader = NULL;
        IWICMetadataHandlerInfo *info = NULL;
        GUID format = {0};
        HRESULT hr;

        hr = CoCreateInstance(tests[i].clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IWICMetadataReader, (void **)&reader);
        check(hr == S_OK, "%s created (%#lx)", tests[i].name, hr);
        if (!reader) continue;
        hr = IWICMetadataReader_GetMetadataHandlerInfo(reader, &info);
        check(hr == tests[i].want, "%s: handler info %#lx (want %#lx)", tests[i].name, hr, tests[i].want);
        if (info) IWICMetadataHandlerInfo_Release(info);
        hr = IWICMetadataReader_GetMetadataFormat(reader, &format);
        check(hr == S_OK && !IsEqualGUID(&format, &zero_guid), "%s: it knows its format (%#lx)", tests[i].name, hr);
        IWICMetadataReader_Release(reader);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
