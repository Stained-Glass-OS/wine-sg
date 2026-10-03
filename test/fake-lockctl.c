/* xwin-taskbar-gate.sh's and elevmin-gate.sh's stand-in for sg-lockctl
 * (0496, 0500): XWINDOWS [--out FILE] copies the list file $SG_FAKE_DIR/list,
 * WINDOWS [--out FILE] the file $SG_FAKE_DIR/windows (to FILE, as sg-lockctl
 * does, or to its output); any other command is appended to
 * $SG_FAKE_DIR/commands. With SG_FAKE_FOCUS set it also acts on the list as
 * the compositor would: XDESKTOP takes the focus from every window,
 * XACTIVATE ID gives it to that one (desktopfront-gate.sh). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
        if (argv[1][0] == 'X' && getenv("SG_FAKE_XWLOG"))
        {
            /* when each list was asked for (ownframe-gate.sh: how soon after it a window is framed) */
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            if ((f = fopen(getenv("SG_FAKE_XWLOG"), "a"))) { fprintf(f, "%ld.%03ld\n", (long)ts.tv_sec, ts.tv_nsec / 1000000); fclose(f); }
        }
        return 0;
    }
    if (getenv("SG_FAKE_FOCUS") && (!strcmp(argv[1], "XDESKTOP") || (!strcmp(argv[1], "XACTIVATE") && argc > 2)))
    {
        char line[1024], out[16384] = "", id[32] = "";
        size_t len = 0;
        if (argc > 2) snprintf(id, sizeof(id), "%s ", argv[2]);
        snprintf(path, sizeof(path), "%s/list", dir);
        if ((f = fopen(path, "r")))
        {
            while (fgets(line, sizeof(line), f) && len < sizeof(out) - sizeof(line))
            {
                char *p = strstr(line, " focused "), *q = strstr(line, " - ");
                if (p && (!id[0] || strncmp(line, id, strlen(id))))
                {
                    /* " focused " -> " - " */
                    memmove(p + 3, p + 9, strlen(p + 9) + 1);
                    memcpy(p, " - ", 3);
                }
                else if (q && id[0] && !strncmp(line, id, strlen(id)))
                {
                    char rest[1024];
                    snprintf(rest, sizeof(rest), "%s", q + 3);
                    snprintf(q, sizeof(line) - (q - line), " focused %s", rest);
                }
                len += snprintf(out + len, sizeof(out) - len, "%s", line);
            }
            fclose(f);
            snprintf(part, sizeof(part), "%s.part", path);
            if ((f = fopen(part, "w"))) { fputs(out, f); fclose(f); rename(part, path); }
        }
    }
    snprintf(path, sizeof(path), "%s/commands", dir);
    if (!(f = fopen(path, "a"))) return 1;
    for (i = 1; i < argc; i++) fprintf(f, "%s%s", i > 1 ? " " : "", argv[i]);
    fputc('\n', f);
    fclose(f);
    puts("OK");
    return 0;
}
