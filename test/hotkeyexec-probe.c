/* Window hot keys and ShellExecuteEx masks (patches/sg/1657), run by
 * test/hotkeyexec-gate.sh in the shell's desktop:
 *  - a process started with STARTF_USEHOTKEY (CreateProcess) or
 *    SEE_MASK_HOTKEY (ShellExecuteEx) gives the hot key to its first window
 *    (WM_GETHOTKEY reads it);
 *  - pressing the hot key sends that window WM_SYSCOMMAND SC_HOTKEY and
 *    brings it to the foreground;
 *  - WM_SETHOTKEY's answers: 2 when another window has the key, 0 for a
 *    child window, -1 for a key that cannot be one, 1 to remove;
 *  - ShellExecuteEx("properties", SEE_MASK_INVOKEIDLIST) opens the file's
 *    property sheet and returns while it is open.
 * WM_SETHOTKEY/WM_GETHOTKEY and SC_HOTKEY did nothing; the masks were
 * ignored ("flags ignored"). */
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
    fflush(stdout);
}

static char outfile[MAX_PATH];
static LRESULT CALLBACK child_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_SYSCOMMAND && (wp & 0xfff0) == SC_HOTKEY)
    {
        FILE *f = fopen(outfile, "a");
        if (f) { fprintf(f, "schotkey %p\n", (void *)lp); fclose(f); }
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static int child(const char *file)
{
    WNDCLASSA wc = {0};
    HWND hwnd;
    MSG msg;
    FILE *f;
    DWORD start = GetTickCount();

    lstrcpyA(outfile, file);
    wc.lpfnWndProc = child_proc;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "SGHotkeyChild";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("SGHotkeyChild", "SG Hotkey Child", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                         50, 50, 300, 200, NULL, NULL, NULL, NULL);
    f = fopen(file, "a");
    if (f) { fprintf(f, "hwnd %p hotkey %04lx\n", hwnd, (DWORD)SendMessageA(hwnd, WM_GETHOTKEY, 0, 0)); fclose(f); }
    while (GetTickCount() - start < 15000)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        if (GetFileAttributesA("sg-hotkey-stop") != INVALID_FILE_ATTRIBUTES) break;
        MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
    }
    return 0;
}

static BOOL read_child(const char *file, HWND *hwnd, DWORD *hotkey, int wait_ms)
{
    DWORD start = GetTickCount();
    char line[256];
    while (GetTickCount() - start < (DWORD)wait_ms)
    {
        FILE *f = fopen(file, "r");
        if (f)
        {
            while (fgets(line, sizeof(line), f))
            {
                void *p; unsigned long hk;
                if (sscanf(line, "hwnd %p hotkey %lx", &p, &hk) == 2) { *hwnd = p; *hotkey = hk; fclose(f); return TRUE; }
            }
            fclose(f);
        }
        Sleep(100);
    }
    return FALSE;
}

static BOOL file_has(const char *file, const char *what)
{
    char line[256];
    FILE *f = fopen(file, "r");
    BOOL ret = FALSE;
    if (!f) return FALSE;
    while (fgets(line, sizeof(line), f)) if (strstr(line, what)) ret = TRUE;
    fclose(f);
    return ret;
}

static void press(WORD vk, BOOL ctrl, BOOL alt)
{
    INPUT in[6];
    int n = 0, i;
    memset(in, 0, sizeof(in));
    if (ctrl) { in[n].type = INPUT_KEYBOARD; in[n++].ki.wVk = VK_CONTROL; }
    if (alt) { in[n].type = INPUT_KEYBOARD; in[n++].ki.wVk = VK_MENU; }
    in[n].type = INPUT_KEYBOARD; in[n++].ki.wVk = vk;
    in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = vk; in[n++].ki.dwFlags = KEYEVENTF_KEYUP;
    if (alt) { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_MENU; in[n++].ki.dwFlags = KEYEVENTF_KEYUP; }
    if (ctrl) { in[n].type = INPUT_KEYBOARD; in[n].ki.wVk = VK_CONTROL; in[n++].ki.dwFlags = KEYEVENTF_KEYUP; }
    for (i = 0; i < n; i++) { SendInput(1, &in[i], sizeof(INPUT)); Sleep(30); }
}

static void pump(int ms)
{
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < (DWORD)ms)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        Sleep(20);
    }
}

