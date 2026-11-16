/* embedmenu-gate.sh's stand-in for a Linux program whose menus are windows
 * of their own (Steam's View, Friends, Games and Help menus, David
 * 2026-11-15): a main window (framed by the taskbar, wine-sg 0762) that opens
 * a menu window under a click, which closes as such menus do -- on losing the
 * X focus, on being unmapped or reparented, or (override-redirect) on its
 * pointer or keyboard grab being broken. Every event that matters is written
 * to the log, one line each:
 *   main XID / open XID / close REASON
 *
 *   embedmenu-app MODE LOG
 * MODE: transient  managed, WM_TRANSIENT_FOR the main window, no title, no
 *                  decorations (_MOTIF_WM_HINTS), _NET_WM_STATE_SKIP_TASKBAR
 *       menu       managed, _NET_WM_WINDOW_TYPE_POPUP_MENU, not transient
 *       plain      managed, no title, no decorations, nothing else (a WM
 *                  could take it for any window)
 *       or         override-redirect, pointer and keyboard grabbed
 * A managed menu takes the X focus itself once mapped (in the session the
 * compositor's window manager gives it); Escape or a click outside closes it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

static FILE *logf;
/* as every toolkit does (EWMH): the process that made the window */
static void set_pid(Display *d, Window w)
{
    long pid = getpid();
    XChangeProperty(d, w, XInternAtom(d, "_NET_WM_PID", False), XA_CARDINAL, 32, PropModeReplace,
                    (unsigned char *)&pid, 1);
}
static void say(const char *fmt, unsigned long v, const char *s)
{
    if (s) fprintf(logf, fmt, s); else fprintf(logf, fmt, v);
    fputc('\n', logf);
    fflush(logf);
}

