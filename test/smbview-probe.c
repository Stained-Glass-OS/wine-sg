/* How File Explorer shows a folder on a network share (patches/sg/0824,
 * 0825), on a stand-in share (test/smbshim.c).
 *
 *   smbview-probe paint N WATCHMS CMDLINE
 *       starts CMDLINE (explorer.exe on the folder), waits for its view to
 *       hold N items and watches the view's pixels on the screen: when they
 *       were first drawn, when they last changed, how many times they changed
 *       after the first drawing, and the longest the window took to answer a
 *       message meanwhile (a busy window stands still):
 *       items=N listed=MS first=MS settled=MS changes=N slowest=MS
 *   smbview-probe stale
 *       a window registered with the shell's list of File Explorer windows
 *       (IShellWindows) at C:\windows by a process that then ends abruptly
 *       (as a crash would): File Explorer opening that folder asks the list
 *       and must not be given the dead window: alive=HR dead=HR */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <exdisp.h>
#include <commctrl.h>
#include <stdio.h>

static HWND lv;
static BOOL CALLBACK child(HWND h, LPARAM l)
{
    char c[64], pc[64];
    GetClassNameA(h, c, sizeof c); GetClassNameA(GetParent(h), pc, sizeof pc);
    if (!strcmp(c, "SysListView32") && !strcmp(pc, "SHELLDLL_DefView")) { lv = h; return FALSE; }
    return TRUE;
}

static unsigned long long frame_hash(HDC screen, HDC mem, HBITMAP bmp, const RECT *r, BOOL *uniform)
{
    BITMAPINFO bi = { { sizeof(BITMAPINFOHEADER), r->right - r->left, -(r->bottom - r->top), 1, 32, BI_RGB } };
    int w = r->right - r->left, h = r->bottom - r->top, i;
    DWORD *px = malloc(w * h * 4);
    unsigned long long hsh = 1469598103934665603ull;
    BitBlt(mem, 0, 0, w, h, screen, r->left, r->top, SRCCOPY);
    GetDIBits(mem, bmp, 0, h, px, &bi, DIB_RGB_COLORS);
    *uniform = TRUE;
    for (i = 0; i < w * h; i++)
    {
        if (px[i] != px[0]) *uniform = FALSE;
        hsh = (hsh ^ (px[i] & 0xffffff)) * 1099511628211ull;
    }
    free(px);
    return hsh;
}

static int paint(int want, int watch, char *cmdline)
{
    int changes = 0, count = 0;
    DWORD t0 = GetTickCount(), listed = 0, first = 0, settled = 0, now, slowest = 0;
    STARTUPINFOA si = { sizeof si }; PROCESS_INFORMATION pi;
    unsigned long long last = 0, hsh;
    HDC screen = GetDC(NULL), mem = CreateCompatibleDC(screen);
    HBITMAP bmp = NULL;
    RECT r = { 0 };
    BOOL uniform;

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 1;
    while ((now = GetTickCount()) - t0 < 60000)
    {
        if (!lv || !IsWindow(lv))
        {
            HWND w = FindWindowA("ExplorerWClass", NULL);
            lv = NULL;
            if (w) EnumChildWindows(w, child, 0);
        }
        if (lv && !listed && (count = SendMessageA(lv, LVM_GETITEMCOUNT, 0, 0)) >= want) listed = now - t0;
        if (lv && listed)
        {
            /* how long the window takes to answer while it draws */
            DWORD_PTR res;
            DWORD s = GetTickCount();
            SendMessageTimeoutA(lv, WM_NULL, 0, 0, SMTO_NORMAL, 5000, &res);
            if (GetTickCount() - s > slowest) slowest = GetTickCount() - s;
        }
        if (lv && IsWindowVisible(lv))
        {
            RECT wr;
            GetWindowRect(lv, &wr);
            wr.right -= GetSystemMetrics(SM_CXVSCROLL) + 2;   /* the scroll bar is not the items */
            if (wr.right > wr.left + 16 && wr.bottom > wr.top + 16)
            {
                if (!bmp || memcmp(&wr, &r, sizeof(r)))
                {
                    if (bmp) DeleteObject(bmp);
                    r = wr;
                    bmp = CreateCompatibleBitmap(screen, r.right - r.left, r.bottom - r.top);
                    SelectObject(mem, bmp);
                }
                hsh = frame_hash(screen, mem, bmp, &r, &uniform);
                if (!uniform && hsh != last)
                {
                    if (!first) first = now - t0;
                    else changes++;
                    settled = now - t0;
                    last = hsh;
                }
            }
        }
        if (listed && first && now - t0 > (settled > listed ? settled : listed) + watch) break;
        Sleep(5);
    }
    if (lv) count = SendMessageA(lv, LVM_GETITEMCOUNT, 0, 0);
    printf("items=%d listed=%lu first=%lu settled=%lu changes=%d slowest=%lu\n", count, listed, first, settled,
           changes, slowest);
    fflush(stdout);
    return 0;
}

static VARIANT pidl_variant(ITEMIDLIST *pidl)
{
    VARIANT v;
    V_VT(&v) = VT_ARRAY | VT_UI1;
    V_ARRAY(&v) = SafeArrayCreateVector(VT_UI1, 0, ILGetSize(pidl));
    memcpy(V_ARRAY(&v)->pvData, pidl, ILGetSize(pidl));
    return v;
}

static int stale(BOOL child)
{
    IShellWindows *sw;
    IDispatch *disp;
    VARIANT where, empty;
    LONG cookie, hwnd = 0;
    HRESULT alive, dead;

    CoInitialize(NULL);
    if (FAILED(CoCreateInstance(&CLSID_ShellWindows, NULL, CLSCTX_LOCAL_SERVER, &IID_IShellWindows, (void **)&sw)))
    {
        printf("no ShellWindows\n");
        return 1;
    }
    where = pidl_variant(ILCreateFromPathW(L"C:\\windows"));
    V_VT(&empty) = VT_EMPTY;
    if (child)
    {
        HWND w = CreateWindowA("static", "smbview-probe", WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL);
        IShellWindows_Register(sw, NULL, (LONG)(LONG_PTR)w, SWC_EXPLORER, &cookie);
        IShellWindows_OnNavigate(sw, cookie, &where);
        Sleep(60000);
    }
    else
    {
        STARTUPINFOA si = { sizeof si };
        PROCESS_INFORMATION pi;
        char cmd[MAX_PATH + 16];

        GetModuleFileNameA(NULL, cmd, MAX_PATH);
        strcat(cmd, " stale-child");
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 1;
        Sleep(3000);
        alive = IShellWindows_FindWindowSW(sw, &where, &empty, SWC_EXPLORER, &hwnd, 0, &disp);
        TerminateProcess(pi.hProcess, 1);
        WaitForSingleObject(pi.hProcess, 5000);
        Sleep(1000);
        hwnd = 0;
        dead = IShellWindows_FindWindowSW(sw, &where, &empty, SWC_EXPLORER, &hwnd, 0, &disp);
        printf("alive=%08lx dead=%08lx\n", alive, dead);
        fflush(stdout);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 5 && !strcmp(argv[1], "paint")) return paint(atoi(argv[2]), atoi(argv[3]), argv[4]);
    if (argc >= 2 && !strcmp(argv[1], "stale")) return stale(FALSE);
    if (argc >= 2 && !strcmp(argv[1], "stale-child")) return stale(TRUE);
    return 2;
}
