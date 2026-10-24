/* regnotifyapc-probe.c: NtNotifyChangeMultipleKeys / NtNotifyChangeKey with an APC routine (wine-sg 6082).
 * GLib's registry watch (GTK programs) passes an APC and no event. Prints KEY=VALUE lines. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

#ifndef STATUS_NOTIFY_CLEANUP
#define STATUS_NOTIFY_CLEANUP ((NTSTATUS)0x0000010B)
#endif

typedef NTSTATUS (WINAPI *notify_multiple_fn)(HANDLE, ULONG, OBJECT_ATTRIBUTES *, HANDLE, PIO_APC_ROUTINE, void *,
                                              IO_STATUS_BLOCK *, ULONG, BOOLEAN, void *, ULONG, BOOLEAN);
typedef NTSTATUS (WINAPI *notify_key_fn)(HANDLE, HANDLE, PIO_APC_ROUTINE, void *, IO_STATUS_BLOCK *, ULONG,
                                         BOOLEAN, void *, ULONG, BOOLEAN);

static notify_multiple_fn pNtNotifyChangeMultipleKeys;
static notify_key_fn pNtNotifyChangeKey;

static LONG calls;
static void *last_context;
static IO_STATUS_BLOCK *last_iosb;
static NTSTATUS last_status;
static DWORD last_thread;

static void WINAPI apc(void *context, IO_STATUS_BLOCK *iosb, ULONG reserved)
{
    InterlockedIncrement(&calls);
    last_context = context;
    last_iosb = iosb;
    last_status = iosb ? iosb->Status : 0xdeadbeef;
    last_thread = GetCurrentThreadId();
}

static void say(const char *key, const char *fmt, ...)
{
    va_list args;
    printf("%s=", key);
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

static HKEY open_key(const char *sub)
{
    HKEY key = NULL;
    char path[128];
    snprintf(path, sizeof(path), "Software\\SgRegApc%s", sub);
    RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    return key;
}

static DWORD WINAPI writer(void *arg)
{
    HKEY key = open_key("");
    DWORD v = (DWORD)(ULONG_PTR)arg;
    Sleep(200);
    RegSetValueExA(key, "v", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    RegCloseKey(key);
    return 0;
}

static void reset(void)
{
    calls = 0;
    last_context = NULL;
    last_iosb = NULL;
    last_status = 0xdeadbeef;
    last_thread = 0;
}

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    IO_STATUS_BLOCK iosb, iosb2;
    HANDLE thread, event;
    NTSTATUS status;
    HKEY key, sub;
    DWORD v = 1;
    int ctx;

    pNtNotifyChangeMultipleKeys = (void *)GetProcAddress(ntdll, "NtNotifyChangeMultipleKeys");
    pNtNotifyChangeKey = (void *)GetProcAddress(ntdll, "NtNotifyChangeKey");
    say("bits", "%d", (int)sizeof(void *) * 8);

    /* an APC and no event, as GLib's g_win32_registry_key_watch does */
    key = open_key("");
    reset();
    memset(&iosb, 0xcc, sizeof(iosb));
    status = pNtNotifyChangeMultipleKeys(key, 0, NULL, NULL, apc, &ctx, &iosb, REG_NOTIFY_CHANGE_LAST_SET,
                                         FALSE, NULL, 0, TRUE);
    say("apc_register", "%#lx", status);
    thread = CreateThread(NULL, 0, writer, (void *)1, 0, NULL);
    WaitForSingleObject(thread, 5000);
    CloseHandle(thread);
    /* a user APC: not in a plain wait, only in an alertable one */
    Sleep(300);
    say("apc_before_alertable", "%ld", calls);
    SleepEx(2000, TRUE);
    say("apc_calls", "%ld", calls);
    say("apc_context", "%d", last_context == &ctx);
    say("apc_iosb", "%d", last_iosb == &iosb);
    say("apc_status", "%#lx", last_status);
    say("apc_thread", "%d", last_thread == GetCurrentThreadId());
    /* one change, one call: the watch is over until registered again */
    v = 2;
    RegSetValueExA(key, "v", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    SleepEx(300, TRUE);
    say("apc_once", "%ld", calls);

    /* NtNotifyChangeKey, a subtree, a new subkey (another handle: a handle keeps its first filter) */
    RegCloseKey(key);
    key = open_key("");
    reset();
    status = pNtNotifyChangeKey(key, NULL, apc, &ctx, &iosb2, REG_NOTIFY_CHANGE_NAME, TRUE, NULL, 0, TRUE);
    say("subtree_register", "%#lx", status);
    sub = open_key("\\child");
    SleepEx(1000, TRUE);
    say("subtree_calls", "%ld", calls);
    say("subtree_iosb", "%d", last_iosb == &iosb2);
    RegCloseKey(sub);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SgRegApc\\child");

    /* closing the handle ends a watch: NotifyCleanup */
    reset();
    status = pNtNotifyChangeMultipleKeys(key, 0, NULL, NULL, apc, &ctx, &iosb, REG_NOTIFY_CHANGE_LAST_SET,
                                         FALSE, NULL, 0, TRUE);
    say("close_register", "%#lx", status);
    RegCloseKey(key);
    SleepEx(1000, TRUE);
    say("close_calls", "%ld", calls);
    say("close_status", "%#lx", last_status);

    /* an event and an APC both */
    key = open_key("");
    reset();
    event = CreateEventA(NULL, TRUE, FALSE, NULL);
    status = pNtNotifyChangeMultipleKeys(key, 0, NULL, event, apc, &ctx, &iosb, REG_NOTIFY_CHANGE_LAST_SET,
                                         FALSE, NULL, 0, TRUE);
    say("both_register", "%#lx", status);
    v = 3;
    RegSetValueExA(key, "v", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    say("both_event", "%d", WaitForSingleObject(event, 2000) == WAIT_OBJECT_0);
    SleepEx(1000, TRUE);
    say("both_calls", "%ld", calls);

    /* the event alone, as before */
    reset();
    ResetEvent(event);
    say("event_register", "%ld", RegNotifyChangeKeyValue(key, FALSE, REG_NOTIFY_CHANGE_LAST_SET, event, TRUE));
    v = 4;
    RegSetValueExA(key, "v", 0, REG_DWORD, (BYTE *)&v, sizeof(v));
    say("event_signaled", "%d", WaitForSingleObject(event, 2000) == WAIT_OBJECT_0);
    SleepEx(300, TRUE);
    say("event_calls", "%ld", calls);
    CloseHandle(event);
    RegCloseKey(key);
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\SgRegApc");

    say("done", "1");
    return 0;
}
