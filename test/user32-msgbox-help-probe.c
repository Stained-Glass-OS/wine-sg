/* Probe for patches/sg/5206: the Help button of a MB_HELP message box acts like F1: the owner
 * gets WM_HELP (or the MSGBOXPARAMS callback is called) with a HELPINFO naming the button
 * (HELPINFO_WINDOW, control id IDHELP, the button's window) and the box's context help id.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static HELPINFO got;
static int got_count;
static const char *titles[] = { "SgHelpBoxCallback", "SgHelpBoxOwner" };
static HWND owner;

static void CALLBACK callback(LPHELPINFO info)
{
    got = *info;
    got_count++;
}

static LRESULT CALLBACK owner_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_HELP)
    {
        got = *(HELPINFO *)lp;
        got_count++;
        return TRUE;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static HWND found_dialog;
static HWND found_button;

static DWORD WINAPI clicker(void *arg)
{
    const char *title = arg;
    HWND dlg = NULL;
    int i;

    for (i = 0; i < 200 && !dlg; i++)
    {
        dlg = FindWindowA("#32770", title);
        if (!dlg) Sleep(25);
    }
    found_dialog = dlg;
    if (!dlg) return 1;
    found_button = GetDlgItem(dlg, IDHELP);
    if (found_button) SendMessageA(found_button, BM_CLICK, 0, 0);
    Sleep(100);
    PostMessageA(dlg, WM_COMMAND, IDOK, 0);
    return 0;
}

static void run_case(int use_callback)
{
    const char *title = titles[use_callback ? 0 : 1];
    MSGBOXPARAMSA mb = {0};
    HANDLE th;
    int ret;

    memset(&got, 0, sizeof(got));
    got_count = 0;
    found_dialog = found_button = NULL;
    mb.cbSize = sizeof(mb);
    mb.hwndOwner = owner;
    mb.hInstance = GetModuleHandleA(NULL);
    mb.lpszText = "text";
    mb.lpszCaption = title;
    mb.dwStyle = MB_OK | MB_HELP;
    mb.dwContextHelpId = 42;
    mb.lpfnMsgBoxCallback = use_callback ? callback : NULL;
    th = CreateThread(NULL, 0, clicker, (void *)title, 0, NULL);
    ret = MessageBoxIndirectA(&mb);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    check(ret == IDOK, "%s: box closed with %d", title, ret);
    check(found_dialog && found_button, "%s: box and its Help button exist", title);
    check(got_count == 1, "%s: help requested %d time(s)", title, got_count);
    check(got.iContextType == HELPINFO_WINDOW, "%s: context type %d", title, got.iContextType);
    check(got.iCtrlId == IDHELP, "%s: control id %d", title, got.iCtrlId);
    check(got.hItemHandle == found_button && got.hItemHandle != NULL, "%s: item is the button %p/%p", title, got.hItemHandle, found_button);
    check(got.dwContextId == 42, "%s: context help id %lu", title, got.dwContextId);
    check(got.cbSize == sizeof(HELPINFO), "%s: size %u", title, got.cbSize);
}

int main(void)
{
    WNDCLASSA wc = {0};

    wc.lpfnWndProc = owner_proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "SgHelpOwner";
    RegisterClassA(&wc);
    owner = CreateWindowA("SgHelpOwner", "owner", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, wc.hInstance, NULL);
    run_case(1);
    run_case(0);
    DestroyWindow(owner);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
