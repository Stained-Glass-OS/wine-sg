/* mciconfig-probe: the MCIAVI configure dialog (patches/sg/6513).
 *
 * MCI_CONFIGURE on an AVI device was a stub (and DRV_CONFIGURE a "Sample AVI Wine Driver !"
 * message box). Opens a small AVI the probe writes, runs "configure", picks Full screen and Zoom
 * by 2 in the dialog from a watcher thread, and checks the options are kept and used: the next
 * open's destination is twice the size, a plain "play" is full screen; Cancel keeps them.
 *
 * Prints name=value lines; see test/mciconfig-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>
#include <stdio.h>
#include <string.h>

#define W 32
#define H 16

static char avi[MAX_PATH];
static volatile int stage; /* 0: OK with Full screen + Zoom; 1: Defaults then Cancel */

static void write_avi(void)
{
    AVISTREAMINFOA info = { 0 };
    BITMAPINFOHEADER bih = { sizeof(bih), W, H, 1, 24, BI_RGB, W * H * 3 };
    static BYTE bits[W * H * 3];
    PAVIFILE file;
    PAVISTREAM stream;
    int i;

    memset(bits, 0x60, sizeof(bits));
    AVIFileInit();
    DeleteFileA(avi);
    if (FAILED(AVIFileOpenA(&file, avi, OF_CREATE | OF_WRITE, NULL))) return;
    info.fccType = streamtypeVIDEO;
    info.fccHandler = mmioFOURCC('D','I','B',' ');
    info.dwScale = 1;
    info.dwRate = 10;
    SetRect(&info.rcFrame, 0, 0, W, H);
    if (SUCCEEDED(AVIFileCreateStreamA(file, &stream, &info)))
    {
        AVIStreamSetFormat(stream, 0, &bih, sizeof(bih));
        for (i = 0; i < 50; i++) AVIStreamWrite(stream, i, 1, bits, sizeof(bits), AVIIF_KEYFRAME, NULL, NULL);
        AVIStreamRelease(stream);
    }
    AVIFileRelease(file);
    AVIFileExit();
}

static MCIERROR mci(const char *cmd, char *ret, int size)
{
    char buf[256];
    MCIERROR err = mciSendStringA(cmd, ret ? ret : buf, ret ? size : sizeof(buf), NULL);
    printf("mci \"%s\" -> %lu %s\n", cmd, err, ret ? ret : "");
    return err;
}

static BOOL CALLBACK find_dialog(HWND hwnd, LPARAM lparam)
{
    char title[64];
    GetWindowTextA(hwnd, title, sizeof(title));
    if (IsWindowVisible(hwnd) && !strcmp(title, "Video Playback Options")) { *(HWND *)lparam = hwnd; return FALSE; }
    return TRUE;
}

static DWORD WINAPI watcher(void *arg)
{
    DWORD process = GetCurrentProcessId();
    HWND dlg = NULL;
    int i;

    for (i = 0; i < 100 && !dlg; i++)
    {
        Sleep(100);
        EnumWindows(find_dialog, (LPARAM)&dlg);
    }
    (void)process;
    if (stage == 0)
    {
        printf("dialog=%d\n", dlg != NULL);
        if (!dlg) return 0;
        printf("dialog_defaults=%d\n", IsDlgButtonChecked(dlg, 0x101) == BST_CHECKED &&
               IsDlgButtonChecked(dlg, 0x102) != BST_CHECKED && IsDlgButtonChecked(dlg, 0x103) != BST_CHECKED &&
               IsDlgButtonChecked(dlg, 0x104) == BST_CHECKED);
        SendMessageA(GetDlgItem(dlg, 0x102), BM_CLICK, 0, 0);
        SendMessageA(GetDlgItem(dlg, 0x103), BM_CLICK, 0, 0);
        SendMessageA(GetDlgItem(dlg, IDOK), BM_CLICK, 0, 0);
    }
    else
    {
        printf("dialog_shows_saved=%d\n", dlg && IsDlgButtonChecked(dlg, 0x102) == BST_CHECKED &&
               IsDlgButtonChecked(dlg, 0x103) == BST_CHECKED);
        if (!dlg) return 0;
        SendMessageA(GetDlgItem(dlg, 0x105), BM_CLICK, 0, 0);
        SendMessageA(GetDlgItem(dlg, IDCANCEL), BM_CLICK, 0, 0);
    }
    return 0;
}

