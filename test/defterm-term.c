/* A stand-in default terminal for test/defterm-gate.sh: started as kernelbase
 * starts the terminal for a new console (--sg-handoff SERVER --title T), it
 * records its arguments, runs a headless conhost on the handed server -- as
 * sg-terminal does for a tab -- types a command into it and keeps what the
 * console writes until conhost ends with the program.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR cl, int show)
{
    int argc, i;
    WCHAR **argv = CommandLineToArgvW(GetCommandLineW(), &argc), cmd[MAX_PATH + 200], sys[MAX_PATH], name[64];
    HANDLE server = NULL, in_r, in_w, out_r, out_w, sig_r, sig_w, list[4];
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    SIZE_T len = 0;
    FILE *f;
    char buf[4096];
    DWORD n;
    static const char typed[] = "set /a 40+2\rexit 7\r";     /* Enter is CR, as a terminal sends it */

    (void)inst; (void)prev; (void)cl; (void)show;
    if ((f = _wfopen(L"C:\\defterm-args.txt", L"w"))) {
        for (i = 1; i < argc; i++) fwprintf(f, L"%ls\n", argv[i]);
        fclose(f);
    }
    for (i = 1; i + 1 < argc; i++) if (!wcscmp(argv[i], L"--sg-handoff")) server = (HANDLE)(ULONG_PTR)wcstoul(argv[i + 1], NULL, 0);
    if (!server) return 1;
    CreatePipe(&in_r, &in_w, &sa, 0); CreatePipe(&out_r, &out_w, &sa, 0);
    /* conhost reads its signal pipe asynchronously: an overlapped pipe, as CreatePseudoConsole makes */
    swprintf(name, ARRAYSIZE(name), L"\\\\.\\pipe\\sg_defterm_signal%lx", GetCurrentProcessId());
    sig_r = CreateNamedPipeW(name, PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE, 1, 4096, 4096, 0, &sa);
    sig_w = CreateFileW(name, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    SetHandleInformation(in_w, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(sig_w, HANDLE_FLAG_INHERIT, 0);
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = in_r;
    si.StartupInfo.hStdOutput = si.StartupInfo.hStdError = out_w;
    list[0] = in_r; list[1] = out_w; list[2] = sig_r; list[3] = server;
    InitializeProcThreadAttributeList(NULL, 1, 0, &len);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, len);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &len);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, list, sizeof(list), NULL, NULL);
    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\\conhost.exe\" --headless --width 80 --height 25 --signal 0x%lx --server 0x%lx",
             sys, (unsigned long)(ULONG_PTR)sig_r, (unsigned long)(ULONG_PTR)server);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, TRUE, EXTENDED_STARTUPINFO_PRESENT | DETACHED_PROCESS, NULL, NULL, &si.StartupInfo, &pi))
        return 2;
    CloseHandle(in_r); CloseHandle(out_w); CloseHandle(sig_r); CloseHandle(server);
    Sleep(1500);
    WriteFile(in_w, typed, sizeof(typed) - 1, &n, NULL);
    if (!(f = _wfopen(L"C:\\defterm-out.txt", L"wb"))) return 3;
    while (ReadFile(out_r, buf, sizeof(buf), &n, NULL) && n) { fwrite(buf, 1, n, f); fflush(f); }
    fclose(f);
    WaitForSingleObject(pi.hProcess, 10000);
    return 0;
}
