#include <windows.h>
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static HANDLE ev_go, ev_done;
static HKL w_before, w_after;
static DWORD WINAPI worker(void *arg)
{
    w_before = GetKeyboardLayout(0);
    SetEvent(ev_done);
    WaitForSingleObject(ev_go, 5000);
    w_after = GetKeyboardLayout(0);
    SetEvent(ev_done);
    return 0;
}
static HKL late;
static DWORD WINAPI late_thread(void *arg) { late = GetKeyboardLayout(0); return 0; }
int main(void)
{
    HKL orig, other = (HKL)(ULONG_PTR)0x0123047f, old;
    HANDLE th, th2;
    ev_go = CreateEventA(NULL, FALSE, FALSE, NULL);
    ev_done = CreateEventA(NULL, FALSE, FALSE, NULL);
    orig = GetKeyboardLayout(0);
    th = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    WaitForSingleObject(ev_done, 5000);
    CHECK(w_before == orig, "worker starts with %p, main %p", w_before, orig);

    /* activate for this thread only */
    old = ActivateKeyboardLayout(other, 0);
    CHECK(old == orig, "old layout %p vs %p", old, orig);
    CHECK(GetKeyboardLayout(0) == other, "own layout %p", GetKeyboardLayout(0));
    old = ActivateKeyboardLayout(orig, 0);
    CHECK(old == other, "back %p", old);

    SetLastError(0xdeadbeef);
    old = ActivateKeyboardLayout(other, KLF_SETFORPROCESS);
    CHECK(old == orig, "setforprocess returned %p err %lu", old, GetLastError());
    CHECK(GetKeyboardLayout(0) == other, "own after setforprocess %p", GetKeyboardLayout(0));
    SetEvent(ev_go);
    WaitForSingleObject(ev_done, 5000);
    CHECK(w_after == other, "worker sees %p, expected %p", w_after, other);
    WaitForSingleObject(th, 5000);
    th2 = CreateThread(NULL, 0, late_thread, NULL, 0, NULL);
    WaitForSingleObject(th2, 5000);
    CHECK(late == other, "late thread sees %p", late);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
