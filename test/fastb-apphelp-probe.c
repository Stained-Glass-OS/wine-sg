/* apphelp: SdbSetPermLayerKeys / SdbGetPermLayerKeys / SetPermLayerState on the AppCompatFlags\Layers key the
 * Compatibility tab and CreateProcess use (patch 3724) */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static BOOL (WINAPI *pSet)(const WCHAR *, const WCHAR *, BOOL);
static BOOL (WINAPI *pGet)(const WCHAR *, WCHAR *, DWORD *, DWORD);
static BOOL (WINAPI *pState)(const WCHAR *, const WCHAR *, DWORD, BOOL, BOOL);

#define KEY L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers"
static const WCHAR path[] = L"C:\\fastbapp\\game.exe";

static BOOL reg_get(HKEY root, WCHAR *out, DWORD cb)
{
    HKEY k; DWORD type, size = cb; LONG e;
    if (RegOpenKeyExW(root, KEY, 0, KEY_READ, &k)) return FALSE;
    e = RegQueryValueExW(k, path, NULL, &type, (BYTE *)out, &size);
    RegCloseKey(k);
    return !e && type == REG_SZ;
}

static void check_layers(const WCHAR *want, BOOL machine, int line)
{
    WCHAR buf[256] = L"?";
    BOOL has = reg_get(machine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER, buf, sizeof(buf));
    if (want) { if (!has || wcscmp(buf, want)) { f++; printf("FAIL line %d: registry has '%ls' (%d), want '%ls'\n", line, has ? buf : L"-", has, want); } }
    else if (has) { f++; printf("FAIL line %d: registry still has '%ls'\n", line, buf); }
}
#define LAYERS(w, m) check_layers(w, m, __LINE__)

