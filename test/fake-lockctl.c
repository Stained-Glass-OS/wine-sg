/* xwin-taskbar-gate.sh's and elevmin-gate.sh's stand-in for sg-lockctl
 * (0496, 0500): XWINDOWS [--out FILE] copies the list file $SG_FAKE_DIR/list,
 * WINDOWS [--out FILE] the file $SG_FAKE_DIR/windows (to FILE, as sg-lockctl
 * does, or to its output); any other command is appended to
 * $SG_FAKE_DIR/commands. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *dir = getenv("SG_FAKE_DIR");
    char path[4096], part[4200], buf[4096];
    FILE *f, *out = stdout;
    size_t n;
    int i;

    if (!dir || argc < 2) return 2;
    if (!strcmp(argv[1], "XWINDOWS") || !strcmp(argv[1], "WINDOWS"))
    {
        if (argc == 4 && !strcmp(argv[2], "--out"))
        {
            snprintf(part, sizeof(part), "%s.part", argv[3]);
            if (!(out = fopen(part, "w"))) return 1;
        }
        snprintf(path, sizeof(path), "%s/%s", dir, argv[1][0] == 'X' ? "list" : "windows");
        if ((f = fopen(path, "r")))
        {
            while ((n = fread(buf, 1, sizeof(buf), f))) fwrite(buf, 1, n, out);
            fclose(f);
        }
        if (out != stdout && (fclose(out) || rename(part, argv[3]))) return 1;
        return 0;
    }
    snprintf(path, sizeof(path), "%s/commands", dir);
    if (!(f = fopen(path, "a"))) return 1;
    for (i = 1; i < argc; i++) fprintf(f, "%s%s", i > 1 ? " " : "", argv[i]);
    fputc('\n', f);
    fclose(f);
    puts("OK");
    return 0;
}