static HWND found;
static const char *wanted;
static BOOL CALLBACK find_title(HWND hwnd, LPARAM lp)
{
    char title[256];
    if (IsWindowVisible(hwnd) && GetWindowTextA(hwnd, title, sizeof(title)) && strstr(title, wanted)) { found = hwnd; return FALSE; }
    return TRUE;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH], cmd[MAX_PATH * 2];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    HWND child_hwnd = NULL, mine, sub;
    DWORD hotkey = 0, start;
    LRESULT r;

    if (argc >= 3 && !strcmp(argv[1], "child")) return child(argv[2]);

    DeleteFileA("sg-hotkey-stop");
    DeleteFileA("child1.txt");
    DeleteFileA("child2.txt");
    GetModuleFileNameA(NULL, self, sizeof(self));

    /* CreateProcess with STARTF_USEHOTKEY */
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USEHOTKEY;
    si.hStdInput = (HANDLE)(ULONG_PTR)MAKEWORD('K', HOTKEYF_CONTROL | HOTKEYF_ALT);
    sprintf(cmd, "\"%s\" child child1.txt", self);
    check(CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi), "CreateProcess with STARTF_USEHOTKEY");
    check(read_child("child1.txt", &child_hwnd, &hotkey, 10000) && hotkey == MAKEWORD('K', HOTKEYF_CONTROL | HOTKEYF_ALT),
          "the new process's first window has the hot key (WM_GETHOTKEY)");
    printf("  child window %p hot key %04lx\n", child_hwnd, hotkey);

    /* our own window in front, then the hot key */
    mine = CreateWindowA("STATIC", "SG Hotkey Parent", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 300, 200,
                         NULL, NULL, NULL, NULL);
    SetForegroundWindow(mine);
    pump(500);
    printf("  foreground before: %p (mine %p)\n", GetForegroundWindow(), mine);
    press('K', TRUE, TRUE);
    start = GetTickCount();
    while (GetTickCount() - start < 4000 && GetForegroundWindow() != child_hwnd) pump(100);
    printf("  foreground after: %p\n", GetForegroundWindow());
    check(file_has("child1.txt", "schotkey"), "pressing the hot key sends the window WM_SYSCOMMAND SC_HOTKEY");
    check(GetForegroundWindow() == child_hwnd, "... and brings it to the foreground");

    /* WM_SETHOTKEY's answers */
    r = SendMessageA(mine, WM_SETHOTKEY, MAKEWORD('K', HOTKEYF_CONTROL | HOTKEYF_ALT), 0);
    check(r == 2, "WM_SETHOTKEY of a key another window has: 2");
    r = SendMessageA(mine, WM_SETHOTKEY, MAKEWORD(VK_ESCAPE, HOTKEYF_CONTROL), 0);
    check(r == -1, "WM_SETHOTKEY of Escape: -1");
    sub = CreateWindowA("STATIC", "", WS_CHILD, 0, 0, 10, 10, mine, NULL, NULL, NULL);
    check(SendMessageA(sub, WM_SETHOTKEY, MAKEWORD('L', HOTKEYF_ALT), 0) == 0, "WM_SETHOTKEY on a child window: 0");
    check(SendMessageA(mine, WM_SETHOTKEY, MAKEWORD('L', HOTKEYF_ALT | HOTKEYF_SHIFT), 0) == 1 &&
          SendMessageA(mine, WM_GETHOTKEY, 0, 0) == MAKEWORD('L', HOTKEYF_ALT | HOTKEYF_SHIFT), "WM_SETHOTKEY sets, WM_GETHOTKEY reads");
    check(SendMessageA(mine, WM_SETHOTKEY, 0, 0) == 1 && SendMessageA(mine, WM_GETHOTKEY, 0, 0) == 0,
          "WM_SETHOTKEY 0 removes it");

    /* ShellExecuteEx with SEE_MASK_HOTKEY */
    {
        SHELLEXECUTEINFOA sei = {0};
        HWND h2 = NULL;
        DWORD hk2 = 0;
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_HOTKEY | SEE_MASK_NOCLOSEPROCESS;
        sei.lpFile = self;
        sei.lpParameters = "child child2.txt";
        sei.nShow = SW_SHOWNORMAL;
        sei.dwHotKey = MAKEWORD('J', HOTKEYF_CONTROL | HOTKEYF_SHIFT);
        check(ShellExecuteExA(&sei), "ShellExecuteEx with SEE_MASK_HOTKEY");
        check(read_child("child2.txt", &h2, &hk2, 10000) && hk2 == MAKEWORD('J', HOTKEYF_CONTROL | HOTKEYF_SHIFT),
              "... the program's window has the hot key");
        printf("  window %p hot key %04lx\n", h2, hk2);
        if (sei.hProcess) CloseHandle(sei.hProcess);
    }

    /* properties through the item's menu */
    {
        char path[MAX_PATH];
        SHELLEXECUTEINFOA sei = {0};
        HANDLE f;
        BOOL ok;

        GetFullPathNameA("sg-props-test.txt", MAX_PATH, path, NULL);
        f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        WriteFile(f, "hello", 5, &start, NULL);
        CloseHandle(f);
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_INVOKEIDLIST;
        sei.lpVerb = "properties";
        sei.lpFile = path;
        sei.nShow = SW_SHOWNORMAL;
        start = GetTickCount();
        ok = ShellExecuteExA(&sei);
        printf("  properties: %d after %lu ms\n", ok, GetTickCount() - start);
        check(ok && GetTickCount() - start < 5000, "ShellExecuteEx(properties, SEE_MASK_INVOKEIDLIST) returns");
        wanted = "sg-props-test";
        found = NULL;
        start = GetTickCount();
        while (!found && GetTickCount() - start < 8000) { pump(200); EnumWindows(find_title, 0); }
        check(found != NULL, "... with the file's property sheet open");
        if (found)
        {
            char title[256];
            GetWindowTextA(found, title, sizeof(title));
            printf("  sheet: %s\n", title);
            PostMessageA(found, WM_CLOSE, 0, 0);
            pump(500);
        }
    }

    CloseHandle(CreateFileA("sg-hotkey-stop", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
    WaitForSingleObject(pi.hProcess, 5000);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    fflush(stdout);
    return failures != 0;
}
