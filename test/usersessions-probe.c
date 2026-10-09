/* Per-user sessions in a shared prefix (patches/sg/1706), run by
 * test/usersessions-gate.sh as several Unix users against one wineserver:
 *   hold NAME   create Local\sgsess-NAME and Global\sgsess-g-NAME events and a
 *               window titled sgsess-NAME, print the session, stay 240 s
 *   look NAME   print the session, whether Local\ and Global\ sgsess-NAME and
 *               the window sgsess-NAME can be found from here
 *   id          print the session only
 *   win         make a window and exit: 0 if it could */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    DWORD session = 0xdeadbeef;
    char name[128];

    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    printf("SESSION %lu\n", session);
    {
        char ws[64] = "", desk[64] = "";
        GetUserObjectInformationA(GetProcessWindowStation(), UOI_NAME, ws, sizeof(ws), NULL);
        GetUserObjectInformationA(GetThreadDesktop(GetCurrentThreadId()), UOI_NAME, desk, sizeof(desk), NULL);
        printf("DESKTOP %s\\%s\n", ws, desk);
    }
    fflush(stdout);
    if (argc < 2) return 1;
    if (!strcmp(argv[1], "id")) return 0;
    if (!strcmp(argv[1], "win"))
    {
        HWND hwnd = CreateWindowA("STATIC", "sgsess-warm", WS_OVERLAPPEDWINDOW, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
        printf("WIN %d %lu\n", hwnd != NULL, hwnd ? 0 : GetLastError());
        return !hwnd;
    }
    if (argc < 3) return 1;

    if (!strcmp(argv[1], "hold"))
    {
        HANDLE local, global;
        HWND hwnd;

        sprintf(name, "Local\\sgsess-%s", argv[2]);
        local = CreateEventA(NULL, TRUE, FALSE, name);
        sprintf(name, "Global\\sgsess-g-%s", argv[2]);
        global = CreateEventA(NULL, TRUE, FALSE, name);
        sprintf(name, "sgsess-%s", argv[2]);
        /* a user's first desktop can take a while to come up (its explorer
         * sets the display up): what is tested is where the window goes */
        for (int i = 0; i < 120; i++)
        {
            if ((hwnd = CreateWindowA("STATIC", name, WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, NULL, NULL))) break;
            Sleep(1000);
        }
        printf("HELD %d %d %d\n", local != NULL, global != NULL, hwnd != NULL);
        if (!hwnd) printf("ERROR %lu\n", GetLastError());
        fflush(stdout);
        Sleep(240000);
        return 0;
    }
    if (!strcmp(argv[1], "look"))
    {
        HANDLE h;

        sprintf(name, "Local\\sgsess-%s", argv[2]);
        h = OpenEventA(SYNCHRONIZE, FALSE, name);
        printf("LOCAL %d\n", h != NULL);
        if (h) CloseHandle(h);
        sprintf(name, "Global\\sgsess-g-%s", argv[2]);
        h = OpenEventA(SYNCHRONIZE, FALSE, name);
        printf("GLOBAL %d\n", h != NULL);
        if (h) CloseHandle(h);
        sprintf(name, "sgsess-%s", argv[2]);
        printf("WINDOW %d\n", FindWindowA(NULL, name) != NULL);
        return 0;
    }
    return 1;
}
