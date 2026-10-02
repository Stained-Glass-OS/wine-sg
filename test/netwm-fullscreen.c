/* netwm-fullscreen WINDOW 1|0 [maximize] -- ask for full screen (or out of
 * it), or with "maximize" to be maximized (or restored), as a program does:
 * a _NET_WM_STATE client message to the root window (EWMH), as GTK sends
 * for Firefox's F11 or its restored maximized window.
 * netwm-fullscreen WINDOW preset-maximized -- _NET_WM_STATE maximized on
 * the window, as a window manager leaves a window it maximized.
 * For linuxembed-gate.sh. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

int main( int argc, char **argv )
{
    Display *d;
    XEvent ev = { 0 };
    Window w;

    if (argc < 3 || !(d = XOpenDisplay( NULL ))) return 2;
    w = strtoul( argv[1], NULL, 0 );
    if (!strcmp( argv[2], "preset-maximized" ))
    {
        Atom atoms[2] = { XInternAtom( d, "_NET_WM_STATE_MAXIMIZED_VERT", False ),
                          XInternAtom( d, "_NET_WM_STATE_MAXIMIZED_HORZ", False ) };
        XChangeProperty( d, w, XInternAtom( d, "_NET_WM_STATE", False ), XA_ATOM, 32, PropModeReplace,
                         (unsigned char *)atoms, 2 );
        XCloseDisplay( d );
        return 0;
    }
    ev.xclient.type = ClientMessage;
    ev.xclient.window = w;
    ev.xclient.message_type = XInternAtom( d, "_NET_WM_STATE", False );
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = atoi( argv[2] ) ? 1 : 0;   /* _NET_WM_STATE_ADD / _REMOVE */
    if (argc > 3 && !strcmp( argv[3], "maximize" ))
    {
        ev.xclient.data.l[1] = XInternAtom( d, "_NET_WM_STATE_MAXIMIZED_VERT", False );
        ev.xclient.data.l[2] = XInternAtom( d, "_NET_WM_STATE_MAXIMIZED_HORZ", False );
    }
    else ev.xclient.data.l[1] = XInternAtom( d, "_NET_WM_STATE_FULLSCREEN", False );
    ev.xclient.data.l[3] = 1;   /* from a program */
    XSendEvent( d, DefaultRootWindow( d ), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev );
    XCloseDisplay( d );
    return 0;
}
