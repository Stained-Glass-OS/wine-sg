/* test/claudecon-gate.sh's probe: a console program as Claude Code is one,
 * on a pseudo console as a Terminal tab hosts it (patches/sg/0794).
 *   claudecon-probe host        the tab: runs the children below, types, reads
 *   claudecon-probe vt          xterm's private forms, SI, colours back to grey
 *   claudecon-probe raw         raw input: Ctrl+C is a key
 *   claudecon-probe deadread    a line read left waiting when its process ends
 *   claudecon-probe read        a line read: LINE=<what was typed>
 * The host prints what it saw: VTCELL, VTSI, VTRESET, RAW, ALIVE, LINE.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000006
#include <windows.h>
#include <stdio.h>
#include <string.h>

static HANDLE out_r;
static char seen[1 << 20];
static volatile LONG nseen;
static CRITICAL_SECTION cs;

static DWORD WINAPI reader(void *arg)
{
    char buf[4096];
    DWORD n;
    (void)arg;
    while (ReadFile(out_r, buf, sizeof(buf), &n, NULL) && n) {
        EnterCriticalSection(&cs);
        if (nseen + n < sizeof(seen) - 1) { memcpy(seen + nseen, buf, n); nseen += n; seen[nseen] = 0; }
        LeaveCriticalSection(&cs);
    }
    return 0;
}

static BOOL saw(const char *s, int ms)
{
    for (; ms > 0; ms -= 100) {
        BOOL r;
        EnterCriticalSection(&cs); r = strstr(seen, s) != NULL; LeaveCriticalSection(&cs);
        if (r) return TRUE;
        Sleep(100);
    }
    return FALSE;
}

static HPCON pc;
static HANDLE run(const char *role)
{
    STARTUPINFOEXW si;
    PROCESS_INFORMATION pi;
    SIZE_T len = 0;
    WCHAR cmd[MAX_PATH + 32], self[MAX_PATH];
    memset(&si, 0, sizeof(si));
    si.StartupInfo.cb = sizeof(si);
    InitializeProcThreadAttributeList(NULL, 1, 0, &len);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, len);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &len);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, pc, sizeof(pc), NULL, NULL);
    GetModuleFileNameW(NULL, self, MAX_PATH);
    swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" %hs", self, role);
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi)) return NULL;
    CloseHandle(pi.hThread);
    return pi.hProcess;
}

static void put(const char *s) { DWORD n; WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), s, (DWORD)strlen(s), &n, NULL); }

static int child_vt(void)
{
    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode, got;
    WCHAR ch[4];
    WORD at[4];
    COORD c;
    FILE *f = fopen("C:\\claudecon-vt.txt", "w");
    GetConsoleMode(out, &mode);
    SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    /* Claude Code's start: save the cursor, then modifyOtherKeys and kitty's keyboard */
    put("\x1b[1;1H\x1b" "7\x1b[3;6H\x1b[>4;2m\x1b[>5u\x1b[<uX");
    c.X = 5; c.Y = 2; ReadConsoleOutputCharacterW(out, ch, 1, c, &got); ReadConsoleOutputAttribute(out, at, 1, c, &got);
    fprintf(f, "VTCELL %c %04x\n", (char)ch[0], at[0]);
    put("\x1b[5;1HA\x0f" "B");
    c.X = 0; c.Y = 4; ReadConsoleOutputCharacterW(out, ch, 2, c, &got);
    fprintf(f, "VTSI %c%c\n", (char)ch[0], (char)ch[1]);
    /* light grey on green, then light grey on black: the colour must end */
    put("\x1b[7;1H\x1b[37;42mG\x1b[37;40mW\x1b[0m\r\n");
    /* true colours, as Claude Code draws (0795) */
    put("\x1b[9;1H\x1b[38;2;10;20;30mT\x1b[48;2;200;100;50mU\x1b[0m\r\n");
    /* a row with a background at the bottom, then a line feed that scrolls */
    put("\x1b[25;1H\x1b[48;2;9;9;9mBGROW\x1b[49m\r\nNEXTROW\r\n");
    fclose(f);
    Sleep(800);
    return 0;
}

static int child_raw(void)
{
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD r;
    DWORD n;
    SetConsoleMode(in, ENABLE_VIRTUAL_TERMINAL_INPUT);   /* raw: no processed input, as Claude Code */
    put("RAWREADY\r\n");
    for (;;) {
        if (!ReadConsoleInputW(in, &r, 1, &n)) return 2;
        if (r.EventType == KEY_EVENT && r.Event.KeyEvent.bKeyDown && r.Event.KeyEvent.uChar.UnicodeChar == 3) break;
    }
    put("GOTCTRLC\r\n");
    Sleep(500);
    return 0;
}

static DWORD WINAPI pending_read(void *arg)
{
    WCHAR buf[64];
    DWORD n;
    (void)arg;
    ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buf, 64, &n, NULL);
    return 0;
}

static int child_deadread(void)
{
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    CloseHandle(CreateThread(NULL, 0, pending_read, NULL, 0, NULL));
    Sleep(700);
    put("DEADREADY\r\n");
    Sleep(300);
    ExitProcess(0);   /* the read is still waiting */
}

