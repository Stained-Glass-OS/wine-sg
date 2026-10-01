/* A screenshot tool's Print Screen (patches/sg/0640): registers Print Screen
 * as its hotkey, as Greenshot does, prints "registered=1|0", then logs
 * "hotkey" to C:\prtsc.log when the key reaches it (20 s at most). */
#include <windows.h>
#include <stdio.h>

int main(void)
{
    DWORD start = GetTickCount();
    MSG msg;
    BOOL ok = RegisterHotKey(NULL, 1, MOD_NOREPEAT, VK_SNAPSHOT);
    printf("registered=%d\n", ok);
    fflush(stdout);
    if (!ok) return 1;
    while (GetTickCount() - start < 20000)
    {
        if (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_HOTKEY)
            {
                FILE *f = fopen("C:\\prtsc.log", "a");
                if (f) { fputs("hotkey\n", f); fclose(f); }
                break;
            }
            DispatchMessageW(&msg);
        }
        else Sleep(20);
    }
    UnregisterHotKey(NULL, 1);
    return 0;
}
