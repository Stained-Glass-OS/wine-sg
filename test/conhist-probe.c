/* wine-sg 9705: console command history per program, as Windows keeps it -- GetConsoleCommandHistory(Length),
 * ExpungeConsoleCommandHistory, SetConsoleNumberOfCommands, Get/SetConsoleHistoryInfo; a line ReadConsole
 * reads goes to the history of the reading program (its console input exe name, its file name by default). */
#include <windows.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)

typedef DWORD (WINAPI *len_w_t)(LPCWSTR);
typedef DWORD (WINAPI *len_a_t)(LPCSTR);
typedef DWORD (WINAPI *get_w_t)(LPWSTR, DWORD, LPCWSTR);
typedef DWORD (WINAPI *get_a_t)(LPSTR, DWORD, LPCSTR);
typedef void (WINAPI *exp_w_t)(LPCWSTR);
typedef BOOL (WINAPI *num_w_t)(DWORD, LPCWSTR);
typedef BOOL (WINAPI *info_t)(CONSOLE_HISTORY_INFO *);
typedef BOOL (WINAPI *exename_t)(LPCWSTR);
typedef BOOL (WINAPI *getexename_t)(DWORD, LPWSTR);

static HANDLE in;

static void type_line(const WCHAR *s)
{
    INPUT_RECORD r[128];
    DWORD n = 0, w;
    for (; *s; s++)
    {
        WCHAR c = *s;
        int k;
        for (k = 0; k < 2; k++)
        {
            memset(&r[n], 0, sizeof(r[n]));
            r[n].EventType = KEY_EVENT;
            r[n].Event.KeyEvent.bKeyDown = !k;
            r[n].Event.KeyEvent.wRepeatCount = 1;
            r[n].Event.KeyEvent.uChar.UnicodeChar = c;
            r[n].Event.KeyEvent.wVirtualKeyCode = c == '\r' ? VK_RETURN : LOBYTE(VkKeyScanW(c));
            n++;
        }
    }
    WriteConsoleInputW(in, r, n, &w);
}

static void read_line(const WCHAR *typed)
{
    WCHAR buf[256];
    DWORD got = 0;
    type_line(typed);
    ReadConsoleW(in, buf, 256, &got, NULL);
}

