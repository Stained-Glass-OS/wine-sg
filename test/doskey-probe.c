/* doskey-probe: doskey.exe and macro expansion in the console host (patches/sg/9262). Wine had no
 * doskey.exe, so Git for Windows' Git CMD -- cmd /K "doskey git=^"...\git.exe^" $*" -- opened with
 * "Can't recognize ...", and the console host expanded no macro. Run in a real console (the gate starts it
 * with start /wait); prints PASS/FAIL lines and RESULT. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE in;
static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static void type_line(const WCHAR *s)
{
    INPUT_RECORD r[512];
    DWORD n = 0, w;
    for (; *s; s++)
    {
        int k;
        for (k = 0; k < 2; k++)
        {
            memset(&r[n], 0, sizeof(r[n]));
            r[n].EventType = KEY_EVENT;
            r[n].Event.KeyEvent.bKeyDown = !k;
            r[n].Event.KeyEvent.wRepeatCount = 1;
            r[n].Event.KeyEvent.uChar.UnicodeChar = *s;
            r[n].Event.KeyEvent.wVirtualKeyCode = *s == '\r' ? VK_RETURN : LOBYTE(VkKeyScanW(*s));
            n++;
        }
    }
    WriteConsoleInputW(in, r, n, &w);
}

static DWORD read_line(WCHAR *buf, DWORD size)
{
    DWORD got = 0;
    ReadConsoleW(in, buf, size - 1, &got, NULL);
    buf[got] = 0;
    return got;
}

/* run CMD in this console, its output to C:\doskey-out.txt; the exit code */
static DWORD run(const WCHAR *cmd)
{
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    WCHAR line[1024];
    DWORD code = 999;
    HANDLE out = CreateFileW(L"C:\\doskey-out.txt", GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);

    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in;
    si.hStdOutput = out;
    si.hStdError = out;
    lstrcpynW(line, cmd, ARRAYSIZE(line));
    if (CreateProcessW(NULL, line, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 60000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    CloseHandle(out);
    return code;
}

static BOOL output_has(const char *text)
{
    char buf[8192];
    DWORD got = 0;
    HANDLE f = CreateFileW(L"C:\\doskey-out.txt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) return FALSE;
    ReadFile(f, buf, sizeof(buf) - 1, &got, NULL);
    CloseHandle(f);
    buf[got] = 0;
    return strstr(buf, text) != NULL;
}

/* a read that never completes (no macro expanded, so no second line) must not hang the gate */
static DWORD WINAPI watchdog(void *arg)
{
    Sleep(45000);
    printf("FAIL  the probe did not finish in 45 s (a read waited for a line that never came)\n");
    printf("RESULT: FAIL\n");
    fflush(stdout);
    ExitProcess(1);
    return 0;
}

int main(void)
{
    WCHAR buf[512], hist[2048];
    DWORD code, n;
    DWORD (WINAPI *history)(WCHAR *, DWORD, WCHAR *) =
        (void *)GetProcAddress(GetModuleHandleA("kernel32"), "GetConsoleCommandHistoryW");

    CloseHandle(CreateThread(NULL, 0, watchdog, NULL, 0, NULL));
    in = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    check(in != INVALID_HANDLE_VALUE, "a console to read from");
    SetConsoleMode(in, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);

    code = run(L"C:\\windows\\system32\\doskey.exe /exename=probe.exe gx=echo [$1] [$2] [$*] $G$L$B$$ $Tsecond $1");
    printf("      doskey exit %lu\n", code);
    check(code == 0, "doskey.exe runs and defines a macro");
    /* Git CMD's shape, through cmd: the quotes escaped with ^ reach doskey as quotes */
    code = run(L"C:\\windows\\system32\\cmd.exe /C \"C:\\windows\\system32\\doskey.exe /exename=probe.exe "
               L"gitm=^\"C:\\Program Files\\Git\\cmd\\git.exe^\" $*\"");
    check(code == 0 && !output_has("recognize"), "cmd /C \"doskey git=^\"...git.exe^\" $*\" (Git CMD) runs doskey");

    code = run(L"C:\\windows\\system32\\doskey.exe /macros:probe.exe");
    check(output_has("gx=echo [$1] [$2] [$*] $G$L$B$$ $Tsecond $1"), "doskey /macros:probe.exe lists gx as defined");
    check(output_has("gitm=\"C:\\Program Files\\Git\\cmd\\git.exe\" $*"), "...and gitm with its quotes");
    run(L"C:\\windows\\system32\\doskey.exe /exename=other.exe ox=other");
    run(L"C:\\windows\\system32\\doskey.exe /macros:all");
    check(output_has("[probe.exe]") && output_has("[other.exe]") && output_has("ox=other"), "doskey /macros:all lists each program's");

    type_line(L"gx one two three\r");
    n = read_line(buf, ARRAYSIZE(buf));
    printf("      read [%ls]\n", buf);
    check(!wcscmp(buf, L"echo [one] [two] [one two three] ><|$ \r\n"), "a macro typed at the start of a line is expanded ($1 $2 $* $G $L $B $$)");
    n = read_line(buf, ARRAYSIZE(buf));
    printf("      read [%ls]\n", buf);
    check(!wcscmp(buf, L"second one\r\n"), "$T: the next command is the next line read");
    type_line(L"gitm status --short\r");
    read_line(buf, ARRAYSIZE(buf));
    printf("      read [%ls]\n", buf);
    check(!wcscmp(buf, L"\"C:\\Program Files\\Git\\cmd\\git.exe\" status --short\r\n"), "Git CMD's git macro expands to git.exe with the arguments");
    type_line(L"GX up\r");
    read_line(buf, ARRAYSIZE(buf));
    read_line(hist, ARRAYSIZE(hist));
    check(!wcsncmp(buf, L"echo [up]", 9), "macro names match without regard to case");
    type_line(L" gx x\r");
    read_line(buf, ARRAYSIZE(buf));
    check(!wcscmp(buf, L" gx x\r\n"), "not at the start of the line: not expanded");
    type_line(L"ox here\r");
    read_line(buf, ARRAYSIZE(buf));
    check(!wcscmp(buf, L"ox here\r\n"), "another program's macro is not expanded");
    memset(hist, 0, sizeof(hist));
    if (history) history(hist, sizeof(hist), (WCHAR *)L"probe.exe");
    check(history && !wcscmp(hist, L"gx one two three"), "the history has the line as typed");

    code = run(L"C:\\windows\\system32\\doskey.exe /exename=probe.exe gx=");
    type_line(L"gx gone\r");
    read_line(buf, ARRAYSIZE(buf));
    check(!wcscmp(buf, L"gx gone\r\n"), "doskey gx= removes the macro");
    code = run(L"C:\\windows\\system32\\doskey.exe /?");
    check(code == 0 && output_has("DOSKEY [/REINSTALL]"), "doskey /? shows its usage");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
