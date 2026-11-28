/* wiascan-probe: scanning through WIA, as NAPS2, Paint.NET and scripts do it.
 *
 *   wiascan-probe list    the WIA 1 and WIA 2 device managers (out of process) list the scanners
 *   wiascan-probe scan1   WIA 1: the device's image item, its properties and a banded transfer (DIB)
 *   wiascan-probe file1   WIA 1: idtGetData into a PNG file
 *   wiascan-probe scan2   WIA 2: the Flatbed item and IWiaTransfer::Download into a stream (BMP file)
 *   wiascan-probe feeder1 WIA 1: the feeder selected on the root, a page per transfer until it is empty
 *   wiascan-probe feeder2 WIA 2: the Feeder item, a page per stream until it is empty
 *   wiascan-probe props   property attributes, checked writes, the property enumerator
 *   wiascan-probe dlg-select|dlg-cancel|dlg-scan1|dlg-scan2   the dialogs (driven by wiadlg-gate.sh)
 *
 * Prints PASS/FAIL lines and RESULT: PASS|FAIL.
 */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <ole2.h>
#include <stdio.h>
#include <string.h>
#include "wia_lh.h"

#ifndef WIA_IPA_ITEM_CATEGORY
#define WIA_IPA_ITEM_CATEGORY 4125
#endif
#ifndef WIA_IPS_PAGES
#define WIA_IPS_PAGES 3096
#endif
#ifndef WIA_TRANSFER_MSG_END_OF_STREAM
#define WIA_TRANSFER_MSG_STATUS 1
#define WIA_TRANSFER_MSG_END_OF_STREAM 2
#define WIA_TRANSFER_MSG_END_OF_TRANSFER 3
#endif
#ifndef WIA_IPS_MAX_HORIZONTAL_SIZE
#define WIA_IPS_MAX_HORIZONTAL_SIZE 6165
#endif
#define P_DEV_ID 2
#define P_VEND_DESC 3
#define P_DEV_DESC 4
#define P_DEV_TYPE 5
#define P_DEV_NAME 7
#define P_DPS_DOCUMENT_HANDLING_CAPABILITIES 3086
#define P_DPS_DOCUMENT_HANDLING_SELECT 3088
#define P_DPS_HORIZONTAL_BED_SIZE 3074
#define P_ITEM_NAME 4098
#define P_FULL_ITEM_NAME 4099
#define P_ITEM_FLAGS 4101
#define P_DATATYPE 4103
#define P_DEPTH 4104
#define P_FORMAT 4106
#define P_TYMED 4108
#define P_PIXELS_PER_LINE 4112
#define P_NUMBER_OF_LINES 4114
#define P_XRES 6147
#define P_YRES 6148
#define P_XPOS 6149
#define P_YPOS 6150
#define P_XEXTENT 6151
#define P_YEXTENT 6152

DEFINE_GUID(FMT_BMP, 0xb96b3cab,0x0728,0x11d3,0x9d,0x7b,0x00,0x00,0xf8,0x1e,0xf3,0x2e);
DEFINE_GUID(FMT_MEMORYBMP, 0xb96b3caa,0x0728,0x11d3,0x9d,0x7b,0x00,0x00,0xf8,0x1e,0xf3,0x2e);
DEFINE_GUID(FMT_PNG, 0xb96b3caf,0x0728,0x11d3,0x9d,0x7b,0x00,0x00,0xf8,0x1e,0xf3,0x2e);
DEFINE_GUID(CAT_FLATBED, 0xfb607b1f,0x43f3,0x488b,0x85,0x5b,0xfb,0x70,0x3e,0xc3,0x42,0xa6);
DEFINE_GUID(CAT_FEEDER, 0xfe131934,0xf84c,0x42ad,0x8d,0xa4,0x61,0x29,0xcd,0xdd,0x72,0x88);
DEFINE_GUID(CLSID_DevMgr2, 0xb6c292bc,0x7c88,0x41ee,0x8b,0x54,0x8e,0xc9,0x26,0x17,0xe5,0x99);

static int failures;

static void check(int ok, const char *what, HRESULT hr)
{
    if (ok) printf("PASS  %s\n", what);
    else
    {
        printf("FAIL  %s (0x%08lx)\n", what, (unsigned long)hr);
        failures++;
    }
    fflush(stdout);
}

static LONG get_long(IWiaPropertyStorage *stg, PROPID id, HRESULT *phr)
{
    PROPSPEC spec;
    PROPVARIANT v;
    HRESULT hr;
    LONG ret = -1;

    spec.ulKind = PRSPEC_PROPID;
    spec.propid = id;
    PropVariantInit(&v);
    hr = IWiaPropertyStorage_ReadMultiple(stg, 1, &spec, &v);
    if (hr == S_OK && v.vt == VT_I4) ret = v.lVal;
    PropVariantClear(&v);
    if (phr) *phr = hr;
    return ret;
}

