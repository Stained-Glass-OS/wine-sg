/* embedconf-gate.sh's stand-in for a Qt program with dock layouts (Kdenlive,
 * OBS: David 2026-11-15, "UI collapses on minimize and maximize"): Qt lays
 * its docks out again for every size its window is given, and a tiny one
 * squeezes them to their minimums, where they stay. This window logs every
 * ConfigureNotify it gets, one line each:
 *   main XID
 *   conf SEQ W H X Y SYNTHETIC
 * (the gate appends "mark STEP" lines to the same log between its steps, so
 * the log says between which steps a size came).
 *
 *   embedconf-app LOG
 */
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main(int argc, char **argv)
{
    Display *d;
    Window w;
    XSetWindowAttributes swa;
    XEvent ev;
    FILE *logf;
    Atom wm_delete;
    unsigned long seq = 0;
    XSizeHints sh = { 0 };

    if (argc < 2 || !(d = XOpenDisplay(NULL)) || !(logf = fopen(argv[1], "a"))) return 2;
    wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", False);
    swa.background_pixel = 0x40a040;
    swa.event_mask = StructureNotifyMask | ExposureMask;
    w = XCreateWindow(d, DefaultRootWindow(d), 150, 120, 500, 340, 0, CopyFromParent, InputOutput, CopyFromParent,
                      CWBackPixel | CWEventMask, &swa);
    XStoreName(d, w, "Dock Test");
    {
        XClassHint ch = { "docktest", "DockTest" };
        XSetClassHint(d, w, &ch);
    }
    /* Qt names a minimum size from its layout */
    sh.flags = PMinSize;
    sh.min_width = 200; sh.min_height = 150;
    XSetWMNormalHints(d, w, &sh);
    XSetWMProtocols(d, w, &wm_delete, 1);
    XMapWindow(d, w);
    fprintf(logf, "main %lu\n", w);
    fflush(logf);
    for (;;)
    {
        XNextEvent(d, &ev);
        if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) break;
        if (ev.type == ConfigureNotify && ev.xconfigure.window == w)
        {
            fprintf(logf, "conf %lu %d %d %d %d %d\n", ++seq, ev.xconfigure.width, ev.xconfigure.height,
                    ev.xconfigure.x, ev.xconfigure.y, ev.xconfigure.send_event ? 1 : 0);
            fflush(logf);
        }
    }
    return 0;
}
