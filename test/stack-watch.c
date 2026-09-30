/* stack-watch MS: for MS milliseconds, watches the shell desktop's windows
 * restack and counts the times the window named "window" (topbar-probe's
 * red one) was put right above the taskbar (the 1024x40 window at the foot)
 * -- a window over the bar, if only until the bar raised itself again.
 * Prints: flashes restacks-of-the-window. test/topbar-gate.sh (0604). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static Window find_named(Display *d, Window w, const char *name)
{
    Window root, parent, *kids = NULL, found = 0;
    unsigned int n, i;
    char *wn = NULL;
    if (XFetchName(d, w, &wn) && wn)
    {
        XWindowAttributes a;
        int m = !strcmp(wn, name);
        XFree(wn);
        if (m && XGetWindowAttributes(d, w, &a) && a.map_state == IsViewable) return w;   /* the shown one */
    }
    if (!XQueryTree(d, w, &root, &parent, &kids, &n)) return 0;
    for (i = 0; i < n && !found; i++) found = find_named(d, kids[i], name);
    if (kids) XFree(kids);
    return found;
}

int main(int argc, char **argv)
{
    Display *d = XOpenDisplay(NULL);
    Window desk, red, bar = 0, root, parent, *kids = NULL;
    unsigned int n, i;
    struct timespec t0, t;
    int ms, flashes = 0, moves = 0;
    if (!d || argc < 2) return 2;
    ms = atoi(argv[1]);
    desk = find_named(d, DefaultRootWindow(d), "shell - Wine Desktop");
    red = find_named(d, DefaultRootWindow(d), "window");
    if (!desk || !red) { printf("none 0\n"); return 1; }
    XQueryTree(d, desk, &root, &parent, &kids, &n);
    for (i = 0; i < n; i++)
    {
        XWindowAttributes a;
        if (XGetWindowAttributes(d, kids[i], &a) && a.x == 0 && a.width == 1024 && a.height == 40) bar = kids[i];
    }
    if (kids) XFree(kids);
    if (!bar) { printf("nobar 0\n"); return 1; }
    XSelectInput(d, desk, SubstructureNotifyMask);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (;;)
    {
        while (XPending(d))
        {
            XEvent ev;
            XNextEvent(d, &ev);
            if (getenv("STACK_WATCH_DEBUG") && ev.type == ConfigureNotify && ev.xconfigure.window != red)
                fprintf(stderr, "other %lx above %lx serial %lu\n", ev.xconfigure.window, ev.xconfigure.above, ev.xconfigure.serial);
            if (ev.type == ConfigureNotify && ev.xconfigure.window == red)
            {
                moves++;
                if (ev.xconfigure.above == bar) flashes++;
                if (getenv("STACK_WATCH_DEBUG"))
                {
                    Window r2, p2, *k2 = NULL; unsigned int n2, j; int ri = -1, bi = -1;
                    XQueryTree(d, desk, &r2, &p2, &k2, &n2);
                    for (j = 0; j < n2; j++) { if (k2[j] == red) ri = j; if (k2[j] == bar) bi = j; }
                    fprintf(stderr, "red %lx above %lx bar %lx | now red at %d bar at %d of %u (serial %lu send %d)\n", red, ev.xconfigure.above, bar, ri, bi, n2, ev.xconfigure.serial, ev.xconfigure.send_event);
                    if (k2) XFree(k2);
                }
            }
        }
        clock_gettime(CLOCK_MONOTONIC, &t);
        if ((t.tv_sec - t0.tv_sec) * 1000 + (t.tv_nsec - t0.tv_nsec) / 1000000 > ms) break;
        { struct timespec nap = { 0, 1000000 }; nanosleep(&nap, NULL); }
    }
    printf("%d %d\n", flashes, moves);
    return 0;
}
