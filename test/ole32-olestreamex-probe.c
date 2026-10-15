/* OleConvertIStorageToOLESTREAMEx: the OLE 1 stream of an OLE 2 storage whose presentation is the data
 * the caller supplies in an HGLOBAL or a stream (it used to answer E_NOTIMPL).  The bytes are checked against
 * the layout of [MS-OLEDS] section 2.2: an embedded object, then a StandardPresentationObject
 * ("METAFILEPICT" / "DIB" / "BITMAP", width, height times -1, the data size, the data). */
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

/* an OLESTREAM that collects what is put into it */
struct mem_stream
{
    OLESTREAM stream;
    BYTE data[1 << 16];
    DWORD size;
};
static DWORD CALLBACK mem_get(LPOLESTREAM s, void *buf, DWORD n) { return 0; }
static DWORD CALLBACK mem_put(LPOLESTREAM s, const void *buf, DWORD n)
{
    struct mem_stream *m = (struct mem_stream *)s;
    if (m->size + n > sizeof(m->data)) return 0;
    memcpy(m->data + m->size, buf, n);
    m->size += n;
    return n;
}
static OLESTREAMVTBL vtbl = { mem_get, mem_put };

static DWORD rd32(const BYTE *p) { DWORD v; memcpy(&v, p, 4); return v; }

/* the offset where the second (presentation) object starts, or 0 */
static DWORD skip_first_object(const struct mem_stream *m)
{
    DWORD pos = 0, name_len, len;

    if (m->size < 24 || rd32(m->data) != 0x501 || rd32(m->data + 4) != 2) return 0;
    name_len = rd32(m->data + 8);
    pos = 12 + name_len + 8;              /* name, width, height */
    len = rd32(m->data + pos);
    pos += 4 + len;
    return pos <= m->size ? pos : 0;
}

static IStorage *make_storage(const WCHAR *path)
{
    IStorage *stg;
    IStream *stream;
    HRESULT hr = StgCreateDocfile(path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);

    if (hr != S_OK) return NULL;
    WriteClassStg(stg, &CLSID_NULL);
    WriteFmtUserTypeStg(stg, 0, L"SG test object");
    IStorage_CreateStream(stg, L"Payload", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stream);
    IStream_Write(stream, "payload bytes", 13, NULL);
    IStream_Release(stream);
    return stg;
}

