/* movesize-probe rect|move|size|place: Notepad's window rectangle, or its menu's
 * Move / Size (WM_SYSCOMMAND SC_MOVE / SC_SIZE, as the menu posts them).
 * place puts it at 100,100, 500x400. (patches/sg/0476) */
#include <windows.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    HWND np = FindWindowW(L"Notepad", NULL);
    RECT rc;

    if (!np) { printf("none\n"); return 1; }
    if (argc > 1 && !strcmp(argv[1], "move")) PostMessageW(np, WM_SYSCOMMAND, SC_MOVE, 0);
    else if (argc > 1 && !strcmp(argv[1], "size")) PostMessageW(np, WM_SYSCOMMAND, SC_SIZE, 0);
    else if (argc > 1 && !strcmp(argv[1], "place")) SetWindowPos(np, NULL, 100, 100, 500, 400, SWP_NOZORDER);
    else
    {
        GetWindowRect(np, &rc);
        printf("%ld %ld %ld %ld\n", rc.left, rc.top, rc.right, rc.bottom);
    }
    return 0;
}
