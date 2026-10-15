/* FDITruncateCabinet (patch 3306), run by test/fastcabtrunc-gate.sh */
#include <windows.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <fdi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

/* the I/O of the cabinet library, on Win32 files */
static void *CDECL cb_alloc(ULONG n) { return malloc(n); }
static void CDECL cb_free(void *p) { free(p); }
static INT_PTR CDECL cb_open(char *name, int oflag, int pmode)
{
    DWORD access = (oflag & _O_RDWR) ? GENERIC_READ | GENERIC_WRITE : (oflag & _O_WRONLY) ? GENERIC_WRITE : GENERIC_READ;
    DWORD disp = (oflag & _O_CREAT) ? CREATE_ALWAYS : OPEN_EXISTING;
    HANDLE h = CreateFileA(name, access, FILE_SHARE_READ, NULL, disp, FILE_ATTRIBUTE_NORMAL, NULL);
    return h == INVALID_HANDLE_VALUE ? -1 : (INT_PTR)h;
}
static UINT CDECL cb_read(INT_PTR h, void *buf, UINT n)
{
    DWORD got = 0;
    if (!ReadFile((HANDLE)h, buf, n, &got, NULL)) return -1;
    return got;
}
static UINT CDECL cb_write(INT_PTR h, void *buf, UINT n)
{
    DWORD got = 0;
    if (!n) return SetEndOfFile((HANDLE)h) ? 0 : (UINT)-1;      /* nothing to write: truncate here */
    if (!WriteFile((HANDLE)h, buf, n, &got, NULL)) return -1;
    return got;
}
static int CDECL cb_close(INT_PTR h) { return CloseHandle((HANDLE)h) ? 0 : -1; }
static LONG CDECL cb_seek(INT_PTR h, LONG dist, int type)
{
    DWORD r = SetFilePointer((HANDLE)h, dist, NULL, type == SEEK_SET ? FILE_BEGIN : type == SEEK_CUR ? FILE_CURRENT : FILE_END);
    return r == INVALID_SET_FILE_POINTER ? -1 : (LONG)r;
}

struct file_spec { const char *name; int folder; const char *data; };

