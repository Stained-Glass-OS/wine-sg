/* test/bashpty-gate.sh's second probe: the console's scroll region (CSI r)
 * and repeat (CSI b), which full-screen Linux programs use (htop, vi) once
 * bash runs in a Terminal tab. It runs itself on a pseudo console; the child
 * writes the sequences and reads the screen back.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000006
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void row(HANDLE out, short y, char *buf, int n)
{
    WCHAR w[128];
    DWORD got = 0;
    COORD c = { 0, y };
    int i;
    ReadConsoleOutputCharacterW(out, w, n, c, &got);
    for (i = 0; i < (int)got && i < n; i++) buf[i] = (char)w[i];
    buf[got] = 0;
    while (got && buf[got - 1] == ' ') buf[--got] = 0;
}

static int child(void)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    FILE *f = _wfopen(L"C:\\vtregion-out.txt", L"w");
    CONSOLE_SCREEN_BUFFER_INFO info;
    char r[8][128];
    DWORD mode, w;
    short top;
    int i;
    const char *seq =
        "\x1b[2J\x1b[H"
        "line0\r\nline1\r\nline2\r\nline3\r\nline4\r\nline5"
        "\x1b[2;4r"                 /* rows 2-4 scroll */
        "\x1b[4;1H\n"               /* a line feed at the region's bottom */
        "\x1b[1;1Hx\x1b[4b"         /* x and four more */
        "\x1b[r";
    GetConsoleMode(out, &mode);
    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
    WriteConsoleA(out, seq, (DWORD)strlen(seq), &w, NULL);
    GetConsoleScreenBufferInfo(out, &info);
    top = info.srWindow.Top;
    for (i = 0; i < 6; i++) { row(out, top + i, r[i], 40); fprintf(f, "ROW%d %s\n", i, r[i]); }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    HANDLE in_r, in_w, out_r, out_w;
    HPCON pc;
    COORD size = { 80, 10 };
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    SIZE_T len = 0;
    WCHAR cmd[MAX_PATH + 16];
    char drain[4096];
    DWORD n;

    if (argc > 1 && !strcmp(argv[1], "child")) return child();
    CreatePipe(&in_r, &in_w, NULL, 0);
    CreatePipe(&out_r, &out_w, NULL, 0);
    if (FAILED(CreatePseudoConsole(size, in_r, out_w, 0, &pc))) return 1;
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    InitializeProcThreadAttributeList(NULL, 1, 0, &len);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, len);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &len);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pc, sizeof(pc), NULL, NULL);
    GetModuleFileNameW(NULL, cmd, MAX_PATH);
    wcscat(cmd, L" child");
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi)) return 2;
    CloseHandle(out_w);
    while (WaitForSingleObject(pi.hProcess, 50) == WAIT_TIMEOUT)
        if (PeekNamedPipe(out_r, NULL, 0, NULL, &n, NULL) && n) ReadFile(out_r, drain, sizeof(drain), &n, NULL);
    ClosePseudoConsole(pc);
    return 0;
}
