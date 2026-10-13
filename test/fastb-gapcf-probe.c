/* cldapi: sync roots, placeholders, connections, hydration through a provider's FETCH_DATA callback (patch 3627) */
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); f++; } } while (0)
static int f;

typedef struct { DWORD StructSize; const WCHAR *ProviderName, *ProviderVersion; const void *SyncRootIdentity;
    DWORD SyncRootIdentityLength; const void *FileIdentity; DWORD FileIdentityLength; GUID ProviderId; } REG;
typedef struct { DWORD StructSize; USHORT HP, HM, PP, PM; DWORD InSync, HardLink, Mgmt; } POL;
typedef struct { DWORD Type; void (CALLBACK *Callback)(const void *, const void *); } CBREG;
typedef struct { DWORD StructSize; LONGLONG ConnectionKey; void *Context; const WCHAR *VolGuid, *VolDos; DWORD Serial;
    LARGE_INTEGER SyncRootFileId; const void *SyncRootIdentity; DWORD SyncRootIdentityLength; LARGE_INTEGER FileId, FileSize;
    const void *FileIdentity; DWORD FileIdentityLength; const WCHAR *NormalizedPath; LARGE_INTEGER TransferKey;
    UCHAR Priority; void *Corr, *Proc; LARGE_INTEGER RequestKey; } CBINFO;
typedef struct { DWORD ParamSize, pad0; DWORD Flags, pad1; LARGE_INTEGER ReqOff, ReqLen, OptOff, OptLen, LastDehy; DWORD Reason; } CBPAR;
typedef struct { DWORD StructSize; DWORD Type; LONGLONG ConnectionKey; LARGE_INTEGER TransferKey; const void *Corr, *Sync;
    LARGE_INTEGER RequestKey; } OPINFO;
typedef struct { DWORD ParamSize, pad0; DWORD Flags; LONG Status; const void *Buffer; LARGE_INTEGER Offset, Length; } OPPAR;
typedef struct { const WCHAR *Name; FILE_BASIC_INFO Basic; LARGE_INTEGER Size; const void *Id; DWORD IdLen; DWORD Flags;
    HRESULT Result; LONGLONG Usn; } PHC;
typedef struct { DWORD Pin, InSync; LARGE_INTEGER FileId, RootId; DWORD IdLen; BYTE Id[64]; } BASIC;
typedef struct { LARGE_INTEGER Start, Len; } RANGE;

#define E32(x) HRESULT_FROM_WIN32(x)
static HRESULT (WINAPI *pRegister)(const WCHAR *, const REG *, const POL *, DWORD);
static HRESULT (WINAPI *pUnregister)(const WCHAR *);
static HRESULT (WINAPI *pConnect)(const WCHAR *, const CBREG *, const void *, DWORD, LONGLONG *);
static HRESULT (WINAPI *pDisconnect)(LONGLONG);
static HRESULT (WINAPI *pCreate)(const WCHAR *, PHC *, DWORD, DWORD, LONG *);
static HRESULT (WINAPI *pInfo)(HANDLE, DWORD, void *, DWORD, DWORD *);
static HRESULT (WINAPI *pRange)(HANDLE, DWORD, LONGLONG, LONGLONG, void *, DWORD, DWORD *);
static HRESULT (WINAPI *pSync)(HANDLE, DWORD, DWORD, void *);
static HRESULT (WINAPI *pPin)(HANDLE, DWORD, DWORD, void *);
static HRESULT (WINAPI *pHydrate)(HANDLE, LONGLONG, LONGLONG, DWORD, void *);
static HRESULT (WINAPI *pExecute)(const OPINFO *, OPPAR *);
static HRESULT (WINAPI *pTkey)(HANDLE, LONGLONG *);
static HRESULT (WINAPI *pUpdate)(HANDLE, const void *, const void *, DWORD, const void *, DWORD, DWORD, void *, void *);
static HRESULT (WINAPI *pRevert)(HANDLE, DWORD, void *);
static HRESULT (WINAPI *pConvert)(HANDLE, const void *, DWORD, DWORD, void *, void *);
static DWORD (WINAPI *pState)(DWORD, DWORD);

static int fetch_calls, fail_mode, last_len, ident_ok;
static LONGLONG last_off;

