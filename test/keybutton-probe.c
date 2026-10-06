/* A stand-in for the touch keyboard (TabTip.exe through App Paths) for
 * test/keybutton-gate.sh: it writes how it was started to C:\tabtip.log and
 * stays a moment with the touch keyboard's window class. */
#include <windows.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
    WNDCLASSA wc = { 0 };
    FILE *f = fopen("C:\\tabtip.log", "a");
    (void)prev; (void)show;
    if (f) { fprintf(f, "started %s\n", cmd); fclose(f); }
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = inst;
    wc.lpszClassName = "IPTip_Main_Window";
    RegisterClassA(&wc);
    CreateWindowA("IPTip_Main_Window", "", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst, NULL);
    Sleep(4000);
    return 0;
}