static DWORD reg_value(const char *name)
{
    DWORD value = 0xdead, size = sizeof(value);
    RegGetValueA(HKEY_CURRENT_USER, "Software\\Stained Glass\\MCIAVI", name, RRF_RT_REG_DWORD, NULL, &value, &size);
    return value;
}

struct full { HWND hwnd; int width, height; };

static BOOL CALLBACK find_full(HWND hwnd, LPARAM lparam)
{
    struct full *f = (struct full *)lparam;
    DWORD pid;
    RECT rc;

    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
    GetWindowRect(hwnd, &rc);
    if (rc.right - rc.left >= f->width && rc.bottom - rc.top >= f->height) { f->hwnd = hwnd; return FALSE; }
    return TRUE;
}

int main(void)
{
    char ret[128], cmd[MAX_PATH + 64];
    struct full full;
    HANDLE thread;
    MSG msg;
    int i;

    setvbuf(stdout, NULL, _IONBF, 0);
    GetTempPathA(MAX_PATH, avi);
    strcat(avi, "mciconfig.avi");
    write_avi();
    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Stained Glass\\MCIAVI");

    sprintf(cmd, "open \"%s\" type avivideo alias m", avi);
    printf("open=%d\n", mci(cmd, NULL, 0) == 0);
    mci("where m destination", ret, sizeof(ret));
    printf("dest_plain=%d\n", !strcmp(ret, "0 0 32 16"));
    printf("configure_test=%d\n", mci("configure m test", NULL, 0) == 0);

    stage = 0;
    thread = CreateThread(NULL, 0, watcher, NULL, 0, NULL);
    printf("configure=%d\n", mci("configure m", NULL, 0) == 0);
    WaitForSingleObject(thread, 15000);
    printf("saved=%d full=%lu zoom=%lu skip=%lu\n", reg_value("FullScreen") == 1 && reg_value("ZoomBy2") == 1 &&
           reg_value("SkipFrames") == 1, reg_value("FullScreen"), reg_value("ZoomBy2"), reg_value("SkipFrames"));
    mci("close m", NULL, 0);

    /* the options at work */
    mci(cmd, NULL, 0);
    mci("where m destination", ret, sizeof(ret));
    printf("dest_zoomed=%d\n", !strcmp(ret, "0 0 64 32"));
    mci("play m", NULL, 0);
    full.hwnd = NULL;
    full.width = GetSystemMetrics(SM_CXSCREEN);
    full.height = GetSystemMetrics(SM_CYSCREEN);
    for (i = 0; i < 30 && !full.hwnd; i++)
    {
        Sleep(100);
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        EnumWindows(find_full, (LPARAM)&full);
    }
    printf("plays_full_screen=%d\n", full.hwnd != NULL);
    mci("stop m", NULL, 0);
    mci("close m", NULL, 0);

    /* Defaults then Cancel keeps them */
    mci(cmd, NULL, 0);
    stage = 1;
    thread = CreateThread(NULL, 0, watcher, NULL, 0, NULL);
    mci("configure m", NULL, 0);
    WaitForSingleObject(thread, 15000);
    printf("cancel_keeps=%d\n", reg_value("FullScreen") == 1 && reg_value("ZoomBy2") == 1);
    mci("close m", NULL, 0);

    RegDeleteKeyA(HKEY_CURRENT_USER, "Software\\Stained Glass\\MCIAVI");
    DeleteFileA(avi);
    printf("done=1\n");
    return 0;
}