static int child_read(void)
{
    WCHAR buf[64];
    char a[64];
    DWORD n = 0;
    put("READREADY\r\n");
    if (!ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE), buf, 63, &n, NULL)) return 2;
    while (n && (buf[n - 1] == '\r' || buf[n - 1] == '\n')) n--;
    buf[n] = 0;
    {
        char line[128];
        WideCharToMultiByte(CP_ACP, 0, buf, -1, a, sizeof(a), NULL, NULL);
        snprintf(line, sizeof(line), "LINE=%s=END\r\n", a);
        put(line);
    }
    Sleep(500);
    return 0;
}

int main(int argc, char **argv)
{
    HANDLE in_r, in_w, out_w, p;
    COORD size = { 80, 25 };
    DWORD n, code;
    if (argc > 1 && !strcmp(argv[1], "vt")) return child_vt();
    if (argc > 1 && !strcmp(argv[1], "raw")) return child_raw();
    if (argc > 1 && !strcmp(argv[1], "deadread")) return child_deadread();
    if (argc > 1 && !strcmp(argv[1], "read")) return child_read();

    InitializeCriticalSection(&cs);
    CreatePipe(&in_r, &in_w, NULL, 0);
    CreatePipe(&out_r, &out_w, NULL, 0);
    if (FAILED(CreatePseudoConsole(size, in_r, out_w, 0, &pc))) { printf("NOPCON\n"); return 1; }
    CloseHandle(CreateThread(NULL, 0, reader, NULL, 0, NULL));

    if ((p = run("vt"))) { WaitForSingleObject(p, 15000); CloseHandle(p); }
    Sleep(500);
    {
        /* the tab's stream: G, then something that ends the green, then W */
        char *g;
        EnterCriticalSection(&cs);
        g = strstr(seen, "G");
        while (g && g[1] == 'W') g = strstr(g + 1, "G");
        if (g) {
            char *w = strchr(g, 'W');
            int ok = 0;
            if (w) {
                char *q;
                for (q = g + 1; q < w; q++) if (q[0] == '\x1b' && q[1] == '[' && (!strncmp(q + 2, "m", 1) || !strncmp(q + 2, "49m", 3) || !strncmp(q + 2, "40m", 3) || !strncmp(q + 2, "0m", 2))) ok = 1;
            }
            printf("VTRESET %s\n", ok ? "yes" : "no");
        } else printf("VTRESET nog\n");
        {
            char *t = strstr(seen, "T"), *u;
            int tc = 0;
            /* the true colours reach the tab: 38;2;10;20;30 before T, 48;2;200;100;50 before U */
            for (t = seen; (t = strstr(t, "38;2;10;20;30m")); t++) if (t[14] == 'T' || strstr(t, "T") - t < 40) { tc |= 1; break; }
            u = strstr(seen, "48;2;200;100;50m");
            if (u && strchr(u, 'U') && strchr(u, 'U') - u < 40) tc |= 2;
            printf("TRUECOLOR %s\n", tc == 3 ? "yes" : tc == 1 ? "fg-only" : tc == 2 ? "bg-only" : "no");
        }
        {
            /* no line feed while the row's background is in effect: from the last
             * 48;2;9;9;9 before NEXTROW, a reset comes before any \r\n */
            char *n = strstr(seen, "NEXTROW"), *b = NULL, *q, *lf;
            for (q = seen; (q = strstr(q, "48;2;9;9;9m")) && (!n || q < n); q++) b = q;
            if (!n || !b) printf("LFBG nobg\n");
            else {
                char *reset1, *reset2, *first_reset;
                lf = strstr(b, "\r\n");
                reset1 = strstr(b, "\x1b[49m"); reset2 = strstr(b, "\x1b[m");
                first_reset = !reset1 ? reset2 : !reset2 ? reset1 : (reset1 < reset2 ? reset1 : reset2);
                printf("LFBG %s\n", (!lf || lf > n || (first_reset && first_reset < lf)) ? "plain" : "colored");
            }
        }
        LeaveCriticalSection(&cs);
    }

    if ((p = run("raw"))) {
        if (saw("RAWREADY", 10000)) { Sleep(500); WriteFile(in_w, "\x03", 1, &n, NULL); }
        code = WaitForSingleObject(p, 8000);
        printf("RAW %s\n", code == WAIT_OBJECT_0 && saw("GOTCTRLC", 2000) ? "key" : "lost");
        if (code != WAIT_OBJECT_0) TerminateProcess(p, 9);
        CloseHandle(p);
    }

    if ((p = run("deadread"))) { WaitForSingleObject(p, 10000); CloseHandle(p); }
    Sleep(500);
    if ((p = run("read"))) {
        printf("ALIVE %s\n", saw("READREADY", 10000) ? "yes" : "no");
        Sleep(500);
        WriteFile(in_w, "hello\r", 6, &n, NULL);
        code = WaitForSingleObject(p, 8000);
        printf("LINE %s\n", code == WAIT_OBJECT_0 && saw("LINE=hello=END", 2000) ? "hello" : "lost");
        if (code != WAIT_OBJECT_0) TerminateProcess(p, 9);
        CloseHandle(p);
    }
    {
        FILE *f = fopen("C:\\claudecon-vt.txt", "r");
        char l[128];
        while (f && fgets(l, sizeof(l), f)) fputs(l, stdout);
        if (f) fclose(f);
    }
    ClosePseudoConsole(pc);
    return 0;
}