static void CALLBACK fetch(const void *info_, const void *par_)
{
    const CBINFO *info = info_;
    const CBPAR *par = par_;
    static BYTE data[10000];
    OPINFO op = {sizeof(op), 0, info->ConnectionKey, info->TransferKey};
    OPPAR p = {sizeof(p)};
    LONGLONG off = par->ReqOff.QuadPart, len = par->ReqLen.QuadPart, done;
    HRESULT hr;
    int i;

    fetch_calls++;
    last_off = off;
    last_len = (int)len;
    ident_ok = info->FileIdentityLength == 4 && !memcmp(info->FileIdentity, "ID-A", 4)
            && info->SyncRootIdentityLength == 3 && !memcmp(info->SyncRootIdentity, "SRI", 3);
    for (i = 0; i < 10000; i++) data[i] = (BYTE)(i * 7 + 3);
    if (fail_mode)
    {
        p.Status = (LONG)0xC0000001;
        hr = pExecute(&op, &p);
        CHECK(hr == S_OK);
        return;
    }
    for (done = 0; done < len; done += 4096)
    {
        LONGLONG n = min(4096, len - done);
        p.Buffer = data + off + done;
        p.Offset.QuadPart = off + done;
        p.Length.QuadPart = n;
        hr = pExecute(&op, &p);
        CHECK(hr == S_OK);
    }
}