static void get_str(IWiaPropertyStorage *stg, PROPID id, WCHAR *buf, int size)
{
    PROPSPEC spec;
    PROPVARIANT v;

    buf[0] = 0;
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = id;
    PropVariantInit(&v);
    if (IWiaPropertyStorage_ReadMultiple(stg, 1, &spec, &v) == S_OK && v.vt == VT_BSTR)
        lstrcpynW(buf, v.bstrVal, size);
    PropVariantClear(&v);
}

static HRESULT set_long(IWiaPropertyStorage *stg, PROPID id, LONG value)
{
    PROPSPEC spec;
    PROPVARIANT v;

    spec.ulKind = PRSPEC_PROPID;
    spec.propid = id;
    v.vt = VT_I4;
    v.lVal = value;
    return IWiaPropertyStorage_WriteMultiple(stg, 1, &spec, &v, 2);
}

static HRESULT set_guid(IWiaPropertyStorage *stg, PROPID id, const GUID *g)
{
    PROPSPEC spec;
    PROPVARIANT v;

    spec.ulKind = PRSPEC_PROPID;
    spec.propid = id;
    v.vt = VT_CLSID;
    v.puuid = (GUID *)g;
    return IWiaPropertyStorage_WriteMultiple(stg, 1, &spec, &v, 2);
}

/* the first scanner's ID, from a device manager */
static BSTR first_device(IEnumWIA_DEV_INFO *devices, ULONG *count, WCHAR *name, int size)
{
    IWiaPropertyStorage *stg;
    ULONG fetched;
    WCHAR id[128];

    *count = 0;
    IEnumWIA_DEV_INFO_GetCount(devices, count);
    if (IEnumWIA_DEV_INFO_Next(devices, 1, &stg, &fetched) != S_OK || fetched != 1) return NULL;
    get_str(stg, P_DEV_ID, id, 128);
    get_str(stg, P_DEV_NAME, name, size);
    IWiaPropertyStorage_Release(stg);
    return SysAllocString(id);
}

static BSTR device_id(BOOL wia2, IUnknown **mgr)
{
    IEnumWIA_DEV_INFO *devices = NULL;
    WCHAR name[128];
    ULONG count;
    BSTR id = NULL;
    HRESULT hr;

    if (wia2)
    {
        hr = CoCreateInstance(&CLSID_DevMgr2, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr2, (void **)mgr);
        if (SUCCEEDED(hr)) hr = IWiaDevMgr2_EnumDeviceInfo((IWiaDevMgr2 *)*mgr, WIA_DEVINFO_ENUM_LOCAL, &devices);
    }
    else
    {
        hr = CoCreateInstance(&CLSID_WiaDevMgr, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr, (void **)mgr);
        if (SUCCEEDED(hr)) hr = IWiaDevMgr_EnumDeviceInfo((IWiaDevMgr *)*mgr, WIA_DEVINFO_ENUM_LOCAL, &devices);
    }
    if (FAILED(hr)) return NULL;
    id = first_device(devices, &count, name, 128);
    IEnumWIA_DEV_INFO_Release(devices);
    return id;
}

static void do_list(void)
{
    IWiaDevMgr *mgr1 = NULL;
    IWiaDevMgr2 *mgr2 = NULL;
    IEnumWIA_DEV_INFO *devices;
    WCHAR name[128];
    ULONG count = 0;
    BSTR id = NULL;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_WiaDevMgr, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr, (void **)&mgr1);
    check(hr == S_OK, "the WIA 1 device manager is the WIA service's (local server)", hr);
    if (mgr1 && SUCCEEDED(hr = IWiaDevMgr_EnumDeviceInfo(mgr1, WIA_DEVINFO_ENUM_LOCAL, &devices)))
    {
        id = first_device(devices, &count, name, 128);
        printf("      WIA 1: %lu devices, first %ls (%ls)\n", count, id ? id : L"-", name);
        check(count == 2 && id && !wcsncmp(id, L"{6BDD1FC6-810F-11D0-BEC7-08002BE2092F}\\", 39),
              "WIA 1 lists the SANE test devices with imaging class IDs", hr);
        check(name[0] != 0, "a device has a name (WIA_DIP_DEV_NAME)", S_OK);
        IEnumWIA_DEV_INFO_Release(devices);
        SysFreeString(id);
    }
    else check(0, "WIA 1 EnumDeviceInfo", hr);
    hr = CoCreateInstance(&CLSID_DevMgr2, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr2, (void **)&mgr2);
    check(hr == S_OK, "the WIA 2 device manager (CLSID_WiaDevMgr2) is registered and served", hr);
    if (mgr2 && SUCCEEDED(hr = IWiaDevMgr2_EnumDeviceInfo(mgr2, WIA_DEVINFO_ENUM_LOCAL, &devices)))
    {
        id = first_device(devices, &count, name, 128);
        check(count == 2 && id != NULL, "WIA 2 lists the same devices", hr);
        IEnumWIA_DEV_INFO_Release(devices);
        SysFreeString(id);
    }
    else check(0, "WIA 2 EnumDeviceInfo", hr);
    if (mgr1) IWiaDevMgr_Release(mgr1);
    if (mgr2) IWiaDevMgr2_Release(mgr2);
}

