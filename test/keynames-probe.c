/* keynames-probe.c (wine-sg 6093): GetKeyNameTextW for keys that type no character -- the names Windows
 * gives them, not X keysym names. Writes KEY=VALUE lines to the file named by argv[1]. */
#include <windows.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    static const struct { const char *name; LONG lparam; } keys[] =
    {
        { "ctrl", 0x1d << 16 }, { "rctrl", (0x1d << 16) | (1 << 24) }, { "shift", 0x2a << 16 },
        { "rshift", 0x36 << 16 }, { "alt", 0x38 << 16 }, { "ralt", (0x38 << 16) | (1 << 24) },
        { "ctrl_dontcare", (0x1d << 16) | 0x2000000 }, { "rctrl_dontcare", (0x1d << 16) | (1 << 24) | 0x2000000 },
        { "a", 0x1e << 16 }, { "f1", 0x3b << 16 }, { "enter", 0x1c << 16 }, { "del", (0x53 << 16) | (1 << 24) },
        { "space", 0x39 << 16 }, { "esc", 0x01 << 16 }, { "bksp", 0x0e << 16 }, { "pgup", (0x49 << 16) | (1 << 24) },
        { "num7", 0x47 << 16 }, { "caps", 0x3a << 16 }, { "win", (0x5b << 16) | (1 << 24) },
    };
    FILE *f = fopen(argv[1], "w");
    unsigned int i;

    for (i = 0; i < ARRAYSIZE(keys); i++)
    {
        WCHAR buf[64] = {0};
        GetKeyNameTextW(keys[i].lparam, buf, ARRAYSIZE(buf));
        fprintf(f, "%s=%ls\n", keys[i].name, buf);
    }
    fprintf(f, "done=1\n");
    fclose(f);
    return 0;
}