int main(void)
{
    HMODULE m = LoadLibraryA("cldapi.dll");
    WCHAR root[MAX_PATH] = L"C:\\sgcf\\root", other[MAX_PATH] = L"C:\\sgcf\\other", path[MAX_PATH];
    REG reg = {sizeof(reg), L"SGProvider", L"1.0", "SRI", 3, "FI", 2, {1, 2, 3, {4, 5, 6, 7, 8, 9, 10, 11}}};
    POL pol = {sizeof(pol)};
    CBREG cbs[2] = {{0, fetch}, {0xffffffff, NULL}};
    PHC ph[2];
    LONG processed = 0;
    LONGLONG key = 0;
    HANDLE h, h2;
    BASIC info;
    DWORD ret;
    BYTE buf[10000];
    RANGE rng[4];
    HRESULT hr;
    int i, ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(m);
    if (!m) goto done;
#define G(n, v) *(FARPROC *)&v = GetProcAddress(m, n); CHECK(v)
    G("CfRegisterSyncRoot", pRegister); G("CfUnregisterSyncRoot", pUnregister); G("CfConnectSyncRoot", pConnect);
    G("CfDisconnectSyncRoot", pDisconnect); G("CfCreatePlaceholders", pCreate); G("CfGetPlaceholderInfo", pInfo);
    G("CfGetPlaceholderRangeInfo", pRange); G("CfSetInSyncState", pSync); G("CfSetPinState", pPin);
    G("CfHydratePlaceholder", pHydrate); G("CfExecute", pExecute); G("CfGetTransferKey", pTkey);
    G("CfUpdatePlaceholder", pUpdate); G("CfRevertPlaceholder", pRevert); G("CfConvertToPlaceholder", pConvert);
    G("CfGetPlaceholderStateFromAttributeTag", pState);

    CreateDirectoryW(L"C:\\sgcf", NULL);
    CreateDirectoryW(root, NULL);
    CreateDirectoryW(other, NULL);

    pUnregister(root);
    hr = pRegister(root, &reg, &pol, 0);
    CHECK(hr == S_OK);
    hr = pRegister(root, &reg, &pol, 0);
    CHECK(hr == E32(378));
    hr = pRegister(root, &reg, &pol, 1);
    CHECK(hr == S_OK);
    hr = pRegister(L"C:\\sgcf\\nonexistent", &reg, &pol, 0);
    CHECK(FAILED(hr));

    hr = pConnect(other, cbs, NULL, 0, &key);
    CHECK(hr == E32(405));
    hr = pConnect(root, cbs, NULL, 0, &key);
    CHECK(hr == S_OK && key);
    hr = pConnect(root, cbs, NULL, 0, &key);
    CHECK(hr == E32(378));

    memset(ph, 0, sizeof(ph));
    ph[0].Name = L"a.txt"; ph[0].Size.QuadPart = 10000; ph[0].Id = "ID-A"; ph[0].IdLen = 4;
    ph[1].Name = L"sub"; ph[1].Basic.FileAttributes = FILE_ATTRIBUTE_DIRECTORY; ph[1].Id = "ID-S"; ph[1].IdLen = 4;
    hr = pCreate(root, ph, 2, 0, &processed);
    CHECK(hr == S_OK && processed == 2 && ph[0].Result == S_OK && ph[1].Result == S_OK);
    hr = pCreate(root, ph, 1, 0, &processed);
    CHECK(FAILED(hr) && ph[0].Result == E32(ERROR_ALREADY_EXISTS));
    hr = pCreate(other, ph, 1, 0, &processed);
    CHECK(hr == E32(390));
    CHECK(GetFileAttributesW(L"C:\\sgcf\\root\\sub") & FILE_ATTRIBUTE_DIRECTORY);

    h = CreateFileW(L"C:\\sgcf\\root\\a.txt", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(h != INVALID_HANDLE_VALUE);
    CHECK(GetFileSize(h, NULL) == 10000);
    memset(&info, 0xcc, sizeof(info));
    hr = pInfo(h, 0, &info, sizeof(info), &ret);
    CHECK(hr == S_OK && info.IdLen == 4 && !memcmp(info.Id, "ID-A", 4) && info.InSync == 0 && info.Pin == 0);
    CHECK(info.FileId.QuadPart != 0 && info.RootId.QuadPart != 0);
    hr = pSync(h, 1, 0, NULL);
    CHECK(hr == S_OK);
    hr = pPin(h, 1, 0, NULL);
    CHECK(hr == S_OK);
    hr = pInfo(h, 0, &info, sizeof(info), &ret);
    CHECK(hr == S_OK && info.InSync == 1 && info.Pin == 1);
    hr = pInfo(h, 0, &info, 8, &ret);
    CHECK(FAILED(hr));

    hr = pRange(h, 1, 0, 10000, rng, sizeof(rng), &ret);
    CHECK(hr == S_OK && ret == 0);

    hr = pHydrate(h, 0, -1, 0, NULL);
    CHECK(hr == S_OK);
    CHECK(fetch_calls == 1 && last_off == 0 && last_len == 10000 && ident_ok);
    SetFilePointer(h, 0, NULL, FILE_BEGIN);
    ok = ReadFile(h, buf, 10000, &ret, NULL) && ret == 10000;
    CHECK(ok);
    for (i = 0; ok && i < 10000; i++) if (buf[i] != (BYTE)(i * 7 + 3)) { CHECK(!"content"); break; }
    hr = pRange(h, 1, 0, 10000, rng, sizeof(rng), &ret);
    CHECK(hr == S_OK && ret == sizeof(RANGE) && rng[0].Start.QuadPart == 0 && rng[0].Len.QuadPart == 10000);
    hr = pHydrate(h, 0, 4096, 0, NULL);
    CHECK(hr == S_OK && fetch_calls == 1); /* already on disk */

    /* a transfer outside a request, with an unaligned offset */
    {
        LONGLONG tk = 0;
        OPINFO op = {sizeof(op), 0, key};
        OPPAR p = {sizeof(p)};
        BYTE b[100] = {0};

        hr = pTkey(h, &tk);
        CHECK(hr == S_OK && tk);
        op.TransferKey.QuadPart = tk;
        p.Buffer = b; p.Offset.QuadPart = 100; p.Length.QuadPart = 100;
        hr = pExecute(&op, &p);
        CHECK(hr == E32(380));
        op.ConnectionKey = key + 1000;
        hr = pExecute(&op, &p);
        CHECK(hr == E32(ERROR_INVALID_HANDLE));
    }

    /* dehydrate */
    hr = pUpdate(h, NULL, NULL, 0, NULL, 0, 0x4, NULL, NULL);
    CHECK(hr == S_OK);
    CHECK(GetFileSize(h, NULL) == 10000);
    hr = pRange(h, 1, 0, 10000, rng, sizeof(rng), &ret);
    CHECK(hr == S_OK && ret == 0);

    /* the provider fails the request */
    fail_mode = 1;
    hr = pHydrate(h, 0, 4096, 0, NULL);
    CHECK(hr == HRESULT_FROM_NT(0xC0000001));
    fail_mode = 0;

    /* reverting a file that is not all on disk fails */
    hr = pRevert(h, 0, NULL);
    CHECK(hr == E32(377));
    hr = pHydrate(h, 0, 10000, 0, NULL);
    CHECK(hr == S_OK);
    hr = pRevert(h, 0, NULL);
    CHECK(hr == S_OK);
    hr = pInfo(h, 0, &info, sizeof(info), &ret);
    CHECK(hr == E32(376));
    hr = pHydrate(h, 0, 4096, 0, NULL);
    CHECK(hr == E32(376));
    CloseHandle(h);

    /* converting an existing file */
    h2 = CreateFileW(L"C:\\sgcf\\root\\plain.txt", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(h2, "hello", 5, &ret, NULL);
    hr = pConvert(h2, "ID-P", 4, 1, NULL, NULL);
    CHECK(hr == S_OK);
    hr = pInfo(h2, 0, &info, sizeof(info), &ret);
    CHECK(hr == S_OK && info.IdLen == 4 && info.InSync == 1);
    hr = pRange(h2, 1, 0, 100, rng, sizeof(rng), &ret);
    CHECK(hr == S_OK && ret == sizeof(RANGE) && rng[0].Len.QuadPart == 5);
    CloseHandle(h2);

    CHECK(pState(0, 0x80000007) == 0);

    hr = pDisconnect(key);
    CHECK(hr == S_OK);
    hr = pDisconnect(key);
    CHECK(hr == E32(ERROR_INVALID_HANDLE));
    hr = pUnregister(root);
    CHECK(hr == S_OK);
    hr = pUnregister(root);
    CHECK(hr == E32(405));
    hr = pConnect(root, cbs, NULL, 0, &key);
    CHECK(hr == E32(405));

done:
    printf("RESULT: %s\n", f ? "FAIL" : "PASS");
    return f != 0;
}