/* WIA 1 banded transfer */

struct band_cb
{
    IWiaDataCallback IWiaDataCallback_iface;
    BYTE *data;
    LONG size, got, headers, terminations, statuses;
    GUID format;
};

static HRESULT WINAPI cb_QueryInterface(IWiaDataCallback *iface, REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWiaDataCallback))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI cb_AddRef(IWiaDataCallback *iface) { return 2; }
static ULONG WINAPI cb_Release(IWiaDataCallback *iface) { return 1; }

static HRESULT WINAPI cb_BandedDataCallback(IWiaDataCallback *iface, LONG msg, LONG status, LONG percent,
                                            LONG offset, LONG length, LONG reserved, LONG reslen, BYTE *buf)
{
    struct band_cb *cb = (struct band_cb *)iface;

    switch (msg)
    {
    case IT_MSG_DATA_HEADER:
    {
        WIA_DATA_CALLBACK_HEADER *h = (WIA_DATA_CALLBACK_HEADER *)buf;
        cb->headers++;
        cb->size = h->lBufferSize;
        cb->format = h->guidFormatID;
        cb->data = calloc(1, cb->size + 16);
        break;
    }
    case IT_MSG_DATA:
        if (cb->data && offset + length <= cb->size) memcpy(cb->data + offset, buf, length);
        cb->got += length;
        break;
    case IT_MSG_STATUS:
        cb->statuses++;
        break;
    case IT_MSG_TERMINATION:
        cb->terminations++;
        break;
    }
    return S_OK;
}

static IWiaDataCallbackVtbl cb_vtbl = { cb_QueryInterface, cb_AddRef, cb_Release, cb_BandedDataCallback };

static IWiaItem *wia1_item(IWiaDevMgr **mgr, IWiaItem **root)
{
    IEnumWiaItem *items;
    IWiaItem *item = NULL;
    ULONG fetched;
    BSTR id = device_id(FALSE, (IUnknown **)mgr);
    HRESULT hr;

    *root = NULL;
    if (!id) { check(0, "a WIA 1 device", E_FAIL); return NULL; }
    hr = IWiaDevMgr_CreateDevice(*mgr, id, root);
    check(hr == S_OK, "WIA 1 CreateDevice gives the root item", hr);
    SysFreeString(id);
    if (FAILED(hr)) return NULL;
    hr = IWiaItem_EnumChildItems(*root, &items);
    if (SUCCEEDED(hr))
    {
        hr = IEnumWiaItem_Next(items, 1, &item, &fetched);
        IEnumWiaItem_Release(items);
    }
    check(hr == S_OK && item, "the root item has an image item", hr);
    return item;
}

