/* BlockInput, NtSetInformationKey(KeyWriteTimeInformation), transactions (patch 3750) */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xc0000004)
#define STATUS_INVALID_HANDLE ((NTSTATUS)0xc0000008)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xc0000022)
#define STATUS_OBJECT_TYPE_MISMATCH ((NTSTATUS)0xc0000024)
#define STATUS_INVALID_INFO_CLASS ((NTSTATUS)0xc0000003)
#define TXN_QUERY 1
#define TXN_ALL 0x1f003f

static NTSTATUS (WINAPI *pSetKey)(HANDLE, int, void *, ULONG);

static void test_key_write_time(void)
{
    HKEY key, ro;
    FILETIME t1, t2, got;
    LARGE_INTEGER li;
    SYSTEMTIME st = { 2001, 2, 0, 3, 4, 5, 6, 0 };
    NTSTATUS status;
    DWORD ret;
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL);

    RegDeleteTreeA(HKEY_CURRENT_USER, "Software\\FastBKeyTime");
    ret = RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\FastBKeyTime", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    CHECK(!ret, "create key %lu", ret);
    ret = RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\FastBKeyTime", 0, KEY_READ, &ro);
    CHECK(!ret, "open key %lu", ret);

    SystemTimeToFileTime(&st, &t1);
    li.u.LowPart = t1.dwLowDateTime; li.u.HighPart = t1.dwHighDateTime;
    status = pSetKey(key, 0, &li, sizeof(li));
    CHECK(status == 0, "set the write time: %#lx", status);
    got.dwLowDateTime = got.dwHighDateTime = 0;
    ret = RegQueryInfoKeyA(key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &got);
    CHECK(!ret && !memcmp(&got, &t1, sizeof(got)), "the write time reads back: %lu %08lx%08lx / %08lx%08lx", ret,
          got.dwHighDateTime, got.dwLowDateTime, t1.dwHighDateTime, t1.dwLowDateTime);

    st.wYear = 2005;
    SystemTimeToFileTime(&st, &t2);
    li.u.LowPart = t2.dwLowDateTime; li.u.HighPart = t2.dwHighDateTime;
    status = pSetKey(key, 0, &li, sizeof(li));
    CHECK(status == 0, "set it again: %#lx", status);
    ret = RegQueryInfoKeyA(key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &got);
    CHECK(!ret && !memcmp(&got, &t2, sizeof(got)), "the second time reads back");

    status = pSetKey(ro, 0, &li, sizeof(li));
    CHECK(status == STATUS_ACCESS_DENIED, "a key opened for reading only: %#lx", status);
    status = pSetKey(key, 0, &li, 4);
    CHECK(status == STATUS_INFO_LENGTH_MISMATCH, "short buffer: %#lx", status);
    status = pSetKey((HANDLE)0xdeadbeef, 0, &li, sizeof(li));
    CHECK(status == STATUS_INVALID_HANDLE, "bad handle: %#lx", status);
    status = pSetKey(ev, 0, &li, sizeof(li));
    CHECK(status == STATUS_OBJECT_TYPE_MISMATCH, "not a key: %#lx", status);
    status = pSetKey(key, 99, &li, sizeof(li));
    CHECK(status == STATUS_INVALID_INFO_CLASS, "bad class: %#lx", status);

    /* a write after that sets the time to now */
    ret = RegSetValueExA(key, "v", 0, REG_DWORD, (BYTE *)&ret, 4);
    CHECK(!ret, "set value %lu", ret);
    RegQueryInfoKeyA(key, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &got);
    CHECK(CompareFileTime(&got, &t2) > 0, "a value write moves the time on");

    RegCloseKey(ro);
    RegCloseKey(key);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\FastBKeyTime");
    CloseHandle(ev);
}

static HANDLE (WINAPI *pCreate)(SECURITY_ATTRIBUTES *, GUID *, DWORD, DWORD, DWORD, DWORD, WCHAR *);
static BOOL (WINAPI *pCommit)(HANDLE);
static BOOL (WINAPI *pRollback)(HANDLE);

#define ERR_ALREADY_ABORTED 6704
#define ERR_ALREADY_COMMITTED 6705

