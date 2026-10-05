/* csdmaxloop-ask XID 1|0 -- ask the window manager to maximize (1) or
 * restore (0) window XID, as a program's own maximize button does
 * (_NET_WM_STATE to the root). For csdmaxloop-gate.sh.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdlib.h>
#include <X11/Xlib.h>

int main( int argc, char **argv )
{
    Display *d;
    XEvent m = { 0 };

    if (argc < 3 || !(d = XOpenDisplay( NULL ))) return 2;
    m.xclient.type = ClientMessage;
    m.xclient.window = strtoul( argv[1], NULL, 0 );
    m.xclient.message_type = XInternAtom( d, "_NET_WM_STATE", False );
    m.xclient.format = 32;
    m.xclient.data.l[0] = atoi( argv[2] ) ? 1 : 0;   /* add, remove */
    m.xclient.data.l[1] = XInternAtom( d, "_NET_WM_STATE_MAXIMIZED_VERT", False );
    m.xclient.data.l[2] = XInternAtom( d, "_NET_WM_STATE_MAXIMIZED_HORZ", False );
    m.xclient.data.l[3] = 1;
    XSendEvent( d, DefaultRootWindow( d ), False, SubstructureRedirectMask | SubstructureNotifyMask, &m );
    XCloseDisplay( d );
    return 0;
}