static void do_scan1(void)
{
    struct band_cb cb = { { &cb_vtbl } };
    IWiaPropertyStorage *stg = NULL, *rstg = NULL;
    IWiaDataTransfer *transfer = NULL;
    WIA_DATA_TRANSFER_INFO info = { sizeof(info), 0, 65536 };
    IWiaDevMgr *mgr = NULL;
    IWiaItem *root, *item;
    BITMAPINFOHEADER *bih;
    LONG w, h, type, i, distinct = 0;
    HRESULT hr;
    WCHAR name[64];

    if (!(item = wia1_item(&mgr, &root))) return;
    IWiaItem_GetItemType(item, &type);
    check(type & WiaItemTypeImage && type & WiaItemTypeFile, "the item is an image (WiaItemTypeImage|File)", type);
    hr = IWiaItem_QueryInterface(root, &IID_IWiaPropertyStorage, (void **)&rstg);
    check(hr == S_OK && get_long(rstg, P_DPS_HORIZONTAL_BED_SIZE, NULL) > 1000, "the root tells the bed size", hr);
    hr = IWiaItem_QueryInterface(item, &IID_IWiaPropertyStorage, (void **)&stg);
    check(hr == S_OK, "the item is its property storage", hr);
    if (FAILED(hr)) return;
    get_str(stg, P_FULL_ITEM_NAME, name, 64);
    printf("      item %ls\n", name);
    hr = set_long(stg, P_XRES, 100);
    if (hr == S_OK) hr = set_long(stg, P_YRES, 100);
    if (hr == S_OK) hr = set_long(stg, P_DATATYPE, WIA_DATA_COLOR);
    if (hr == S_OK) hr = set_long(stg, P_XEXTENT, 200);
    if (hr == S_OK) hr = set_long(stg, P_YEXTENT, 150);
    if (hr == S_OK) hr = set_guid(stg, P_FORMAT, &FMT_BMP);
    if (hr == S_OK) hr = set_long(stg, P_TYMED, TYMED_CALLBACK);
    check(hr == S_OK, "resolution, colour, area, format and medium are written", hr);
    check(get_long(stg, P_DEPTH, NULL) == 24, "colour makes the depth 24", get_long(stg, P_DEPTH, NULL));
    hr = IWiaItem_QueryInterface(item, &IID_IWiaDataTransfer, (void **)&transfer);
    check(hr == S_OK, "the item transfers (IWiaDataTransfer)", hr);
    if (FAILED(hr)) return;
    hr = IWiaDataTransfer_idtGetBandedData(transfer, &info, &cb.IWiaDataCallback_iface);
    check(hr == S_OK, "idtGetBandedData scans a page", hr);
    check(cb.headers == 1 && cb.terminations == 1 && cb.got == cb.size && cb.size > 0,
          "the callback gets a header, all the data and the end", cb.got);
    bih = (BITMAPINFOHEADER *)cb.data;
    w = bih ? bih->biWidth : 0;
    h = bih ? bih->biHeight : 0;
    printf("      DIB %ldx%ld %d bpp, %ld bytes, %ld status messages\n", w, h, bih ? bih->biBitCount : 0, cb.size,
           cb.statuses);
    check(bih && bih->biSize == sizeof(*bih) && w == 200 && h == 150 && bih->biBitCount == 24,
          "the data is a 200x150 24-bit DIB (no file header), the area asked for", w);
    if (bih)
    {
        /* the test device's colour pattern: not a blank page */
        BYTE *bits = cb.data + sizeof(*bih);
        DWORD first = bits[0] | bits[1] << 8 | bits[2] << 16;
        for (i = 0; i < 600 * 100 && i < cb.size - (LONG)sizeof(*bih) - 3; i += 3)
            if ((bits[i] | bits[i + 1] << 8 | bits[i + 2] << 16) != first) distinct++;
        check(distinct > 100, "the page has the device's picture on it", distinct);
    }
    free(cb.data);

    /* grayscale, a second transfer of the same device object */
    memset(&cb, 0, sizeof(cb));
    cb.IWiaDataCallback_iface.lpVtbl = &cb_vtbl;
    set_long(stg, P_DATATYPE, WIA_DATA_GRAYSCALE);
    set_guid(stg, P_FORMAT, &FMT_MEMORYBMP);
    hr = IWiaDataTransfer_idtGetBandedData(transfer, &info, &cb.IWiaDataCallback_iface);
    bih = (BITMAPINFOHEADER *)cb.data;
    check(hr == S_OK && bih && bih->biBitCount == 8 && bih->biClrUsed == 256 && IsEqualGUID(&cb.format, &FMT_MEMORYBMP),
          "a second scan, grayscale MEMORYBMP: 8 bits with its palette", hr);
    free(cb.data);
    IWiaDataTransfer_Release(transfer);
    IWiaPropertyStorage_Release(stg);
    IWiaPropertyStorage_Release(rstg);
    IWiaItem_Release(item);
    IWiaItem_Release(root);
    IWiaDevMgr_Release(mgr);
}

static void do_file1(void)
{
    IWiaPropertyStorage *stg;
    IWiaDataTransfer *transfer;
    IWiaDevMgr *mgr = NULL;
    IWiaItem *root, *item;
    STGMEDIUM medium;
    WCHAR path[MAX_PATH];
    BYTE sig[8] = { 0 };
    DWORD got = 0;
    HANDLE file;
    HRESULT hr;

    if (!(item = wia1_item(&mgr, &root))) return;
    IWiaItem_QueryInterface(item, &IID_IWiaPropertyStorage, (void **)&stg);
    set_long(stg, P_XRES, 75);
    set_long(stg, P_YRES, 75);
    hr = set_guid(stg, P_FORMAT, &FMT_PNG);
    if (hr == S_OK) hr = set_long(stg, P_TYMED, TYMED_FILE);
    check(hr == S_OK, "PNG to a file is accepted", hr);
    IWiaItem_QueryInterface(item, &IID_IWiaDataTransfer, (void **)&transfer);
    GetCurrentDirectoryW(MAX_PATH, path);
    lstrcatW(path, L"\\wia1scan.png");
    memset(&medium, 0, sizeof(medium));
    medium.tymed = TYMED_FILE;
    medium.lpszFileName = path;
    hr = IWiaDataTransfer_idtGetData(transfer, &medium, NULL);
    check(hr == S_OK, "idtGetData scans into the program's file", hr);
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file != INVALID_HANDLE_VALUE)
    {
        ReadFile(file, sig, 8, &got, NULL);
        CloseHandle(file);
    }
    check(got == 8 && !memcmp(sig, "\x89PNG\r\n\x1a\n", 8), "the file is a PNG", got);
    /* no name: a temporary file, whose name comes back */
    memset(&medium, 0, sizeof(medium));
    medium.tymed = TYMED_FILE;
    hr = IWiaDataTransfer_idtGetData(transfer, &medium, NULL);
    check(hr == S_OK && medium.lpszFileName && GetFileAttributesW(medium.lpszFileName) != INVALID_FILE_ATTRIBUTES,
          "without a name, a temporary file is made and named", hr);
    if (medium.lpszFileName) DeleteFileW(medium.lpszFileName);
    IWiaDataTransfer_Release(transfer);
    IWiaPropertyStorage_Release(stg);
    IWiaItem_Release(item);
    IWiaItem_Release(root);
    IWiaDevMgr_Release(mgr);
}

