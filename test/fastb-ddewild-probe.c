/* shell32 (3705): the shell's DDE server answers XTYP_WILDCONNECT (null service and/or topic) with the
 * pairs that match.  Two processes: the server side runs ShellDDEInit(TRUE) and a message loop; the
 * client lists the conversations DdeConnectList makes. */
#include <windows.h>
#include <ddeml.h>
#include <stdio.h>
#include <string.h>

static int failed;
#define CHECK(c, ...) do { if (!(c)) { failed++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static int server(void)
{
    typedef void (WINAPI *init_t)(BOOL);
    init_t init = (init_t)GetProcAddress(LoadLibraryA("shell32.dll"), (LPCSTR)188);
    HANDLE ready = OpenEventA(EVENT_MODIFY_STATE, FALSE, "sgdde_ready"), quit = OpenEventA(SYNCHRONIZE, FALSE, "sgdde_quit");
    MSG msg;

    if (!init || !ready || !quit) return 3;
    init(TRUE);
    SetEvent(ready);
    for (;;)
    {
        DWORD r = MsgWaitForMultipleObjects(1, &quit, FALSE, 60000, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0 || r == WAIT_TIMEOUT) break;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
    }
    init(FALSE);
    return 0;
}

/* A DDE client on the raw protocol: a broadcast WM_DDE_INITIATE with the service and topic atoms (null
 * for "any"), and every WM_DDE_ACK that comes back names one service/topic pair of one server. */
static char found[32][64];
static int found_count;

static LRESULT CALLBACK client_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DDE_ACK)
    {
        char svc[32] = "", top[32] = "";
        ATOM app = LOWORD(lp), topic = HIWORD(lp);

        if (app) GlobalGetAtomNameA(app, svc, sizeof(svc));
        if (topic) GlobalGetAtomNameA(topic, top, sizeof(top));
        if (found_count < 32) sprintf(found[found_count++], "%s/%s", svc, top);
        if (app) GlobalDeleteAtom(app);
        if (topic) GlobalDeleteAtom(topic);
        PostMessageW((HWND)wp, WM_DDE_TERMINATE, (WPARAM)hwnd, 0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int list_pairs(HWND client, const char *service, const char *topic, char pairs[][64], int max)
{
    ATOM a = service ? GlobalAddAtomA(service) : 0, t = topic ? GlobalAddAtomA(topic) : 0;
    MSG msg;
    int i, n;

    found_count = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_DDE_INITIATE, (WPARAM)client, MAKELPARAM(a, t), SMTO_ABORTIFHUNG, 5000, NULL);
    for (i = 0; i < 10; i++)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(20);
    }
    if (a) GlobalDeleteAtom(a);
    if (t) GlobalDeleteAtom(t);
    /* several shell servers can run (the prefix's own explorer): each pair once */
    for (i = 0, n = 0; i < found_count && n < max; i++)
    {
        int k;
        for (k = 0; k < n; k++) if (!strcmp(pairs[k], found[i])) break;
        if (k == n) strcpy(pairs[n++], found[i]);
    }
    return n;
}

static BOOL has(char pairs[][64], int n, const char *pair)
{
    int i;
    for (i = 0; i < n; i++) if (!lstrcmpiA(pairs[i], pair)) return TRUE;
    return FALSE;
}

int main(int argc, char **argv)
{
    HANDLE ready, quit;
    PROCESS_INFORMATION pi;
    STARTUPINFOA si = { sizeof(si) };
    char cmd[MAX_PATH + 16], pairs[16][64];
    DWORD code = 99;
    HWND client;
    WNDCLASSW wc = { 0 };
    int n;

    if (argc > 1) return server();
    ready = CreateEventA(NULL, TRUE, FALSE, "sgdde_ready");
    quit = CreateEventA(NULL, TRUE, FALSE, "sgdde_quit");
    GetModuleFileNameA(NULL, cmd, MAX_PATH);
    strcat(cmd, " server");
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) { printf("FAIL cannot start the server\nRESULT: FAIL\n"); return 1; }
    CHECK(WaitForSingleObject(ready, 30000) == WAIT_OBJECT_0, "server did not start");
    Sleep(200);
    wc.lpfnWndProc = client_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpszClassName = L"SGDDEClient";
    RegisterClassW(&wc);
    client = CreateWindowW(L"SGDDEClient", L"client", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);

    /* topic and service given: an ordinary connect still works */
    n = list_pairs(client, "Folders", "Shell", pairs, 16);
    CHECK(n == 1 && has(pairs, n, "Folders/Shell"), "exact pair: %d %s", n, n ? pairs[0] : "");

    /* a null service: every pair of the topic */
    n = list_pairs(client, NULL, "Progman", pairs, 16);
    CHECK(has(pairs, n, "Progman/Progman") && has(pairs, n, "AppProperties/Progman") && !has(pairs, n, "Folders/Shell")
          && !has(pairs, n, "AppProperties/Shell"), "null service, topic Progman: %d pairs", n);
    n = list_pairs(client, NULL, "Shell", pairs, 16);
    CHECK(has(pairs, n, "Folders/Shell") && has(pairs, n, "AppProperties/Shell") && !has(pairs, n, "Progman/Progman")
          && !has(pairs, n, "AppProperties/Progman"), "null service, topic Shell: %d pairs", n);

    /* a null topic: every pair of the service */
    n = list_pairs(client, "Folders", NULL, pairs, 16);
    CHECK(n == 1 && has(pairs, n, "Folders/Shell"), "service Folders, null topic: %d", n);
    n = list_pairs(client, "Progman", NULL, pairs, 16);
    CHECK(has(pairs, n, "Progman/Progman") && !has(pairs, n, "Folders/Shell"), "service Progman, null topic: %d", n);

    /* both null: all four */
    n = list_pairs(client, NULL, NULL, pairs, 16);
    CHECK(has(pairs, n, "Progman/Progman") && has(pairs, n, "AppProperties/Progman") && has(pairs, n, "Folders/Shell")
          && has(pairs, n, "AppProperties/Shell"), "both null: %d pairs", n);

    /* no match: nothing */
    n = list_pairs(client, NULL, "NoSuchTopic", pairs, 16);
    CHECK(n == 0, "unknown topic: %d", n);
    n = list_pairs(client, "NoSuchService", NULL, pairs, 16);
    CHECK(n == 0, "unknown service: %d", n);

    DestroyWindow(client);
    SetEvent(quit);
    WaitForSingleObject(pi.hProcess, 20000);
    GetExitCodeProcess(pi.hProcess, &code);
    CHECK(code == 0, "server exit %lu", code);
    printf("failures: %d\nRESULT: %s\n", failed, failed ? "FAIL" : "PASS");
    return failed != 0;
}