int main(void)
{
    HMODULE m = LoadLibraryA("apphelp.dll");
    WCHAR buf[256]; DWORD size; BOOL ret;

    pSet = (void *)GetProcAddress(m, "SdbSetPermLayerKeys");
    pGet = (void *)GetProcAddress(m, "SdbGetPermLayerKeys");
    pState = (void *)GetProcAddress(m, "SetPermLayerState");
    if (!pSet || !pGet || !pState) { printf("FAIL exports\nRESULT: FAIL\n"); return 1; }
    pSet(path, NULL, FALSE); pSet(path, NULL, TRUE);

    /* set / get */
    ret = pSet(path, L"WIN98 RUNASADMIN", FALSE); CHECK(ret, "set %lu", GetLastError());
    LAYERS(L"WIN98 RUNASADMIN", FALSE);
    size = sizeof(buf); memset(buf, 0, sizeof(buf));
    ret = pGet(path, buf, &size, 1);
    CHECK(ret && !wcscmp(buf, L"WIN98 RUNASADMIN") && size == (wcslen(L"WIN98 RUNASADMIN") + 1) * 2, "get %d '%ls' %lu", ret, buf, size);
    size = 4; SetLastError(0); ret = pGet(path, buf, &size, 1);
    CHECK(!ret && GetLastError() == ERROR_INSUFFICIENT_BUFFER && size == (wcslen(L"WIN98 RUNASADMIN") + 1) * 2, "small buffer %d %lu size %lu", ret, GetLastError(), size);
    size = sizeof(buf); ret = pGet(path, buf, &size, 2); CHECK(!ret, "no machine layers yet");
    SetLastError(0); size = sizeof(buf); ret = pGet(L"C:\\fastbapp\\other.exe", buf, &size, 3);
    CHECK(!ret && GetLastError() == ERROR_NOT_FOUND, "unknown program %d %lu", ret, GetLastError());
    SetLastError(0); ret = pGet(NULL, buf, &size, 1); CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "NULL path");
    SetLastError(0); size = sizeof(buf); ret = pGet(path, buf, &size, 8); CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "bad flags");
    SetLastError(0); ret = pSet(NULL, L"X", FALSE); CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "set NULL path");

    /* machine layers, user layers win */
    ret = pSet(path, L"WINXPSP3", TRUE); CHECK(ret, "set machine %lu", GetLastError());
    LAYERS(L"WINXPSP3", TRUE);
    size = sizeof(buf); ret = pGet(path, buf, &size, 2); CHECK(ret && !wcscmp(buf, L"WINXPSP3"), "machine layers '%ls'", buf);
    size = sizeof(buf); ret = pGet(path, buf, &size, 0); CHECK(ret && !wcscmp(buf, L"WIN98 RUNASADMIN"), "user layers win: '%ls'", buf);
    pSet(path, NULL, FALSE);
    LAYERS(NULL, FALSE);
    size = sizeof(buf); ret = pGet(path, buf, &size, 3); CHECK(ret && !wcscmp(buf, L"WINXPSP3"), "falls back to the machine's: '%ls'", buf);
    size = sizeof(buf); ret = pGet(path, buf, &size, 1); CHECK(!ret, "user flag only: none");
    pSet(path, L"", TRUE);
    LAYERS(NULL, TRUE);

    /* add and remove */
    ret = pState(path, L"WIN98", 0, FALSE, TRUE); CHECK(ret, "add");
    LAYERS(L"WIN98", FALSE);
    ret = pState(path, L"RUNASADMIN", 0, FALSE, TRUE); LAYERS(L"WIN98 RUNASADMIN", FALSE);
    ret = pState(path, L"win98", 0, FALSE, TRUE); LAYERS(L"WIN98 RUNASADMIN", FALSE);   /* no duplicate, case-blind */
    ret = pState(path, L"WIN95 256COLOR", 0, FALSE, TRUE); LAYERS(L"WIN98 RUNASADMIN WIN95 256COLOR", FALSE);
    ret = pState(path, L"Win98 256color", 0, FALSE, FALSE); CHECK(ret, "remove"); LAYERS(L"RUNASADMIN WIN95", FALSE);
    ret = pState(path, L"NOTTHERE", 0, FALSE, FALSE); CHECK(ret, "remove a layer that is not there"); LAYERS(L"RUNASADMIN WIN95", FALSE);
    ret = pState(path, L"RUNASADMIN WIN95", 0, FALSE, FALSE); LAYERS(NULL, FALSE);   /* the last layer takes the value away */
    ret = pState(path, L"HIGHDPIAWARE", 0, TRUE, TRUE); LAYERS(L"HIGHDPIAWARE", TRUE);
    LAYERS(NULL, FALSE);
    SetLastError(0); ret = pState(path, NULL, 0, FALSE, TRUE); CHECK(!ret && GetLastError() == ERROR_INVALID_PARAMETER, "no layers");
    pSet(path, NULL, TRUE);
    LAYERS(NULL, TRUE);
    {
        void (WINAPI *pDir)(void *, WCHAR *, DWORD) = (void *)GetProcAddress(m, "SdbGetAppPatchDir");
        WCHAR win[MAX_PATH], want[MAX_PATH];
        GetWindowsDirectoryW(win, MAX_PATH); swprintf(want, MAX_PATH, L"%ls\\AppPatch", win);
        wcscpy(buf, L"junk"); pDir(NULL, buf, 256); CHECK(!wcscmp(buf, want), "AppPatch dir '%ls'", buf);
        wcscpy(buf, L"junk"); pDir(NULL, buf, 4); CHECK(!buf[0], "too small a buffer gives an empty string");
    }
    {
        /* ShimFlushCache: "the caller must be an administrator" */
        BOOL (WINAPI *pFlush)(HWND, HINSTANCE, const char *, int) = (void *)GetProcAddress(m, "ShimFlushCache");
        BYTE sidbuf[SECURITY_MAX_SID_SIZE]; DWORD sl = sizeof(sidbuf); SID_AND_ATTRIBUTES dis;
        HANDLE tok = NULL, restricted = NULL, imp = NULL; BOOL admin = FALSE;

        CHECK(pFlush != NULL, "no ShimFlushCache");
        CreateWellKnownSid(WinBuiltinAdministratorsSid, NULL, sidbuf, &sl);
        CheckTokenMembership(NULL, sidbuf, &admin);
        CHECK(admin, "this test runs as an administrator");
        SetLastError(0xdeadbeef);
        if (pFlush) { ret = pFlush(NULL, NULL, NULL, 0); CHECK(ret, "administrator: ShimFlushCache %d %lu", ret, GetLastError()); }
        dis.Sid = sidbuf; dis.Attributes = 0;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &tok) &&
            CreateRestrictedToken(tok, 0, 1, &dis, 0, NULL, 0, NULL, &restricted) &&
            DuplicateTokenEx(restricted, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenImpersonation, &imp) &&
            SetThreadToken(NULL, imp))
        {
            BOOL still = TRUE;
            CheckTokenMembership(NULL, sidbuf, &still);
            CHECK(!still, "the restricted token is no administrator");
            if (pFlush)
            {
                SetLastError(0xdeadbeef);
                ret = pFlush(NULL, NULL, NULL, 0);
                CHECK(!ret && GetLastError() == ERROR_ACCESS_DENIED, "non administrator: ShimFlushCache %d %lu", ret, GetLastError());
            }
            SetThreadToken(NULL, NULL);
        }
        else CHECK(0, "no restricted token: %lu", GetLastError());
    }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