/* WIA 2 */

struct xfer_cb
{
    IWiaTransferCallback IWiaTransferCallback_iface;
    IStream *streams[32];
    int count, ends, end_transfer, statuses;
    WCHAR name[64];
};

static HRESULT WINAPI xcb_QueryInterface(IWiaTransferCallback *iface, REFIID riid, void **obj)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IWiaTransferCallback))
    {
        *obj = iface;
        return S_OK;
    }
    *obj = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI xcb_AddRef(IWiaTransferCallback *iface) { return 2; }
static ULONG WINAPI xcb_Release(IWiaTransferCallback *iface) { return 1; }

static HRESULT WINAPI xcb_TransferCallback(IWiaTransferCallback *iface, LONG flags, WiaTransferParams *params)
{
    struct xfer_cb *cb = (struct xfer_cb *)iface;

    if (params->lMessage == WIA_TRANSFER_MSG_END_OF_STREAM) cb->ends++;
    else if (params->lMessage == WIA_TRANSFER_MSG_END_OF_TRANSFER) cb->end_transfer++;
    else if (params->lMessage == WIA_TRANSFER_MSG_STATUS) cb->statuses++;
    return S_OK;
}

static HRESULT WINAPI xcb_GetNextStream(IWiaTransferCallback *iface, LONG flags, BSTR name, BSTR full,
                                        IStream **stream)
{
    struct xfer_cb *cb = (struct xfer_cb *)iface;

    if (cb->count == 32) return E_FAIL;
    lstrcpynW(cb->name, name, 64);
    CreateStreamOnHGlobal(NULL, TRUE, &cb->streams[cb->count]);
    *stream = cb->streams[cb->count++];
    IStream_AddRef(*stream);
    return S_OK;
}

static IWiaTransferCallbackVtbl xcb_vtbl = { xcb_QueryInterface, xcb_AddRef, xcb_Release, xcb_TransferCallback,
                                             xcb_GetNextStream };

static IWiaItem2 *wia2_child(IWiaDevMgr2 **mgr, IWiaItem2 **root, const GUID *category)
{
    IEnumWiaItem2 *items;
    IWiaItem2 *item, *found = NULL;
    ULONG fetched, n = 0;
    BSTR id = device_id(TRUE, (IUnknown **)mgr);
    HRESULT hr;

    *root = NULL;
    if (!id) { check(0, "a WIA 2 device", E_FAIL); return NULL; }
    hr = IWiaDevMgr2_CreateDevice(*mgr, 0, id, root);
    SysFreeString(id);
    check(hr == S_OK, "WIA 2 CreateDevice gives the root item", hr);
    if (FAILED(hr)) return NULL;
    if (SUCCEEDED(hr = IWiaItem2_EnumChildItems(*root, NULL, &items)))
    {
        while (IEnumWiaItem2_Next(items, 1, &item, &fetched) == S_OK && fetched == 1)
        {
            GUID cat;
            n++;
            if (!found && SUCCEEDED(IWiaItem2_GetItemCategory(item, &cat)) && IsEqualGUID(&cat, category))
                found = item;
            else IWiaItem2_Release(item);
        }
        IEnumWiaItem2_Release(items);
    }
    check(n == 2, "the root has a Flatbed and a Feeder item", n);
    return found;
}

static void check_bmp(IStream *stream, LONG w, LONG h, int bpp, const char *what)
{
    LARGE_INTEGER zero = { { 0 } };
    BYTE head[64];
    ULONG got = 0;
    BITMAPFILEHEADER *bfh = (BITMAPFILEHEADER *)head;
    BITMAPINFOHEADER *bih = (BITMAPINFOHEADER *)(head + sizeof(*bfh));
    STATSTG stat;

    IStream_Stat(stream, &stat, STATFLAG_NONAME);
    IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
    IStream_Read(stream, head, sizeof(head), &got);
    printf("      stream %lu bytes, %ldx%ld %d bpp\n", (ULONG)stat.cbSize.QuadPart, got >= 54 ? bih->biWidth : 0,
           got >= 54 ? bih->biHeight : 0, got >= 54 ? bih->biBitCount : 0);
    check(got >= 54 && head[0] == 'B' && head[1] == 'M' && bfh->bfSize == stat.cbSize.QuadPart
          && (w < 0 || bih->biWidth == w) && (h < 0 || bih->biHeight == h) && bih->biBitCount == bpp, what, got);
}

