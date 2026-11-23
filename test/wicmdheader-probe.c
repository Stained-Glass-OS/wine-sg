/* wicmdheader-probe.c: test/wicmdheader-gate.sh (wine-sg 9501). IWICMetadataWriterInfo::GetHeader returns
 * the header a metadata writer puts in front of its data in a container: the structure followed by the
 * header bytes, sized like GetPatterns.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <initguid.h>
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

static IWICImagingFactory *factory;

static IWICMetadataWriterInfo *writer_info(const CLSID *clsid)
{
    IWICComponentInfo *info;
    IWICMetadataWriterInfo *writer = NULL;
    if (FAILED(IWICImagingFactory_CreateComponentInfo(factory, clsid, &info))) return NULL;
    IWICComponentInfo_QueryInterface(info, &IID_IWICMetadataWriterInfo, (void **)&writer);
    IWICComponentInfo_Release(info);
    return writer;
}

static void check_header(const char *name, const CLSID *clsid, const GUID *container, const void *bytes,
                         UINT length, UINT position, UINT offset)
{
    IWICMetadataWriterInfo *writer = writer_info(clsid);
    BYTE buffer[256];
    WICMetadataHeader *header = (WICMetadataHeader *)buffer;
    UINT actual = 0;
    HRESULT hr;
    char buf[300];

    if (!writer) { sprintf(buf, "%s: writer info", name); check(0, buf); return; }
    hr = IWICMetadataWriterInfo_GetHeader(writer, container, 0, NULL, &actual);
    sprintf(buf, "%s: size query hr %#lx, %u bytes (want %u)", name, hr, actual, (UINT)sizeof(*header) + length);
    check(hr == S_OK && actual == sizeof(*header) + length, buf);
    if (length)
    {
        hr = IWICMetadataWriterInfo_GetHeader(writer, container, actual - 1, header, &actual);
        sprintf(buf, "%s: a buffer one byte short is WINCODEC_ERR_INSUFFICIENTBUFFER (%#lx)", name, hr);
        check(hr == WINCODEC_ERR_INSUFFICIENTBUFFER, buf);
    }
    memset(buffer, 0xcc, sizeof(buffer));
    hr = IWICMetadataWriterInfo_GetHeader(writer, container, sizeof(buffer), header, &actual);
    sprintf(buf, "%s: header hr %#lx length %lu position %lu data offset %lu", name, hr, header->Length,
            header->Position.LowPart, header->DataOffset.LowPart);
    check(hr == S_OK && header->Length == length && header->Position.QuadPart == position &&
          header->DataOffset.QuadPart == offset, buf);
    if (length)
    {
        sprintf(buf, "%s: the header bytes follow the structure and are the format's", name);
        check(header->Header == buffer + sizeof(*header) && !memcmp(header->Header, bytes, length), buf);
    }
    IWICMetadataWriterInfo_Release(writer);
}

int main(void)
{
    IWICMetadataWriterInfo *writer;
    UINT actual;
    HRESULT hr;
    char buf[200];

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory,
                                (void **)&factory)))
    {
        printf("FAIL  no imaging factory\nRESULT: FAIL\n");
        return 1;
    }

    check_header("App1 in JPEG", &CLSID_WICApp1MetadataWriter, &GUID_ContainerFormatJpeg, "Exif\0\0", 6, 0, 0);
    check_header("screen descriptor in GIF", &CLSID_WICLSDMetadataWriter, &GUID_ContainerFormatGif, "GIF89a", 6, 0, 0);
    check_header("graphic control in GIF", &CLSID_WICGCEMetadataWriter, &GUID_ContainerFormatGif, "\x21\xf9\x04", 3, 0, 3);
    check_header("IFD in TIFF", &CLSID_WICIfdMetadataWriter, &GUID_ContainerFormatTiff, NULL, 0, 0, 0);

    writer = writer_info(&CLSID_WICApp1MetadataWriter);
    if (writer)
    {
        hr = IWICMetadataWriterInfo_GetHeader(writer, &GUID_ContainerFormatPng, 0, NULL, &actual);
        sprintf(buf, "a container the writer does not support is WINCODEC_ERR_COMPONENTNOTFOUND (%#lx)", hr);
        check(hr == WINCODEC_ERR_COMPONENTNOTFOUND, buf);
        hr = IWICMetadataWriterInfo_GetHeader(writer, &GUID_ContainerFormatJpeg, 0, NULL, NULL);
        sprintf(buf, "no size pointer is E_INVALIDARG (%#lx)", hr);
        check(hr == E_INVALIDARG, buf);
        hr = IWICMetadataWriterInfo_GetHeader(writer, NULL, 0, NULL, &actual);
        sprintf(buf, "no container is E_INVALIDARG (%#lx)", hr);
        check(hr == E_INVALIDARG, buf);
        IWICMetadataWriterInfo_Release(writer);
    }
    else check(0, "App1 writer info");

    IWICImagingFactory_Release(factory);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