/* builds an uncompressed cabinet; folder 0xfffd/0xfffe/0xffff in a spec are the continued kinds, living in the first or last folder */
static BYTE *build_cab(int nfolders, const struct file_spec *files, int nfiles, int reserve, int prev, int next, SIZE_T *out_size)
{
    int header_resv = reserve ? 4 : 0, folder_resv = reserve ? 3 : 0, data_resv = reserve ? 2 : 0;
    BYTE *buf = calloc(1, 65536), *p;
    SIZE_T pos, folders_at, files_at, data_at, i, f;
    UINT32 foldoff[16] = {0};
    UINT16 ndata[16];
    SIZE_T flen[16] = {0};
    UINT32 foff[64];
    UINT16 flags = (reserve ? 4 : 0) | (prev ? 1 : 0) | (next ? 2 : 0);

    memcpy(buf, "MSCF", 4);
    buf[0x18] = 3; buf[0x19] = 1;
    buf[0x1A] = nfolders; buf[0x1C] = nfiles;
    buf[0x1E] = flags & 0xff; buf[0x1F] = flags >> 8;
    buf[0x20] = 0x34; buf[0x21] = 0x12;       /* set id */
    pos = 0x24;
    if (reserve)
    {
        buf[pos] = header_resv; buf[pos + 2] = folder_resv; buf[pos + 3] = data_resv;
        pos += 4;
        memset(buf + pos, 0x5a, header_resv);
        pos += header_resv;
    }
    if (prev) { strcpy((char *)buf + pos, "prev.cab"); pos += 9; strcpy((char *)buf + pos, "Disk 1"); pos += 7; }
    if (next) { strcpy((char *)buf + pos, "next.cab"); pos += 9; strcpy((char *)buf + pos, "Disk 3"); pos += 7; }
    folders_at = pos;
    files_at = folders_at + nfolders * (8 + folder_resv);
    p = buf + files_at;
    for (i = 0; i < (SIZE_T)nfiles; ++i)
    {
        int fol = files[i].folder;
        int real = (fol == 0xfffd || fol == 0xffff) ? 0 : fol == 0xfffe ? nfolders - 1 : fol;
        UINT32 len = strlen(files[i].data);
        foff[i] = flen[real];
        flen[real] += len;
        memcpy(p, &len, 4);
        memcpy(p + 4, &foff[i], 4);
        p[8] = fol & 0xff; p[9] = fol >> 8;
        p[10] = 0x21; p[11] = 0x32; p[12] = 0x43; p[13] = 0x54;
        strcpy((char *)p + 16, files[i].name);
        p += 16 + strlen(files[i].name) + 1;
    }
    data_at = p - buf;
    for (f = 0; f < (SIZE_T)nfolders; ++f)
    {
        BYTE *d = buf + data_at;
        SIZE_T k;

        foldoff[f] = d - buf;
        (void)k;
        data_at += 0;
        ndata[f] = flen[f] ? 1 : 0;
        if (flen[f])
        {
            UINT16 cb = flen[f];
            memcpy(d + 4, &cb, 2);
            memcpy(d + 6, &cb, 2);
            memset(d + 8, 0x77, data_resv);
            {
                SIZE_T at = 0;
                for (i = 0; i < (SIZE_T)nfiles; ++i)
                {
                    int fol = files[i].folder;
                    int real = (fol == 0xfffd || fol == 0xffff) ? 0 : fol == 0xfffe ? nfolders - 1 : fol;
                    if (real != (int)f) continue;
                    memcpy(d + 8 + data_resv + at, files[i].data, strlen(files[i].data));
                    at += strlen(files[i].data);
                }
            }
            data_at += 8 + data_resv + flen[f];
        }
    }
    /* now the folder entries */
    for (f = 0; f < (SIZE_T)nfolders; ++f)
    {
        BYTE *e = buf + folders_at + f * (8 + folder_resv);
        memcpy(e, &foldoff[f], 4);
        memcpy(e + 4, &ndata[f], 2);
        memset(e + 8, 0x66, folder_resv);
    }
    {
        UINT32 total = data_at, fa = files_at;
        memcpy(buf + 8, &total, 4);
        memcpy(buf + 0x10, &fa, 4);
        *out_size = data_at;
    }
    (void)foff;
    return buf;
}

static void write_file(const char *path, const BYTE *data, SIZE_T size)
{
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    DWORD n;
    WriteFile(h, data, size, &n, NULL);
    CloseHandle(h);
}

static BYTE *read_file(const char *path, SIZE_T *size)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD n, sz;
    BYTE *buf;
    if (h == INVALID_HANDLE_VALUE) { *size = 0; return NULL; }
    sz = GetFileSize(h, NULL);
    buf = malloc(sz + 1);
    ReadFile(h, buf, sz, &n, NULL);
    CloseHandle(h);
    *size = sz;
    return buf;
}

/* the files FDICopy extracts */
struct listing { char names[32][64]; char data[32][256]; int count; int cur; char tmp[MAX_PATH]; };

static INT_PTR CDECL notify(FDINOTIFICATIONTYPE type, PFDINOTIFICATION pfdin)
{
    struct listing *l = pfdin->pv;
    switch (type)
    {
    case fdintCOPY_FILE:
    {
        INT_PTR h;
        l->cur = l->count++;
        strcpy(l->names[l->cur], pfdin->psz1);
        snprintf(l->tmp, MAX_PATH, "%s.extract", l->names[l->cur]);
        h = cb_open(l->tmp, _O_WRONLY | _O_CREAT, 0);
        return h;
    }
    case fdintCLOSE_FILE_INFO:
    {
        SIZE_T n;
        BYTE *d;
        cb_close(pfdin->hf);
        d = read_file(l->tmp, &n);
        if (d) { memcpy(l->data[l->cur], d, n < 255 ? n : 255); l->data[l->cur][n < 255 ? n : 255] = 0; free(d); }
        DeleteFileA(l->tmp);
        return TRUE;
    }
    default:
        return 0;
    }
}

