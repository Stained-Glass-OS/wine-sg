/* shellrestart-gate.sh's hands (0755).
 *   holddesk-probe SECONDS   a program on the desktop with no window on the
 *                            screen (a tray program, a crashed one held by its
 *                            debugger): the desktop outlives the shell */
#include <windows.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    {
        int s = argc > 1 ? atoi(argv[1]) : 60;
        HWND w = CreateWindowExA(0, "STATIC", "holddesk", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, 0, 0);
        GetDesktopWindow();
        Sleep(s * 1000);
        if (w) DestroyWindow(w);
    }
    return 0;
}
