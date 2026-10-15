/* IViewObject::GetColorSet of the OLE data cache: the palette Draw would use.  CF_DIB: the colour table of
 * the bitmap; CF_BITMAP: none (S_FALSE); a metafile: the first CreatePalette record; an aspect without data:
 * OLE_E_BLANK; bad lindex / aspect: DV_E_LINDEX / DV_E_DVASPECT.  It used to answer E_NOTIMPL. */
#define COBJMACROS
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

static HGLOBAL make_dib(WORD bits, DWORD colors_used, const RGBQUAD *table, UINT table_count)
{
    DWORD table_bytes = (bits <= 8 ? (colors_used ? colors_used : 1u << bits) : 0) * sizeof(RGBQUAD);
    DWORD image = 4 * 4 * (bits < 8 ? 1 : bits / 8);
    HGLOBAL g = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, sizeof(BITMAPINFOHEADER) + table_bytes + image);
    BITMAPINFO *info = GlobalLock(g);
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = 4;
    info->bmiHeader.biHeight = 4;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = bits;
    info->bmiHeader.biClrUsed = colors_used;
    if (table) memcpy(info->bmiColors, table, table_count * sizeof(RGBQUAD));
    GlobalUnlock(g);
    return g;
}

static HRESULT cache_data(IOleCache *cache, IDataObject *data, CLIPFORMAT cf, DWORD tymed, DWORD aspect, STGMEDIUM *med)
{
    FORMATETC fmt = { cf, NULL, aspect, -1, tymed };
    DWORD conn;
    HRESULT hr = IOleCache_Cache(cache, &fmt, ADVF_PRIMEFIRST, &conn);
    if (FAILED(hr)) return hr;
    return IDataObject_SetData(data, &fmt, med, TRUE);
}

