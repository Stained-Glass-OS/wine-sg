/* embedfs-gate.sh's stand-in for a full-screen game run under Proton
 * (Enshrouded, David 2026-11-15): a window that asks for full screen (EWMH
 * _NET_WM_STATE to the root, as SDL and Proton's winex11 do), then after
 * SECONDS goes away -- MODE destroy: XDestroyWindow, the program running on;
 * MODE exit: the process ends without destroying it (the X server does);
 * moveexit / keepexit: a click into it asks to be moved (_NET_WM_MOVERESIZE)
 * and it ends while the button is down -- keepexit keeping the press's grab.
 *   embedfs-app MODE SECONDS LOG     (LOG gets "main XID")
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

int main(int argc, char **argv)
{
    Display *d;
    Window w, root;
    XSetWindowAttributes swa;
    XEvent ev;
    FILE *logf;
    int secs;

    if (argc < 4 || !(d = XOpenDisplay(NULL)) || !(logf = fopen(argv[3], "a"))) return 2;
    secs = atoi(argv[2]);
    root = DefaultRootWindow(d);
    swa.background_pixel = 0x804020;
    swa.event_mask = StructureNotifyMask;
    w = XCreateWindow(d, root, 200, 150, 640, 400, 0, CopyFromParent, InputOutput, CopyFromParent,
                      CWBackPixel | CWEventMask, &swa);
    XStoreName(d, w, "Game");
    {
        XClassHint ch = { "game", "Game" };
        XSetClassHint(d, w, &ch);
    }
    XMapWindow(d, w);
    XSync(d, False);
    fprintf(logf, "main %lu\n", w);
    fflush(logf);
    sleep(3);   /* framed by now */
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom(d, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;   /* _NET_WM_STATE_ADD */
    ev.xclient.data.l[1] = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    ev.xclient.data.l[3] = 1;
    XSendEvent(d, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
    XFlush(d);
    if (!strcmp(argv[1], "moveexit") || !strcmp(argv[1], "keepexit"))
    {
        /* a click into it asks the window manager to move it (_NET_WM_MOVERESIZE, as SDL's
         * borderless windows and GTK's title bars do), and the program ends at once, the
         * button still down */
        XSelectInput(d, w, StructureNotifyMask | ButtonPressMask);
        for (;;)
        {
            XNextEvent(d, &ev);
            if (ev.type == ButtonPress) break;
        }
        {
            int rx = ev.xbutton.x_root, ry = ev.xbutton.y_root;
            memset(&ev, 0, sizeof(ev));
            ev.xclient.data.l[0] = rx;
            ev.xclient.data.l[1] = ry;
        }
        ev.xclient.type = ClientMessage;
        ev.xclient.window = w;
        ev.xclient.message_type = XInternAtom(d, "_NET_WM_MOVERESIZE", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[2] = 8;   /* move */
        ev.xclient.data.l[3] = 1;
        ev.xclient.data.l[4] = 1;
        /* keepexit: the press's own grab kept (as Steam, and a game confining the pointer) */
        if (strcmp(argv[1], "keepexit")) XUngrabPointer(d, CurrentTime);
        XSendEvent(d, root, False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
        XFlush(d);
        usleep(strcmp(argv[1], "keepexit") ? 300000 : 1000000);
        return 0;
    }
    sleep(secs);
    if (!strcmp(argv[1], "exit")) return 0;
    XDestroyWindow(d, w);
    XSync(d, False);
    fprintf(logf, "destroyed\n");
    fflush(logf);
    sleep(5);
    return 0;
}
