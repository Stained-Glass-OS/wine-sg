/* PSS probe (wine-sg 0620): what Python 3.12's os.getppid and a UPX-packed
 * python312.dll ask of kernel32's process snapshot API. */
#include <windows.h>
#include <processsnapshot.h>
#include <stdio.h>
#include <wchar.h>

typedef DWORD (WINAPI *capture_fn)(HANDLE, PSS_CAPTURE_FLAGS, DWORD, HPSS *);
typedef DWORD (WINAPI *query_fn)(HPSS, PSS_QUERY_INFORMATION_CLASS, void *, DWORD);
typedef DWORD (WINAPI *free_fn)(HANDLE, HPSS);
typedef DWORD (WINAPI *walkcreate_fn)(const PSS_ALLOCATOR *, HPSSWALK *);
typedef DWORD (WINAPI *walk_fn)(HPSS, PSS_WALK_INFORMATION_CLASS, HPSSWALK, void *, DWORD);
typedef DWORD (WINAPI *walkfree_fn)(HPSSWALK);

int wmain(int argc, WCHAR **argv)
{
    static const char *names[] = { "PssCaptureSnapshot", "PssDuplicateSnapshot", "PssFreeSnapshot",
        "PssQuerySnapshot", "PssWalkMarkerCreate", "PssWalkMarkerFree", "PssWalkMarkerGetPosition",
        "PssWalkMarkerSeekToBeginning", "PssWalkMarkerSetPosition", "PssWalkSnapshot" };
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    PSS_PROCESS_INFORMATION info;
    HPSS snap;
    HPSSWALK walk;
    WCHAR self[MAX_PATH];
    unsigned i, found = 0;
    DWORD err;
    char buf[64];

    if (argc > 1 && !wcscmp(argv[1], L"parent"))
    {
        /* run the probe again, as a child, and say our id */
        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        WCHAR cmd[MAX_PATH + 64];
        GetModuleFileNameW(NULL, self, MAX_PATH);
        swprintf(cmd, ARRAYSIZE(cmd), L"\"%ls\" child %lu", self, GetCurrentProcessId());
        if (!CreateProcessW(self, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi)) return 2;
        WaitForSingleObject(pi.hProcess, 30000);
        return 0;
    }

    for (i = 0; i < ARRAYSIZE(names); i++) if (GetProcAddress(k32, names[i])) found++;
    printf("exports=%u/%u\n", found, (unsigned)ARRAYSIZE(names));
    if (found != ARRAYSIZE(names)) return 1;

    capture_fn capture = (capture_fn)GetProcAddress(k32, "PssCaptureSnapshot");
    query_fn query = (query_fn)GetProcAddress(k32, "PssQuerySnapshot");
    free_fn pssfree = (free_fn)GetProcAddress(k32, "PssFreeSnapshot");
    walkcreate_fn walkcreate = (walkcreate_fn)GetProcAddress(k32, "PssWalkMarkerCreate");
    walk_fn walksnap = (walk_fn)GetProcAddress(k32, "PssWalkSnapshot");
    walkfree_fn walkfree = (walkfree_fn)GetProcAddress(k32, "PssWalkMarkerFree");

    /* as CPython's os.getppid */
    err = capture(GetCurrentProcess(), PSS_CAPTURE_NONE, 0, &snap);
    printf("capture=%lu\n", err);
    if (err) return 1;
    err = query(snap, PSS_QUERY_PROCESS_INFORMATION, &info, sizeof(info));
    printf("query=%lu\n", err);
    printf("pid=%lu self=%lu\n", info.ProcessId, GetCurrentProcessId());
    printf("ppid=%lu\n", info.ParentProcessId);
    WideCharToMultiByte(CP_UTF8, 0, info.ImageFileName, -1, buf, sizeof(buf), NULL, NULL);
    printf("image=%s\n", buf);
    printf("priority=%lu workingset=%s\n", info.PriorityClass, info.WorkingSetSize ? "yes" : "no");
    printf("short=%lu\n", query(snap, PSS_QUERY_PROCESS_INFORMATION, &info, 8));
    printf("threads=%lu\n", query(snap, PSS_QUERY_THREAD_INFORMATION, &info, sizeof(info)));
    err = walkcreate(NULL, &walk);
    printf("walk=%lu\n", err ? err : walksnap(snap, PSS_WALK_THREADS, walk, &info, sizeof(info)));
    if (!err) walkfree(walk);
    printf("free=%lu\n", pssfree(GetCurrentProcess(), snap));
    if (argc > 2 && !wcscmp(argv[1], L"child")) printf("expected-ppid=%ls\n", argv[2]);
    return 0;
}
