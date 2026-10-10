/* user32 EndTask (patches/sg/2242): asks a window to close, or terminates its process. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("PASS  %s\n", #c); else { printf("FAIL  %s: ", #c); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static BOOL (WINAPI *pEndTask)(HWND, BOOL, BOOL);
static int ignore_close;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_CLOSE)
    {
        if (ignore_close) return 0;
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) { PostQuitMessage(7); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int run_child(const char *mode)
{
    WNDCLASSW wc = {0};
    MSG msg;
    ignore_close = !strcmp(mode, "ignore");
    wc.lpfnWndProc = proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = ignore_close ? L"SgEndTaskIgnore" : L"SgEndTaskClose";
    RegisterClassW(&wc);
    CreateWindowW(wc.lpszClassName, L"endtask", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 100, 100, NULL, NULL, wc.hInstance, NULL);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) DispatchMessageW(&msg);
    return (int)msg.wParam;
}

static HANDLE start(const char *self, const char *mode, const WCHAR *cls, HWND *hwnd)
{
    char cmd[MAX_PATH + 32];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    int i;

    snprintf(cmd, sizeof(cmd), "\"%s\" child %s", self, mode);
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return NULL;
    CloseHandle(pi.hThread);
    for (i = 0; i < 60 && !(*hwnd = FindWindowW(cls, NULL)); i++) Sleep(100);
    return pi.hProcess;
}

int main(int argc, char **argv)
{
    char self[MAX_PATH];
    HWND hwnd = NULL;
    HANDLE proc_h;
    DWORD code;
    BOOL ret;

    if (argc > 2 && !strcmp(argv[1], "child")) return run_child(argv[2]);
    pEndTask = (void *)GetProcAddress(GetModuleHandleA("user32.dll"), "EndTask");
    CHECK(pEndTask != NULL, "EndTask exported");
    if (!pEndTask) { printf("RESULT: FAIL\n"); return 1; }
    GetModuleFileNameA(NULL, self, sizeof(self));

    SetLastError(0xdeadbeef);
    DisableProcessWindowsGhosting();
    CHECK(GetLastError() == 0xdeadbeef, "DisableProcessWindowsGhosting sets no error: %lx", GetLastError());

    SetLastError(0);
    ret = pEndTask((HWND)(ULONG_PTR)0xdeadbeef, FALSE, FALSE);
    CHECK(!ret && GetLastError() == ERROR_INVALID_WINDOW_HANDLE, "bad window: %d %lu", ret, GetLastError());

    /* a program that closes when asked */
    proc_h = start(self, "close", L"SgEndTaskClose", &hwnd);
    CHECK(proc_h && hwnd, "child with a window started");
    if (proc_h && hwnd)
    {
        ret = pEndTask(hwnd, FALSE, FALSE);
        CHECK(ret, "EndTask on a responsive window: %lu", GetLastError());
        CHECK(WaitForSingleObject(proc_h, 5000) == WAIT_OBJECT_0, "it exited by itself");
        GetExitCodeProcess(proc_h, &code);
        CHECK(code == 7, "with its own exit code (%lu)", code);
        CloseHandle(proc_h);
    }

    /* one that ignores the request */
    hwnd = NULL;
    proc_h = start(self, "ignore", L"SgEndTaskIgnore", &hwnd);
    CHECK(proc_h && hwnd, "second child started");
    if (proc_h && hwnd)
    {
        ret = pEndTask(hwnd, FALSE, FALSE);
        CHECK(ret, "EndTask asks politely: %lu", GetLastError());
        CHECK(WaitForSingleObject(proc_h, 700) == WAIT_TIMEOUT, "an ignored request leaves it running");
        ret = pEndTask(hwnd, FALSE, TRUE);
        CHECK(ret, "forced: %lu", GetLastError());
        CHECK(WaitForSingleObject(proc_h, 5000) == WAIT_OBJECT_0, "terminated");
        GetExitCodeProcess(proc_h, &code);
        CHECK(code == 1, "exit code of a terminated process (%lu)", code);
        CloseHandle(proc_h);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