static void do_scan2(void)
{
    struct xfer_cb cb = { { &xcb_vtbl } };
    IWiaPropertyStorage *stg;
    IWiaTransfer *transfer;
    IWiaDevMgr2 *mgr = NULL;
    IWiaItem2 *root, *item;
    WCHAR name[64];
    HRESULT hr;
    int i;

    if (!(item = wia2_child(&mgr, &root, &CAT_FLATBED)))
    {
        check(0, "the Flatbed item (WIA_CATEGORY_FLATBED)", E_FAIL);
        return;
    }
    IWiaItem2_QueryInterface(item, &IID_IWiaPropertyStorage, (void **)&stg);
    get_str(stg, P_ITEM_NAME, name, 64);
    check(!lstrcmpW(name, L"Flatbed"), "the item is named Flatbed", S_OK);
    hr = set_long(stg, P_XRES, 75);
    if (hr == S_OK) hr = set_long(stg, P_YRES, 75);
    if (hr == S_OK) hr = set_long(stg, P_DATATYPE, WIA_DATA_GRAYSCALE);
    if (hr == S_OK) hr = set_guid(stg, P_FORMAT, &FMT_BMP);
    check(hr == S_OK, "WIA 2 properties are written", hr);
    hr = IWiaItem2_QueryInterface(item, &IID_IWiaTransfer, (void **)&transfer);
    check(hr == S_OK, "the item transfers (IWiaTransfer)", hr);
    if (FAILED(hr)) return;
    hr = IWiaTransfer_Download(transfer, 0, &cb.IWiaTransferCallback_iface);
    check(hr == S_OK, "Download scans the page", hr);
    check(cb.count == 1 && cb.ends == 1 && cb.end_transfer == 1, "one stream, its end, the end of the transfer",
          cb.count);
    if (cb.count) check_bmp(cb.streams[0], get_long(stg, P_PIXELS_PER_LINE, NULL),
                            get_long(stg, P_NUMBER_OF_LINES, NULL), 8, "the stream is the 8-bit BMP file of the item's area");
    for (i = 0; i < cb.count; i++) IStream_Release(cb.streams[i]);
    IWiaTransfer_Release(transfer);
    IWiaPropertyStorage_Release(stg);
    IWiaItem2_Release(item);
    IWiaItem2_Release(root);
    IWiaDevMgr2_Release(mgr);
}

static void do_feeder2(void)
{
    struct xfer_cb cb = { { &xcb_vtbl } };
    IWiaPropertyStorage *stg;
    IWiaTransfer *transfer;
    IWiaDevMgr2 *mgr = NULL;
    IWiaItem2 *root, *item;
    HRESULT hr;
    int i;

    if (!(item = wia2_child(&mgr, &root, &CAT_FEEDER)))
    {
        check(0, "the Feeder item (WIA_CATEGORY_FEEDER)", E_FAIL);
        return;
    }
    IWiaItem2_QueryInterface(item, &IID_IWiaPropertyStorage, (void **)&stg);
    set_long(stg, P_XRES, 50);
    set_long(stg, P_YRES, 50);
    set_long(stg, P_DATATYPE, WIA_DATA_COLOR);
    hr = set_long(stg, WIA_IPS_PAGES, 3);
    check(hr == S_OK, "the feeder takes a number of pages", hr);
    IWiaItem2_QueryInterface(item, &IID_IWiaTransfer, (void **)&transfer);
    hr = IWiaTransfer_Download(transfer, 0, &cb.IWiaTransferCallback_iface);
    check(hr == S_OK && cb.count == 3 && cb.ends == 3, "three pages, a stream each", cb.count);
    if (cb.count) check_bmp(cb.streams[cb.count - 1], -1, -1, 24, "a fed page is a 24-bit BMP file");
    for (i = 0; i < cb.count; i++) IStream_Release(cb.streams[i]);
    /* all pages (0): until the feeder is empty -- the test device's holds ten sheets */
    memset(&cb, 0, sizeof(cb));
    cb.IWiaTransferCallback_iface.lpVtbl = &xcb_vtbl;
    set_long(stg, WIA_IPS_PAGES, 0);
    hr = IWiaTransfer_Download(transfer, 0, &cb.IWiaTransferCallback_iface);
    printf("      all pages: %d streams\n", cb.count);
    check(hr == S_OK && cb.count == 10 && cb.ends == 10 && cb.end_transfer == 1,
          "all pages: every sheet of the feeder, then the end of the transfer", hr);
    IWiaTransfer_Release(transfer);
    IWiaPropertyStorage_Release(stg);
    IWiaItem2_Release(item);
    IWiaItem2_Release(root);
    IWiaDevMgr2_Release(mgr);
}

