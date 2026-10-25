/* printguard-probe.c (wine-sg 6101): PrintWindow(PW_RENDERFULLCONTENT) of a window whose process keeps
 * others from its handles, as KeePassXC does (its process DACL leaves out PROCESS_DUP_HANDLE).
 *   printguard-probe.exe host DIR   -- such a process: a green window at 100,100 (300x200), a gray
 *                                      window of another class over its right half; DIR\host.ready
 *   printguard-probe.exe print DIR  -- PrintWindow of the green window from this process; writes
 *                                      DIR\print.out: dup= (PROCESS_DUP_HANDLE open refused?), ok=, left=, right= */
#include <windows.h>
#include <aclapi.h>
#include <stdio.h>
#include <string.h>

static char path[MAX_PATH];

static const char *file(const char *dir, const char *name)
{
    snprintf(path, sizeof(path), "%s\\%s", dir, name);
    return path;
}

static int host(const char *dir)
{
    SID_IDENTIFIER_AUTHORITY world = { SECURITY_WORLD_SID_AUTHORITY };
    EXPLICIT_ACCESS_W ea = {0};
    WNDCLASSW wc = {0};
    PSID everyone;
    PACL acl = NULL;
    DWORD start;
    FILE *f;
    MSG msg;

    /* as KeePassXC's Bootstrap does: only what others need to wait for it and query it */
    AllocateAndInitializeSid(&world, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone);
    ea.grfAccessPermissions = SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE;
    ea.grfAccessMode = SET_ACCESS;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.ptstrName = everyone;
    SetEntriesInAclW(1, &ea, NULL, &acl);
    SetSecurityInfo(GetCurrentProcess(), SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                    NULL, NULL, acl, NULL);

    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hbrBackground = CreateSolidBrush(RGB(0, 200, 0));
    wc.lpszClassName = L"SgPrintGuard";
    RegisterClassW(&wc);
    wc.hbrBackground = GetStockObject(GRAY_BRUSH);
    wc.lpszClassName = L"SgPrintGuardCover";
    RegisterClassW(&wc);
    ShowWindow(CreateWindowW(L"SgPrintGuard", L"SgPrintGuard", WS_POPUP, 100, 100, 300, 200, NULL, NULL, NULL, NULL), SW_SHOW);
    ShowWindow(CreateWindowW(L"SgPrintGuardCover", L"cover", WS_POPUP, 250, 50, 300, 300, NULL, NULL, NULL, NULL), SW_SHOW);
    start = GetTickCount();
    while (GetTickCount() - start < 1500)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(20);
    }
    if ((f = fopen(file(dir, "host.ready"), "w"))) fclose(f);
    while (GetTickCount() - start < 40000)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        Sleep(20);
    }
    return 0;
}

static int print(const char *dir)
{
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), 300, -200, 1, 32, BI_RGB }};
    HWND hwnd = FindWindowW(L"SgPrintGuard", NULL);
    HANDLE process;
    DWORD pid = 0, *bits;
    HBITMAP bmp;
    HDC dc;
    FILE *f;
    BOOL ok;

    if (!(f = fopen(file(dir, "print.out"), "w"))) return 2;
    GetWindowThreadProcessId(hwnd, &pid);
    process = OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid);
    fprintf(f, "dup=%s\n", process ? "allowed" : "refused");
    if (process) CloseHandle(process);
    dc = CreateCompatibleDC(NULL);
    bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    SelectObject(dc, bmp);
    ok = PrintWindow(hwnd, dc, PW_RENDERFULLCONTENT);
    fprintf(f, "ok=%d\nleft=%06lx\nright=%06lx\n", ok, bits[100 * 300 + 50] & 0xffffff, bits[100 * 300 + 250] & 0xffffff);
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) return 2;
    if (!strcmp(argv[1], "host")) return host(argv[2]);
    if (!strcmp(argv[1], "print")) return print(argv[2]);
    return 2;
}