int main(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32");
    len_w_t lenW = (len_w_t)GetProcAddress(k32, "GetConsoleCommandHistoryLengthW");
    len_a_t lenA = (len_a_t)GetProcAddress(k32, "GetConsoleCommandHistoryLengthA");
    get_w_t getW = (get_w_t)GetProcAddress(k32, "GetConsoleCommandHistoryW");
    get_a_t getA = (get_a_t)GetProcAddress(k32, "GetConsoleCommandHistoryA");
    exp_w_t expW = (exp_w_t)GetProcAddress(k32, "ExpungeConsoleCommandHistoryW");
    num_w_t numW = (num_w_t)GetProcAddress(k32, "SetConsoleNumberOfCommandsW");
    info_t getinfo = (info_t)GetProcAddress(k32, "GetConsoleHistoryInfo");
    info_t setinfo = (info_t)GetProcAddress(k32, "SetConsoleHistoryInfo");
    exename_t setexe = (exename_t)GetProcAddress(k32, "SetConsoleInputExeNameW");
    getexename_t getexe = (getexename_t)GetProcAddress(k32, "GetConsoleInputExeNameW");
    CONSOLE_HISTORY_INFO hi = { sizeof(hi) };
    WCHAR buf[256], name[MAX_PATH];
    char bufA[256];
    DWORD n;

    CHECK(lenW && lenA && getW && getA && expW && numW && getinfo && setinfo, "kernel32 exports the history functions");
    if (!(lenW && getW && expW && numW && getinfo && setinfo)) { printf("RESULT: FAIL\n"); return 1; }
    in = CreateFileA("CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    CHECK(in != INVALID_HANDLE_VALUE, "a console input");
    SetConsoleMode(in, ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);

    name[0] = 0;
    getexe(MAX_PATH, name);
    CHECK(!lstrcmpiW(name, L"probe.exe"), "the console input exe name starts as the program's file name (%ls)", name);
    CHECK(getinfo(&hi) && hi.HistoryBufferSize == 50 && hi.NumberOfHistoryBuffers == 4 && hi.dwFlags == 0,
          "GetConsoleHistoryInfo: Windows' defaults (%u commands, %u buffers, flags %lu)", hi.HistoryBufferSize,
          hi.NumberOfHistoryBuffers, hi.dwFlags);
    hi.cbSize = 4;
    CHECK(!getinfo(&hi) && GetLastError() == ERROR_INVALID_PARAMETER, "a wrong cbSize: ERROR_INVALID_PARAMETER");
    hi.cbSize = sizeof(hi);

    read_line(L"first line\r");
    n = lenW(L"probe.exe");
    CHECK(n == sizeof(L"first line"), "GetConsoleCommandHistoryLengthW after one line: %lu bytes (want %u)", n, (unsigned)sizeof(L"first line"));
    CHECK(lenW(L"PROBE.EXE") == n, "the executable name matches without regard to case");
    memset(buf, 0xcc, sizeof(buf));
    n = getW(buf, sizeof(buf), L"probe.exe");
    CHECK(n == sizeof(L"first line") && !memcmp(buf, L"first line", sizeof(L"first line")), "GetConsoleCommandHistoryW: %lu bytes, %ls", n, buf);
    SetLastError(0);
    n = getW(buf, 4, L"probe.exe");
    CHECK(!n && GetLastError() == ERROR_BUFFER_OVERFLOW, "a buffer too small: 0, ERROR_BUFFER_OVERFLOW (%lu, %lu)", n, GetLastError());
    n = getA(bufA, sizeof(bufA), "probe.exe");
    CHECK(n == 11 && !strcmp(bufA, "first line"), "GetConsoleCommandHistoryA: %lu bytes", n);
    CHECK(lenW(L"nosuch.exe") == 0, "a program with no history: 0");

    setexe(L"other.exe");
    read_line(L"second\r");
    CHECK(lenW(L"other.exe") == sizeof(L"second") && lenA("other.exe") == 7, "the next reader's line goes to its own history");
    CHECK(lenW(L"probe.exe") == sizeof(L"first line"), "the first program's history is unchanged");
    read_line(L"third\r");
    n = getW(buf, sizeof(buf), L"other.exe");
    CHECK(n == sizeof(L"second") + sizeof(L"third") && !lstrcmpW(buf, L"second") && !lstrcmpW(buf + 7, L"third"),
          "two commands, oldest first, each NUL-ended (%lu bytes)", n);
    expW(L"Other.exe");
    CHECK(lenW(L"other.exe") == 0, "ExpungeConsoleCommandHistoryW empties it");

    setexe(L"probe.exe");
    CHECK(numW(1, L"probe.exe"), "SetConsoleNumberOfCommandsW(1)");
    read_line(L"a\r");
    read_line(L"b\r");
    n = getW(buf, sizeof(buf), L"probe.exe");
    CHECK(n == sizeof(L"b") && !lstrcmpW(buf, L"b"), "a history of one command keeps the newest (%lu bytes, %ls)", n, buf);

    hi.HistoryBufferSize = 10; hi.NumberOfHistoryBuffers = 2; hi.dwFlags = HISTORY_NO_DUP_FLAG;
    CHECK(setinfo(&hi), "SetConsoleHistoryInfo");
    memset(&hi, 0, sizeof(hi)); hi.cbSize = sizeof(hi);
    CHECK(getinfo(&hi) && hi.HistoryBufferSize == 10 && hi.NumberOfHistoryBuffers == 2 && hi.dwFlags == HISTORY_NO_DUP_FLAG,
          "GetConsoleHistoryInfo gives it back");
    read_line(L"x\r");
    read_line(L"y\r");
    read_line(L"x\r");
    n = getW(buf, sizeof(buf), L"probe.exe");
    CHECK(n == sizeof(L"b") + sizeof(L"y") + sizeof(L"x") && !lstrcmpW(buf, L"b") && !lstrcmpW(buf + 2, L"y") && !lstrcmpW(buf + 4, L"x"),
          "HISTORY_NO_DUP_FLAG: a repeated command is kept once, last (%lu bytes)", n);
    hi.dwFlags = 0x100;
    CHECK(!setinfo(&hi), "SetConsoleHistoryInfo with an unknown flag fails");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