int main(void)
{
    static const RGBQUAD table4[4] =
    {
        {0x10, 0x20, 0x30, 0}, {0x40, 0x50, 0x60, 0}, {0x70, 0x80, 0x90, 0}, {0xa0, 0xb0, 0xc0, 0},
    };
    IOleCache *cache;
    IDataObject *data;
    IViewObject *view;
    STGMEDIUM med;
    LOGPALETTE *pal;
    HRESULT hr;
    char text[160];

    OleInitialize(NULL);
    hr = CreateDataCache(NULL, &CLSID_NULL, &IID_IOleCache, (void **)&cache);
    check(hr == S_OK, "CreateDataCache");
    IOleCache_QueryInterface(cache, &IID_IDataObject, (void **)&data);
    IOleCache_QueryInterface(cache, &IID_IViewObject, (void **)&view);

    /* nothing cached */
    pal = (LOGPALETTE *)0xdeadbeef;
    hr = IViewObject_GetColorSet(view, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal);
    sprintf(text, "nothing cached: OLE_E_BLANK (hr %#lx)", hr);
    check(hr == OLE_E_BLANK && !pal, text);

    /* a 4-colour DIB */
    memset(&med, 0, sizeof(med));
    med.tymed = TYMED_HGLOBAL;
    med.hGlobal = make_dib(2, 0, table4, 4);
    hr = cache_data(cache, data, CF_DIB, TYMED_HGLOBAL, DVASPECT_CONTENT, &med);
    check(hr == S_OK, "cache a 4 colour DIB");
    pal = NULL;
    hr = IViewObject_GetColorSet(view, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal);
    sprintf(text, "DIB: S_OK with a palette (hr %#lx)", hr);
    check(hr == S_OK && pal, text);
    if (hr == S_OK && pal)
    {
        unsigned int i;
        BOOL same = pal->palNumEntries == 4 && pal->palVersion == 0x300;
        for (i = 0; same && i < 4; i++)
            same = pal->palPalEntry[i].peRed == table4[i].rgbRed && pal->palPalEntry[i].peGreen == table4[i].rgbGreen &&
                   pal->palPalEntry[i].peBlue == table4[i].rgbBlue && pal->palPalEntry[i].peFlags == 0;
        sprintf(text, "DIB: the palette is the colour table (%u entries)", pal->palNumEntries);
        check(same, text);
        CoTaskMemFree(pal);
    }

    /* the same DIB asked about another aspect, a bad index, a bad aspect */
    pal = (LOGPALETTE *)0xdeadbeef;
    hr = IViewObject_GetColorSet(view, DVASPECT_ICON, -1, NULL, NULL, NULL, &pal);
    check(hr == OLE_E_BLANK, "an aspect that is not cached: OLE_E_BLANK");
    hr = IViewObject_GetColorSet(view, DVASPECT_CONTENT, 0, NULL, NULL, NULL, &pal);
    check(hr == DV_E_LINDEX, "lindex 0: DV_E_LINDEX");
    hr = IViewObject_GetColorSet(view, 3, -1, NULL, NULL, NULL, &pal);
    check(hr == DV_E_DVASPECT, "aspect 3: DV_E_DVASPECT");
    hr = IViewObject_GetColorSet(view, DVASPECT_CONTENT, -1, NULL, NULL, NULL, NULL);
    check(hr == E_INVALIDARG, "a NULL result pointer: E_INVALIDARG");
    IViewObject_Release(view);
    IDataObject_Release(data);
    IOleCache_Release(cache);

    /* a true colour DIB has no palette */
    CreateDataCache(NULL, &CLSID_NULL, &IID_IOleCache, (void **)&cache);
    IOleCache_QueryInterface(cache, &IID_IDataObject, (void **)&data);
    IOleCache_QueryInterface(cache, &IID_IViewObject, (void **)&view);
    memset(&med, 0, sizeof(med));
    med.tymed = TYMED_HGLOBAL;
    med.hGlobal = make_dib(24, 0, NULL, 0);
    hr = cache_data(cache, data, CF_DIB, TYMED_HGLOBAL, DVASPECT_CONTENT, &med);
    check(hr == S_OK, "cache a 24 bit DIB");
    pal = (LOGPALETTE *)0xdeadbeef;
    hr = IViewObject_GetColorSet(view, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal);
    sprintf(text, "24 bit DIB: S_FALSE and no palette (hr %#lx)", hr);
    check(hr == S_FALSE && !pal, text);
    IViewObject_Release(view);
    IDataObject_Release(data);
    IOleCache_Release(cache);

    /* a metafile with a CreatePalette record */
    {
        HDC dc = CreateMetaFileA(NULL);
        HPALETTE hpal;
        LOGPALETTE *lp = calloc(1, sizeof(LOGPALETTE) + 2 * sizeof(PALETTEENTRY));
        HMETAFILE mf;
        METAFILEPICT *pict;

        lp->palVersion = 0x300;
        lp->palNumEntries = 3;
        lp->palPalEntry[0].peRed = 1; lp->palPalEntry[0].peGreen = 2; lp->palPalEntry[0].peBlue = 3;
        lp->palPalEntry[1].peRed = 4; lp->palPalEntry[1].peGreen = 5; lp->palPalEntry[1].peBlue = 6;
        lp->palPalEntry[2].peRed = 7; lp->palPalEntry[2].peGreen = 8; lp->palPalEntry[2].peBlue = 9;
        hpal = CreatePalette(lp);
        SelectPalette(dc, hpal, FALSE);
        RealizePalette(dc);
        Rectangle(dc, 0, 0, 10, 10);
        mf = CloseMetaFile(dc);
        DeleteObject(hpal);
        free(lp);

        CreateDataCache(NULL, &CLSID_NULL, &IID_IOleCache, (void **)&cache);
        IOleCache_QueryInterface(cache, &IID_IDataObject, (void **)&data);
        IOleCache_QueryInterface(cache, &IID_IViewObject, (void **)&view);
        memset(&med, 0, sizeof(med));
        med.tymed = TYMED_MFPICT;
        med.hMetaFilePict = GlobalAlloc(GMEM_MOVEABLE, sizeof(METAFILEPICT));
        pict = GlobalLock(med.hMetaFilePict);
        pict->mm = MM_ANISOTROPIC; pict->xExt = 100; pict->yExt = 100; pict->hMF = mf;
        GlobalUnlock(med.hMetaFilePict);
        hr = cache_data(cache, data, CF_METAFILEPICT, TYMED_MFPICT, DVASPECT_CONTENT, &med);
        check(hr == S_OK, "cache a metafile with a palette");
        pal = NULL;
        hr = IViewObject_GetColorSet(view, DVASPECT_CONTENT, -1, NULL, NULL, NULL, &pal);
        sprintf(text, "metafile: S_OK (hr %#lx)", hr);
        check(hr == S_OK && pal, text);
        if (hr == S_OK && pal)
        {
            sprintf(text, "metafile: the palette of the CreatePalette record (%u entries)", pal->palNumEntries);
            check(pal->palNumEntries == 3 && pal->palPalEntry[0].peRed == 1 && pal->palPalEntry[1].peGreen == 5 &&
                  pal->palPalEntry[2].peBlue == 9, text);
            CoTaskMemFree(pal);
        }
        IViewObject_Release(view);
        IDataObject_Release(data);
        IOleCache_Release(cache);
    }

    OleUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
