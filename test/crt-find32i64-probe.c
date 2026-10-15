/* Probe for patches/sg/4513: _findfirst32i64/_findnext32i64 and the wide
 * forms of the DLL named on the command line (msvcr80 .. msvcr120, ucrtbase).
 * A directory with a small file and a sparse file past 4 GiB: every entry is
 * found once with its 64-bit size, 32-bit times near now and its attributes.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <io.h>
#include <errno.h>
#include <stdlib.h>

struct fd32i64 { unsigned attrib; int time_create, time_access, time_write; __int64 size; char name[260]; };
struct wfd32i64 { unsigned attrib; int time_create, time_access, time_write; __int64 size; WCHAR name[260]; };

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static const struct { const char *name; __int64 size; DWORD attr; unsigned attrib; } files[] =
{
    {"small.txt", 3, FILE_ATTRIBUTE_NORMAL, 0},
    {"big.bin", 5LL << 30, FILE_ATTRIBUTE_NORMAL, 0},
    {"ro.txt", 7, FILE_ATTRIBUTE_READONLY, _A_RDONLY},
};

static int find_index(const char *name)
{
    unsigned int i;
    for (i = 0; i < ARRAYSIZE(files); i++) if (!strcmp(files[i].name, name)) return i;
    return -1;
}

int main(int argc, char **argv)
{
    intptr_t (__cdecl *pfirst)(const char *, struct fd32i64 *);
    int (__cdecl *pnext)(intptr_t, struct fd32i64 *);
    intptr_t (__cdecl *pwfirst)(const WCHAR *, struct wfd32i64 *);
    int (__cdecl *pwnext)(intptr_t, struct wfd32i64 *);
    int (__cdecl *pclose)(intptr_t);
    int *(__cdecl *perrno)(void);
    char dir[MAX_PATH], path[MAX_PATH + 32], pattern[MAX_PATH + 8];
    WCHAR wpattern[MAX_PATH + 8];
    int seen[ARRAYSIZE(files)], wseen[ARRAYSIZE(files)];
    unsigned int i, dots = 0, wdots = 0;
    struct fd32i64 fd;
    struct wfd32i64 wfd;
    HMODULE mod;
    intptr_t h;
    int now = (int)time(NULL), r;

    if (argc < 2 || !(mod = LoadLibraryA(argv[1]))) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    pfirst = (void *)GetProcAddress(mod, "_findfirst32i64");
    pnext = (void *)GetProcAddress(mod, "_findnext32i64");
    pwfirst = (void *)GetProcAddress(mod, "_wfindfirst32i64");
    pwnext = (void *)GetProcAddress(mod, "_wfindnext32i64");
    pclose = (void *)GetProcAddress(mod, "_findclose");
    perrno = (void *)GetProcAddress(mod, "_errno");
    check(pfirst && pnext && pwfirst && pwnext && pclose && perrno, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }

    GetTempPathA(sizeof(dir), dir);
    strcat(dir, "find32i64");
    CreateDirectoryA(dir, NULL);
    for (i = 0; i < ARRAYSIZE(files); i++)
    {
        LARGE_INTEGER size;
        HANDLE f;

        sprintf(path, "%s\\%s", dir, files[i].name);
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
        f = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        size.QuadPart = files[i].size;
        SetFilePointerEx(f, size, NULL, FILE_BEGIN);
        SetEndOfFile(f);
        CloseHandle(f);
        SetFileAttributesA(path, files[i].attr);
    }

    sprintf(pattern, "%s\\*", dir);
    MultiByteToWideChar(CP_ACP, 0, pattern, -1, wpattern, ARRAYSIZE(wpattern));
    memset(seen, 0, sizeof(seen)); memset(wseen, 0, sizeof(wseen));

    memset(&fd, 0xcc, sizeof(fd));
    for (h = pfirst(pattern, &fd), r = h == -1 ? -1 : 0; h != -1 && !r; r = pnext(h, &fd))
    {
        int k = find_index(fd.name);
        if (k < 0) { dots += fd.name[0] == '.'; continue; }
        seen[k]++;
        check(fd.size == files[k].size && (fd.attrib & ~_A_ARCH) == files[k].attrib &&
              abs(fd.time_write - now) < 600 && abs(fd.time_create - now) < 600,
              "%s: size %I64d attrib %#x write %d (now %d)", fd.name, fd.size, fd.attrib, fd.time_write, now);
        memset(&fd, 0xcc, sizeof(fd));
    }
    check(h != -1 && r == -1 && *perrno() == ENOENT, "narrow walk ends with ENOENT (%d)", *perrno());
    if (h != -1) pclose(h);

    for (h = pwfirst(wpattern, &wfd), r = h == -1 ? -1 : 0; h != -1 && !r; r = pwnext(h, &wfd))
    {
        char name[260];
        int k;

        WideCharToMultiByte(CP_ACP, 0, wfd.name, -1, name, sizeof(name), NULL, NULL);
        if ((k = find_index(name)) < 0) { wdots += name[0] == '.'; continue; }
        wseen[k]++;
        check(wfd.size == files[k].size && (wfd.attrib & ~_A_ARCH) == files[k].attrib && abs(wfd.time_access - now) < 600,
              "wide %s: size %I64d attrib %#x", name, wfd.size, wfd.attrib);
    }
    if (h != -1) pclose(h);

    for (i = 0; i < ARRAYSIZE(files); i++)
        check(seen[i] == 1 && wseen[i] == 1, "%s found %d / %d times", files[i].name, seen[i], wseen[i]);
    check(dots == 2 && wdots == 2, "dot entries %u / %u", dots, wdots);

    sprintf(pattern, "%s\\nothing*", dir);
    *perrno() = 0;
    h = pfirst(pattern, &fd);
    check(h == -1 && *perrno() == ENOENT, "no match: %Id errno %d", h, *perrno());

    for (i = 0; i < ARRAYSIZE(files); i++)
    {
        sprintf(path, "%s\\%s", dir, files[i].name);
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(path);
    }
    RemoveDirectoryA(dir);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