/* WIA 1 takes a page from the feeder per transfer, until it is empty */
static void do_feeder1(void)
{
    struct band_cb cb = { { &cb_vtbl } };
    WIA_DATA_TRANSFER_INFO info = { sizeof(info), 0, 65536 };
    IWiaPropertyStorage *stg, *rstg;
    IWiaDataTransfer *transfer;
    IWiaDevMgr *mgr = NULL;
    IWiaItem *root, *item;
    HRESULT hr = S_OK;
    int pages = 0;

    if (!(item = wia1_item(&mgr, &root))) return;
    IWiaItem_QueryInterface(root, &IID_IWiaPropertyStorage, (void **)&rstg);
    IWiaItem_QueryInterface(item, &IID_IWiaPropertyStorage, (void **)&stg);
    check(get_long(rstg, P_DPS_DOCUMENT_HANDLING_CAPABILITIES, NULL) & FEED, "the device has a feeder", S_OK);
    hr = set_long(rstg, P_DPS_DOCUMENT_HANDLING_SELECT, FEEDER | FLATBED);
    check(hr == E_INVALIDARG, "feeder and flatbed at once are refused", hr);
    hr = set_long(rstg, P_DPS_DOCUMENT_HANDLING_SELECT, FEEDER);
    check(hr == S_OK, "the feeder is selected on the root", hr);
    set_long(stg, P_XRES, 50);
    set_long(stg, P_YRES, 50);
    set_guid(stg, P_FORMAT, &FMT_BMP);
    IWiaItem_QueryInterface(item, &IID_IWiaDataTransfer, (void **)&transfer);
    while (pages < 15)
    {
        memset(&cb, 0, sizeof(cb));
        cb.IWiaDataCallback_iface.lpVtbl = &cb_vtbl;
        hr = IWiaDataTransfer_idtGetBandedData(transfer, &info, &cb.IWiaDataCallback_iface);
        free(cb.data);
        if (hr != S_OK) break;
        pages++;
    }
    printf("      %d pages, then 0x%08lx\n", pages, (unsigned long)hr);
    check(pages == 10 && hr == WIA_ERROR_PAPER_EMPTY, "a page per transfer, then WIA_ERROR_PAPER_EMPTY", hr);
    IWiaDataTransfer_Release(transfer);
    IWiaPropertyStorage_Release(stg);
    IWiaPropertyStorage_Release(rstg);
    IWiaItem_Release(item);
    IWiaItem_Release(root);
    IWiaDevMgr_Release(mgr);
}

static void do_props(void)
{
    IWiaPropertyStorage *stg;
    IEnumSTATPROPSTG *props;
    IWiaDevMgr2 *mgr = NULL;
    IWiaItem2 *root, *item;
    STATPROPSTG stat;
    PROPSPEC spec;
    PROPVARIANT v;
    ULONG flags, fetched, n = 0, count = 0;
    BOOL named = FALSE;
    HRESULT hr;

    if (!(item = wia2_child(&mgr, &root, &CAT_FLATBED))) return;
    IWiaItem2_QueryInterface(item, &IID_IWiaPropertyStorage, (void **)&stg);
    spec.ulKind = PRSPEC_PROPID;
    spec.propid = P_XRES;
    hr = IWiaPropertyStorage_GetPropertyAttributes(stg, 1, &spec, &flags, &v);
    check(hr == S_OK && (flags & WIA_PROP_RW) == WIA_PROP_RW && flags & WIA_PROP_RANGE && v.vt == (VT_VECTOR | VT_I4)
          && v.cal.cElems == 4 && v.cal.pElems[WIA_RANGE_MIN] == 1 && v.cal.pElems[WIA_RANGE_MAX] == 1200,
          "the resolution is a range of the device's (1-1200)", hr);
    PropVariantClear(&v);
    spec.propid = P_DATATYPE;
    hr = IWiaPropertyStorage_GetPropertyAttributes(stg, 1, &spec, &flags, &v);
    check(hr == S_OK && flags & WIA_PROP_LIST && v.vt == (VT_VECTOR | VT_I4) && v.cal.cElems == 2 + 3,
          "the data types are a list (threshold, grayscale, colour)", hr);
    PropVariantClear(&v);
    hr = set_long(stg, P_XRES, 5000);
    check(hr == E_INVALIDARG, "a resolution out of range is refused", hr);
    hr = set_long(stg, P_PIXELS_PER_LINE, 10);
    check(FAILED(hr), "a read-only property is not written", hr);
    spec.ulKind = PRSPEC_LPWSTR;
    spec.lpwstr = (WCHAR *)L"Horizontal Resolution";
    hr = IWiaPropertyStorage_ReadMultiple(stg, 1, &spec, &v);
    check(hr == S_OK && v.vt == VT_I4, "a property is read by its name", hr);
    PropVariantClear(&v);
    hr = IWiaPropertyStorage_Enum(stg, &props);
    check(hr == S_OK, "the properties enumerate (IEnumSTATPROPSTG over the process boundary)", hr);
    if (SUCCEEDED(hr))
    {
        while (IEnumSTATPROPSTG_Next(props, 1, &stat, &fetched) == S_OK && fetched == 1)
        {
            n++;
            if (stat.propid == P_XRES && stat.lpwstrName && !lstrcmpW(stat.lpwstrName, L"Horizontal Resolution"))
                named = TRUE;
            CoTaskMemFree(stat.lpwstrName);
        }
        IEnumSTATPROPSTG_Release(props);
    }
    IWiaPropertyStorage_GetCount(stg, &count);
    printf("      %lu properties enumerated, GetCount %lu\n", n, count);
    check(n > 30 && n == count && named, "every property comes with its id, type and name", n);
    /* NAPS2's WIA driver releases an item, then the same pointer again as its property storage, and
     * goes on using its other references (as on Windows) */
    IWiaItem2_Release(item);
    IWiaItem2_Release(item);
    count = 0;
    hr = IWiaPropertyStorage_GetCount(stg, &count);
    check(hr == S_OK && count == n, "an item released once too often still answers through its other references", hr);
    IWiaPropertyStorage_Release(stg);
    IWiaItem2_Release(root);
    IWiaDevMgr2_Release(mgr);
}

