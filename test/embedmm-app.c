/* embedmm-app.c: embedmm-gate.sh's stand-in for a full-screen client of two monitors, as
 * sdl-freerdp3 /multimon makes its windows (seen under the shell, wine-sg 7826): one window per
 * monitor (_MOTIF_WM_HINTS as SDL sets it), each fixed at its monitor's size and place (min = max);
 * the first full screen in its _NET_WM_STATE as it is mapped, the second asking for nothing more --
 * a window that cannot be resized, exactly its monitor.
 *   embedmm-app W H SECONDS LOG     (LOG gets "A XID" and "B XID")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

static Window make(Display *d, int x, int y, int w, int h, const char *name, int fs_initial)
{
    XSetWindowAttributes swa;
    XSizeHints *hints = XAllocSizeHints();
    long motif[5] = { 2, 0, 1, 0, 0 };   /* flags: decorations; decorations: all -- as SDL's */
    Window win;
    swa.background_pixel = 0x204080;
    win = XCreateWindow(d, DefaultRootWindow(d), x, y, w, h, 0, CopyFromParent, InputOutput, CopyFromParent,
                        CWBackPixel, &swa);
    XStoreName(d, win, name);
    hints->flags = USPosition | PMinSize | PMaxSize;
    hints->x = x; hints->y = y;
    hints->min_width = hints->max_width = w;
    hints->min_height = hints->max_height = h;
    XSetWMNormalHints(d, win, hints);
    XFree(hints);
    XChangeProperty(d, win, XInternAtom(d, "_MOTIF_WM_HINTS", False), XInternAtom(d, "_MOTIF_WM_HINTS", False), 32,
                    PropModeReplace, (unsigned char *)motif, 5);
    if (fs_initial)
    {
        Atom fs = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
        XChangeProperty(d, win, XInternAtom(d, "_NET_WM_STATE", False), XA_ATOM, 32, PropModeReplace,
                        (unsigned char *)&fs, 1);
    }
    return win;
}

int main(int argc, char **argv)
{
    Display *d;
    Window a, b;
    FILE *logf;
    int w, h;

    if (argc < 5 || !(d = XOpenDisplay(NULL)) || !(logf = fopen(argv[4], "a"))) return 2;
    w = atoi(argv[1]); h = atoi(argv[2]);
    a = make(d, 0, 0, w, h, "mmA", 1);
    b = make(d, w, 0, w, h, "mmB", 0);
    XMapWindow(d, a);
    XMapWindow(d, b);
    XSync(d, False);
    fprintf(logf, "A %lu\nB %lu\n", a, b);
    fflush(logf);
    sleep(atoi(argv[3]));
    return 0;
}