static BOOL list_cab(HFDI fdi, const char *path, struct listing *l)
{
    char dir[MAX_PATH], base[MAX_PATH];
    const char *slash = strrchr(path, '\\');
    memset(l, 0, sizeof(*l));
    l->cur = -1;
    strcpy(base, slash + 1);
    memcpy(dir, path, slash + 1 - path);
    dir[slash + 1 - path] = 0;
    return FDICopy(fdi, base, dir, 0, notify, NULL, l);
}

static UINT32 le32(const BYTE *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24); }
static UINT16 le16(const BYTE *p) { return p[0] | (p[1] << 8); }

int main(void)
{
    static const struct file_spec four[] =
    {
        { "a.txt", 0, "alpha alpha" },
        { "b.txt", 1, "bravo" }, { "c.txt", 1, "charlie charlie charlie" },
        { "d.txt", 2, "delta" },
        { "e.txt", 3, "echo" }, { "f.txt", 3, "foxtrot foxtrot" },
    };
    static const struct file_spec spanning[] =
    {
        { "tail.txt", 0xfffd, "ends here" },
        { "mid.txt", 0, "middle" },
        { "two.txt", 1, "second folder" },
        { "head.txt", 0xfffe, "begins here" },
    };
    HFDI fdi;
    ERF erf;
    char dir[MAX_PATH], path[MAX_PATH];
    BYTE *cab, *got;
    SIZE_T size, gsize;
    struct listing l;
    BOOL r;
    int i;

    fdi = FDICreate(cb_alloc, cb_free, cb_open, cb_read, cb_write, cb_close, cb_seek, cpuUNKNOWN, &erf);
    if (!fdi) { check(0, "FDICreate"); return 1; }
    GetTempPathA(MAX_PATH, dir);
    snprintf(path, MAX_PATH, "%sfastcabtrunc.cab", dir);

    /* four folders cut at the third */
    cab = build_cab(4, four, 6, 0, 0, 0, &size);
    write_file(path, cab, size);
    r = list_cab(fdi, path, &l);
    CHECKF(r && l.count == 6 && !strcmp(l.data[5], "foxtrot foxtrot"), "the cabinet we built extracts (%d files)", l.count);
    r = FDITruncateCabinet(fdi, path, 2);
    CHECKF(r, "FDITruncateCabinet(2) (%lu)", GetLastError());
    got = read_file(path, &gsize);
    CHECKF(gsize && le16(got + 0x1A) == 2 && le16(got + 0x1C) == 3 && le32(got + 8) == gsize, "two folders, three files, the size field is the file's (%Iu, %u, %u, %u)", gsize,
            got ? le16(got + 0x1A) : 0, got ? le16(got + 0x1C) : 0, got ? le32(got + 8) : 0);
    {
        /* header 36, folders 16, files (16 + 6) * 3, data blocks (8 + 11) + (8 + 28) */
        SIZE_T expect = 0x24 + 2 * 8 + 3 * 22 + (8 + 11) + (8 + 5 + 23);
        CHECKF(gsize == expect, "the file is as long as it should be (%Iu, want %Iu)", gsize, expect);
        CHECKF(got && le32(got + 0x10) == 0x24 + 2 * 8 && le32(got + 0x24) == 0x24 + 16 + 66, "the file table and the first folder's data moved (%u, %u)",
                got ? le32(got + 0x10) : 0, got ? le32(got + 0x24) : 0);
    }
    free(got);
    r = list_cab(fdi, path, &l);
    CHECKF(r && l.count == 3 && !strcmp(l.names[0], "a.txt") && !strcmp(l.names[1], "b.txt") && !strcmp(l.names[2], "c.txt"),
            "the files of the folders that stay (%d: %s %s %s)", l.count, l.names[0], l.names[1], l.names[2]);
    CHECKF(!strcmp(l.data[0], "alpha alpha") && !strcmp(l.data[1], "bravo") && !strcmp(l.data[2], "charlie charlie charlie"),
            "and their contents (%s / %s / %s)", l.data[0], l.data[1], l.data[2]);

    /* nothing at all left */
    write_file(path, cab, size);
    r = FDITruncateCabinet(fdi, path, 0);
    got = read_file(path, &gsize);
    CHECKF(r && gsize == 0x24 && le16(got + 0x1A) == 0 && le16(got + 0x1C) == 0 && le32(got + 8) == 0x24, "cut at the first folder: a cabinet of a header (%Iu)", gsize);
    free(got);

    /* the last folder only */
    write_file(path, cab, size);
    r = FDITruncateCabinet(fdi, path, 3);
    r = r && list_cab(fdi, path, &l);
    CHECKF(r && l.count == 4 && !strcmp(l.names[3], "d.txt") && !strcmp(l.data[3], "delta"), "cut at the fourth folder: four files left (%d)", l.count);

    /* beyond the folders: nothing changes */
    write_file(path, cab, size);
    r = FDITruncateCabinet(fdi, path, 4);
    got = read_file(path, &gsize);
    CHECKF(r && gsize == size && !memcmp(got, cab, size), "a folder number past the last leaves the cabinet alone (%Iu)", gsize);
    free(got);
    r = FDITruncateCabinet(fdi, path, 4000);
    CHECKF(r, "and far past it");
    free(cab);

    /* reserved areas in the header, the folders and the data blocks */
    cab = build_cab(4, four, 6, 1, 0, 0, &size);
    write_file(path, cab, size);
    r = FDITruncateCabinet(fdi, path, 1);
    r = r && list_cab(fdi, path, &l);
    CHECKF(r && l.count == 1 && !strcmp(l.names[0], "a.txt") && !strcmp(l.data[0], "alpha alpha"), "with reserved areas (%d files)", l.count);
    got = read_file(path, &gsize);
    CHECKF(gsize && le32(got + 8) == gsize && le16(got + 0x1A) == 1, "and the size field is right (%Iu)", gsize);
    free(got);
    free(cab);

    /* a cabinet that continues from one and into another */
    cab = build_cab(2, spanning, 4, 0, 1, 1, &size);
    write_file(path, cab, size);
    r = FDITruncateCabinet(fdi, path, 1);
    got = read_file(path, &gsize);
    CHECKF(r && gsize && le16(got + 0x1E) == 1, "the link to the next cabinet goes with the last folder: only PREV is left (flags %u)", got ? le16(got + 0x1E) : 99);
    CHECKF(got && le16(got + 0x1C) == 2 && !strcmp((char *)got + 0x24, "prev.cab") && !strcmp((char *)got + 0x24 + 9, "Disk 1"),
            "the names of the previous cabinet stay (%u files)", got ? le16(got + 0x1C) : 0);
    CHECKF(got && le16(got + 0x1A) == 1 && le32(got + 0x10) == 0x24 + 9 + 7 + 8, "and the folder follows them at once (file table at %u)", got ? le32(got + 0x10) : 0);
    CHECKF(got && le32(got + 8) == gsize, "the size field is the file's");
    free(got);
    free(cab);

    cab = build_cab(2, spanning, 4, 0, 0, 1, &size);
    write_file(path, cab, size);
    r = FDITruncateCabinet(fdi, path, 1);
    got = read_file(path, &gsize);
    CHECKF(r && gsize && le16(got + 0x1E) == 0 && le16(got + 0x1A) == 1 && le32(got + 0x10) == 0x24 + 8,
            "a cabinet with only a next one: no flags and no names (flags %u, files at %u)", got ? le16(got + 0x1E) : 99, got ? le32(got + 0x10) : 0);
    free(got);
    free(cab);

    /* errors */
    write_file(path, (const BYTE *)"not a cabinet at all, no, not at all............", 48);
    r = FDITruncateCabinet(fdi, path, 0);
    CHECKF(!r && erf.erfOper == FDIERROR_NOT_A_CABINET, "not a cabinet (%d)", erf.erfOper);
    snprintf(path, MAX_PATH, "%sfastcabtrunc-none.cab", dir);
    r = FDITruncateCabinet(fdi, path, 0);
    CHECKF(!r && erf.erfOper == FDIERROR_CABINET_NOT_FOUND, "no such file (%d)", erf.erfOper);
    r = FDITruncateCabinet(fdi, NULL, 0);
    CHECKF(!r, "no name");
    r = FDITruncateCabinet(NULL, path, 0);
    CHECKF(!r, "no context");
    (void)i;

    snprintf(path, MAX_PATH, "%sfastcabtrunc.cab", dir);
    DeleteFileA(path);
    FDIDestroy(fdi);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
