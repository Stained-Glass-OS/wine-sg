/* conpty-probe: a terminal's own conpty.dll, as node-pty (VS Code) and Windows
 * Terminal ship it, runs over Wine's pseudo consoles (patches/sg/7003).
 *   conpty-probe.exe DIR      DIR\conpty.dll is the terminal's copy (a stand-in)
 * Prints key=1|0 lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* as Microsoft documents them (not in every mingw-w64) */
typedef void *PCON;
#define HPCON PCON
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE 0x00020016
#endif

typedef HRESULT (WINAPI *create_fn)(COORD, HANDLE, HANDLE, DWORD, HPCON *);
typedef HRESULT (WINAPI *hpc_fn)(HPCON);
typedef HRESULT (WINAPI *resize_fn)(HPCON, COORD);
typedef HRESULT (WINAPI *showhide_fn)(HPCON, BOOL);
typedef HRESULT (WINAPI *reparent_fn)(HPCON, HWND);
typedef void (WINAPI *close_fn)(HPCON);

static CRITICAL_SECTION cs;
static char out[1 << 20];
static size_t out_len;
static volatile LONG eof;

static DWORD WINAPI reader(void *arg)
{
    HANDLE pipe = arg;
    char buf[4096];
    DWORD n;
    while (ReadFile(pipe, buf, sizeof(buf), &n, NULL) && n)
    {
        EnterCriticalSection(&cs);
        if (out_len + n < sizeof(out)) { memcpy(out + out_len, buf, n); out_len += n; out[out_len] = 0; }
        LeaveCriticalSection(&cs);
    }
    InterlockedExchange(&eof, 1);
    return 0;
}

/* whether TEXT shows up in the output after offset FROM within MS */
static BOOL wait_for(const char *text, size_t from, DWORD ms)
{
    DWORD start = GetTickCount();
    for (;;)
    {
        BOOL found;
        EnterCriticalSection(&cs);
        found = out_len > from && strstr(out + from, text) != NULL;
        LeaveCriticalSection(&cs);
        if (found) return TRUE;
        if (GetTickCount() - start > ms) return FALSE;
        Sleep(50);
    }
}

static size_t mark(void) { size_t n; EnterCriticalSection(&cs); n = out_len; LeaveCriticalSection(&cs); return n; }

static void type(HANDLE in, const char *s) { DWORD n; WriteFile(in, s, strlen(s), &n, NULL); }

int main(int argc, char **argv)
{
    char path[MAX_PATH], cmdline[] = "cmd.exe /q /k prompt $G";
    HMODULE dll;
    create_fn create;
    HANDLE in_read, in_write, out_read, out_write, thread;
    HPCON hpc;
    HRESULT hr;
    STARTUPINFOEXA si = { { sizeof(si) } };
    PROCESS_INFORMATION pi;
    SIZE_T size;
    COORD c = { 80, 25 }, c2 = { 100, 30 };
    size_t m;
    BOOL ok;

    InitializeCriticalSection(&cs);
    snprintf(path, sizeof(path), "%s\\conpty.dll", argc > 1 ? argv[1] : ".");
    dll = LoadLibraryA(path);
    printf("loaded=%d\n", dll != NULL);
    if (!dll) return 1;
    create = (create_fn)GetProcAddress(dll, "ConptyCreatePseudoConsole");
    CreatePipe(&in_read, &in_write, NULL, 0);
    CreatePipe(&out_read, &out_write, NULL, 0);
    hr = create ? create(c, in_read, out_write, 0, &hpc) : E_FAIL;
    printf("create_hr=%#lx\n", hr);
    printf("builtin_used=%d\n", hr != (HRESULT)0x8000dead);
    printf("created=%d\n", hr == S_OK);
    if (hr != S_OK) { printf("done=1\n"); return 0; }
    CloseHandle(in_read); CloseHandle(out_write);
    thread = CreateThread(NULL, 0, reader, out_read, 0, NULL);

    InitializeProcThreadAttributeList(NULL, 1, 0, &size);
    si.lpAttributeList = HeapAlloc(GetProcessHeap(), 0, size);
    InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &size);
    UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc, sizeof(hpc), NULL, NULL);
    ok = CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi);
    printf("spawned=%d\n", ok);
    if (!ok) { printf("done=1\n"); return 0; }

    type(in_write, "echo first-line-on-pty\r");
    printf("output=%d\n", wait_for("first-line-on-pty", 0, 30000));
    if (getenv("CONPTY_DUMP")) { EnterCriticalSection(&cs); fwrite(out, 1, out_len, stderr); LeaveCriticalSection(&cs); }
    Sleep(500);

    /* the other signals, then a resize: the host must still read each one whole */
    ((showhide_fn)GetProcAddress(dll, "ConptyShowHidePseudoConsole"))(hpc, TRUE);
    ((reparent_fn)GetProcAddress(dll, "ConptyReparentPseudoConsole"))(hpc, GetDesktopWindow());
    hr = ((hpc_fn)GetProcAddress(dll, "ConptyClearPseudoConsole"))(hpc);
    printf("clear_ok=%d\n", hr == S_OK);
    Sleep(500);
    m = mark();
    hr = ((resize_fn)GetProcAddress(dll, "ConptyResizePseudoConsole"))(hpc, c2);
    printf("resize_ok=%d\n", hr == S_OK);
    Sleep(1500);
    /* the host repaints its buffer after a resize: what was cleared is not drawn again */
    EnterCriticalSection(&cs);
    printf("cleared_not_redrawn=%d\n", out_len > m && !strstr(out + m, "first-line-on-pty"));
    LeaveCriticalSection(&cs);

    m = mark();
    type(in_write, "echo after-the-signals\r");
    printf("signals_in_sync=%d\n", wait_for("after-the-signals", m, 15000));

    /* released, the host goes with its last client: the output pipe ends */
    hr = ((hpc_fn)GetProcAddress(dll, "ConptyReleasePseudoConsole"))(hpc);
    printf("release_ok=%d\n", hr == S_OK);
    type(in_write, "exit\r");
    WaitForSingleObject(pi.hProcess, 15000);
    printf("eof_after_release=%d\n", WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0 && eof);
    ((close_fn)GetProcAddress(dll, "ConptyClosePseudoConsole"))(hpc);
    printf("done=1\n");
    return 0;
}