int main(void)
{
    static const struct { CLIPFORMAT cf; const char *name; DWORD header; LONG w, h; DWORD size; } cases[] =
    {
        {CF_METAFILEPICT, "METAFILEPICT", 8, 2540, 1270, 40},
        {CF_DIB, "DIB", 0, 1000, 500, 64},
        {CF_BITMAP, "BITMAP", 0, 300, 700, 33},
        {CF_DIB, "DIB", 0, 10, 20, 0},
    };
    static struct mem_stream out;
    WCHAR dir[MAX_PATH], path[MAX_PATH];
    IStorage *stg;
    STGMEDIUM medium;
    HRESULT hr;
    unsigned int i, tymed_i;
    char text[160];

    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%ssg_olestream_%lu.stg", dir, GetCurrentProcessId());
    DeleteFileW(path);
    out.stream.lpstbl = &vtbl;
    stg = make_storage(path);
    check(stg != NULL, "a storage with an object in it");
    if (!stg) { printf("RESULT: FAIL\n"); return 1; }

    /* without presentation data: the old function, and the Ex with a NULL format */
    memset(out.data, 0, sizeof(out.data)); out.size = 0;
    hr = OleConvertIStorageToOLESTREAM(stg, &out.stream);
    check(hr == S_OK, "OleConvertIStorageToOLESTREAM");
    {
        DWORD pos = skip_first_object(&out);
        sprintf(text, "the embedded object is first, then an empty presentation object (pos %lu of %lu)", pos, out.size);
        check(pos && out.size == pos + 8 && rd32(out.data + pos) == 0x501 && rd32(out.data + pos + 4) == 0, text);
    }
    memset(&medium, 0, sizeof(medium));
    memset(out.data, 0, sizeof(out.data)); out.size = 0;
    hr = OleConvertIStorageToOLESTREAMEx(stg, 0, 0, 0, 0, &medium, &out.stream);
    check(hr == S_OK, "Ex with a NULL clipboard format is S_OK");
    {
        DWORD pos = skip_first_object(&out);
        check(pos && out.size == pos + 8 && rd32(out.data + pos + 4) == 0, "Ex with a NULL format has no presentation object");
    }

    for (tymed_i = 0; tymed_i < 2; tymed_i++)
    {
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        {
            BYTE src[128];
            DWORD pos, k, expect_len;
            BYTE *pres;
            BOOL fine = TRUE;

            for (k = 0; k < sizeof(src); k++) src[k] = (BYTE)(0x40 + k * 3 + i);
            memset(&medium, 0, sizeof(medium));
            if (tymed_i == 0)
            {
                void *p;
                medium.tymed = TYMED_HGLOBAL;
                medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeof(src));
                p = GlobalLock(medium.hGlobal);
                memcpy(p, src, sizeof(src));
                GlobalUnlock(medium.hGlobal);
            }
            else
            {
                medium.tymed = TYMED_ISTREAM;
                CreateStreamOnHGlobal(NULL, TRUE, &medium.pstm);
                IStream_Write(medium.pstm, src, sizeof(src), NULL);
                IStream_Seek(medium.pstm, (LARGE_INTEGER){{0}}, STREAM_SEEK_SET, NULL);
            }
            memset(out.data, 0, sizeof(out.data)); out.size = 0;
            hr = OleConvertIStorageToOLESTREAMEx(stg, cases[i].cf, cases[i].w, cases[i].h, cases[i].size, &medium, &out.stream);
            sprintf(text, "%s via %s: S_OK (hr %#lx)", cases[i].name, tymed_i ? "a stream" : "an HGLOBAL", hr);
            check(hr == S_OK, text);
            pos = skip_first_object(&out);
            check(pos != 0, "  the embedded object comes first");
            if (hr == S_OK && pos)
            {
                DWORD nlen = strlen(cases[i].name) + 1;
                pres = out.data + pos;
                expect_len = cases[i].header + cases[i].size;
                fine &= rd32(pres) == 0x501 && rd32(pres + 4) == 5;
                fine &= rd32(pres + 8) == nlen && !memcmp(pres + 12, cases[i].name, nlen);
                fine &= (LONG)rd32(pres + 12 + nlen) == cases[i].w;
                fine &= (LONG)rd32(pres + 16 + nlen) == -cases[i].h;
                fine &= rd32(pres + 20 + nlen) == expect_len;
                check(fine, "  header, class name, width, height times -1, data size");
                check(!memcmp(pres + 24 + nlen + cases[i].header, src, cases[i].size), "  the supplied bytes");
                check(out.size == pos + 24 + nlen + expect_len, "  nothing after the data");
            }
            if (tymed_i == 0) GlobalFree(medium.hGlobal); else IStream_Release(medium.pstm);
        }
    }

    /* arguments */
    memset(&medium, 0, sizeof(medium));
    medium.tymed = TYMED_FILE;
    out.size = 0;
    hr = OleConvertIStorageToOLESTREAMEx(stg, CF_DIB, 1, 1, 4, &medium, &out.stream);
    check(hr == E_INVALIDARG, "a TYMED_FILE medium is E_INVALIDARG");
    medium.tymed = TYMED_HGLOBAL;
    medium.hGlobal = GlobalAlloc(GMEM_MOVEABLE, 8);
    hr = OleConvertIStorageToOLESTREAMEx(stg, CF_DIB, 1, 1, 4000, &medium, &out.stream);
    check(hr == E_INVALIDARG, "a size larger than the HGLOBAL is E_INVALIDARG");
    GlobalFree(medium.hGlobal);
    hr = OleConvertIStorageToOLESTREAMEx(stg, CF_DIB, 1, 1, 4, NULL, &out.stream);
    check(hr == E_INVALIDARG, "a NULL medium with a format is E_INVALIDARG");
    hr = OleConvertIStorageToOLESTREAMEx(NULL, 0, 0, 0, 0, NULL, &out.stream);
    check(hr == E_INVALIDARG, "a NULL storage is E_INVALIDARG");
    hr = OleConvertIStorageToOLESTREAMEx(stg, 0, 0, 0, 0, NULL, NULL);
    check(hr == E_INVALIDARG, "a NULL stream is E_INVALIDARG");

    IStorage_Release(stg);
    DeleteFileW(path);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
