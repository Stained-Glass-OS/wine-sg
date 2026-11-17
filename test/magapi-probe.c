/* magapi-probe: the Magnification API on the compositor (magnify.exe's and other programs') -- for
 * magapi-gate.sh. Prints one line per call. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { float transform[5][5]; } MAGCOLOREFFECT;

int main(int argc, char **argv)
{
    HMODULE m = LoadLibraryA("magnification.dll");
    BOOL (WINAPI *init)(void), (WINAPI *uninit)(void);
    BOOL (WINAPI *set)(float, int, int), (WINAPI *get)(float *, int *, int *);
    BOOL (WINAPI *seteff)(MAGCOLOREFFECT *), (WINAPI *cmd)(const char *);
    MAGCOLOREFFECT inv = {{ { -1, 0, 0, 0, 0 }, { 0, -1, 0, 0, 0 }, { 0, 0, -1, 0, 0 }, { 0, 0, 0, 1, 0 }, { 1, 1, 1, 0, 1 } }};
    float level = 0; int x = -1, y = -1;

    if (argc > 1 && !strcmp(argv[1], "aware")) SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if (!m) { printf("no dll\n"); return 1; }
    init = (void *)GetProcAddress(m, "MagInitialize");
    uninit = (void *)GetProcAddress(m, "MagUninitialize");
    set = (void *)GetProcAddress(m, "MagSetFullscreenTransform");
    get = (void *)GetProcAddress(m, "MagGetFullscreenTransform");
    seteff = (void *)GetProcAddress(m, "MagSetFullscreenColorEffect");
    cmd = (void *)GetProcAddress(m, "SgMagCommand");
    if (!init || !uninit || !set || !get || !seteff || !cmd) { printf("missing exports\n"); return 1; }
    printf("init %d\n", init());
    printf("set %d\n", set(2.5f, 100, 50));
    get(&level, &x, &y);
    printf("get %d %d %d\n", (int)(level * 100 + 0.5f), x, y);
    printf("effect %d\n", seteff(&inv));
    printf("lens %d\n", cmd("lens 200 300 200"));
    printf("bad %d\n", cmd("nonsense"));
    printf("small %d\n", set(0.5f, 0, 0));
    fflush(stdout);
    Sleep(500);
    printf("uninit %d\n", uninit());
    fflush(stdout);
    Sleep(1500);
    printf("done\n");
    return 0;
}