int main(int argc, char **argv)
{
    Display *d;
    Window root, main_win, menu = 0;
    XSetWindowAttributes swa;
    XEvent ev;
    const char *mode;
    int grabbed = 0, focused = 0;
    Atom wm_delete;

    if (argc < 3 || !(d = XOpenDisplay(NULL)) || !(logf = fopen(argv[2], "a"))) return 2;
    mode = argv[1];
    root = DefaultRootWindow(d);
    wm_delete = XInternAtom(d, "WM_DELETE_WINDOW", False);

    swa.background_pixel = 0x2050c0;
    swa.event_mask = ButtonPressMask | KeyPressMask | StructureNotifyMask | ExposureMask;
    main_win = XCreateWindow(d, root, 100, 100, 420, 260, 0, CopyFromParent, InputOutput, CopyFromParent,
                             CWBackPixel | CWEventMask, &swa);
    XStoreName(d, main_win, "Menu Test");
    {
        XClassHint ch = { "menutest", "MenuTest" };
        XSetClassHint(d, main_win, &ch);
    }
    XSetWMProtocols(d, main_win, &wm_delete, 1);
    set_pid(d, main_win);
    XMapWindow(d, main_win);
    say("main %lu", main_win, NULL);

    for (;;)
    {
        XNextEvent(d, &ev);
        if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete) break;
        if (ev.type == ButtonPress && ev.xbutton.window == main_win && !menu)
        {
            int rx = ev.xbutton.x_root, ry = ev.xbutton.y_root + 4;
            int or = !strcmp(mode, "or");
            swa.background_pixel = 0xd02020;
            swa.override_redirect = or;
            swa.event_mask = ButtonPressMask | KeyPressMask | StructureNotifyMask | FocusChangeMask | ExposureMask;
            menu = XCreateWindow(d, root, rx, ry, 160, 120, 0, CopyFromParent, InputOutput, CopyFromParent,
                                 CWBackPixel | CWEventMask | CWOverrideRedirect, &swa);
            if (!or)
            {
                XClassHint ch = { "menutest", "MenuTest" };
                XSizeHints sh = { 0 };
                XSetClassHint(d, menu, &ch);
                XStoreName(d, menu, "");
                sh.flags = USPosition | PPosition;
                sh.x = rx; sh.y = ry;
                XSetWMNormalHints(d, menu, &sh);
                if (strcmp(mode, "menu"))
                {
                    long hints[5] = { 2 /* MWM_HINTS_DECORATIONS */, 0, 0, 0, 0 };
                    Atom m = XInternAtom(d, "_MOTIF_WM_HINTS", False);
                    XChangeProperty(d, menu, m, m, 32, PropModeReplace, (unsigned char *)hints, 5);
                }
                if (!strcmp(mode, "transient"))
                {
                    Atom skip = XInternAtom(d, "_NET_WM_STATE_SKIP_TASKBAR", False);
                    XSetTransientForHint(d, menu, main_win);
                    XChangeProperty(d, menu, XInternAtom(d, "_NET_WM_STATE", False), XA_ATOM, 32,
                                    PropModeReplace, (unsigned char *)&skip, 1);
                }
                if (!strcmp(mode, "menu"))
                {
                    Atom t = XInternAtom(d, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
                    XChangeProperty(d, menu, XInternAtom(d, "_NET_WM_WINDOW_TYPE", False), XA_ATOM, 32,
                                    PropModeReplace, (unsigned char *)&t, 1);
                }
            }
            set_pid(d, menu);
            /* a toolkit takes a moment to lay its menu out (CEF tens of ms):
             * mapped in the click's own millisecond, a focus asked for at the
             * click's time ties with the menu's and wins (X compares times) */
            XFlush(d);
            usleep(30000);
            XMapRaised(d, menu);
            continue;
        }
        if (!menu) continue;
        if (ev.type == MapNotify && ev.xmap.window == menu)
        {
            /* shown (as the compositor lists it: a mapped window) */
            say("open %lu", menu, NULL);
            if (!strcmp(mode, "or"))
            {
                int p = XGrabPointer(d, menu, False, ButtonPressMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
                                     None, None, CurrentTime);
                int k = XGrabKeyboard(d, menu, False, GrabModeAsync, GrabModeAsync, CurrentTime);
                grabbed = p == GrabSuccess && k == GrabSuccess;
                if (!grabbed) say("close grab-refused %s", 0, p != GrabSuccess ? "pointer" : "keyboard");
            }
            else XSetInputFocus(d, menu, RevertToParent, CurrentTime);
            continue;
        }
        {
            const char *why = NULL;
            if (getenv("EMBEDMENU_VERBOSE") && (ev.type == FocusIn || ev.type == FocusOut))
                fprintf(logf, "# %s mode %d detail %d serial %lu\n", ev.type == FocusIn ? "in" : "out",
                        ev.xfocus.mode, ev.xfocus.detail, ev.xfocus.serial);
            if (ev.type == FocusIn && ev.xfocus.window == menu && ev.xfocus.detail != NotifyPointer) focused = 1;
            if (ev.type == FocusOut && ev.xfocus.window == menu && ev.xfocus.detail != NotifyInferior &&
                ev.xfocus.detail != NotifyPointer)
            {
                /* a managed menu closes when the focus goes; an override-redirect
                 * one when its grab is taken from it */
                if (!strcmp(mode, "or") ? ev.xfocus.mode == NotifyUngrab && grabbed : focused && ev.xfocus.mode == NotifyNormal)
                    why = "focus-out";
            }
            if (ev.type == UnmapNotify && ev.xunmap.window == menu) why = "unmapped";
            if (ev.type == ReparentNotify && ev.xreparent.window == menu) why = "reparented";
            if (ev.type == KeyPress && XLookupKeysym(&ev.xkey, 0) == XK_Escape) why = "escape";
            if (ev.type == ButtonPress && ev.xbutton.window == menu &&
                (ev.xbutton.x < 0 || ev.xbutton.y < 0 || ev.xbutton.x >= 160 || ev.xbutton.y >= 120))
                why = "click-outside";
            if (why)
            {
                say("close %s", 0, why);
                if (grabbed) { XUngrabKeyboard(d, CurrentTime); XUngrabPointer(d, CurrentTime); grabbed = 0; }
                XDestroyWindow(d, menu);
                XSync(d, False);
                menu = 0;
                focused = 0;
            }
        }
    }
    return 0;
}
