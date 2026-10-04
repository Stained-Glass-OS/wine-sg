/* test/bashpty-gate.sh's probe: bash.exe on a pseudo console, as a Terminal
 * tab runs PowerShell -- there is no Unix terminal under it. What is typed
 * reaches bash, its output comes back, the terminal's size follows the
 * console's, and bash's exit status is bash.exe's. The console's output goes
 * to C:\bashpty-out.txt.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000006
#include <windows.h>
#include <stdio.h>

static HANDLE out_r;
static DWORD WINAPI reader(void *arg)
{
    FILE *f = _wfopen(L"C:\\bashpty-out.txt", L"wb");
    char buf[4096];
    DWORD n;
    (void)arg;
    while (ReadFile(out_r, buf, sizeof(buf), &n, NULL) && n) { fwrite(buf, 1, n, f); fflush(f); }
    fclose(f);
    return 0;
}

static void type(HANDLE in, const char *s) { DWORD n; WriteFile(in, s, (DWORD)strlen(s), &n, NULL); Sleep(1500); }

int main(void)
{
    HANDLE in_r, in_w, out_w;
    HPCON pc;
    COORD size = { 100, 30 }, small = { 90, 20 };
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    SIZE_T len = 0;
    WCHAR cmd[] = L"C:\\windows\\system32\\bash.exe";
    DWORD code = 999;
    WCHAR profile[MAX_PATH] = L"";

    CreatePipe(&in_r, &in_w, NULL, 0);
    CreatePipe(&out_r, &out_w, NULL, 0);
    if (FAILED(CreatePseudoConsole(size, in_r, out_w, 0, &pc))) { printf("NOPCON\n"); return 1; }
    CloseHandle(CreateThread(NULL, 0, reader, NULL, 0, NULL));
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    InitializeProcThreadAttributeList(NULL, 1, 0, &len);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, len);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &len);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pc, sizeof(pc), NULL, NULL);
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);     /* where a Terminal tab starts */
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, profile[0] ? profile : NULL,
                        &si.StartupInfo, &pi)) {
        printf("NOSTART %lu\n", GetLastError()); return 1;
    }
    Sleep(4000);    /* a login shell starts */
    type(in_w, "echo SUM-$((6*7))\r");
    type(in_w, "echo HERE-$PWD\r");
    type(in_w, "stty size\r");
    ResizePseudoConsole(pc, small); Sleep(1500);
    type(in_w, "stty size\r");
    type(in_w, "exit 5\r");
    if (WaitForSingleObject(pi.hProcess, 20000) == WAIT_TIMEOUT) printf("HUNG\n");
    GetExitCodeProcess(pi.hProcess, &code);
    printf("EXIT %lu\n", code);
    ClosePseudoConsole(pc);
    Sleep(500);
    return 0;
}