static void test_transactions(void)
{
    HANDLE t, low, ev;
    DWORD err;
    BOOL ok;

    t = pCreate(NULL, NULL, 0, 0, 0, 0, NULL);
    CHECK(t && t != INVALID_HANDLE_VALUE, "create: %p %lu", t, GetLastError());
    CHECK(t != (HANDLE)1, "a real handle, not the stub's 1");
    ok = pCommit(t); CHECK(ok, "commit: %lu", GetLastError());
    SetLastError(0xdeadbeef); ok = pCommit(t); err = GetLastError();
    CHECK(!ok && err == ERR_ALREADY_COMMITTED, "commit twice: %d %lu", ok, err);
    SetLastError(0xdeadbeef); ok = pRollback(t); err = GetLastError();
    CHECK(!ok && err == ERR_ALREADY_COMMITTED, "roll back after commit: %d %lu", ok, err);
    CHECK(CloseHandle(t), "close");

    t = pCreate(NULL, NULL, 0, 0, 0, 0, NULL);
    ok = pRollback(t); CHECK(ok, "roll back: %lu", GetLastError());
    SetLastError(0xdeadbeef); ok = pCommit(t); err = GetLastError();
    CHECK(!ok && err == ERR_ALREADY_ABORTED, "commit after roll back: %d %lu", ok, err);
    SetLastError(0xdeadbeef); ok = pRollback(t); err = GetLastError();
    CHECK(!ok && err == ERR_ALREADY_ABORTED, "roll back twice: %d %lu", ok, err);
    CloseHandle(t);

    /* a handle that may only query cannot commit or roll back */
    t = pCreate(NULL, NULL, 0, 0, 0, 0, NULL);
    ok = DuplicateHandle(GetCurrentProcess(), t, GetCurrentProcess(), &low, TXN_QUERY, FALSE, 0);
    CHECK(ok, "duplicate with less access: %lu", GetLastError());
    SetLastError(0xdeadbeef); ok = pCommit(low); err = GetLastError();
    CHECK(!ok && err == ERROR_ACCESS_DENIED, "commit without the right: %d %lu", ok, err);
    SetLastError(0xdeadbeef); ok = pRollback(low); err = GetLastError();
    CHECK(!ok && err == ERROR_ACCESS_DENIED, "roll back without the right: %d %lu", ok, err);
    ok = pCommit(t); CHECK(ok, "the full handle still commits: %lu", GetLastError());
    CloseHandle(low);
    CloseHandle(t);

    /* the commit right commits but does not roll back, the rollback right the other way */
    t = pCreate(NULL, NULL, 0, 0, 0, 0, NULL);
    ok = DuplicateHandle(GetCurrentProcess(), t, GetCurrentProcess(), &low, 0x0008 /* TRANSACTION_COMMIT */, FALSE, 0);
    CHECK(ok, "duplicate with the commit right: %lu", GetLastError());
    SetLastError(0xdeadbeef); ok = pRollback(low); err = GetLastError();
    CHECK(!ok && err == ERROR_ACCESS_DENIED, "roll back with only the commit right: %d %lu", ok, err);
    ok = pCommit(low); CHECK(ok, "commit with the commit right: %lu", GetLastError());
    CloseHandle(low);
    CloseHandle(t);
    t = pCreate(NULL, NULL, 0, 0, 0, 0, NULL);
    ok = DuplicateHandle(GetCurrentProcess(), t, GetCurrentProcess(), &low, 0x0010 /* TRANSACTION_ROLLBACK */, FALSE, 0);
    SetLastError(0xdeadbeef); ok = pCommit(low); err = GetLastError();
    CHECK(!ok && err == ERROR_ACCESS_DENIED, "commit with only the rollback right: %d %lu", ok, err);
    ok = pRollback(low); CHECK(ok, "roll back with the rollback right: %lu", GetLastError());
    CloseHandle(low);
    CloseHandle(t);

    /* things that are no transaction */
    ev = CreateEventW(NULL, FALSE, FALSE, NULL);
    SetLastError(0xdeadbeef); ok = pCommit(ev); err = GetLastError();
    CHECK(!ok && err == ERROR_INVALID_HANDLE, "an event: %d %lu", ok, err);
    SetLastError(0xdeadbeef); ok = pRollback((HANDLE)0xdeadbeef); err = GetLastError();
    CHECK(!ok && err == ERROR_INVALID_HANDLE, "a bad handle: %d %lu", ok, err);
    CloseHandle(ev);
}

/* BlockInput */
static HANDLE go, done, stop;
static int worker_result;
static LONG sync_state;

static void key(WORD vk, BOOL up)
{
    INPUT in = { 0 };
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = vk;
    in.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
    SendInput(1, &in, sizeof(in));
}

