/* Wait completion packets (patches/sg/6125): NtCreateWaitCompletionPacket,
 * NtAssociateWaitCompletionPacket, NtCancelWaitCompletionPacket. A packet ties a waitable object to
 * an I/O completion port: once the object is signalled, a completion with the packet's key, APC
 * context, status and information is queued to the port. The Windows App SDK's CoreMessagingXP
 * (WinUI 3's dispatcher) uses them; they did not exist, and App Installer died at the first call.
 * Prints KEY=VALUE lines. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>

typedef NTSTATUS (WINAPI *create_fn)(HANDLE *, ACCESS_MASK, OBJECT_ATTRIBUTES *);
typedef NTSTATUS (WINAPI *assoc_fn)(HANDLE, HANDLE, HANDLE, void *, void *, NTSTATUS, ULONG_PTR, BOOLEAN *);
typedef NTSTATUS (WINAPI *cancel_fn)(HANDLE, BOOLEAN);

int main(void)
{
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    create_fn create = (create_fn)GetProcAddress(ntdll, "NtCreateWaitCompletionPacket");
    assoc_fn assoc = (assoc_fn)GetProcAddress(ntdll, "NtAssociateWaitCompletionPacket");
    cancel_fn cancel = (cancel_fn)GetProcAddress(ntdll, "NtCancelWaitCompletionPacket");
    HANDLE port, event, packet, dup;
    BOOLEAN already = 2;
    DWORD bytes;
    ULONG_PTR key;
    OVERLAPPED *ov;
    NTSTATUS status;
    BOOL got;

    printf("exports=%d\n", create && assoc && cancel);
    if (!create || !assoc || !cancel) return 1;
    port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
    event = CreateEventW(NULL, FALSE, FALSE, NULL);   /* auto-reset: the packet's wait consumes it */
    status = create(&packet, GENERIC_ALL, NULL);
    printf("create=0x%08lx\n", status);
    printf("dup=%d\n", DuplicateHandle(GetCurrentProcess(), packet, GetCurrentProcess(), &dup, 0, FALSE, DUPLICATE_SAME_ACCESS) && CloseHandle(dup));

    /* not signalled: queued when it is */
    status = assoc(packet, port, event, (void *)0x1234, (void *)0x5678, 0x77, 42, &already);
    printf("assoc=0x%08lx already=%d\n", status, already);
    got = GetQueuedCompletionStatus(port, &bytes, &key, &ov, 200);
    printf("early=%d\n", got);
    SetEvent(event);
    got = GetQueuedCompletionStatus(port, &bytes, &key, &ov, 2000);
    printf("queued=%d key=%#Ix apc=%p info=%lu\n", got, key, ov, bytes);
    printf("consumed=%d\n", WaitForSingleObject(event, 0) == WAIT_TIMEOUT);

    /* signalled already: queued at once */
    SetEvent(event);
    already = 2;
    status = assoc(packet, port, event, (void *)0xa, (void *)0xb, 0, 7, &already);
    printf("assoc2=0x%08lx already2=%d\n", status, already);
    got = GetQueuedCompletionStatus(port, &bytes, &key, &ov, 0);
    printf("queued2=%d key2=%#Ix info2=%lu\n", got, key, bytes);

    /* cancelled before the object is signalled: nothing is queued */
    status = assoc(packet, port, event, (void *)0xc, NULL, 0, 0, &already);
    printf("assoc3=0x%08lx\n", status);
    printf("cancel=0x%08lx\n", cancel(packet, FALSE));
    SetEvent(event);
    got = GetQueuedCompletionStatus(port, &bytes, &key, &ov, 300);
    printf("after_cancel=%d\n", got);
    printf("still_set=%d\n", WaitForSingleObject(event, 0) == WAIT_OBJECT_0);
    printf("cancel_idle=0x%08lx\n", cancel(packet, FALSE));
    printf("close=%d\n", CloseHandle(packet));
    return 0;
}
