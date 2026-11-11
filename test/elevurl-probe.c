/* elevurl-probe: the gate's program and https handler (test/elevurl-gate.sh).
 *   elevurl-probe open URL   ShellExecuteEx(URL); prints "OPEN ok|fail <error>"
 *   elevurl-probe handle URL (the https handler) appends
 *                            "<URL> elevated <0|1> <SID>" to %SG_GATE_LOG%
 *   elevurl-probe shell      waits up to 60 s for this person's shell to serve
 *                            elevated programs' pages; prints "SHELL yes|no"
 *   elevurl-probe squat SID  (another account) serves SID's pipe for 60 s,
 *                            logging "squatter <URL>" for what it is sent
 *   elevurl-probe send SID URL (another account) sends SID's shell a URL;
 *                            prints "SEND ok <answer>" or "SEND fail <error>" */
#include <windows.h>
#include <shellapi.h>
#include <sddl.h>
#include <stdio.h>

int wmain(int argc, WCHAR **argv)
{
    if (argc == 3 && !wcscmp(argv[1], L"open"))
    {
        SHELLEXECUTEINFOW sei = { sizeof(sei) };
        BOOL ok;
        sei.fMask = SEE_MASK_FLAG_NO_UI;
        sei.lpFile = argv[2];
        sei.nShow = SW_SHOWNORMAL;
        ok = ShellExecuteExW(&sei);
        printf("OPEN %s %lu\n", ok ? "ok" : "fail", ok ? 0 : GetLastError());
        return 0;
    }
    if (argc == 3 && !wcscmp(argv[1], L"handle"))
    {
        BYTE buf[256];
        TOKEN_ELEVATION el = { 0 };
        WCHAR log[MAX_PATH], *sid = NULL;
        HANDLE token;
        DWORD len;
        FILE *f;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return 1;
        GetTokenInformation(token, TokenElevation, &el, sizeof(el), &len);
        if (GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len))
            ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &sid);
        if (!GetEnvironmentVariableW(L"SG_GATE_LOG", log, MAX_PATH) || !(f = _wfopen(log, L"a"))) return 1;
        fprintf(f, "%ls elevated %lu %ls\n", argv[2], el.TokenIsElevated, sid ? sid : L"?");
        fclose(f);
        return 0;
    }
    if (argc == 2 && !wcscmp(argv[1], L"shell"))
    {
        BYTE buf[256];
        WCHAR name[128], *sid = NULL;
        HANDLE token;
        DWORD len;
        int i;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token) ||
            !GetTokenInformation(token, TokenUser, buf, sizeof(buf), &len) ||
            !ConvertSidToStringSidW(((TOKEN_USER *)buf)->User.Sid, &sid)) return 1;
        swprintf(name, 128, L"\\\\.\\pipe\\sg-unelevated-open-%ls", sid);
        for (i = 0; i < 120; i++)
        {
            if (WaitNamedPipeW(name, 500) || GetLastError() == ERROR_SEM_TIMEOUT) { printf("SHELL yes\n"); return 0; }
            Sleep(500);
        }
        printf("SHELL no\n");
        return 0;
    }
    if (argc == 3 && !wcscmp(argv[1], L"squat"))
    {
        WCHAR name[128], url[2048], log[MAX_PATH];
        DWORD got, answer = 42, done, start = GetTickCount();
        HANDLE p;
        FILE *f;
        swprintf(name, 128, L"\\\\.\\pipe\\sg-unelevated-open-%ls", argv[2]);
        p = CreateNamedPipeW(name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                             1, 4096, 4096, 0, NULL);
        printf("SQUAT %s\n", p == INVALID_HANDLE_VALUE ? "no" : "ready");
        fflush(stdout);
        if (p == INVALID_HANDLE_VALUE) return 1;
        while (GetTickCount() - start < 60000)
        {
            if (!ConnectNamedPipe(p, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) continue;
            if (ReadFile(p, url, sizeof(url) - 2, &got, NULL))
            {
                url[got / 2] = 0;
                if (GetEnvironmentVariableW(L"SG_GATE_LOG", log, MAX_PATH) && (f = _wfopen(log, L"a")))
                {
                    fprintf(f, "squatter %ls end\n", url);
                    fclose(f);
                }
                WriteFile(p, &answer, sizeof(answer), &done, NULL);
            }
            DisconnectNamedPipe(p);
        }
        return 0;
    }
    if (argc == 4 && !wcscmp(argv[1], L"send"))
    {
        WCHAR name[128];
        DWORD mode = PIPE_READMODE_MESSAGE, done, answer = 0;
        HANDLE p;
        swprintf(name, 128, L"\\\\.\\pipe\\sg-unelevated-open-%ls", argv[2]);
        p = CreateFileW(name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (p == INVALID_HANDLE_VALUE) { printf("SEND fail %lu\n", GetLastError()); return 0; }
        SetNamedPipeHandleState(p, &mode, NULL, NULL);
        if (!WriteFile(p, argv[3], (lstrlenW(argv[3]) + 1) * 2, &done, NULL) ||
            !ReadFile(p, &answer, sizeof(answer), &done, NULL)) { printf("SEND fail %lu\n", GetLastError()); return 0; }
        printf("SEND ok %lu\n", answer);
        return 0;
    }
    return 2;
}