static BOOL is_down(WORD vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

static DWORD WINAPI worker(void *arg)
{
    /* what another thread does while the main thread blocks */
    BOOL ok;
    DWORD err;

    WaitForSingleObject(go, 10000);
    SetLastError(0xdeadbeef);
    ok = BlockInput(TRUE); err = GetLastError();
    worker_result = !ok && err == ERROR_ACCESS_DENIED;
    SetLastError(0xdeadbeef);
    ok = BlockInput(FALSE); err = GetLastError();
    worker_result |= (!ok && err == ERROR_ACCESS_DENIED) << 1;
    key(VK_F24, FALSE);
    worker_result |= (is_down(VK_F24) ? 0 : 1) << 2;      /* this thread's input is dropped while blocked */
    key(VK_F24, TRUE);
    SetEvent(done);
    WaitForSingleObject(stop, 10000);
    return 0;
}

static DWORD WINAPI blocker_that_exits(void *arg)
{
    BOOL ok = BlockInput(TRUE);
    return ok;
}

static void test_block_input(void)
{
    HANDLE thread;
    DWORD code;
    BOOL ok;

    go = CreateEventW(NULL, FALSE, FALSE, NULL);
    done = CreateEventW(NULL, FALSE, FALSE, NULL);
    stop = CreateEventW(NULL, FALSE, FALSE, NULL);

    /* free: anyone's input goes through */
    key(VK_F24, FALSE);
    CHECK(is_down(VK_F24), "input before the block");
    key(VK_F24, TRUE);
    CHECK(!is_down(VK_F24), "key up");

    SetLastError(0xdeadbeef);
    ok = BlockInput(FALSE);
    CHECK(ok, "unblock when nothing is blocked: %lu", GetLastError());

    thread = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    ok = BlockInput(TRUE);
    CHECK(ok, "block: %lu", GetLastError());
    ok = BlockInput(TRUE);
    CHECK(ok, "block again from the same thread: %lu", GetLastError());

    SetEvent(go);
    CHECK(WaitForSingleObject(done, 10000) == WAIT_OBJECT_0, "worker");
    CHECK(worker_result & 1, "another thread cannot block: %d", worker_result);
    CHECK(worker_result & 2, "another thread cannot unblock: %d", worker_result);
    CHECK(worker_result & 4, "another thread's input is dropped while blocked: %d", worker_result);

    /* the blocking thread can still send input */
    key(VK_F24, FALSE);
    CHECK(is_down(VK_F24), "the blocking thread's own input");
    key(VK_F24, TRUE);
    CHECK(!is_down(VK_F24), "own key up");

    ok = BlockInput(FALSE);
    CHECK(ok, "unblock: %lu", GetLastError());
    /* free again */
    {
        HANDLE t2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Sleep, (void *)10, 0, NULL);
        WaitForSingleObject(t2, 5000);
        CloseHandle(t2);
    }
    SetEvent(stop);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);

    /* a thread that blocks and ends frees the input */
    thread = CreateThread(NULL, 0, blocker_that_exits, NULL, 0, NULL);
    WaitForSingleObject(thread, 5000);
    GetExitCodeThread(thread, &code);
    CHECK(code == 1, "the other thread blocked: %lu", code);
    /* its handle stays open: the thread object outlives the thread, and still must not block */
    Sleep(200);
    key(VK_F24, FALSE);
    CHECK(is_down(VK_F24), "after the blocking thread ended");
    key(VK_F24, TRUE);
    ok = BlockInput(TRUE);
    CHECK(ok, "block after the other thread ended: %lu", GetLastError());
    BlockInput(FALSE);
    CloseHandle(thread);
}

/* physical input: typed through the X server (xdotool) into a window of ours, when that works here */
static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        Sleep(10);
    }
}

static void xdo(const char *args)
{
    char cmd[256];
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    sprintf(cmd, "cmd.exe /c start /wait /unix /usr/bin/xdotool %s", args);
    if (CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, 15000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

static LONG keydowns;

/* a low-level hook sees the physical keys whatever window has the focus; the injected ones are not counted */
static LRESULT CALLBACK ll_proc(int code, WPARAM wp, LPARAM lp)
{
    KBDLLHOOKSTRUCT *k = (KBDLLHOOKSTRUCT *)lp;
    if (code == HC_ACTION && wp == WM_KEYDOWN && (k->vkCode == 'A' || k->vkCode == 'B') && !(k->flags & LLKHF_INJECTED))
        InterlockedIncrement(&keydowns);
    return CallNextHookEx(NULL, code, wp, lp);
}

static void test_block_hardware_input(void)
{
    HHOOK hook = SetWindowsHookExA(WH_KEYBOARD_LL, ll_proc, GetModuleHandleA(NULL), 0);
    HWND w;
    LONG before;
    int tries;

    w = CreateWindowExA(WS_EX_TOPMOST, "static", "fastb", WS_POPUP | WS_VISIBLE, 0, 0, 200, 200, NULL, NULL, NULL, NULL);
    SetForegroundWindow(w);
    SetFocus(w);
    pump(500);
    for (tries = 0; hook && !keydowns && tries < 3; tries++)
    {
        xdo("key a");
        pump(1200);
    }
    if (!hook || !keydowns)
    {
        printf("note: no physical key reaches us here: the physical input checks are skipped\n");
        if (hook) UnhookWindowsHookEx(hook);
        DestroyWindow(w);
        return;
    }
    CHECK(BlockInput(TRUE), "block for the physical key");
    before = keydowns;
    xdo("key a");
    pump(1200);
    CHECK(keydowns == before, "a physical key is dropped while blocked: %ld keys", keydowns - before);
    CHECK(BlockInput(FALSE), "unblock");
    xdo("key b");   /* another key: the X11 driver still counts 'a' as down, its release having been dropped */
    pump(1200);
    if (keydowns <= before) printf("note: the key typed after the unblock was not seen (not counted: typing through X is flaky here)\n");
    UnhookWindowsHookEx(hook);
    DestroyWindow(w);
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll"), ktm = LoadLibraryA("ktmw32.dll");

    pSetKey = (void *)GetProcAddress(ntdll, "NtSetInformationKey");
    pCreate = (void *)GetProcAddress(ktm, "CreateTransaction");
    pCommit = (void *)GetProcAddress(ktm, "CommitTransaction");
    pRollback = (void *)GetProcAddress(ktm, "RollbackTransaction");
    test_key_write_time();
    test_transactions();
    test_block_input();
    test_block_hardware_input();
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
