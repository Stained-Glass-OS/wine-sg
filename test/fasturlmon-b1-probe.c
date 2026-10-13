#define COBJMACROS
#include <windows.h>
#include <urlmon.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static int fails, checks;
#define CHK(c) do { checks++; if (!(c)) { fails++; P("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
int main(void)
{
    WCHAR path[MAX_PATH], url[MAX_PATH + 16]; HANDLE f; DWORD w; char data[10000]; int i;
    IStream *stream = NULL, *mem; HRESULT hr; ULARGE_INTEGER cb, rd, wr; HGLOBAL hg; void *p;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    for (i = 0; i < (int)sizeof(data); i++) data[i] = (char)('a' + i % 23);
    GetTempPathW(MAX_PATH, path); lstrcatW(path, L"sgum1.bin");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL); WriteFile(f, data, sizeof(data), &w, NULL); CloseHandle(f);
    lstrcpyW(url, L"file:///"); lstrcatW(url, path);
    for (p = url; *(WCHAR*)p; p = (WCHAR*)p + 1) if (*(WCHAR*)p == '\\') *(WCHAR*)p = '/';
    hr = URLOpenBlockingStreamW(NULL, url, &stream, 0, NULL);
    CHK(hr == S_OK && stream);
    if (stream) {
        ULARGE_INTEGER off = {{0}}, len = {{100}};
        CHK(IStream_LockRegion(stream, off, len, LOCK_WRITE) == STG_E_INVALIDFUNCTION);
        CHK(IStream_UnlockRegion(stream, off, len, LOCK_WRITE) == STG_E_INVALIDFUNCTION);
        CreateStreamOnHGlobal(NULL, TRUE, &mem);
        cb.QuadPart = 5000; rd.QuadPart = wr.QuadPart = 0;
        hr = IStream_CopyTo(stream, mem, cb, &rd, &wr);
        CHK(hr == S_OK && rd.QuadPart == 5000 && wr.QuadPart == 5000);
        cb.QuadPart = 1ull << 40;
        hr = IStream_CopyTo(stream, mem, cb, &rd, &wr);
        CHK(hr == S_OK && rd.QuadPart == sizeof(data) - 5000 && wr.QuadPart == rd.QuadPart);
        GetHGlobalFromStream(mem, &hg); p = GlobalLock(hg);
        CHK(p && memcmp(p, data, sizeof(data)) == 0);
        GlobalUnlock(hg);
        IStream_Release(mem); IStream_Release(stream);
    }
    DeleteFileW(path);
    P("checks %d fails %d\nRESULT: %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
