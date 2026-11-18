/* embedgame-gate.sh's stand-in for a game run by Linux Steam under Proton (David 2026-11-17:
 * Skyrim opened "really small" in its frame until Win+Up). A Wine/Proton game window (an X
 * top-level, no decorations: winex11 gives a WS_POPUP window _MOTIF_WM_HINTS decorations 0)
 * that goes full screen one of the ways Wine and Proton's winex11 do:
 *   fsstate: mapped small (640x400), framed, then sized to the whole screen (root
 *            coordinates) and _NET_WM_STATE_FULLSCREEN asked of the root
 *   stateonly: mapped small, then only _NET_WM_STATE_FULLSCREEN asked, and sized after
 *            (a d3d9 exclusive switch: the state first, the mode's size after)
 *   resize:  mapped small, then a plain XMoveResizeWindow to the whole screen, no state
 *   born:    created already the size of the screen at 0,0, no full-screen state
 *   early:   mapped small, then at once (before it is framed) sized to the screen and
 *            _NET_WM_STATE_FULLSCREEN asked -- a game that starts full screen
 *   earlystate: mapped small and _NET_WM_STATE_FULLSCREEN asked at once (before it is framed),
 *            the sizing left to the window manager -- as SDL and native games do
 *   back:    as resize, then 6 s later sized back to 800x600 at 100,80 (full screen to windowed)
 *   embedgame-app MODE LOG    (LOG gets "main XID", then "size W H" for each size it is given)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

static void no_decorations(Display *d, Window w)
{
    long hints[5] = { 1 << 1, 0, 0, 0, 0 };   /* MWM_HINTS_DECORATIONS: none */
    Atom a = XInternAtom(d, "_MOTIF_WM_HINTS", False);
    XChangeProperty(d, w, a, a, 32, PropModeReplace, (unsigned char *)hints, 5);
}

static void ask_fullscreen(Display *d, Window w)
{
    XEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom(d, "_NET_WM_STATE", False);
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = 1;   /* _NET_WM_STATE_ADD */
    ev.xclient.data.l[1] = XInternAtom(d, "_NET_WM_STATE_FULLSCREEN", False);
    ev.xclient.data.l[3] = 1;
    XSendEvent(d, DefaultRootWindow(d), False, SubstructureNotifyMask | SubstructureRedirectMask, &ev);
}

int main(int argc, char **argv)
{
    Display *d;
    Window w, root;
    XSetWindowAttributes swa;
    XEvent ev;
    FILE *logf;
    int sw, sh, born, lw = 0, lh = 0;

    if (argc < 3 || !(d = XOpenDisplay(NULL)) || !(logf = fopen(argv[2], "a"))) return 2;
    root = DefaultRootWindow(d);
    sw = DisplayWidth(d, DefaultScreen(d));
    sh = DisplayHeight(d, DefaultScreen(d));
    born = !strcmp(argv[1], "born");
    swa.background_pixel = 0x204080;
    swa.event_mask = StructureNotifyMask;
    w = XCreateWindow(d, root, born ? 0 : 200, born ? 0 : 150, born ? sw : 640, born ? sh : 400, 0,
                      CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &swa);
    XStoreName(d, w, "Game");
    {
        XClassHint ch = { "steam_app_72850", "steam_app_72850" };
        XSetClassHint(d, w, &ch);
    }
    no_decorations(d, w);
    XMapWindow(d, w);
    XSync(d, False);
    fprintf(logf, "main %lu\n", w);
    lw = born ? sw : 640; lh = born ? sh : 400;
    fprintf(logf, "size %d %d\n", lw, lh);
    fflush(logf);
    if (!strcmp(argv[1], "earlystate"))
    {
        ask_fullscreen(d, w);
        XFlush(d);
    }
    else if (!strcmp(argv[1], "early"))
    {
        XMoveResizeWindow(d, w, 0, 0, sw, sh);
        ask_fullscreen(d, w);
        XFlush(d);
    }
    else if (!born)
    {
        /* framed by now (the shell asks the compositor's list every second) */
        int i;
        for (i = 0; i < 40; i++)
        {
            while (XPending(d)) XNextEvent(d, &ev);
            usleep(100000);
        }
        if (!strcmp(argv[1], "fsstate"))
        {
            XMoveResizeWindow(d, w, 0, 0, sw, sh);
            ask_fullscreen(d, w);
        }
        else if (!strcmp(argv[1], "stateonly"))
        {
            ask_fullscreen(d, w);
            XFlush(d);
            usleep(300000);
            XMoveResizeWindow(d, w, 0, 0, sw, sh);
        }
        else XMoveResizeWindow(d, w, 0, 0, sw, sh);
        XFlush(d);
        if (!strcmp(argv[1], "back"))
        {
            int i;
            for (i = 0; i < 60; i++)
            {
                while (XPending(d))
                {
                    XNextEvent(d, &ev);
                    if (ev.type == ConfigureNotify && ev.xconfigure.window == w &&
                        (ev.xconfigure.width != lw || ev.xconfigure.height != lh))
                    {
                        lw = ev.xconfigure.width; lh = ev.xconfigure.height;
                        fprintf(logf, "size %d %d\n", lw, lh);
                        fflush(logf);
                    }
                }
                usleep(100000);
            }
            fprintf(logf, "windowed\n");
            fflush(logf);
            XMoveResizeWindow(d, w, 100, 80, 800, 600);
            XFlush(d);
        }
    }
    for (;;)
    {
        XNextEvent(d, &ev);
        if (ev.type == ConfigureNotify && ev.xconfigure.window == w &&
            (ev.xconfigure.width != lw || ev.xconfigure.height != lh))
        {
            lw = ev.xconfigure.width; lh = ev.xconfigure.height;
            fprintf(logf, "size %d %d\n", lw, lh);
            fflush(logf);
        }
        if (ev.type == DestroyNotify && ev.xdestroywindow.window == w) break;
    }
    return 0;
}
