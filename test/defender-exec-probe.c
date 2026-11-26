/* SG Defender's real-time protection on execute (wine-sg 9456): a program, a DLL,
 * an installer package, a batch file and a script are scanned before they run,
 * and AmsiScanBuffer/AmsiScanString reach the same engine. The EICAR test
 * string stands in for malware (test/defender-exec-gate.sh; a stand-in service).
 *   defender-exec-probe.exe DIR [allowed]   DIR holds the gate's files;
 *   "allowed": the service is down or too slow -- everything must run */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } \
                           else { printf("ok  %s\n", #c); } } while (0)

static WCHAR dir[MAX_PATH];

static const WCHAR *at(const WCHAR *name)
{
    static WCHAR buf[4][MAX_PATH];
    static int n;
    WCHAR *p = buf[n++ & 3];
    swprintf(p, MAX_PATH, L"%ls\\%ls", dir, name);
    return p;
}

/* start CMD, wait, its exit code (or -1 and the error) */
static DWORD run(const WCHAR *cmd, DWORD *err)
{
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR line[1024];
    DWORD code = (DWORD)-1;
    lstrcpynW(line, cmd, ARRAYSIZE(line));
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = GetStdHandle(STD_OUTPUT_HANDLE);
    *err = 0;
    if (!CreateProcessW(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) { *err = GetLastError(); return code; }
    WaitForSingleObject(pi.hProcess, 60000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return code;
}

typedef HRESULT (WINAPI *init_t)(const WCHAR *, void **);
typedef HRESULT (WINAPI *scanbuf_t)(void *, void *, ULONG, const WCHAR *, void *, int *);
typedef HRESULT (WINAPI *scanstr_t)(void *, const WCHAR *, const WCHAR *, void *, int *);
typedef UINT (WINAPI *openpkg_t)(const WCHAR *, ULONG *);

int wmain(int argc, WCHAR **argv)
{
    BOOL allowed = argc > 2 && !wcscmp(argv[2], L"allowed");
    WCHAR cmd[1024], msg[512];
    DWORD err, code;
    HMODULE h;
    init_t pInit;
    scanbuf_t pScanBuf;
    scanstr_t pScanStr;
    openpkg_t pOpenPkg;
    void *ctx = NULL;
    int result;
    ULONG pkg = 0;
    UINT r;
    /* the test string, put together here: the probe itself must not hold it (it is scanned when it starts) */
    char eicar[80];

    setvbuf(stdout, NULL, _IONBF, 0);
    strcpy(eicar, "X5O!P%@AP[4\\PZX54(P^)7CC)7}$EICAR");
    strcat(eicar, "-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*");
    if (argc < 2) return 2;
    lstrcpynW(dir, argv[1], MAX_PATH);

    /* a program */
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", at(L"clean.exe"));
    code = run(cmd, &err);
    CHECK(code == 42, "a clean program runs (exit %lu, error %lu)", code, err);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\"", at(L"infected.exe"));
    code = run(cmd, &err);
    if (allowed)
        CHECK(code == 42, "without SG Defender the program runs (exit %lu, error %lu)", code, err);
    else {
        CHECK(code == (DWORD)-1 && err == ERROR_VIRUS_INFECTED, "an infected program is not started: ERROR_VIRUS_INFECTED (exit %lu, error %lu)", code, err);
        msg[0] = 0;
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, ERROR_VIRUS_INFECTED, 0, msg, ARRAYSIZE(msg), NULL);
        CHECK(wcsstr(msg, L"contains a virus") != NULL, "...and the error says why (%ls)", msg);
        CHECK(GetFileAttributesW(at(L"infected.exe")) == INVALID_FILE_ATTRIBUTES, "...and the file is gone (quarantined)");
    }

    /* a DLL */
    h = LoadLibraryW(at(L"clean.dll"));
    CHECK(h != NULL, "a clean DLL loads (error %lu)", GetLastError());
    if (h) FreeLibrary(h);
    SetLastError(0);
    h = LoadLibraryW(at(L"infected.dll"));
    if (allowed) CHECK(h != NULL, "without SG Defender the DLL loads (error %lu)", GetLastError());
    else CHECK(!h && GetLastError() == ERROR_VIRUS_INFECTED, "an infected DLL is not loaded: ERROR_VIRUS_INFECTED (%lu)", GetLastError());

    /* a batch file: cmd scans it first */
    swprintf(cmd, ARRAYSIZE(cmd), L"cmd.exe /c \"%ls\"", at(L"clean.bat"));
    code = run(cmd, &err);
    CHECK(code == 7, "a clean batch file runs (exit %lu)", code);
    swprintf(cmd, ARRAYSIZE(cmd), L"cmd.exe /c \"%ls\"", at(L"infected.bat"));
    code = run(cmd, &err);
    if (allowed) CHECK(code == 7, "without SG Defender the batch file runs (exit %lu)", code);
    else CHECK(code == ERROR_VIRUS_INFECTED, "an infected batch file is not run: errorlevel 225 (exit %lu)", code);

    /* a script: cscript scans it first */
    swprintf(cmd, ARRAYSIZE(cmd), L"cscript.exe //nologo \"%ls\"", at(L"clean.vbs"));
    code = run(cmd, &err);
    CHECK(code == 5, "a clean script runs (exit %lu)", code);
    swprintf(cmd, ARRAYSIZE(cmd), L"cscript.exe //nologo \"%ls\"", at(L"infected.vbs"));
    code = run(cmd, &err);
    if (allowed) CHECK(code == 5, "without SG Defender the script runs (exit %lu)", code);
    else CHECK(code == ERROR_VIRUS_INFECTED, "an infected script is not run (exit %lu)", code);

    /* an installer package: scanned before it is opened */
    pOpenPkg = (openpkg_t)GetProcAddress(LoadLibraryW(L"msi.dll"), "MsiOpenPackageW");
    r = pOpenPkg(at(L"clean.msi"), &pkg);
    CHECK(r != ERROR_VIRUS_INFECTED, "a clean package is opened (not a real one: %u)", r);
    r = pOpenPkg(at(L"infected.msi"), &pkg);
    if (allowed) CHECK(r != ERROR_VIRUS_INFECTED, "without SG Defender the package is opened (%u)", r);
    else CHECK(r == ERROR_VIRUS_INFECTED, "an infected package is not opened: ERROR_VIRUS_INFECTED (%u)", r);

    /* AMSI: SG Defender is the system's provider */
    h = LoadLibraryW(L"amsi.dll");
    pInit = (init_t)GetProcAddress(h, "AmsiInitialize");
    pScanBuf = (scanbuf_t)GetProcAddress(h, "AmsiScanBuffer");
    pScanStr = (scanstr_t)GetProcAddress(h, "AmsiScanString");
    CHECK(pInit && pInit(L"PowerShell_C:\\probe", &ctx) == S_OK, "AmsiInitialize");
    result = 0;
    CHECK(pScanBuf(ctx, eicar, strlen(eicar), L"script.ps1", NULL, &result) == S_OK, "AmsiScanBuffer");
    if (allowed) CHECK(result < 32768, "without SG Defender nothing is detected (%d)", result);
    else CHECK(result >= 32768, "AmsiScanBuffer: the test string is detected (%d)", result);
    {
        WCHAR wide[100];
        MultiByteToWideChar(CP_ACP, 0, eicar, -1, wide, ARRAYSIZE(wide));
        result = 0;
        CHECK(pScanStr(ctx, wide, L"script.ps1", NULL, &result) == S_OK, "AmsiScanString");
        if (!allowed) CHECK(result >= 32768, "AmsiScanString (UTF-16 text): detected (%d)", result);
        result = 0;
        pScanStr(ctx, L"Write-Host 'hello'", L"script.ps1", NULL, &result);
        CHECK(result < 32768, "clean script text is not detected (%d)", result);
    }
    printf(f ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return f ? 1 : 0;
}
