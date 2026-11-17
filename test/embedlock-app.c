/* embedlock-app.c: embedlock-gate.sh's stand-in (wine-sg 7827) -- two windows framed by the shell, the X
 * focus passed between them about a thousand times a second while each asks the window manager for states
 * (_NET_WM_STATE to the root, maximized on and off), for SECONDS; then it ends.
 *   embedlock-app SECONDS LOG [fs]   (LOG gets "A XID", "B XID", then "go" when it starts, "done N")
 * fs: window A asks for full screen once framed, and the program ends in the middle of the storm, its
 * windows still up (the X server destroys them) -- as a full-screen game exits */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Window make(Display *d, int x, const char *name)
{
    XSetWindowAttributes swa;
    Window w;
    swa.background_pixel = 0x406080;
    w = XCreateWindow(d, DefaultRootWindow(d), x, 100, 300, 200, 0, CopyFromParent, InputOutput, CopyFromParent,
                      CWBackPixel, &swa);
    XStoreName(d, w, name);
    XMapWindow(d, w);
    return w;
}

static void ask_fs(Display *d, Window w)
{
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom(d, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;
    ev.xclient.data.l[1] = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    ev.xclient.data.l[3] = 1;
    XSendEvent(d, DefaultRootWindow(d), False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
}

static void ask(Display *d, Window w, long action)
{
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom(d, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = action;
    ev.xclient.data.l[1] = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    ev.xclient.data.l[2] = XInternAtom(d, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    ev.xclient.data.l[3] = 1;
    XSendEvent(d, DefaultRootWindow(d), False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
}

int main(int argc, char **argv)
{
    Display *d;
    Window a, b;
    FILE *logf;
    time_t end;
    long n = 0;

    if (argc < 3 || !(d = XOpenDisplay(NULL)) || !(logf = fopen(argv[2], "a"))) return 2;
    a = make(d, 100, "lockA");
    b = make(d, 500, "lockB");
    XSync(d, False);
    fprintf(logf, "A %lu\nB %lu\n", a, b);
    fflush(logf);
    while (access(argv[2], F_OK) == 0)   /* until the gate says "framed" (the log gains a "go" line) */
    {
        char line[64]; int go = 0;
        FILE *f = fopen(argv[2], "r");
        while (f && fgets(line, sizeof(line), f)) if (!strncmp(line, "framed", 6)) go = 1;
        if (f) fclose(f);
        if (go) break;
        usleep(100000);
    }
    if (argc > 3 && !strcmp(argv[3], "fs")) { ask_fs(d, a); XSync(d, False); sleep(2); }
    end = time(NULL) + atoi(argv[1]);
    while (time(NULL) < end)
    {
        XSetInputFocus(d, (n & 1) ? a : b, RevertToParent, CurrentTime);
        ask(d, (n & 1) ? b : a, n & 2 ? 1 : 0);
        XSync(d, False);
        usleep(1000);   /* a busy client, not a flood the shell needs minutes to read */
        n++;
    }
    if (argc > 3 && !strcmp(argv[3], "fs")) { fprintf(logf, "done %ld (ended in the storm)\n", n); fflush(logf); _exit(0); }
    XSync(d, False);
    fprintf(logf, "done %ld\n", n);
    fflush(logf);
    return 0;
}
