/* cmdcomplete-probe: for cmdcomplete-gate.sh (issue #14, Tab completion in cmd).
 *   cmdcomplete-probe.exe DIR LINE...
 * Starts "cmd.exe /q /k prompt $G" in DIR on a pseudo console (conhost, as a terminal
 * runs it) and types each LINE followed by Enter, as keys: "{T}" in a LINE is Tab, "{S}"
 * Shift+Tab (the terminal's ESC [ Z). Then "exit". Prints "DONE exit N" or "DONE timeout".
 * The gate reads what the commands wrote to files.
 * Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later */
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000006
#include <windows.h>
#include <stdio.h>
#include <string.h>

static DWORD WINAPI drain(void *arg)
{
    char buf[4096];
    DWORD n;
    FILE *log = getenv("CMDCOMPLETE_LOG") ? fopen(getenv("CMDCOMPLETE_LOG"), "wb") : NULL;
    while (ReadFile(arg, buf, sizeof(buf), &n, NULL) && n)
        if (log) { fwrite(buf, 1, n, log); fflush(log); }
    if (log) fclose(log);
    return 0;
}

static void type(HANDLE in, const char *s)
{
    char out[4096];
    size_t o = 0;
    DWORD n;
    for (; *s && o < sizeof(out) - 4; s++)
    {
        if (!strncmp(s, "{T}", 3)) { out[o++] = '\t'; s += 2; }
        else if (!strncmp(s, "{S}", 3)) { memcpy(out + o, "\x1b[Z", 3); o += 3; s += 2; }
        else out[o++] = *s;
    }
    out[o++] = '\r';
    WriteFile(in, out, o, &n, NULL);
}

int main(int argc, char **argv)
{
    char cmdline[] = "cmd.exe /q /k prompt $G";
    HANDLE in_read, in_write, out_read, out_write;
    HPCON hpc;
    STARTUPINFOEXA si = { { sizeof(si) } };
    PROCESS_INFORMATION pi;
    SIZE_T size;
    COORD c = { 100, 30 };
    DWORD code = 0;
    int i;

    if (argc < 2) return 2;
    CreatePipe(&in_read, &in_write, NULL, 0);
    CreatePipe(&out_read, &out_write, NULL, 0);
    if (CreatePseudoConsole(c, in_read, out_write, 0, &hpc) != S_OK) { printf("DONE nopty\n"); return 1; }
    CloseHandle(in_read);
    CloseHandle(out_write);
    CreateThread(NULL, 0, drain, out_read, 0, NULL);
    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, size);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL);
    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, argv[1],
                        &si.StartupInfo, &pi)) { printf("DONE nocmd %lu\n", GetLastError()); return 1; }
    Sleep(1500);
    for (i = 2; i < argc; i++)
    {
        type(in_write, argv[i]);
        Sleep(700);
    }
    type(in_write, "exit");
    if (WaitForSingleObject(pi.hProcess, 30000)) { printf("DONE timeout\n"); TerminateProcess(pi.hProcess, 1); }
    else { GetExitCodeProcess(pi.hProcess, &code); printf("DONE exit %lu\n", code); }
    ClosePseudoConsole(hpc);
    return 0;
}
