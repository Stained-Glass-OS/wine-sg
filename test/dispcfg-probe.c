/* dispcfg-probe: what a program's display setup sees -- the current mode and
 * how many modes the primary display offers -- for dispcfg-gate.sh. */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    DEVMODEW dm = { .dmSize = sizeof(dm) };
    DWORD i, n = 0;
    BOOL cur;
    if (argc > 1 && !freopen(argv[1], "w", stdout)) return 1;   /* run by explorer: no stdout */
    cur = EnumDisplaySettingsW(NULL, ENUM_CURRENT_SETTINGS, &dm);
    printf("CURRENT %d %lux%lu\n", cur, cur ? dm.dmPelsWidth : 0, cur ? dm.dmPelsHeight : 0);
    for (i = 0; ; i++)
    {
        DEVMODEW m = { .dmSize = sizeof(m) };
        if (!EnumDisplaySettingsW(NULL, i, &m)) break;
        if (m.dmPelsWidth && m.dmPelsHeight) n++;
    }
    printf("MODES %lu\n", n);
    fclose(stdout);
    return 0;
}