/* the dialogs: driven by the gate (xdotool), the results checked here */
static void do_dialogs(const char *which)
{
    IWiaDevMgr *mgr = NULL;
    IWiaDevMgr2 *mgr2 = NULL;
    WCHAR path[MAX_PATH];
    HRESULT hr;

    if (!strcmp(which, "select"))
    {
        BSTR id = NULL;
        CoCreateInstance(&CLSID_WiaDevMgr, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr, (void **)&mgr);
        if (!mgr) { check(0, "device manager", E_FAIL); return; }
        hr = IWiaDevMgr_SelectDeviceDlgID(mgr, NULL, 1 /* scanner */, WIA_SELECT_DEVICE_NODEFAULT, &id);
        printf("      chose %ls\n", id ? id : L"-");
        check(hr == S_OK && id && wcsstr(id, L"\\000"), "Select Device: OK gives the chosen device's ID", hr);
        SysFreeString(id);
        IWiaDevMgr_Release(mgr);
    }
    else if (!strcmp(which, "cancel"))
    {
        IWiaItem *root = NULL;
        CoCreateInstance(&CLSID_WiaDevMgr, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr, (void **)&mgr);
        if (!mgr) { check(0, "device manager", E_FAIL); return; }
        hr = IWiaDevMgr_SelectDeviceDlg(mgr, NULL, 0, 0, NULL, &root);
        check(hr == S_FALSE && !root, "Select Device: Cancel answers S_FALSE", hr);
        IWiaDevMgr_Release(mgr);
    }
    else if (!strcmp(which, "scan1"))
    {
        BYTE head[2] = { 0 };
        DWORD got = 0;
        HANDLE file;
        GUID format = FMT_BMP;
        BSTR name;
        CoCreateInstance(&CLSID_WiaDevMgr, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr, (void **)&mgr);
        if (!mgr) { check(0, "device manager", E_FAIL); return; }
        GetCurrentDirectoryW(MAX_PATH, path);
        lstrcatW(path, L"\\dlgscan1.bmp");
        name = SysAllocString(path);
        hr = IWiaDevMgr_GetImageDlg(mgr, NULL, 1, 0, WIA_INTENT_IMAGE_TYPE_COLOR, NULL, name, &format);
        check(hr == S_OK, "GetImageDlg: Select Device, then Scan using <device>, then the file", hr);
        file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (file != INVALID_HANDLE_VALUE) { ReadFile(file, head, 2, &got, NULL); CloseHandle(file); }
        check(got == 2 && head[0] == 'B' && head[1] == 'M', "the scan is the BMP file asked for", got);
        SysFreeString(name);
        IWiaDevMgr_Release(mgr);
    }
    else if (!strcmp(which, "scan2"))
    {
        IWiaItem2 *item = NULL;
        BSTR folder, file, *paths = NULL, id;
        LONG count = 0, i;
        CoCreateInstance(&CLSID_DevMgr2, NULL, CLSCTX_LOCAL_SERVER, &IID_IWiaDevMgr2, (void **)&mgr2);
        if (!mgr2) { check(0, "device manager 2", E_FAIL); return; }
        id = device_id(TRUE, (IUnknown **)&mgr2);
        GetCurrentDirectoryW(MAX_PATH, path);
        folder = SysAllocString(path);
        file = SysAllocString(L"dlgscan2");
        hr = IWiaDevMgr2_GetImageDlg(mgr2, 0, id, NULL, folder, file, &count, &paths, &item);
        printf("      %ld files, first %ls\n", count, count ? paths[0] : L"-");
        check(hr == S_OK && count == 1 && paths && wcsstr(paths[0], L"dlgscan2.jpg"),
              "New Scan: Scan saves the page as name.jpg (Photo profile) in the folder", hr);
        check(count && GetFileAttributesW(paths[0]) != INVALID_FILE_ATTRIBUTES, "the file is there", S_OK);
        check(item != NULL, "the item scanned from comes back", S_OK);
        for (i = 0; i < count; i++) SysFreeString(paths[i]);
        CoTaskMemFree(paths);
        if (item) IWiaItem2_Release(item);
        SysFreeString(folder);
        SysFreeString(file);
        SysFreeString(id);
        IWiaDevMgr2_Release(mgr2);
    }
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "list";

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (!strcmp(mode, "list")) do_list();
    else if (!strcmp(mode, "scan1")) do_scan1();
    else if (!strcmp(mode, "file1")) do_file1();
    else if (!strcmp(mode, "scan2")) do_scan2();
    else if (!strcmp(mode, "feeder2")) do_feeder2();
    else if (!strcmp(mode, "feeder1")) do_feeder1();
    else if (!strcmp(mode, "props")) do_props();
    else if (!strncmp(mode, "dlg-", 4)) do_dialogs(mode + 4);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
